#ifndef CONNECTIONRAII_H
#define CONNECTIONRAII_H

#include <mysql/mysql.h>
#include "connectionpool.h"

class ConnectionRAII
{
private:
    MYSQL *mysql_RAII;
    ConnectionPool *conn_pool_RAII;

public:
    ConnectionRAII(MYSQL **mysql, ConnectionPool *conn_pool);
    ~ConnectionRAII();

};

#endif // CONNECTIONRAII_H
