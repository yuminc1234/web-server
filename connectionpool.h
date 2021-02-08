#ifndef CONNECTIONPOOL_H
#define CONNECTIONPOOL_H

#include <queue>
#include <stdlib.h>
#include <mysql/mysql.h>
#include <semaphore.h>
#include <pthread.h>
#include <string.h>
#include <iostream> // debug

using namespace std;

class ConnectionPool
{
public:
    static ConnectionPool* get_instance();
    void connPool_init(string _host, string _user, string _password, string _db, int _port, int _max_connections);
    MYSQL* get_connection();
    bool release_connection(MYSQL* mysql);
    void destroy_pool();

private:
    ConnectionPool(){}
    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;
    ~ConnectionPool();

    queue<MYSQL *> connections;
    int max_connections;

    string host;
    string user;
    string password;
    string db;
    int port;

    sem_t freeConn;
    pthread_mutex_t conn_mutex;
};

#endif // CONNECTIONPOOL_H
