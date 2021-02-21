#ifndef LOG_H
#define LOG_H

#include <string.h>
#include <queue>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <stdarg.h>
#include<stdio.h>
#include <iostream>
#include <sys/time.h>

#include "blockqueue.h"

using namespace std;

class Log
{
private:
    Log();
    Log(const Log&) = delete;
    Log& operator=(const Log&) = delete;
    ~Log();
    static void* write_to_log(void *args);
    void write();

    // FILE *fopen(char *filename, char *mode)
    BlockQueue<string> *log_queue;
    int max_lines;
    int max_size;
    int max_queue_size;
    long long count_lines;
    int today;
    char root[250];
    char filename[50];
    FILE *fp;
    pthread_mutex_t log_mutex;
    bool is_close;
    pthread_t tid;
	
public:
    static Log* get_instance();
    bool log_init(const char *_filename, bool _is_close,
                  int _max_lines = 5000000, int _max_size = 8192, int _max_queue_size = 800);
    void add_log(int level, const char* format, ...);
    void flush(); 
};

#define LOG_DEBUG(format, ...) if(0 == is_close) {Log::get_instance()->add_log(0, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_INFO(format, ...) if(0 == is_close) {Log::get_instance()->add_log(1, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_WARN(format, ...) if(0 == is_close) {Log::get_instance()->add_log(2, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_ERROR(format, ...) if(0 == is_close) {Log::get_instance()->add_log(3, format, ##__VA_ARGS__); Log::get_instance()->flush();}

#endif // LOG_H
