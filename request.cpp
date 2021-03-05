#include "request.h"

const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

unordered_map<string, string> users;
pthread_mutex_t sql_mutex = PTHREAD_MUTEX_INITIALIZER;

int Request::user_count = 0;
int Request::epollfd = -1;

void Request::close_conn(bool is_close) {

    if (is_close && sockfd != -1) {
        // void removfd(int epollfd, int fd)
        removfd(epollfd, sockfd);
        sockfd = -1;
        user_count--;
    }
}

void Request::init() {
    check_state = CHECK_REQUESTLINE;
    is_linger = false;
    method = GET;
    content_length = 0;
    bytes_read = 0;
    bytes_write = 0;
    state = 0;
    cgi = 0;
    mysql = NULL;
    bytes_to_send = 0;
    bytes_have_send = 0;
    timer_flag = 0;
    improv = 0;

    // memset(read_buf, 0, READ_MAX_BUF);
    memset(write_buf, 0, WRITE_MAX_BUF);
}

void Request::init(int _connfd, string _root, string _user, string _password, string _db, bool _close_log) {
    sockfd = _connfd;
    root = _root;
    user = _user;
    password = _password;
    db = _db;
    is_close = _close_log;

    addfd(epollfd, sockfd, 1, 1);
    user_count++;

    init();
}

Request::LINE_STATUS Request::parse_line() {
    string &str = content;
    int pos = str.find("\r\n", 0);
    if (pos < 0) {
        return LINE_BAD;
    }
    line = str.substr(0, pos);
    if (str.size() >= pos + 2) {
        // Delete line from content
        str = str.substr(pos + 2);
    }
    return LINE_OK;
}

bool Request::read() {
    int data_read;
    char read_buf[READ_MAX_BUF];
    while(1) {
        data_read = recv(sockfd, read_buf + bytes_read, READ_MAX_BUF - bytes_read, 0);
        if (data_read < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                break;
            }
            return false;
        }
        if (data_read == 0) {
            return false;
        }
        bytes_read += data_read;
    }
    content = read_buf;
    return true;
}

