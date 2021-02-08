#include "connectionraii.h"

ConnectionRAII::ConnectionRAII(MYSQL **mysql, ConnectionPool *conn_pool) {
    cout << "entering connectionRAII " << endl;
    cout << "conn_pool is " << conn_pool << endl;
	*mysql = conn_pool->get_connection();

    mysql_RAII = *mysql;
    cout << "mysql_RAII is" << mysql_RAII << endl;
    conn_pool_RAII = conn_pool;
    cout << "leaving connectionRAII" << endl;
}

ConnectionRAII::~ConnectionRAII() {
    conn_pool_RAII->release_connection(mysql_RAII);
    cout << "release connection" << endl;
}
