#include "connectionpool.h"

ConnectionPool::~ConnectionPool() {
    destroy_pool();
}

ConnectionPool* ConnectionPool::get_instance() {
    static ConnectionPool conn_pool;
    return &conn_pool;
}

void ConnectionPool::connPool_init(string _host, string _user, string _password, string _db, int _port, int _max_connections, bool _close_log) {

    host = _host;
    user = _user;
    password = _password;
    db = _db;
    port = _port;
    max_connections = _max_connections;
is_close = _close_log;
    conn_mutex = PTHREAD_MUTEX_INITIALIZER;

    for (int i = 0; i < max_connections; i++) {
        MYSQL *mysql = mysql_init(NULL);
        if (!mysql) {
	   LOG_ERROR("%s", "mysql init error");
            exit(1);
        }

        mysql = mysql_real_connect(mysql, host.c_str(), user.c_str(),
                                   password.c_str(), db.c_str(), port, NULL, 0);
        if (!mysql) {
		        LOG_ERROR("%s", "mysql connect error");
            exit(1);
        }

        connections.push(mysql);
    }

    sem_init(&freeConn, 0, connections.size());
    max_connections = connections.size();
}

MYSQL *ConnectionPool::get_connection() {
    if (connections.size() == 0) {
        return NULL;
    }
    sem_wait(&freeConn);
    pthread_mutex_lock(&conn_mutex);
    MYSQL* mysql = connections.front();
    connections.pop();
    pthread_mutex_unlock(&conn_mutex);
    return mysql;
}

bool ConnectionPool::release_connection(MYSQL *mysql) {
    if (!mysql) {
        return false;
    }
    pthread_mutex_lock(&conn_mutex);
    connections.push(mysql);
    pthread_mutex_unlock(&conn_mutex);
    sem_post(&freeConn);
    return true;
}

void ConnectionPool::destroy_pool() {
    pthread_mutex_lock(&conn_mutex);
    int n = connections.size();
    for (int i = 0; i < n; i++) {
        MYSQL *mysql = connections.front();
        connections.pop();
        mysql_close(mysql);
	mysql_library_end();
    }
    pthread_mutex_unlock(&conn_mutex);

    pthread_mutex_destroy(&conn_mutex);
    sem_destroy(&freeConn);
}


