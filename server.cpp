#include "server.h"

Server::Server() {
    users = new Request[MAX_FD];
    char buffer[200];
    getcwd(buffer, 200);
    root = (string)buffer + "/root";
    user_timers = new client_data[MAX_FD];
}

Server::~Server() {
    close(epollfd);
    close(m_listenfd);
    close(pipefd[1]);
    close(pipefd[2]);
    delete []users;
    delete []user_timers;
    delete pool;
}

void Server::thread_pool() {
    pool = new ThreadPool(m_conn_pool, thread_num);
}

void Server::conn_pool() {
    m_conn_pool = ConnectionPool::get_instance();
    m_conn_pool->connPool_init("localhost", user, password, db, 3306, max_connections, is_close);

    // Search results in database
    users->init_mysql_result(m_conn_pool);
}

void Server::init(int _port, string _user, string _password, string _db,
                  int _max_connections, int _thread_num, bool _close_log) {
    port = _port;
    user = _user;
    password = _password;
    db = _db;
    max_connections = _max_connections;
    thread_num = _thread_num;
    is_close = _close_log;
}

void Server::log_write() {
  Log *log = Log::get_instance();
	log->log_init("server_log", is_close);
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
            LOG_ERROR("%s", "Internal server busy");
            return;
        }
        users[connfd].init(connfd, root, user, password, db, is_close);
        user_timers[connfd].address = client_address;
        user_timers[connfd].sockfd = connfd;
        HeapTimer *timer = new HeapTimer(3 * TIME_SLOT);
        timer->user_data = &user_timers[connfd];
        // How to modify
        timer->cb_func = cb_func;
        user_timers[connfd].timer = timer;
        utils->timer_heap.add_timer(timer);
    }
}

void Server::eventLoop() {
    epollfd = epoll_create(5);
    assert(epollfd != -1);

    // ET
    m_listenfd = listen_bind();
    addfd(epollfd, m_listenfd, 1, 0);
    Request::epollfd = epollfd;

    utils = Utils::get_instance();
    utils->utils_init(pipefd);

    int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret != -1);
    setnonblocking(pipefd[1]);
    // ET and not one shot???
    addfd(epollfd, pipefd[0], 1, 0);
    utils->addsig(SIGALRM, utils->sig_handler);
    utils->addsig(SIGTERM, utils->sig_handler);
    Utils::pipefd = pipefd;
    bool timeout = false;
    bool stop_server = false;

    alarm(TIME_SLOT);

    while(!stop_server) {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUM, -1);
        if ((number < 0) && (errno != EINTR)) {
            LOG_ERROR("%s", "epoll failure");
            break;
        }

        for (int i = 0; i < number; i++) {
            int sockfd = events[i].data.fd;

            // Receive connection requests from clients
            if (sockfd == m_listenfd) {
                accept_connection();
            } else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                HeapTimer *timer = user_timers[sockfd].timer;
                deal_timer(timer, sockfd);
            } else if (sockfd == pipefd[0] && events[i].events & EPOLLIN) {
                char signals[1024];
                int ret = recv(pipefd[0], signals, sizeof (signals), 0);
                if (ret == -1) {
			LOG_ERROR("%s", "dealsignal failure");
                    continue;
                }
                if (ret == 0) {
			LOG_ERROR("%s", "dealsignal failure");
                    continue;
                }
                for (int i = 0; i < ret; i++) {
                    switch (signals[i]) {
                    case SIGALRM:
                        timeout = true;
                        break;
                    case SIGTERM:
                        stop_server = true;
                    }
                }
            }
            else if (events[i].events & EPOLLIN) {
                HeapTimer *timer = user_timers[sockfd].timer;
                if (timer) {
                    adjust_timer(timer);

                }
                pool->add_task(users + sockfd, 0);
                while(true) {
                    if (users[sockfd].improv == 1) {
                        if (users[sockfd].timer_flag == 1) {
                            deal_timer(timer, sockfd);
                            users[sockfd].timer_flag = 0;
                        }
                        users[sockfd].improv = 0;
                        break;
                    }
                }
            } else if (events[i].events & EPOLLOUT) {
                HeapTimer *timer = user_timers[sockfd].timer;
                if (timer) {
                    adjust_timer(timer);
                }
                pool->add_task(users + sockfd, 1);
                while (true) {
                    if (users[sockfd].improv == 1) {
                        if (users[sockfd].timer_flag == 1) {
                            deal_timer(timer, sockfd);
                            users[sockfd].timer_flag = 0;
                        }
                        users[sockfd].improv = 0;
                        break;
                    }
                }
            }
        }
        if (timeout) {
            utils->sigalrm_handler();
	    LOG_INFO("%s", "timer tick");
            timeout = false;
        }
    }
}

void Server::deal_timer(HeapTimer *timer, int sockfd) {
    timer->cb_func(&user_timers[sockfd]);
    if (timer) {
        utils->timer_heap.del_timer(timer);
    }
    LOG_INFO("close fd %d", user_timers[sockfd].sockfd);
}

void Server::adjust_timer(HeapTimer *timer) {
	time_t now = time(NULL);
        timer->expire = now + 3 * TIME_SLOT;
        utils->timer_heap.adjust_timer(timer);
	LOG_INFO("%s", "adjust timer");
}
