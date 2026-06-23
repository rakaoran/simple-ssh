#include "stdio.h"
#include <arpa/inet.h>
#include <asm-generic/ioctls.h>
#include <asm-generic/socket.h>
#include <fcntl.h>
#include <limits.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <pty.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#define DEFAULT_PORT 10987

const char COMMAND = 0;
const char WINSIZE = 1;
uint32_t pack(char *inbuf, uint16_t in_size, char type, char *outbuf, uint16_t out_size);
void enable_raw_mode();
void disable_raw_mode();
void get_win_size(uint16_t *col, uint16_t *row);

int main(int argc, char **argv) {
    int epfd = epoll_create1(EPOLL_CLOEXEC);

    unsigned short port;
    if (argc == 1) {
        port = DEFAULT_PORT;
    } else {
        int p = atoi(*(argv + 1));
        if (p < 1 || p > USHRT_MAX) {
            fprintf(stderr, "Invalid port range.\n");
            exit(1);
        }
        port = p;
    }

    int server_fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (server_fd == -1) {
        perror("socket");
        exit(0);
    }
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt SO_REUSEADDR failed");
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in sa;
    sa.sin_addr.s_addr = INADDR_ANY;
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);

    int err = connect(server_fd, (struct sockaddr *)&sa, sizeof(sa));
    if (err == -1) {
        perror("connect ossoso");
        exit(EXIT_FAILURE);
    }

    enable_raw_mode();

    struct epoll_event server_event;
    server_event.events = EPOLLIN | EPOLLHUP | EPOLLRDHUP | EPOLLERR;
    server_event.data.fd = server_fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &server_event);
    struct epoll_event stdin_event;
    stdin_event.events = EPOLLIN | EPOLLHUP | EPOLLRDHUP | EPOLLERR;
    stdin_event.data.fd = STDIN_FILENO;
    epoll_ctl(epfd, EPOLL_CTL_ADD, STDIN_FILENO, &stdin_event);

    struct epoll_event event;
    char packedbuf[10000];
    char buf[10000];
    uint16_t col, row;
    get_win_size(&col, &row);
    uint16_t winsize[2];
    *winsize = htons(col);
    *(winsize + 1) = htons(row);
    uint32_t n = pack((char *)winsize, sizeof(winsize), WINSIZE, packedbuf, sizeof(packedbuf));
    err = send(server_fd, packedbuf, n, MSG_NOSIGNAL);
    if (err == -1) {
        perror("send winsize");
        exit(EXIT_FAILURE);
    }
    char c;
    while (1) {
        err = epoll_wait(epfd, &event, 1, -1);
        if (err == -1) {
            perror("epoll_wait");
            break;
        }
        if (event.events & (EPOLLIN)) {
            if (event.data.fd == server_fd) {
                int n = read(server_fd, buf, sizeof(buf));
                if (n == -1) {
                    perror("read from server");
                    break;
                }
                if (n == 0) {
                    break;
                }
                err = write(STDIN_FILENO, buf, n);
                if (err == -1) {
                    perror("read from server");
                    break;
                }
            } else {
                int n = read(STDIN_FILENO, &c, 1);
                if (n <= 0) {
                    perror("read from stdin");
                    break;
                }
                int packet_size = pack(&c, 1, COMMAND, packedbuf, sizeof(packedbuf));
                err = send(server_fd, packedbuf, packet_size, MSG_NOSIGNAL);
                if (err == -1) {
                    perror("read from server");
                    break;
                }
            }
        } else {
            break;
        }
    }
    close(server_fd);
    close(epfd);
    disable_raw_mode();
}
struct termios old, new;
void enable_raw_mode() {
    tcgetattr(0, &old);

    cfmakeraw(&new);
    tcsetattr(STDIN_FILENO, 0, &new);
}

void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, 0, &old);
}

void get_win_size(uint16_t *col, uint16_t *row) {
    struct winsize ws;

    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1) {
        perror("ioctl");
        exit(EXIT_FAILURE);
    }

    *col = ws.ws_col;
    *row = ws.ws_row;
}

uint32_t pack(char *inbuf, uint16_t in_size, char type, char *outbuf, uint16_t out_size) {
    uint32_t total_size = in_size + 1 + sizeof(uint32_t);
    if (out_size < total_size) {
        return -1;
    }
    *((uint32_t *)(outbuf)) = htonl(total_size);
    *(outbuf + sizeof(uint32_t)) = type;
    memcpy((outbuf + 1 + sizeof(uint32_t)), inbuf, in_size);
    return total_size;
}
