#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H

#include <pthread.h>
#include <iostream>

using namespace std;

template <class T>
class BlockQueue
{
private:
    int max_size;
    int cur_size;
    int front;
    int back;
    T* arr;
    pthread_mutex_t block_mutex;
    pthread_cond_t block_cv;

public:
    BlockQueue(int _max_size = 1000) :max_size(_max_size),cur_size(0),
        front(-1), back(-1)
    {
        if (max_size <= 0) {
            exit(1);
        }
        arr = new T[max_size];
        block_mutex = PTHREAD_MUTEX_INITIALIZER;
        block_cv = PTHREAD_COND_INITIALIZER;
    }

    ~BlockQueue(){
        pthread_mutex_lock(&block_mutex);
        if (arr != NULL) {
            delete []arr;
        }
        pthread_mutex_unlock(&block_mutex);

        pthread_mutex_destroy(&block_mutex);
        pthread_cond_destroy(&block_cv);
    }

    bool push(const T& item) {
        pthread_mutex_lock(&block_mutex);
        if (cur_size >= max_size) {
            pthread_cond_broadcast(&block_cv);
            pthread_mutex_unlock(&block_mutex);
            return false;
        }
        back = (back + 1) % max_size;
        arr[back] = item;
        cur_size++;
        pthread_cond_broadcast(&block_cv);
        pthread_mutex_unlock(&block_mutex);
        return true;
    }

    bool pop(T& item) {
        pthread_mutex_lock(&block_mutex);
        if (cur_size <= 0) {
            if (pthread_cond_wait(&block_cv, &block_mutex)) {
               pthread_mutex_unlock(&block_mutex);
               return false;
            }
        }
        front = (front + 1) % max_size;
        item = arr[front];
        cur_size--;
        pthread_mutex_unlock(&block_mutex);
        return true;
    }

    bool is_empty(){
        pthread_mutex_lock(&block_mutex);
        if (cur_size == 0) {
            pthread_mutex_unlock(&block_mutex);
            return true;
        }
        pthread_mutex_unlock(&block_mutex);
        return false;
    }

    bool is_full(){
        pthread_mutex_lock(&block_mutex);
        if (cur_size >= max_size) {
            pthread_mutex_unlock(&block_mutex);
            return true;
        }
        pthread_mutex_unlock(&block_mutex);
        return false;
    }

    bool get_front(T& item) {
        pthread_mutex_lock(&block_mutex);
        if (cur_size == 0) {
            pthread_mutex_unlock(&block_mutex);
            return false;
        }
        item = arr[front];
        pthread_mutex_unlock(&block_mutex);
        return true;
    }

    bool get_back(T& item) {
        pthread_mutex_lock(&block_mutex);
        if (cur_size == 0) {
            pthread_mutex_unlock(&block_mutex);
            return false;
        }
        item = arr[back];
        pthread_mutex_unlock(&block_mutex);
        return true;
    }

    void clear(){
        pthread_mutex_lock(&block_mutex);
        cur_size = 0;
        front = -1;
        back = -1;
        pthread_mutex_unlock(&block_mutex);
    }
};

#endif // BLOCKQUEUE_H
