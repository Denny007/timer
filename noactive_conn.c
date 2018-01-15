/*
 * Description：处理非活动连接，利用alarm函数周期性的触发SIGALRM信号，该信号的
 *              信号处理函数利用管道通知主循环（同一事件源）执行定时器链表上的
 *              定时任务，即关闭非活动的连接
 * Author：     Denny
 * 
 * */

#include <assert.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <stdbool.h>
#include <libgen.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>


#include "list_timer.h"

/* 超时时间 */
#define TIMESLOT 5
/* epoll处理的最大事件数目 */
#define MAX_EVENT_NUMBER 1024

/* 信号管道 */
static int pipefd[2];
static int epollfd = 0;

/* 添加非阻塞选项 */
static int set_nonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL); /* 获取fd的flag */
    int new_option = old_option | O_NONBLOCK; /* 添加非阻塞标识 */
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

/* 添加fd到epoll事件表 */
static void add_fd(int epollfd, int fd)
{
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;  /* 边缘触发 */
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    set_nonblocking(fd);
}

static void add_sig(int sig, void (*handler)(int), bool restart)
{
    struct sigaction sa;
    int ret;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if(restart)
    {
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    ret = sigaction( sig, &sa, NULL);

    if(ret == -1)
    {
        exit(-1);
    }
}

/* create a socket and bind */
static int socket_new(const char *ip, const int port)
{
    struct sockaddr_in servaddr;
    int sockfd;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        perror("socket error:");
        return -1;
    }

    bzero(&servaddr, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &servaddr.sin_addr);
    //inet_pton(AF_INET, INADDR_ANY, &servaddr.sin_addr);
    servaddr.sin_port = htons(port);

    /* 设置套接字选项避免地址使用错误 */
    int on = 1;
    if((setsockopt(sockfd, SOL_SOCKET,SO_REUSEADDR, &on, sizeof(on)))<0)  
    {  
        perror("setsockopt failed");  
        exit(EXIT_FAILURE);  
    }  
    if(bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("bind error: ");
        return -1;
    }

    if (listen(sockfd, 5) < 0)
    {
    	perror("bind error: ");
        return -1;
    }
    return sockfd;
}

/* 将信号写入管道，以通知主循环 */
void sig_handler(int sig)
{
    int save_errno = errno;
    int msg = sig;
    send( pipefd[1], (char*)&msg, 1, 0);
    errno = save_errno;
}

/* 定时器回调函数，删除非活动连接socket上的注册事件，并关闭之 */
void cb_func(struct client_data* user_data)
{
    epoll_ctl( epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0 );
    assert(user_data);
    close(user_data->sockfd);
    printf("close fd %d\n", user_data->sockfd);
}

/* 处理定时任务 */
void timer_handler()
{
     printf("time is out \n");
    /* 定时处理任务 */
    tick();

    /* 
     * 一次alarm调用只会引起一次SIGALRM信号
     * 所以我们要重新定时，以不断触发SIGALRM信号
     */
    alarm( TIMESLOT );
}

/* 统一事件源，监听信号，以及添加信号处理函数 */
static void set_sig_pipe(int epollfd)
{
    int ret;
    /* 创建信号管道 */
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    if(ret == -1)
    {
        perror("create socketpair failed \n");
        return;
    }

    set_nonblocking(pipefd[1]);  /* 将写端设置为非阻塞 */
    add_fd(epollfd, pipefd[0]);  /* 将读端添加到epoll事件集中进行监听 */

    /* 添加信号处理 */
    add_sig(SIGALRM, sig_handler, true);
    add_sig(SIGTERM, sig_handler, true);
}

