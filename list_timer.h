#ifndef __LIST_TIMER_H__
#define __LIST_TIMER_H__

#include <time.h>

#define BUFFER_SIZE 64

struct util_timer;
/* 用户数据结构：客户端socket地址、socket文件描述符、读缓存、定时器 */
struct client_data{
    struct sockaddr_in address;
    int sockfd;
    char buf[BUFFER_SIZE];
    struct util_timer* timer;
};

/* 定时器结构体 */
struct util_timer{
    timer_t expire;                     /* 任务的超时时间，这里使用绝对时间 */
    void (*cb_func) (struct client_data *);    /* 任务的回调函数 */
    struct client_data *user_data;             /* 回调函数处理的客户数据，由定时器的执行者传递给回调函数 */
    struct util_timer *prev;                   /* 指向前一个定时器 */
    struct util_timer *next;                   /* 指向后一个定时器 */
};

/* 双向链表 */
struct timer_list{
    struct util_timer* head;
    struct util_timer* tail;
};
struct timer_list m_list;

void add_timer_nohead(struct util_timer* timer, struct util_timer* lst_head);
void add_timer(struct util_timer *timer);
void adjust_timer(struct util_timer *timer);
void del_timer(struct util_timer *timer);
void print_list();
void tick();

#endif