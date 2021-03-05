#include "command.h"

Command::Command()
{
    port = 9006;
    thread_num = 8;
    max_connections = 8;
    close_log = 0;
}

void Command::parse(int argc, char* argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "t:p:s:c:")) != -1) {
        switch(opt) {
        case 't':
            thread_num = atoi(optarg);
            break;
        case 'p':
            port = atoi(optarg);
            break;
        case 's':
            max_connections = atoi(optarg);
            break;
	case 'c':
	    close_log = atoi(optarg);
	    break;
        default:
            break;
        }
    }
}
