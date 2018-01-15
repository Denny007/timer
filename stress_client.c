/*
 * 压力测试：使用epoll对服务器发起连接，然后互相传递数据
 **/

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>

#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>

static const char* request = "GET http://localhost/index.html HTTP/1.1\r\nConnection: keep-alive\r\n\r\nxxxxxxxxxxxx";

/* 设置socket描述符为非阻塞 */
int setnonblocking( int fd )
{
    int old_option = fcntl( fd, F_GETFL );
    int new_option = old_option | O_NONBLOCK;
    fcntl( fd, F_SETFL, new_option );
    return old_option;
}

/* 添加事件到event */
void addfd(int epoll_fd, int fd)
{
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLOUT | EPOLLET | EPOLLERR;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

/* 向socket 描述符写入数据 */
bool write_nbytes(int sockfd, const char* buffer, int len )
{
    int bytes_write = 0;
    printf( "write out %d bytes to socket %d\n", len, sockfd);
    while( 1 ) 
    {   
        bytes_write = send( sockfd, buffer, len, 0 );
        if ( bytes_write == -1 )
        {   
            return false;
        }   
        else if ( bytes_write == 0 ) 
        {   
            return false;
        }   

        len -= bytes_write;
        buffer = buffer + bytes_write;
        if ( len <= 0 ) 
        {   
            return true;
        }   
    }   
}

/* 从sockfd中读取数据 */
bool read_once( int sockfd, char* buffer, int len )
{
    int bytes_read = 0;
    memset( buffer, '\0', len );
    bytes_read = recv( sockfd, buffer, len, 0 );
    if ( bytes_read == -1 )
    {
        return false;
    }
    else if ( bytes_read == 0 )
    {
        return false;
    }
	printf( "read in %d bytes from socket %d with content:\n %s\n", bytes_read, sockfd, buffer );

    return true;
}

/* 创建num个连接 */
int start_conn(int epoll_fd, int num, const char* ip, int port )
{
    int count = 0;
    int i;
    struct sockaddr_in address;
    bzero( &address, sizeof( address ) );
    address.sin_family = AF_INET;
    inet_pton( AF_INET, ip, &address.sin_addr );
    address.sin_port = htons( port );

    /* 创建num个socket */
    for (i = 0; i < num; ++i )
    {
        sleep( 1 );
        int sockfd = socket(PF_INET, SOCK_STREAM, 0 );
        printf( "create 1 sock\n" );
        if( sockfd < 0 )
        {
            continue;
        }

        if (connect(sockfd, ( struct sockaddr* )&address, sizeof( address ) ) == 0)
        {          
            count++;
            printf("build connection %d\n", count);
            addfd(epoll_fd, sockfd);
        }
    }
    return count;
}

/* 从epoll事件集中删除事件，同时关闭socket描述符 */
void close_conn( int epoll_fd, int sockfd )
{
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, sockfd, 0 );
    close( sockfd );
}

static void handle_event(int epoll_fd, struct epoll_event *events, int nums, char *buffer)
{
    int i;
    int sockfd;
    for (i = 0; i < nums; i++ )
    {   
        sockfd = events[i].data.fd;
        /* 处理可读描述符，接收数据 */
        if ( events[i].events & EPOLLIN )
        {   
            /* 从描述符中读取数据 */
            if (!read_once( sockfd, buffer, 2048 ) )
            {
                close_conn( epoll_fd, sockfd );
            }
            struct epoll_event event;
            event.events = EPOLLOUT | EPOLLET | EPOLLERR;  //修改描述符为可写和边缘触发
            event.data.fd = sockfd;
            epoll_ctl( epoll_fd, EPOLL_CTL_MOD, sockfd, &event );
        }
        /* 处理可写描述符，发送数据 */
        else if(events[i].events & EPOLLOUT ) 
        {
            if (!write_nbytes(sockfd, request, strlen(request)))
            {
                close_conn( epoll_fd, sockfd );
            }
            struct epoll_event event;
            event.events = EPOLLIN | EPOLLET | EPOLLERR;//修改描述符为可读和边缘触发
            event.data.fd = sockfd;
            epoll_ctl( epoll_fd, EPOLL_CTL_MOD, sockfd, &event );
        }
        /* 文件描述符发生错误,关闭连接和epoll描述符 */
        else if( events[i].events & EPOLLERR )
        {
            close_conn( epoll_fd, sockfd );
        }
    }
}

int main( int argc, char* argv[] )
{
    int nConnection = 0;
    //assert( argc == 4 );
    if (argc != 4)
    {
        printf("usage: client <IP_Address> <port> connection<num> \n");
        exit(1);
    }
    /* 创建epoll描述符 */
    int epoll_fd = epoll_create(100);

    nConnection = start_conn(epoll_fd, atoi(argv[3]), argv[1], atoi(argv[2]));
    if (nConnection <= 0)
    {
        printf("can not connect to the server\n");
        return -1;
    }
    
    
    /* epoll 事件集*/
    struct epoll_event events[10000];
    char buffer[2048];

    while (1)
    {
        sleep(5);
        /* 阻塞等待事件集中有事件发生，获取已经准备好的事件描述符*/
        int fds = epoll_wait(epoll_fd, events, 10000, 2000);
        /* 处理准备好的事件描述符 */ 
        handle_event(epoll_fd, events, fds, buffer);
    }
}

