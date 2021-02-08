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

const int MAX_EVENT_NUM = 10000;
const int MAX_FD = 65536;

class Server
{  
private:
    int listen_bind();
    void accept_connection();

public:
    Server();
    ~Server();
    void init(int _port, string _user, string _password, string _db, int _max_connections, int _thread_num);
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
};

#endif // SERVER_H
