
/*
 * Description: 使用双向链表存储定时器（升序排列），这里主要实现增加、删除、
 *              定时器到期时链表调整以及处理到期时的任务
 * Author:      Denny
 * 
 * */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "list_timer.h"


/* 将目标定时器timer添加到节点lst_head之后的链表中 */
void add_timer_nohead(struct util_timer* timer, struct util_timer* lst_head)
{
    struct util_timer *prev = lst_head;
    struct util_timer *tmp = prev->next;

    /* 遍历lst_head节点之后的链表内容，直到找到一个超时时间大于
        目标定时器的超时时间的节点，并将目标定时器插入该节点之前 */
    while(tmp)
    {
        if(timer->expire < tmp->expire)
        {
            prev->next = timer;
            timer->next = tmp;
            tmp->prev = timer;
            timer->prev = prev;
            break;
        }
        prev = tmp;
        tmp = tmp->next;
    }

    /* 如果遍历完lst_head节点之后的链表内容，仍未找到超时时间大于目标定时器的超时时间的节点，
        则将目标定时器插入链表尾部，并把它设置为链表新的尾节点 */
    if(!tmp)
    {
        prev->next = timer;
        timer->prev = prev;

    }
}

/* 将目标定时器timer添加到链表中 */
void add_timer(struct util_timer *timer)
{
    assert(timer != NULL);
    /* 如果head为NULL, 头尾都指向timer */
    if(m_list.head == NULL)
    {
        m_list.head = timer;
        m_list.tail = timer;
        return;
    }

    /* 
     * 如果目标定时器timer的超时时间小于当前
     * 链表中所有的定时器的超时时间，则把该定时器
     * 插入链表头部，作为链表新的头结点，否则调用
     * add_timer_nohead()函数，将它插入到链表合
     * 适的位置，以保证链表的升序特性 
     * */
    if(timer->expire < m_list.head->expire)
    {
        timer->next = m_list.head;
        m_list.head->prev = timer;
        m_list.head = timer;
        return;
    }

    /* 遍历头结点之后的链表数据 */
    add_timer_nohead(timer, m_list.head);

} 

/* 
 * 当某个定时任务发生变化时，调整对应的定时器在链表中的位置，
 * 这个函数只考虑被调整的定时器的超时时间延长的情况,即该定时器
 * 需要往链表的尾部移动 
 * */
void adjust_timer(struct util_timer *timer)
{
    if(!timer)
    {
        return;
    }

    struct util_timer *tmp = timer->next;
    /* 
     * 如果被调整的目标定时器的处在链表的尾部，或者该定时器
     * 新的超时值仍然小于其下一个定时器的超时值，则不用调整
     **/    
    if(!tmp || (timer->expire < tmp->expire ))
    {
        return;
    }

    /* 
     * 如果被调整的目标定时器是链表的头结点，
     * 则将该定时器从链表取出并重新插入链表
     * */
    if(timer == m_list.head )
    {
        m_list.head = m_list.head->next;
        m_list.head->prev = NULL;
        timer->next = NULL;
        add_timer_nohead(timer, m_list.head);
    }
    /*
     * 如果不是头结点，则将该定时器从链表取出，
     * 然后插入其原来所在位置之后的链表中
     */
    else
    {
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer_nohead(timer, timer->next);
    }
}

/* 将目标定时器timer从链表中删除 */
void del_timer(struct util_timer *timer)
{
     assert(timer != NULL);

     /* 只有一个定时器的情况 */
    if((timer == m_list.head)  && (timer == m_list.tail))
    {
        free(timer);
        m_list.head = NULL;
        m_list.tail = NULL;
        return;
    }

    /* 
     * 目标定时器为链表的头结点，将链表的头结点
     * 重置为原头结点的下一个节点，然后删除目标定时器
     */
    if(timer == m_list.head)
    {
        m_list.head = m_list.head->next;
        m_list.head->prev = NULL;
        free(timer);
        timer = NULL;
        return;
    }

    /* 
     * 目标定时器为链表的尾结点，将链表的尾结点
     * 重置为原尾结点的前一个节点，然后删除目标定时器
     */
    if(timer == m_list.tail)
    {
        m_list.tail = m_list.tail->prev;   /*  */
        m_list.tail->next = NULL;
        free(timer);
        timer = NULL;
        return;
    }

    /* 
     * 如果目标定时器位于链表的中间，则把它前后的
     * 定时器串联起来，然后删除目标定时器 
     * */
    timer->prev->next = timer->next;    /* 前一个节点的next指向当前节点的next */
    timer->next->prev = timer->prev;    /* 当前节点的下一个节点的prev指向当前节点的prev */
    free(timer);
    timer = NULL;

}

/* 遍历链表中的定时器并输出 */
void print_list()
{
    struct util_timer *tmp = m_list.head;
    while(tmp)
    {
        printf("the timer is %d\n", tmp->expire);
        tmp = tmp->next;
    }
}

/*
 * SIGALAM信号每次触发就在其信号处理函数(如果使用同一事件源，则是主函数)
 * 中执行一次tick函数，以处理链表上到期的任务
 * */
void tick()
{
    assert(m_list.head != NULL);

    time_t cur = time(NULL);  /* 获取系统当前时间 */
    struct util_timer *tmp = m_list.head;

    /*
     * 从头结点开始处理每个定时器，
     * 直到遇到一个尚未到期的定时器
     */
    while(tmp)
    {
        /*
         * 因为每个定时器都使用绝对时间作为超时值，
         * 所以我们可以把定时器的超时值和系统当前时间
         * 进行对比，以判断定时器是否到期 
         * */
        if(cur < tmp->expire)
        {
            break;
        }
        /* 调整定时器的回调函数，以执行定时任务 */
        tmp->cb_func(tmp->user_data);

        /* 
         * 执行完定时器中的定时任务之后，
         * 就将它从链表中删除，并重置头结点
         */
        m_list.head = tmp->next;
        if(m_list.head)
        {
            m_list.head->prev = NULL;
        }
        free(tmp);
        tmp = m_list.head;
    }
}

