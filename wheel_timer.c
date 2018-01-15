
/*
 * Description: 使用时间轮实现定时器，这里主要实现增加、删除
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

#include "wheel_timer.h"

void init_wheel()
{
    int i;
    wh.cur_slot = 0;
    for(i = 0; i < N; i++)
    {
        wh.slots[i] = NULL;
    }
}

/* 根据定时值timeout创建一个定时器，并把它插入合适的槽中 */
struct wheel_timer* add_timer(int timeout)
{
    if(timeout < 0)
    {
        return NULL;
    }

    /* 滴答数 */
    int ticks = 0;

    /* 
     * 根据待插入定时器的超时值计算它将在时间轮转动多少个滴答后触发，
     * 并将该滴答数存储于变量ticks中。如果待插入定时器的超时值小于时间轮
     * 的槽间隔SI，则将ticks向上折合为1，否则就将ticks向下折合为timeout/SI
     */
    if(timeout < SI)
    {
        ticks = 1;
    }
    else
    {
        ticks = timeout / SI;
    }

    /* 待插入的定时器在时间轮转动多少圈之后被触发 */
    int rotation = ticks / N;
    /* 定时器应该被插入到哪个槽中 */
    int ts = (wh.cur_slot + (ticks % N)) % N;
    
    /* 创建新的定时器，它在时间轮转动rotation圈之后被触发，且位于第ts个槽上 */
    struct wheel_timer *timer = (struct wheel_timer *)malloc(sizeof(struct wheel_timer));
    timer->rotation = rotation;
    timer->time_slot = ts;

    /*
     * 如果第ts个槽上尚无任何定时器，则把新建的定时器插入其中
     * 并将该定时器设置为该槽的头结点
     */
    if( wh.slots[ts] == NULL )
    {
        printf("add timer, rotaion is %d, ts is %d, cur_slot is %d \n", rotation, ts, wh.cur_slot);
        wh.slots[ts] = timer;
    }
    /* 在第ts个槽中插入定时器 */
    else
    {
        timer->next = wh.slots[ts];
        wh.slots[ts]->prev = timer;
        wh.slots[ts] = timer;
    }

    return timer;
}

void del_timer(struct wheel_timer *timer)
{
    if(timer == NULL)
    {
        return;
    }
    int ts = timer->time_slot;

    /* slots[ts]是目标定时器所在槽的头结点，
     * 如果目标定时器是该头结点，则需要重置
     * 第ts个槽的头结点
     * */
    if(timer == wh.slots[ts])
    {
        wh.slots[ts] = wh.slots[ts]->next;
        /* 不是只有一个节点 */
        if(wh.slots[ts] != NULL)
        {
            wh.slots[ts]->prev = NULL;
        }
        free(timer);
        timer = NULL;
    }
    /* 不是头结点的情况 */
    else{
        timer->prev->next = timer->next;
        /* 如果不是最后一个节点 */
        if(timer->next != NULL)
        {
            timer->next->prev = timer->prev;
        }
        free(timer);
        timer = NULL;
    }
}

/*
 * SI时间到后，调用该函数，时间轮向前滚动一个槽的间隔
 */
void tick()
{
    struct wheel_timer *tmp = wh.slots[wh.cur_slot];   /* 时间轮当前槽的头结点 */
    printf("The current slot is %d \n", wh.cur_slot);
    while(tmp != NULL)
    {
        printf("tick the timer once \n");
        /* 如果定时器的ratation值大于0， 则它在这一轮不起作用 */
        if(tmp->rotation > 0)
        {
            tmp->rotation--;
            tmp = tmp->next;   /* 指向该槽的下一个节点 */
        }
        /* 否则，说明定时器已经到期，于是执行定时任务，然后删除该定时器 */
        else{
            tmp->cb_func(tmp->user_data);
            /* 如果是头结点 */
            if(tmp == wh.slots[wh.cur_slot])
            {
                printf("delete header in cur_slot\n");
                wh.slots[wh.cur_slot] = tmp->next;
                free(tmp);
                /* 该节点存在 */
                if(wh.slots[wh.cur_slot] != NULL)
                {
                    wh.slots[wh.cur_slot]->prev = NULL;
                }
                tmp = wh.slots[wh.cur_slot];      /* tmp指向头结点 */
            }
            /* 不是头结点的情况 */
            else
            {
                tmp->prev->next = tmp->next;
                /* 下一个节点不为空 */
                if(tmp->next != NULL)
                {
                    tmp->next->prev = tmp->prev;
                }
                struct wheel_timer *timer = tmp;
                free(tmp);
                tmp = timer;
            }
        }        
    }
    wh.cur_slot = (++wh.cur_slot) % N;   /* 更新时间轮的当前槽，以反映时间轮的转动 */
}

int main(int argc, char *argv[])
{
    init_wheel();

    return 0;
}