int main(int argc, char* argv[])
{
    if( argc <= 2 )
    {
        printf( "usage: %s ip_address port_number\n", basename(argv[0]));
        return 1;
    }
    
    int ret = 0;
    int listenfd = 0;
    struct epoll_event events[MAX_EVENT_NUMBER];
    int i, number;

    const char* ip = argv[1];
    const int port = atoi(argv[2]);
    /* socket的监听描述符 */
    listenfd = socket_new(ip, port);
    
    epollfd = epoll_create(5);
    if(epollfd == -1)
    {
        perror("create epoll failed \n");
        return -1;
    }
    add_fd(epollfd, listenfd);
    
    /* 统一事件源，将信号和IO处理一起处理 */
    set_sig_pipe(epollfd);

    bool stop_server = false;
    struct client_data *users = (struct client_data*)malloc(sizeof(struct client_data));
    bool timeout = false;
    alarm(TIMESLOT); /* 定时器 */

    while(!stop_server)
    {
        //获取就绪的文件描述符个数
        number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if ( ( number < 0 ) && ( errno != EINTR ) )
        {
            printf( "epoll failure\n" );
            break;
        }

        for(i = 0; i < number; i++)
        {
            int sockfd = events[i].data.fd;
            /* 处理新的客户连接 */
            if(sockfd == listenfd)
            {
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept( listenfd, ( struct sockaddr* )&client_address, &client_addrlength );
                /* 添加connfd到epoll事件集中 */
                add_fd( epollfd, connfd );

                /* 填充用户数据 */
                users[connfd].address = client_address;
                users[connfd].sockfd = connfd;
                
                /* 
                 * 创建定时器，设置其回调函数与超时时间，然后绑定定时器与用户数据
                 * 最后将定时器添加到链表timer_list中  
                 * */
                struct util_timer *timer = (struct util_timer *)malloc(sizeof(struct util_timer));
                timer->user_data = &users[connfd];   /* 用户数据，传递给回调函数处理 */
                timer->cb_func = cb_func;            /* 定时器的回调函数 */
                time_t cur = time(NULL);
                timer->expire = cur + 3 * TIMESLOT;

                users[connfd].timer = timer;
                add_timer(timer);                   /* 添加到链表 */

            }
            /* 处理信号 */
            else if( ( sockfd == pipefd[0] ) && ( events[i].events & EPOLLIN ) )
            {
                int sig;
                char signals[1024];
                ret = recv( pipefd[0], signals, sizeof(signals), 0);
                if( ret == -1 )
                {
                    /* 可以在这里添加错误处理 */
                    continue;
                }
                else if( ret == 0 )
                {
                    continue;
                }
                else
                {
                    for(i = 0; i < ret; ++i )
                    {
                        switch( signals[i] )
                        {
                            case SIGALRM:
                            {
                                /* 
                                 * 收到SIGALRM时，将timeout用来标记有定时任务需要处理，但不立即处理定时任务
                                 * 因为定时任务的优先级不是很高，我们优先处理其他更重要的任务
                                 * */
                                timeout = true;
                                break;
                            }
                            case SIGTERM:
                            {
                                stop_server = true;
                            }
                        }
                    }
                }
            }
            /* 处理客户连接接收到的数据 */
            else if(events[i].events & EPOLLIN )
            {
                memset(users[sockfd].buf, '\0', BUFFER_SIZE);
                ret = recv(sockfd, users[sockfd].buf, BUFFER_SIZE - 1, 0);
                printf( "get %d bytes of client data %s from %d\n", ret, users[sockfd].buf, sockfd );
                struct util_timer* timer = users[sockfd].timer;
                if(ret < 0)
                {
                    /* 如果发生读错误，则关闭连接，并移除其对应的定时器 */
                    if(errno != EAGAIN)
                    {
                        cb_func(&users[sockfd]);
                        if(timer)
                        {
                            del_timer(timer);
                        }
                    }
                }
                else if(ret == 0)
                {
                    /* 对方关闭连接，则我们也关闭连接，并移除对应的定时器 */
                    cb_func( &users[sockfd] );
                    if( timer )
                    {
                        del_timer( timer );
                    }
                }
                else
                {
                    /* 有数据可读，则调整该连接对应的定时器，以延迟该连接被关闭的时间 */
                    if(timer)
                    {
                        time_t cur = time( NULL );
                        timer->expire = cur + 3 * TIMESLOT;
                        printf( "adjust timer once\n" );
                        adjust_timer( timer );
                    }

                }
            }
            else{

            }
        }
        /* 最后处理定时事件，因为I/O事件拥有更高的优先级
         * 当然，这样做将导致定时任务不能精确的按照预期执行
         */
        if(timeout)
        {
            timer_handler();
            timeout = false;
        }
    }

    close(listenfd);
    close(pipefd[0]);
    close(pipefd[1]);
    free(users);
    users = NULL;

    return 0;

}

/* 测试双向链表 */
/*void test()
{
    struct util_timer *timer1 = (struct util_timer *)malloc(sizeof(struct util_timer));
    struct util_timer *timer2 = (struct util_timer *)malloc(sizeof(struct util_timer));
    struct util_timer *timer3 = (struct util_timer *)malloc(sizeof(struct util_timer));
    time_t cur = time( NULL );
    
    timer1->expire = cur + TIMESLOT;
    timer2->expire = cur + 2 * TIMESLOT;
    timer3->expire = cur +  3 * TIMESLOT;
    add_timer(timer1);
    add_timer(timer2);
    add_timer(timer3);

    print_list();


    del_timer(timer2);
     print_list();
}*/