Request::HTTP_CODE Request::process_read() {
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE res = NO_REQUEST;
    while((check_state == CHECK_CONTENT && line_status == LINE_OK) ||
          (line_status = parse_line()) == LINE_OK) {
	    LOG_INFO("%s", line.c_str());
        switch(check_state) {
        case CHECK_REQUESTLINE:
            res = parse_requestline();
            // Error checking *********CHECK******
            if (res == BAD_REQUEST) {
                return res;
            }
            break;
        case CHECK_HEADER:
            res = parse_header();
            // Erroor checking
            if (res == GET_REQUEST) {
                return do_request();
            }
            break;
        case CHECK_CONTENT:
            res = parse_content();
            if (res == GET_REQUEST) {
                return do_request();
            }
            line_status = LINE_OPEN;
            break;
        default:
            return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

Request::HTTP_CODE Request::parse_requestline() {
    string _method, _url, _version;
    stringstream s(line);
    s >> _method >> _url >> _version;
    if (_method == "" || _url == "" || _version == "") {
        return BAD_REQUEST;
    }
    // Get method
    if (_method == "GET") {
        method = GET;
    } else if (_method == "POST") {
        method = POST;
        cgi = 1;
    } else {
        return BAD_REQUEST;
    }

    // Get version
    if (_version != "HTTP/1.1") {
        return BAD_REQUEST;
    }
    version = "HTTP/1.1";

    // Get url
    if (!_url.compare(0, 7, "http://")) {
        _url = _url.substr(7);
    } else if (!_url.compare(0, 8, "https://")) {
        _url = _url.substr(8);
    }
    int pos = _url.find('/', 0);
    if (pos < 0) {
        return BAD_REQUEST;
    }
    url = _url.substr(pos);
    if (url.size() == 1) {
        url = "/judge.html";
    }
    check_state = CHECK_HEADER;
    return NO_REQUEST;

    /*
    // Get method
    int pos = line.find("GET", 0);
    if (pos < 0) {
        pos = line.find("POST", 0);
        if (pos < 0) {
            return BAD_REQUEST;
        }
        method = POST;
        cgi = 1;
    } else {
        method = GET;
    }

    // Parse url
    pos = line.find("http://", 0);
    if (pos >= 0) {
       line = line.substr(pos + 7);
    } else {
        pos = line.find("https:", 0);
        if (pos >= 0) {
            line = line.substr(pos + 8);
        }
    }
    pos = line.find('/', 0);
    if (pos < 0) {
        return BAD_REQUEST;
    }
    line = line.substr(pos);
    int pos1 = line.find(' ', 0);
    if (pos1 < 0) {
        return BAD_REQUEST;
    }
    url = line.substr(0, pos1 - pos);
    if (url.size() == 1) {
        url = "/judge.html";
    }

    // Get version
    pos = line.find("HTTP/1.1", pos1);
    if (pos < 0) {
        return BAD_REQUEST;
    }
    version = "HTTP/1.1";
    check_state = CHECK_HEADER;
    return NO_REQUEST;
    */
}

Request::HTTP_CODE Request::parse_header() {
    if (line == "") {
        if (content_length != 0) {
            check_state = CHECK_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    // Get connection type
    if (!line.compare(0, 12, "Connection: ")) {
        line = line.substr(12);
        if (!line.compare(0, 10, "keep-alive")) {
            is_linger = true;
        }
        return NO_REQUEST;
    }
    // Get content length
    if (!line.compare(0, 16, "Content-Length: ")) {
        content_length = atoi(line.substr(16).c_str());
        return NO_REQUEST;
    }
    LOG_INFO("oop!unknown header:%s", line.c_str());
    return NO_REQUEST;
}

Request::HTTP_CODE Request::parse_content() {
    if (!content.empty()) {
        sql_login = content;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

Request::HTTP_CODE Request::do_request() {
    real_file = root;
    int pos = url.find('/', 0);

    // Register and login verification
    if (cgi == 1 && (url[pos + 1] == '2' || url[pos + 1] == '3')) {
        string name, password;
        int pos1 = sql_login.find('&', 0);
        name = sql_login.substr(5, pos1 - 5);
        password = sql_login.substr(pos1 + 10);
        if (url[pos + 1] == '3') {
            string sql_insert = "INSERT INTO user(username, password) VALUES(";
            sql_insert += "'" + name + "', '" + password + "')";
            if (!users.count(name)) {
                pthread_mutex_lock(&sql_mutex);
                int res = -1;
                if (!users.count(name)) {
                    res = mysql_query(mysql, sql_insert.c_str());
                    users[name] = password;
                }
                pthread_mutex_unlock(&sql_mutex);
                // Successfully log in
                if (!res) {
                    url = "/log.html";
                } else {
                    url = "/registerError.html";
                }
            } else {
                url = "/registerError.html";
            }
        } else {
            if (users.count(name) && users[name] == password) {
                url = "/welcome.html";
            } else {
                url = "/registerError.html";
            }
        }
    }

    // Register
    if (url[pos + 1] == '0') {
        real_file += "/register.html";
    }
    // Log in
    else if (url[pos + 1] == '1') {
        real_file += "/log.html";
    }
    // Picture
    else if (url[pos + 1] == '5') {
        real_file += "/picture.html";
    }
    // Video
    else if (url[pos + 1] == '6' ) {
        real_file += "/video.html";
    }
    // Fans
    else if (url[pos + 1] == '7') {
        real_file += "/fans.html";
    }
    // Others
    else {
        real_file += url;
    }
    char real_file_address[FILE_NAME_LEN];
    strcpy(real_file_address, real_file.c_str());

    if (stat(real_file_address, &file_stat) < 0) {
        return NO_REQUEST;
    }
    if (!(file_stat.st_mode & S_IROTH)) {
        return FORBIDDEN_REQUEST;
    }
    if (S_ISDIR(file_stat.st_mode)) {
        return BAD_REQUEST;
    }
    int fd = open(real_file_address, O_RDONLY);
    file_address = (char *)mmap(0, file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}

void Request::unmap() {
    if (file_address) {
        munmap(file_address, file_stat.st_size);
        file_address = 0;
    }
}

// Build responce
bool Request::process_write(HTTP_CODE res) {
    bool flag = false;
    const char* tmp_ok_form;
    switch (res) {
    case INTERNAL_ERROR:
        flag = add_status_line(500, error_500_title);
        flag &= add_header(strlen(error_500_form));
        flag &= add_content(error_500_form);
        break;
    case BAD_REQUEST:
        flag = add_status_line(400, error_400_title);
        flag &= add_header(strlen(error_400_form));
        flag &= add_content(error_400_form);
        break;
    case NO_RESOURCE:
        flag = add_status_line(404, error_404_title);
        flag &= add_header(strlen(error_404_form));
        flag &= add_content(error_404_form);
        break;
    case FORBIDDEN_REQUEST:
        flag = add_status_line(403, error_403_title);
        flag &= add_header(strlen(error_403_form));
        flag &= add_content(error_403_form);
        break;
    case FILE_REQUEST:
        flag = add_status_line(200, ok_200_title);
        if (file_stat.st_size != 0) {
            flag &= add_header(file_stat.st_size);
            if (!flag) return false;
            iv[0].iov_base = write_buf;
            iv[0].iov_len = bytes_write;
            iv[1].iov_base = file_address;
            iv[1].iov_len = file_stat.st_size;
            iv_count = 2;
            bytes_to_send = bytes_write + file_stat.st_size;
	  //  cout << "bytes_to_send in process_write: " << bytes_to_send << endl; 
            return true;
        } else {
            tmp_ok_form = "<html><body></body></html>";
            flag &= add_header(strlen(tmp_ok_form));
            flag &= add_content(tmp_ok_form);
        }
        break;
    default:
        return false;
    }
    if (!flag) {
        return false;
    }
    iv[0].iov_base = write_buf;
    iv[0].iov_len = bytes_write;
    iv_count = 1;
    bytes_to_send = bytes_write;
    return true;
}

// Write responce
bool Request::write() {
   //  int bytes_have_send = 0;
    int tmp = 0;

    if (bytes_to_send == 0) {
        modfd(epollfd, sockfd, EPOLLIN, 1);
        init();
        return true;
    }

    while(1) {
        tmp = writev(sockfd, iv, iv_count);
        if (tmp >= 0) {
            bytes_have_send += tmp;
            bytes_to_send -= tmp;
	    //cout << "tmp: " << tmp << endl;
	    //cout << "bytes_have_send: " << bytes_have_send << endl;
	    //cout << "bytes_to_send: " << bytes_to_send << endl;
	    //cout << "**********************" << endl;
        }

	else {
            if (errno == EAGAIN) {
	//	cout << "end of write" << endl;
                if (iv[0].iov_len <= bytes_have_send) {
	//		cout << "end of write 1" << endl;
                    iv[0].iov_len = 0;
                    iv[1].iov_base = file_address + (bytes_have_send - bytes_write);
                    iv[1].iov_len = bytes_to_send;
                } else {
	//		cout << "end of write 2" << endl;
                    iv[0].iov_base = write_buf + bytes_have_send;
                    iv[0].iov_len -= bytes_have_send;
                }
                modfd(epollfd, sockfd, EPOLLOUT, 1);
                return true;
            }
            unmap();
            return false;
        }

        if (bytes_to_send <= 0) {
            unmap();
            modfd(epollfd, sockfd, EPOLLIN, 1);
            if (is_linger) {
                init();
                return true;
            }
            return false;
        }
    }
}

bool Request::add_response(const char *format, ...) {
    if (bytes_write >= WRITE_MAX_BUF) {
        return false;
    }
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(write_buf + bytes_write, WRITE_MAX_BUF - 1 -
                        bytes_write, format, arg_list);
    if (len >= (WRITE_MAX_BUF - 1 - bytes_write)) {
        va_end(arg_list);
        return false;
    }
    bytes_write += len;
    va_end(arg_list);
    LOG_INFO("request: %s", write_buf);
    return true;
}

bool Request::add_status_line(int status, const char *title) {
    return add_response("HTTP/1.1 %d %s\r\n", status, title);
}

bool Request::add_header(int _content_length) {
    return add_content_length(_content_length) && add_linger() && add_blank_line();
}

bool Request::add_content_length(int _content_length) {
    return add_response("Content-Length: %d\r\n",_content_length);
}

bool Request::add_linger() {
    if (is_linger == true) {
        return add_response("Connection: %s\r\n", "keep-alive");
    }
    return add_response("Connection: %s\r\n", "close");
}

bool Request::add_blank_line() {
    return add_response("\r\n");
}

bool Request::add_content(const char *_content) {
    return add_response("%s", _content);
}

void Request::process() {
    HTTP_CODE read_res = process_read();
    if (read_res == NO_REQUEST) {
        modfd(epollfd, sockfd, EPOLLIN, 1);
    }
    bool write_res = process_write(read_res);
    if (!write_res) {
        close_conn();
    }
    modfd(epollfd, sockfd, EPOLLOUT, 1);
}

void Request::init_mysql_result(ConnectionPool *conn_pool) {
    MYSQL *mysql = NULL;
    ConnectionRAII connectionRAII(&mysql, conn_pool);
    if (mysql_query(mysql, "SELECT username, password FROM user")) {
        LOG_ERROR("SELECT error:%s",mysql_error(mysql));
        return;
    }

    MYSQL_RES* result = mysql_store_result(mysql);
    if (result == NULL) {
	    LOG_INFO("%s", "empty database");
        return;
    }

    MYSQL_ROW row;
    while((row = mysql_fetch_row(result))) {
        string tmp1(row[0]);
        string tmp2(row[1]);
        users[tmp1] = tmp2;
    }
}

