#ifndef TIMER_H
#define TIMER_H

#include <iostream>
#include <netinet/in.h>
#include <time.h>
#include <signal.h>
#include<assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_map>

#include "request.h"

using namespace std;

class HeapTimer;

struct client_data {
    sockaddr_in address;
    int sockfd;
    HeapTimer* timer;
};

class HeapTimer {
public:
    int id;
    time_t expire;
    client_data* user_data;
    // Callback function
    void (*cb_func)(client_data *);
    HeapTimer(){}
    HeapTimer(int delay) {
        expire = time(NULL) + delay;
    }
};

class TimeHeap {
private:
    const int TIME_SLOT = 15; // 15s
    HeapTimer** array;
    int capacity;
    int cur_size;
    int arr_id;
    unordered_map<int, int> hash_map;

    void sift_down(int parent);
    void sift_up(int child);
    void heap_swap(int pos1, int pos2);
    void resize();

public:
    TimeHeap(int cap);
    ~TimeHeap();
    void add_timer(HeapTimer* timer);
    void del_timer(HeapTimer* timer);
    void adjust_timer(HeapTimer* timer);
    void pop_timer();
    int tick();
    bool is_empty() const {
        return cur_size == 0;
    }
};

class Utils {
private:
    const int CAPACITY = 1000;
    // int epollfd;

    Utils(){}
    ~Utils(){}
    Utils(const Utils&) = delete;
    Utils& operator=(const Utils&) = delete;    
    
public:
    static int* pipefd;
    
    TimeHeap timer_heap = TimeHeap(CAPACITY);
    static Utils* get_instance();
    void utils_init(int* _pipefd);
    void sigalrm_handler();
    static void sig_handler(int sig);
    void addsig(int sig, void(handler)(int));

};

void cb_func(client_data* user_data);

#endif // TIMER_H
