#ifndef COMMAND_H
#define COMMAND_H

#include <unistd.h>
#include <stdlib.h>

using namespace std;

class Command {
public:
    int port;
    int thread_num;
    int max_connections;

    Command();
    ~Command(){}
    void parse(int argc, char* argv[]);

};

#endif // COMMAND_H
