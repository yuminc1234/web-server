#include "command.h"
#include "server.h"

int main(int argc, char *argv[]) {
    string user = "root";
    string password = "123456";
    string db = "ymcdb";
    
    // Parse command line
    Command command;
    command.parse(argc, argv);

    // define a server
    Server server;
    server.init(command.port, user, password, db, command.max_connections,
          command.thread_num, 0);
	
    server.log_write();
    server.conn_pool();
    server.thread_pool();

    server.eventLoop();

    return 0;
}
