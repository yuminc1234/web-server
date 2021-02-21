#include "log.h"

Log::Log() {
    count_lines = 0;
    getcwd(root, 250);
    strcat(root, "/log");
    log_mutex = PTHREAD_MUTEX_INITIALIZER;
}

Log::~Log() {
    // delete log_queue;
    if (fp != nullptr) {
        fclose(fp);
    }
    is_close = true;
    pthread_join(tid, NULL);
    delete log_queue;
}

Log* Log::get_instance() {
    static Log log;
    return &log;
}

bool Log::log_init(const char *_filename, bool _is_close, int _max_lines,
                   int _max_size, int _max_queue_size){
    
    max_size = _max_size;
    if (max_size <= 0) {
        exit(1);
    }
    strcpy(filename, _filename);
    is_close = _is_close;
    max_lines = _max_lines;
    max_queue_size = _max_queue_size;
    
    log_queue = new BlockQueue<string>(max_queue_size);
    is_close = false;

    pthread_create(&tid, NULL, write_to_log, NULL);

    time_t t = time(NULL);
    struct tm *lt = localtime(&t);

    char full_filename[300] = {0};
    int len = snprintf(full_filename, sizeof(full_filename), "%s/", root);
    len += snprintf(full_filename+len, sizeof(full_filename)-len, "%d_%02d_%02d_", lt->tm_year+1900, lt->tm_mon+1, lt->tm_mday);
    len += snprintf(full_filename+len, sizeof(full_filename)-len, "%s", filename);
    
    today = lt->tm_mday;

    fp = fopen(full_filename, "a");
    if (fp == NULL) {
	    LOG_ERROR("%s", "open log file error");
      return false;
    }

    return true;
}

// ThreadFunc
void *Log::write_to_log(void *args) {
    Log::get_instance()->write();
    return nullptr;
}

void Log::write() {
    string s;
    while (!is_close) {
	  if (!log_queue->is_empty()) {
    	if (log_queue->pop(s)) {
        	pthread_mutex_lock(&log_mutex);
        	fputs(s.c_str(), fp);
        	pthread_mutex_unlock(&log_mutex);
    	}
	}
    }
    return;
}

void Log::add_log(int level, const char* format, ...) {
    string s;
    switch (level) {
    case 0:
        s = "[debug]:";
        break;
    case 1:
        s = "[info]:";
        break;
    case 2:
        s = "[warn]:";
        break;
    case 3:
        s = "[erro]:";
        break;
    default:
        s = "[info]:";
    }
    
    struct timeval tv= {0,0};
    gettimeofday(&tv, NULL);
    time_t t = tv.tv_sec;
    struct tm *lt = localtime(&t);

    pthread_mutex_lock(&log_mutex);

    // Open a new log file
    if (lt->tm_mday != today || (count_lines != 0 && count_lines % max_lines == 0)) {
        char new_log[300] = {0};
        fflush(fp);
        fclose(fp);
        int len = snprintf(new_log, sizeof(new_log), "%s/", root);
	len += snprintf(new_log+len, sizeof(new_log)-len, "%d_%02d_%02d_", lt->tm_year+1900, lt->tm_mon+1, lt->tm_mday);
	len += snprintf(new_log+len, sizeof(new_log)-len, "%s", filename);
        if (lt->tm_mday != today) {
            today = lt->tm_mday;
            count_lines = 0;
        } else {
		snprintf(new_log+len, sizeof(new_log)-len, ".%lld", count_lines/max_lines);
        }
        fp = fopen(new_log, "a");
    }
    count_lines++;
    pthread_mutex_unlock(&log_mutex);

    va_list arg_list;
    va_start(arg_list, format);

    char log_line[max_size] = {0};
    int n = snprintf(log_line, max_size - 1, "%d-%02d-%02d %02d:%02d:%02d.%06ld"
                    " %s", lt->tm_year+1900, lt->tm_mon+1, lt->tm_mday,
                     lt->tm_hour, lt->tm_min, lt->tm_sec, tv.tv_usec, s.c_str()
                     );
    int m = vsnprintf(log_line + n, max_size - 1 - n, format, arg_list);
    log_line[m + n] = '\n';
    log_line[m + n + 1] = '\0';

    string log_str = log_line;

    if (!log_queue->is_full()) {
        log_queue->push(log_str);
    } else {
        pthread_mutex_lock(&log_mutex);
        fputs(log_str.c_str(), fp);
        pthread_mutex_unlock(&log_mutex);
    }
    va_end(arg_list);
}

void Log::flush() {
    pthread_mutex_lock(&log_mutex);
    fflush(fp);
    pthread_mutex_unlock(&log_mutex);
}





