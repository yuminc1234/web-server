#include "threadpool.h"

ThreadPool::ThreadPool(ConnectionPool* _conn_pool, int _thread_num, int _max_requests):
    conn_pool(_conn_pool), thread_num(_thread_num), max_requests(_max_requests),
    is_stop(true)
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
       // cout << "create threads" << i << endl;
        if ( pthread_create(&threads[i], NULL, worker, this) != 0 ) {
            throw exception();
        }
        if ( pthread_detach(threads[i]) ) {
            throw exception();
        }
    }
}

void ThreadPool::stop() {
  //  cout << "ThreadPool::stop()" << endl;
    if (is_stop)
        return;
    pthread_mutex_lock(&queue_mutex);
    is_stop = true;
    pthread_cond_broadcast(&cv);
    pthread_mutex_unlock(&queue_mutex);    
    // threads.clear();
    pthread_mutex_destroy(&queue_mutex);
    pthread_cond_destroy(&cv);
    // cout << "ThreadPool::stop() end" <<endl;
}

bool ThreadPool::add_task(Request* request, int _state) {
    pthread_mutex_lock(&queue_mutex);
   // cout << "ThreadPool::add_task()" << pthread_self() << endl;
    if (tasks.size() >= max_requests) {
        pthread_mutex_unlock(&queue_mutex);
        cout << "too many requests" << endl;
        return false;
    }
    tasks.push(request);
    request->state = _state;
    pthread_cond_signal(&cv);
    pthread_mutex_unlock(&queue_mutex);
    return true;
}

void* ThreadPool::worker(void* arg) {
   // cout << "ThreadPool::worker() tid :" << pthread_self() << endl;
    ThreadPool *pool = (ThreadPool *)arg;
    pool->run();
    return NULL;
}

void ThreadPool::run() {
   // cout << "ThreadPool::run() tid : " <<  pthread_self() << " start." << endl;
    while(!is_stop) {
	Request* request = take();
       // cout << "get task()" << endl;
        if (request) {
	//	cout << "get task()" << endl;
	//	cout << "request state is" << request->state<<endl;
            // Read
            // come back to check?
            // ******************MODIFY************
            if (request->state == 0) {
                if (request->read()) {
                    ConnectionRAII connectionRAII(&request->mysql, conn_pool);
                    request->process();
                }
            } else {
                request->write();
            }
        }
    }
   // cout << "ThreadPool::run() tid : " << pthread_self() << " exit." << endl;
}

Request* ThreadPool::take() {
    pthread_mutex_lock(&queue_mutex);
    while(tasks.empty() && !is_stop) {
     //   cout << "is_stop:" << is_stop << endl;
     //   cout << "tasks.size():" << tasks.size() << endl;
     //   cout << "ThreadPool::take() tid : " << pthread_self() << " wait." << endl;
        pthread_cond_wait(&cv, &queue_mutex);
    }
    if (is_stop) {
        pthread_mutex_unlock(&queue_mutex);
        pthread_exit(NULL);
    }
  //  cout << "ThreadPool::take() tid : " << pthread_self() << " wake up." << endl;
    Request* request;
    request = tasks.front();
    tasks.pop();
    pthread_mutex_unlock(&queue_mutex);
    return request;
}


