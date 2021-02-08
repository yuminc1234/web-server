#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>

int setnonblocking(int fd);
// Register event, ET mode, EPOLLONESHOT
void addfd(int epollfd, int fd, bool enable_et, bool one_shot);
// Delete fd from epollfd
void removfd(int epollfd, int fd);
// Reset EPOLLONESHOT
void modfd(int epollfd, int fd, int ev, bool enable_et);


