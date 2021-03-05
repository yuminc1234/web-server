#include "threadpool.h"

ThreadPool::ThreadPool(ConnectionPool* _conn_pool, int _thread_num, int _max_requests):
    thread_num(_thread_num), max_requests(_max_requests), is_stop(true),
       	conn_pool(_conn_pool)
{
    if (thread_num <= 0 || max_requests <= 0) {
        throw exception();
    }
     queue_mutex = PTHREAD_MUTEX_INITIALIZER;
     cv = PTHREAD_COND_INITIALIZER;
     start();
}

ThreadPool::~ThreadPool() {
    if (!is_stop) {
        stop();
    }
}

void ThreadPool::start() {
    is_stop = false;
    threads.reserve(thread_num);
    for (int i = 0; i < thread_num; i++) {
        if ( pthread_create(&threads[i], NULL, worker, this) != 0 ) {
            throw exception();
        }
        if ( pthread_detach(threads[i]) ) {
            throw exception();
        }
    }
}

void ThreadPool::stop() {
    if (is_stop)
        return;
    pthread_mutex_lock(&queue_mutex);
    is_stop = true;
    pthread_cond_broadcast(&cv);
    pthread_mutex_unlock(&queue_mutex);    
    pthread_mutex_destroy(&queue_mutex);
    pthread_cond_destroy(&cv);
}

bool ThreadPool::add_task(Request* request, int _state) {
    pthread_mutex_lock(&queue_mutex);
    if (tasks.size() >= max_requests) {
        pthread_mutex_unlock(&queue_mutex);
        return false;
    }
    tasks.push(request);
    request->state = _state;
    pthread_cond_signal(&cv);
    pthread_mutex_unlock(&queue_mutex);
    return true;
}

void* ThreadPool::worker(void* arg) {
    ThreadPool *pool = (ThreadPool *)arg;
    pool->run();
    return NULL;
}

void ThreadPool::run() {
    while(!is_stop) {
	  Request* request = take();
        if (request) {
            if (request->state == 0) {
                if (request->read()) {
                    request->improv = 1;
                    ConnectionRAII connectionRAII(&request->mysql, conn_pool);
                    request->process();
                } else {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            } else {
                if (request->write()) {
                    request->improv = 1;
                } else {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        }
    }
}

Request* ThreadPool::take() {
    pthread_mutex_lock(&queue_mutex);
    while(tasks.empty() && !is_stop) {
        pthread_cond_wait(&cv, &queue_mutex);
    }
    if (is_stop) {
        pthread_mutex_unlock(&queue_mutex);
        pthread_exit(NULL);
    }
    Request* request;
    request = tasks.front();
    tasks.pop();
    pthread_mutex_unlock(&queue_mutex);
    return request;
}


