#ifndef SERVER_H
#define SERVER_H

#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <cassert>

#include "request.h"
#include "epoll.h"
#include "threadpool.h"
#include "connectionpool.h"
#include "timer.h"
#include "log.h"

const int MAX_EVENT_NUM = 10000;
const int MAX_FD = 65536;
const int TIME_SLOT = 5;

class Server
{  
private:
    int listen_bind();
    void accept_connection();
    void deal_timer(HeapTimer* timer, int sockfd);
    void adjust_timer(HeapTimer* timer);

public:
    Server();
    ~Server();
    void init(int _port, string _user, string _password, string _db, int _max_connections, int _thread_num, bool _close_log);
    void log_write();
    void thread_pool();
    void conn_pool();
    void eventLoop();

    int m_listenfd;
    int port;

    // epoll
    int epollfd;
    epoll_event events[MAX_EVENT_NUM];

    // ThreadPool
    int thread_num;
    ThreadPool *pool;

    // Request
    Request* users;
    string root;

    // MySQL
    ConnectionPool *m_conn_pool;
    // string host;
    string user;
    string password;
    string db;
    int max_connections;

    // Timer
    Utils *utils;
    client_data* user_timers;
    int pipefd[2];
    
    // Log
    bool is_close;
};

#endif // SERVER_H
