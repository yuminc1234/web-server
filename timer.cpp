#include "timer.h"
#include "epoll.h"

TimeHeap::TimeHeap(int cap) : capacity(cap), cur_size(0), arr_id(0){
	/*
   try {
       	array = new (nothrow) HeapTimer*[capacity];
   } catch(bad_alloc) {
        throw exception();
    }*/
	array = new (nothrow) HeapTimer*[capacity];
	if (!array) {
		throw exception();
	}
    for (int i = 0; i < capacity; i++) {
        array[i] = nullptr;
    }
}

TimeHeap::~TimeHeap() {
    for (int i = 0; i < cur_size; i++) {
        delete array[i];
    }
    delete []array;
}

void TimeHeap::add_timer(HeapTimer *timer) {
    if (!timer) {
        return;
    }
    if (cur_size >= capacity) {
        resize();
    }
    timer->id = arr_id++;
    array[cur_size] = timer;
    hash_map[timer->id] = cur_size;
    sift_up(cur_size);
    cur_size++;
}

void TimeHeap::del_timer(HeapTimer *timer) {
    if (!timer) {
        return;
    }
    timer->cb_func = nullptr;
}

void TimeHeap::adjust_timer(HeapTimer *timer) {
    // timer->expire = _expire;
    int pos = hash_map[timer->id];
    sift_down(pos);
}

void TimeHeap::pop_timer() {
    if (is_empty()) {
        return;
    }
    if (array[0]) {
	int id = array[0]->id;
	hash_map.erase(id);
        delete array[0];
	cur_size--;
	if (cur_size > 0) {
		array[0] = array[cur_size];
		hash_map[array[0]->id] = 0;
        	sift_down(0);
	}
    }
}

void TimeHeap::sift_up(int child) {
    while((child - 1) / 2 >= 0) {
        int parent = (child - 1) / 2;
        if (array[parent]->expire <= array[child]->expire) {
            break;
        }
        heap_swap(child, parent);
        child = parent;
    }
}

void TimeHeap::sift_down(int parent) {
    while(2 * parent + 1 < cur_size) {
        int child = 2 * parent + 1;
        //HeapTimer* timer = array[child];
        if (2 * parent + 2 < cur_size && array[child]->expire > array[child + 1]->expire) {
            //timer = array[2 * parent + 2];
            child ++;
        }
        if (array[parent]->expire <= array[child]->expire) {
            break;
        }
        heap_swap(child, parent);
        parent = child;
    }
}

void TimeHeap::resize() {
	HeapTimer** tmp = new (nothrow) HeapTimer*[2 * capacity];
	if (!tmp) {
		throw exception();
	}
	/*
	try {
		tmp = new (nothrow) HeapTimer*[2 * capacity];
	} catch(bad_alloc) {
        	throw exception();
    	}*/

    for (int i = 0; i < 2 * capacity; i++) {
        tmp[i] = nullptr;
    }
    capacity = 2 * capacity;
    for (int i = 0; i < cur_size; i++) {
        tmp[i] = array[i];
	tmp[i]->id = array[i]->id;
    }
    delete []array;
    array = tmp;
}

void TimeHeap::heap_swap(int pos1, int pos2) {
    int id1 = array[pos1]->id;
    int id2 = array[pos2]->id;
    hash_map[id1] = pos2;
    hash_map[id2] = pos1;
    HeapTimer* tmp = array[pos1];
    array[pos1] = array[pos2];
    array[pos2] = tmp;
}

int TimeHeap::tick() {
    int time_slot = TIME_SLOT;
    time_t now = time(NULL);
    HeapTimer* tmp;
    while(!is_empty()) {
        tmp = array[0];
        if (!tmp) {
            break;
        }
        if (tmp->expire > now) {
            break;
        }
        if (tmp->cb_func) {
            tmp->cb_func(tmp->user_data);
        }
        pop_timer();
        tmp = array[0];
    }
    if (!is_empty()) {
        time_slot = array[0]->expire - time(NULL);
    }
    return time_slot;
}

Utils* Utils::get_instance() {
    static Utils utils;
    return &utils;
}

void Utils::utils_init(int* _pipefd) {
    pipefd = _pipefd;
}

void Utils::addsig(int sig, void(handler)(int)) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

void Utils::sig_handler(int sig) {
    int old_errno = errno;
    int msg = sig;
    send(pipefd[1], (char *)&msg, 1, 0);
    errno = old_errno;
}

void Utils::sigalrm_handler() {
    int time_slot = timer_heap.tick();
    alarm(time_slot);
}

int *Utils::pipefd = 0;

void cb_func(client_data *user_data) {
    removfd(Request::epollfd, user_data->sockfd);
    assert(user_data);
    close(user_data->sockfd);
    Request::user_count--;
}

