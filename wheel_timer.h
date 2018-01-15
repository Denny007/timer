#ifndef __WHEEL_TIMER__
#define __WHEEL_TIMER__

#include <time.h>

#define BUFFER_SIZE 64

#define N   60        /* 时间轮上槽的数目 */
#define SI  1        /* 每1秒时间轮转动一次，即槽间隔为1秒 */

/* 用户数据结构：客户端socket地址、socket文件描述符、读缓存、定时器 */
struct client_data{
    struct sockaddr_in address;
    int sockfd;
    char buf[BUFFER_SIZE];
    struct wheel_timer* timer;
};


/* 定时器结构体 */
struct wheel_timer{
    int rotation;                     /* 记录定时器在时间轮转多少圈之后生效 */
    int time_slot;                    /* 记录定时器属于时间轮的哪个槽(对应的链表) */
    void (*cb_func) (struct client_data *);    /* 任务的回调函数 */
    struct client_data *user_data;             /* 回调函数处理的客户数据，由定时器的执行者传递给回调函数 */
    struct wheel_timer *prev;                   /* 指向前一个定时器 */
    struct wheel_timer *next;                   /* 指向后一个定时器 */
};


/* 时间轮 */
struct wheel{
    struct wheel_timer *slots[N];    /* 时间轮上的槽，其中每个元素指向一个定时器链表，链表无序 */
    int cur_slot;                   /* 时间轮的当前槽 */
};


struct wheel wh;


struct wheel_timer* add_timer(int timeout);
void del_timer(struct wheel_timer *timer);
void tick();

#endif