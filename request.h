#ifndef REQUEST_H
#define REQUEST_H

#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <errno.h>
#include <sstream>
#include <pthread.h>
#include <unordered_map>
#include <mysql.h>
#include <stdarg.h>
#include <iostream>

#include "epoll.h"
#include "connectionpool.h"
#include "connectionraii.h"
#include "log.h"

using namespace std;

const int FILE_NAME_LEN = 200;
const int READ_MAX_BUF = 2048;
const int WRITE_MAX_BUF = 1024;

class Request
{
public:
    enum METHOD {GET, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT, PATCH};
    enum CHECK_STATE {CHECK_REQUESTLINE, CHECK_HEADER, CHECK_CONTENT};
    enum HTTP_CODE {NO_REQUEST, GET_REQUEST, BAD_REQUEST, NO_RESOURCE,
                    FORBIDDEN_REQUEST, FILE_REQUEST, INTERNAL_ERROR,
                    CLOSED_CONNECTION};
    // Delete LINE_OPEN
    enum LINE_STATUS {LINE_OK, LINE_BAD, LINE_OPEN};

    static int epollfd;
    static int user_count;
    int state;
    MYSQL* mysql;
    int timer_flag;
    int improv;
	bool is_close;
    Request() {}
    ~Request(){}

    void init(int _connfd, string _root, string _user, string _password, string _db, bool _close_log);
    void close_conn(bool is_close = true);
    void process();
    void init_mysql_result(ConnectionPool* conn_pool);
    bool read();
    bool write();

private:
    void init();    
    HTTP_CODE process_read();
    bool process_write(HTTP_CODE res);

    HTTP_CODE parse_requestline();
    HTTP_CODE parse_header();
    HTTP_CODE parse_content();
    HTTP_CODE do_request();
    LINE_STATUS parse_line();

    void unmap();
    bool add_response(const char* format, ...);
    bool add_content(const char* _content);
    bool add_status_line(int status, const char* title);
    bool add_header(int _content_length);
    bool add_content_length(int _content_length);
    bool add_linger();
    bool add_blank_line();

    int sockfd;
    // sockaddr_in address;

    // Request and responce
    string url;
    string version;
    METHOD method;

    int cgi;
    bool is_linger;
    int content_length;
    string real_file;
    string root;

    // Database   
    string user;
    string password;
    string db;
    string sql_login;


    // Read request
    // char read_buf[READ_MAX_BUF];
    string content;
    string line;
    int bytes_read;
    CHECK_STATE check_state;

    // Build responce
    char write_buf[WRITE_MAX_BUF];
    int bytes_write;
    int bytes_to_send;

    // Write responce
    char* file_address;
    struct stat file_stat;
    struct iovec iv[2];
    int iv_count;
};

#endif // REQUEST_H
