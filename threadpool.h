#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>
#include <vector>
#include <queue>
#include <functional>
#include <exception>
#include <iostream> // Debug
#include "request.h"
#include "connectionraii.h"

using namespace std;

class ThreadPool
{
private:
    //typedef function<void()> Task;
    vector<pthread_t> threads;
    queue<Request *> tasks;

    int thread_num;
    int max_requests;

    pthread_mutex_t queue_mutex;
    pthread_cond_t cv;
    bool is_stop;

    ConnectionPool *conn_pool;
    static void *worker(void* arg);
    void run();
    Request* take();

    void start();
    void stop();

public:
    ThreadPool(ConnectionPool* _conn_pool, int _thread_num = 5, int _max_requests = 10000);
    ~ThreadPool();   
    bool add_task(Request* request, int _state);
};

#endif // THREADPOOL_H
