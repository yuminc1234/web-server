#include "server.h"

Server::Server() {
    users = new Request[MAX_FD];
    char buffer[200];
    getcwd(buffer, 200);
    root = (string)buffer + "/root";
    cout << "root is " << endl;
}

Server::~Server() {
    close(epollfd);
    close(m_listenfd);
    delete []users;
    delete pool;
}

void Server::thread_pool() {
    pool = new ThreadPool(m_conn_pool, thread_num);
}

void Server::conn_pool() {
    m_conn_pool = ConnectionPool::get_instance();
    // port?
    m_conn_pool->connPool_init("localhost", user, password, db, 3306, max_connections);

    // Search results in database
    // **********************MODIFY********************
    users->init_mysql_result(m_conn_pool);
}

void Server::init(int _port, string _user, string _password, string _db,
                  int _max_connections, int _thread_num) {
    port = _port;
    user = _user;
    password = _password;
    db = _db;
    max_connections = _max_connections;
    thread_num = _thread_num;
}


int Server::listen_bind() {
    int listenfd = socket( PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    // Graceful close
    struct linger tmp = {1, 1};
    setsockopt( listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));

    int optval = 1;
    //  Eliminate "Address already in use" error from bind
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    ret = bind(listenfd, (struct sockaddr*)&address, sizeof (address));
    assert(ret >= 0);

    // Make it a listening socket ready to accept connection requests
    ret = listen(listenfd, 5);
    assert(ret >= 0);

    return listenfd;
}

void Server::accept_connection() {
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof (client_address);
    int connfd = 0;
    while ((connfd = accept(m_listenfd, (struct sockaddr* )&client_address,
                            &client_addrlength)) >= 0) {
        if (Request::user_count >= MAX_FD) {
            cout << "Internal server busy" << endl;
            return;
        }

        users[connfd].init(connfd, root, user, password, db);
    }
}

void Server::eventLoop() {
    epollfd = epoll_create(5);
    assert(epollfd != -1);

    // ET + not one shot
    m_listenfd = listen_bind();
    addfd(epollfd, m_listenfd, 1, 0);
    Request::epollfd = epollfd;

    while(true) {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUM, -1);
        if ((number < 0) && (errno != EINTR)) {
            cout << "epoll failure" << endl;
            break;
        }

        for (int i = 0; i < number; i++) {
            int sockfd = events[i].data.fd;

            // Receive new connection from client ?
            if (sockfd == m_listenfd) {
                accept_connection();
            } else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                cout << "error event" << endl;
                continue;
            } else if (events[i].events & EPOLLIN) {
                pool->add_task(users + sockfd, 0);
            } else if (events[i].events & EPOLLOUT) {
                pool->add_task(users + sockfd, 1);
            }
        }
    }
}
