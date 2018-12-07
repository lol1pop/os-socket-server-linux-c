#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

void error(const char *s)
{
    perror(s);
    exit(-1);
}

/*
 * Функции-обертки.
 */
int Socket(int domain, int type, int protocol)
{
    int rc;

    rc = socket(domain, type, protocol);
    if(rc == -1) error("socket()");

    return rc;
}

int Bind(int socket, struct sockaddr *addr, socklen_t addrlen)
{
    int rc;

    rc = bind(socket, addr, addrlen);
    if(rc == -1) error("bind()");

    return rc;
}

int Listen(int socket, int backlog)
{
    int rc;

    rc = listen(socket, backlog);
    if(rc == -1) error("listen()");

    return rc;
}

int Accept(int socket, struct sockaddr *addr, socklen_t *addrlen)
{
    int rc;

    for(;;) {
        rc = accept(socket, addr, addrlen);
        if(rc != -1) break;
        if(errno == EINTR || errno == ECONNABORTED) continue;
        error("accept()");
    }

    return rc;
}

void Close(int fd)
{
    int rc;

    for(;;) {
        rc = close(fd);
        if(!rc) break;
        if(errno == EINTR) continue;
        error("close()");
    }
}

size_t Read(int fd, void *buf, size_t count)
{
    ssize_t rc;

    for(;;) {
        rc = read(fd, buf, count);
        if(rc != -1) break;
        if(errno == EINTR) continue;
        error("read()");
    }

    return rc;
}

size_t Write(int fd, const void *buf, size_t count)
{
    ssize_t rc;

    for(;;) {
        rc = write(fd, buf, count);
        if(rc != -1) break;
        if(errno == EINTR) continue;
        error("write()");
    }

    return rc;
}

void *Malloc(size_t size)
{
    void *rc;

    rc = malloc(size);
    if(rc == NULL) error("malloc()");

    return rc;
}

void Pthread_create(pthread_t *thread, pthread_attr_t *attr,
                    void *(*start_routine)(void *), void *arg)
{
    int rc;

    rc = pthread_create(thread, attr, start_routine, arg);
    if(rc) {
        errno = rc;
        error("pthread_create()");
    }
}

/*
 * Чтение строки из сокета.
 */
size_t reads(int socket, char *s, size_t size)
{
    char *p;
    size_t n, rc;

    /* Проверить корректность переданных аргументов. */
    if(s == NULL) {
        errno = EFAULT;
        error("reads()");
    }
    if(!size) return 0;

    p = s;
    size--;
    n = 0;
    while(n < size) {
        rc = Read(socket, p, 1);
        if(rc == 0) break;
        if(*p == '\n') {
            p++;
            n++;
            break;
        }
        p++;
        n++;
    }
    *p = 0;

    return n;
}

/*
 * Запись count байтов в сокет.
 */
size_t writen(int socket, const char *buf, size_t count)
{
    const char *p;
    size_t n, rc;

    /* Проверить корректность переданных аргументов. */
    if(buf == NULL) {
        errno = EFAULT;
        error("writen()");
    }

    p = buf;
    n = count;
    while(n) {
        rc = Write(socket, p, n);
        n -= rc;
        p += rc;
    }

    return count;
}

void *serve_client(void *arg)
{
    int socket;
    char s[256];
    ssize_t rc;

    /* Перевести поток в отсоединенное (detached) состояние. */
    pthread_detach(pthread_self());

    socket = *((int *) arg);
    free(arg);

    while((rc = reads(socket, s, 256)) > 0) {
        if(writen(socket, s, rc) == -1) break;
    }
    Close(socket);

    return NULL;
}

int main(void)
{
    int lsocket;    /* Дескриптор прослушиваемого сокета. */
    int csocket;    /* Дескриптор присоединенного сокета. */
    struct sockaddr_in servaddr;
    int *arg;
    pthread_t thread;

    /* Создать сокет. */
    lsocket = Socket(PF_INET, SOCK_STREAM, 0);

    /* Инициализировать структуру адреса сокета сервера. */
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(1027);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    /* Связать сокет с локальным адресом протокола. */
    Bind(lsocket, (struct sockaddr *) &servaddr, sizeof(servaddr));

    /* Преобразовать неприсоединенный сокет в пассивный. */
    Listen(lsocket, 5);

    for(;;) {
        csocket = Accept(lsocket, NULL, 0);

        arg = Malloc(sizeof(int));
        *arg = csocket;

        Pthread_create(&thread, NULL, serve_client, arg);
    }

    return 0;
}