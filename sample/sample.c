#include<stdio.h>
#include<stdlib.h>
#include<sys/time.h>
#include<unistd.h>
#include<time.h>
#include"MESA_timer.h"


typedef struct event_t{
    int id;
}event_t;

void event_cb(void * event)
{
    event_t * _event = (event_t *)event;
    struct timeval now;
    gettimeofday(&now, NULL);
    long curtime = now.tv_sec * 1000000 + now.tv_usec;
    printf("event %d timeout, current time is %ld\n", _event->id, curtime / 1000);
    return;
}

void unit_test_for_queue()
{
    MESA_timer_t * timer = MESA_timer_create(0, TM_TYPE_QUEUE);

    struct timeval current_time;
    long timeout = 5000;
    long curtime;
    MESA_timer_index_t * indexs[10000];
    event_t events[10000];
    int i = 0;
    while(i < 10000)
    {
        events[i].id = i;
        gettimeofday(&current_time, NULL);
        curtime = current_time.tv_sec * 1000000 + current_time.tv_usec;

        MESA_timer_check(timer, curtime, 10000);
        if(MESA_timer_add(timer, curtime, timeout, event_cb, &events[i], &indexs[i]) < 0)
        {
            printf("add event %d failed\n", i);
        }
        else
        {
            printf("add event %d success, it will occur at %ld\n", i, (curtime + timeout) / 1000);
        }

        usleep(100);
        i++;
    }
    printf("%ld\n", MESA_timer_count(timer));
    printf("%ld\n", MESA_timer_memsize(timer));
    MESA_timer_destroy(timer, NULL, NULL);
}


void unit_test_for_wheel()
{
    MESA_timer_t * timer = MESA_timer_create(3000, TM_TYPE_WHEEL);

    srand((int)time(NULL));

    struct timeval current_time;
    long curtime;

    MESA_timer_index_t * indexs[10000];
    event_t events[10000];
    int i = 0;

    while(i < 10000)
    {
        long timeout = (rand() % 10 + 1) * 1000;
        events[i].id = i;
        gettimeofday(&current_time, NULL);
        curtime = current_time.tv_sec * 1000000 + current_time.tv_usec;

        MESA_timer_check(timer, curtime, 10000);
        if(MESA_timer_add(timer, curtime, timeout, event_cb, &events[i], &indexs[i]) < 0)
        {
            printf("add event %d failed\n", i);
        }
        else
        {
            printf("add event %d at time %ld, timeout %ld, it will occur at %ld\n", \
                    i, curtime / 1000, timeout,  (curtime + timeout) / 1000);
        }

        usleep(100);
        i++;
    }
    MESA_timer_destroy(timer, NULL, NULL);
}


void wheel_test()
{
    MESA_timer_t * timer = MESA_timer_create(6, TM_TYPE_WHEEL);

    srand((int)time(NULL));

    struct timeval current_time;
    long curtime;

    MESA_timer_index_t * indexs[20];
    event_t events[20];
    int i = 0;

    while(i < 20)
    {
        long timeout = rand() % 10;
        events[i].id = i;
        curtime = i;
        MESA_timer_check(timer, curtime, 10000);
        if(MESA_timer_add(timer, curtime, timeout, event_cb, &events[i], &indexs[i]) < 0)
        {
            printf("add event %d failed\n", i - 10);
        }
        else
        {
            printf("add event %d at time %ld, timeout %ld, it will occur at %ld\n", \
                    i, curtime, timeout,  curtime + timeout);
        }

        sleep(1);
        i++;
    }
    MESA_timer_destroy(timer, NULL, NULL);
}



int main()
{
    /* unit_test_for_wheel(); */
    unit_test_for_queue();
    /* wheel_test(); */
    return 0;
}
