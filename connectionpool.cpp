#include "connectionpool.h"

ConnectionPool::~ConnectionPool() {
    destroy_pool();
}

ConnectionPool* ConnectionPool::get_instance() {
    static ConnectionPool conn_pool;
    cout << "Address of conn_pool is " << &conn_pool << endl;
    return &conn_pool;
}

void ConnectionPool::connPool_init(string _host, string _user, string _password, string _db, int _port, int _max_connections) {

    host = _host;
    user = _user;
    password = _password;
    db = _db;
    port = _port;
    max_connections = _max_connections;

    conn_mutex = PTHREAD_MUTEX_INITIALIZER;

    for (int i = 0; i < max_connections; i++) {
        MYSQL *mysql = mysql_init(NULL);
        if (!mysql) {
            cout << "mysql_init error" << mysql_error(mysql);
            exit(1);
        }

        mysql = mysql_real_connect(mysql, host.c_str(), user.c_str(),
                                   password.c_str(), db.c_str(), port, NULL, 0);
        if (!mysql) {
            cout << "connect error" << mysql_error(mysql);
            exit(1);
        }

        connections.push(mysql);
    }

    sem_init(&freeConn, 0, connections.size());
    cout << "the size of connections is " << connections.size() << endl;
    max_connections = connections.size();
}

MYSQL *ConnectionPool::get_connection() {
    if (connections.size() == 0) {
        return NULL;
    }
    sem_wait(&freeConn);
   cout << "entering get_connection " << endl;
    pthread_mutex_lock(&conn_mutex);
    cout << "lock conn_mutex" << endl;
    // Check the size of connections?
    MYSQL* mysql = connections.front();
    connections.pop();
    pthread_mutex_unlock(&conn_mutex);
    cout << "unlock conn_mutex" << endl;
    cout << "get connection " << mysql << endl;
    return mysql;
}

bool ConnectionPool::release_connection(MYSQL *mysql) {
    if (!mysql) {
        return false;
    }
    pthread_mutex_lock(&conn_mutex);
    cout << "lock conn_mutex" << endl;
    connections.push(mysql);
    pthread_mutex_unlock(&conn_mutex);
    cout << "unlock conn_mutex" << endl;
    sem_post(&freeConn);
    return true;
}

void ConnectionPool::destroy_pool() {
    pthread_mutex_lock(&conn_mutex);
    cout << "lock conn_mutex" << endl;
    int n = connections.size();
    for (int i = 0; i < n; i++) {
        MYSQL *mysql = connections.front();
        connections.pop();
        mysql_close(mysql);
    }
    pthread_mutex_unlock(&conn_mutex);
    cout << "unlock conn_mutex" << endl;

    pthread_mutex_destroy(&conn_mutex);
    sem_destroy(&freeConn);
}


