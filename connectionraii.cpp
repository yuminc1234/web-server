#include "connectionraii.h"

ConnectionRAII::ConnectionRAII(MYSQL **mysql, ConnectionPool *conn_pool) {
	*mysql = conn_pool->get_connection();

    mysql_RAII = *mysql;
    conn_pool_RAII = conn_pool;
}

ConnectionRAII::~ConnectionRAII() {
    conn_pool_RAII->release_connection(mysql_RAII);
}
