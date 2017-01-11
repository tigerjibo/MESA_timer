/************************************************
*				MESA timer API
* author:tangqi@iie.ac.cn,zhengchao@iie.ac.cn
* version: v1.0
* last modify:2015-8-19
************************************************/
#include "MESA_timer.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/queue.h>

const char *MESA_timer_version_VERSION_20150918 = "MESA_timer_version_VERSION_20150918";

#ifndef IN_TIMER 
#define IN_TIMER 1
#endif

#ifndef NOT_IN_TIMER
#define NOT_IN_TIMER 0
#endif

/**
 * A timer element's structure
 **/
typedef struct _timer_elem_t{
    long expire;                 /* event's absolute expire */
    int rotation_cnt;            /* used in time wheel */
    int cursor;                  /* use in time wheel, representing the spoke index */
    timeout_cb_t timeout_cb;      /* event's callback function */

    void *event;                 /* event */
    event_free_cb_t free_cb;     /* event free callback function */

    int status;                  /* whether the elem is in timer: IN_TIMER or NOT_IN_TIMER */
    TAILQ_ENTRY(_timer_elem_t) ENTRYS;
}timer_elem_t;


/**
 * DouleLinkedList: TQ
 **/
TAILQ_HEAD(TQ, _timer_elem_t);

/**
 * Time queue structure
 **/
typedef struct _timer_queue_t{
    long last_expire_time;      /* time queue's last emement's expire */
    struct TQ queue;            /* a queue to store timer ENTRYS */
}timer_queue_t;



/**
 * Time wheel structure
 **/
typedef struct _timer_wheel_t{
    long wheel_size;                            /* size of time wheel */
    long create_time;                           /* creating time of wheel, we consider it as the first add's time */
    long spoke_index;                           /* current spoke index*/
    long last_check_relative_tick;              /* last check's relative ticks */
    struct TQ *spokes;       /* queues array */
}timer_wheel_t;


/**
 * Timer's structure
 **/
typedef struct _MESA_timer_inner_t{
    int type;                           /* type of timer: TM_TYPE_QUEUE and TM_TYPE_WHEEL */
    union{                              /* time queue or time wheel */
        timer_queue_t timer_queue;
        timer_wheel_t timer_wheel;
    };
    long elem_cnt;                      /* timer ENTRYSs' count */
    long mem_ocupy;                     /* memory occupation */
}MESA_timer_inner_t;



MESA_timer_t *MESA_timer_create(long wheel_size, int tm_type)
{
    MESA_timer_inner_t *timer = NULL;
    switch(tm_type)
    {
        case TM_TYPE_QUEUE:
        {
            timer = (MESA_timer_inner_t *)malloc(sizeof(MESA_timer_inner_t));
            timer->type = TM_TYPE_QUEUE;
            timer->timer_queue.last_expire_time = -1l;
            TAILQ_INIT(&(timer->timer_queue.queue));

            timer->elem_cnt = 0;
            timer->mem_ocupy = sizeof(MESA_timer_inner_t);
            break;
        }
        case TM_TYPE_WHEEL:
        {
            if(wheel_size <= 0 || wheel_size > MAX_WHEEL_SIZE)
            {
                return (MESA_timer_t *)NULL;
            }
            timer = (MESA_timer_inner_t *)malloc(sizeof(MESA_timer_inner_t));
            timer->type = TM_TYPE_WHEEL;
            timer->timer_wheel.wheel_size = wheel_size;
            timer->timer_wheel.create_time = -1;
            timer->timer_wheel.last_check_relative_tick = -1;
            timer->timer_wheel.spoke_index = 0;
            timer->timer_wheel.spokes = (struct TQ *)malloc(sizeof(struct TQ) * wheel_size);

            int i;
            for(i = 0; i < wheel_size; i++)
            {
                TAILQ_INIT(&(timer->timer_wheel.spokes[i]));
            }
            timer->elem_cnt = 0;
            timer->mem_ocupy = sizeof(MESA_timer_inner_t) + sizeof(struct TQ) * wheel_size;

            break;
        }
        default:
            break;
    }

    return (MESA_timer_t *)timer;
}



void MESA_timer_destroy(MESA_timer_t *timer)
{
    assert(timer != NULL);

    MESA_timer_inner_t *_timer = (MESA_timer_inner_t *)timer;
    switch(_timer->type)
    {
        case TM_TYPE_QUEUE:
        {
            timer_elem_t *tmp_elem = TAILQ_FIRST(&(_timer->timer_queue.queue));
            timer_elem_t *tmp;
            while(tmp_elem != NULL)
            {
                tmp = TAILQ_NEXT(tmp_elem, ENTRYS);
                if(tmp_elem->free_cb != NULL)
                {
                    tmp_elem->free_cb(tmp_elem->event);
                }
                free(tmp_elem);
                tmp_elem = tmp;
            }
            break;
        }
        case TM_TYPE_WHEEL:
        {
            int i;
            for(i = 0; i < _timer->timer_wheel.wheel_size; i++)
            {
                struct TQ *spoke = &(_timer->timer_wheel.spokes[i]);
                timer_elem_t *tmp_elem = TAILQ_FIRST(spoke);
                timer_elem_t *tmp;
                while(tmp_elem != NULL)
                {
                    tmp = TAILQ_NEXT(tmp_elem, ENTRYS);
                    if(tmp_elem->free_cb != NULL)
                    {
                        tmp_elem->free_cb(tmp_elem->event);
                    }
                    free(tmp_elem);
                    tmp_elem = tmp;
                }
            }
            free(_timer->timer_wheel.spokes);
            break;
        }
        default:
            break;
    }
    free(timer);
    return;
}



int MESA_timer_add(MESA_timer_t *timer, 
                   long current_time, 
                   long timeout,
                   timeout_cb_t timeout_cb,
                   void *event, 
                   event_free_cb_t free_cb,
                   MESA_timer_index_t **index)
{
    assert(timer != 0 && current_time >= 0 && timeout >= 0);

    MESA_timer_inner_t *_timer = (MESA_timer_inner_t *)timer;
    switch(_timer->type)
    {
        case TM_TYPE_QUEUE:
        {
            long expire = current_time + timeout;
            if(expire < _timer->timer_queue.last_expire_time)
            {
                *index = NULL;
                return -1;
            }
            _timer->timer_queue.last_expire_time = expire;

            timer_elem_t *elem = (timer_elem_t *)malloc(sizeof(timer_elem_t));
            elem->expire = expire;
            elem->timeout_cb = timeout_cb;

            elem->event = event;
            elem->free_cb = free_cb;
            /* insert a timer ENTRYS to tail of timer queue */
            TAILQ_INSERT_TAIL(&(_timer->timer_queue.queue), elem, ENTRYS);
            elem->status= IN_TIMER;

            _timer->elem_cnt += 1;
            _timer->mem_ocupy += sizeof(timer_elem_t);

            *index = (MESA_timer_index_t *)elem;
            return 0;
        }
        case TM_TYPE_WHEEL:
        {
            timer_wheel_t *wheel = &(_timer->timer_wheel);

            /* the first timer ENTRYS start the timer, and current_time's relative time is 0 */
            if(wheel->last_check_relative_tick == -1)
            {
                wheel->create_time = current_time;
                wheel->spoke_index = 0;
                wheel->last_check_relative_tick = 0;
            }

            timer_elem_t *elem = (timer_elem_t *)malloc(sizeof(timer_elem_t));
            elem->expire = current_time + timeout;
            elem->timeout_cb = timeout_cb;

            elem->event = event;
            elem->free_cb = free_cb;

            long td = timeout % wheel->wheel_size;
            elem->rotation_cnt = timeout / wheel->wheel_size;
            long cursor = (current_time - wheel->create_time + td) % wheel->wheel_size;
            elem->cursor = cursor;

            /* insert a timer ENTRYS to tail of timer queue */
            TAILQ_INSERT_TAIL(&(wheel->spokes[cursor]), elem, ENTRYS);
            elem->status = IN_TIMER;

            /* update stat data */
            _timer->elem_cnt += 1;
            _timer->mem_ocupy += sizeof(timer_elem_t);

            *index = (MESA_timer_index_t *)elem;
            return 0;
        }
        default:
        {
            *index = NULL;
            return -1;
        }
    }
}



long MESA_timer_del(MESA_timer_t *timer, MESA_timer_index_t* index)
{
    assert(timer != NULL && index != NULL);

    MESA_timer_inner_t *_timer = (MESA_timer_inner_t *)timer;
    timer_elem_t *elem = (timer_elem_t *)index;

    if(elem->status == NOT_IN_TIMER)
    {
        return -1;
    }

    long ret_timeout = -1;
    switch(_timer->type)
    {
        case TM_TYPE_QUEUE:
        {
            ret_timeout = elem->expire;
            /* remove from timer queue */
            TAILQ_REMOVE(&(_timer->timer_queue.queue), elem, ENTRYS);
            elem->status = NOT_IN_TIMER;

            _timer->elem_cnt --;
            _timer->mem_ocupy -= sizeof(timer_elem_t);

            /* update timer queue's last_expire_time */
            if(!TAILQ_EMPTY(&(_timer->timer_queue.queue)))
            {
                timer_elem_t *tail = TAILQ_LAST(&(_timer->timer_queue.queue), TQ);
                _timer->timer_queue.last_expire_time = tail->expire;
            }
            else
            {
                _timer->timer_queue.last_expire_time = -1;
            }

            if(elem->free_cb != NULL)
            {
                elem->free_cb(elem->event);
            }
            free(elem);
            break;
        }
        case TM_TYPE_WHEEL:
        {
            ret_timeout = elem->expire;
            TAILQ_REMOVE(&(_timer->timer_wheel.spokes[elem->cursor]), elem, ENTRYS);
            elem->status = NOT_IN_TIMER;

            _timer->elem_cnt --;
            _timer->mem_ocupy -= sizeof(timer_elem_t);

            if(elem->free_cb != NULL)
            {
                elem->free_cb(elem->event);
            }
            free(elem);
            break;
        }
        default:
            break;
    }
    return ret_timeout;
}



long MESA_timer_check(MESA_timer_t *timer, long current_time, long max_cb_times)
{
    assert(timer != NULL && current_time >= 0 && max_cb_times >= 0);

    long cb_cnt = 0;
    MESA_timer_inner_t *_timer = (MESA_timer_inner_t *)timer;

    switch(_timer->type)
    {
        case TM_TYPE_QUEUE:
        {
            timer_elem_t *tmp_elem = TAILQ_FIRST(&(_timer->timer_queue.queue));
            timer_elem_t *tmp;
            while(tmp_elem != NULL)
            {
                if(cb_cnt >= max_cb_times)
                {
                    break;
                }
                /* All ENTRYSs after tmp_elem haven't time out */
                if(current_time < tmp_elem->expire)
                {
                    break;
                }

                tmp = TAILQ_NEXT(tmp_elem, ENTRYS);
                TAILQ_REMOVE(&(_timer->timer_queue.queue), tmp_elem, ENTRYS);
                tmp_elem->status= NOT_IN_TIMER;
            
                _timer->elem_cnt --;
                _timer->mem_ocupy -= sizeof(timer_elem_t);

                /* elem has timed out */
                tmp_elem->timeout_cb(tmp_elem->event);
                cb_cnt ++;

                if(tmp_elem->status == NOT_IN_TIMER)
                {
                    if(tmp_elem->free_cb != NULL)
                    {
                        tmp_elem->free_cb(tmp_elem->event);
                    }
                    free(tmp_elem);
                }
                tmp_elem = tmp;
            }
            return cb_cnt;
        }
        case TM_TYPE_WHEEL:
        {
            timer_wheel_t *wheel = &(_timer->timer_wheel);
            struct TQ *spoke;

            if(wheel->create_time == -1)
                return 0;

            int i, cb_max_flag = 0;
            long tickcnt = current_time - wheel->create_time - wheel->last_check_relative_tick;
            wheel->last_check_relative_tick += tickcnt;

            for(i = 0; i < tickcnt; i++)
            {
                spoke = &(wheel->spokes[wheel->spoke_index]);
                timer_elem_t *tmp_elem = TAILQ_FIRST(spoke);
                timer_elem_t *tmp;
                while(tmp_elem != NULL)
                {
                    if(tmp_elem->rotation_cnt != 0)
                    {
                        tmp_elem->rotation_cnt --;
                        tmp_elem = TAILQ_NEXT(tmp_elem, ENTRYS);
                    }
                    else
                    {
                        if(cb_cnt >= max_cb_times)
                        {
                            cb_max_flag = 1;
                            break;
                        }
                        tmp = TAILQ_NEXT(tmp_elem, ENTRYS);
                        TAILQ_REMOVE(spoke, tmp_elem, ENTRYS);
                        tmp_elem->status = NOT_IN_TIMER;

                        _timer->elem_cnt --;
                        _timer->mem_ocupy -= sizeof(timer_elem_t);
                        
                        tmp_elem->timeout_cb(tmp_elem->event);
                        cb_cnt ++;

                        if(tmp_elem->status == NOT_IN_TIMER)
                        {
                            if(tmp_elem->free_cb != NULL)
                            {
                                tmp_elem->free_cb(tmp_elem->event); 
                            }
                            free(tmp_elem);
                        }
                        tmp_elem = tmp;
                    }
                }
                if(cb_max_flag == 1)
                    break;
                wheel->spoke_index = (wheel->spoke_index + 1) % wheel->wheel_size;
            }
            return cb_cnt;
        }
        default:
        {
            return -1;
        }
    }
}



int MESA_timer_reset(MESA_timer_t *timer, MESA_timer_index_t *index, long current_time, long timeout)
{
    assert(timer != NULL && index != NULL);

    MESA_timer_inner_t *_timer = (MESA_timer_inner_t *)timer;
    timer_elem_t *elem = (timer_elem_t *)index;

    switch(_timer->type)
    {
        case TM_TYPE_QUEUE:
        {
            /* remove from timer queue */
            if(elem->status == IN_TIMER)
            {
                TAILQ_REMOVE(&(_timer->timer_queue.queue), elem, ENTRYS);
                elem->status = NOT_IN_TIMER;
                _timer->elem_cnt --;
                _timer->mem_ocupy -= sizeof(timer_elem_t);
            }

            /* update timer queue's last_expire_time */
            if(!TAILQ_EMPTY(&(_timer->timer_queue.queue)))
            {
                timer_elem_t *tail = TAILQ_LAST(&(_timer->timer_queue.queue), TQ);
                _timer->timer_queue.last_expire_time = tail->expire;
            }
            else
            {
                _timer->timer_queue.last_expire_time = -1;
            }

            long expire = current_time + timeout;
            if(expire < _timer->timer_queue.last_expire_time)
            {
                return -1;
            }
            _timer->timer_queue.last_expire_time = expire;
            elem->expire = expire;
            /* insert a timer ENTRYS to tail of timer queue */
            if(elem->status == NOT_IN_TIMER)
            {
                TAILQ_INSERT_TAIL(&(_timer->timer_queue.queue), elem, ENTRYS);
                elem->status = IN_TIMER;
                _timer->elem_cnt ++;
                _timer->mem_ocupy += sizeof(timer_elem_t);
            }
            
            return 0;
        }
        case TM_TYPE_WHEEL:
        {
            timer_wheel_t *wheel = &(_timer->timer_wheel);
            if(elem->status == IN_TIMER)
            {
                TAILQ_REMOVE(&(wheel->spokes[elem->cursor]), elem, ENTRYS);
                elem->status = NOT_IN_TIMER;
                _timer->elem_cnt --;
                _timer->mem_ocupy -= sizeof(timer_elem_t);
            }

            /* the first timer ENTRYS start the timer, and current_time's relative time is 0 */
            if(wheel->last_check_relative_tick == -1)
            {
                wheel->create_time = current_time;
                wheel->last_check_relative_tick = 0;
                wheel->spoke_index = 0;
            }

            /* if expire time of user's reset operation is earlier than Timer's current time, 
             * we need set it with current time and make it timeout when next checking */
            long timer_curr_time = wheel->create_time + wheel->last_check_relative_tick;
            long user_reset_expire_time = current_time + timeout;
            if(timer_curr_time >= user_reset_expire_time)
            {
                timeout = 1;
                current_time = timer_curr_time;
            }

            elem->expire = current_time + timeout;

            long td = timeout % wheel->wheel_size;
            elem->rotation_cnt = timeout / wheel->wheel_size;
            long cursor = (current_time - wheel->create_time + td) % wheel->wheel_size;
            elem->cursor = cursor;

            /* insert a timer ENTRYS to tail of timer queue */
            if(elem->status == NOT_IN_TIMER)
            {
                TAILQ_INSERT_TAIL(&(wheel->spokes[cursor]), elem, ENTRYS);
                elem->status = IN_TIMER;
                _timer->elem_cnt ++;
                _timer->mem_ocupy += sizeof(timer_elem_t);
            }
            return 0;
        }
        default:
        {
            return -1;
        }
    }
}


long MESA_timer_count(MESA_timer_t *timer)
{
    assert(timer != NULL);
    return ((MESA_timer_inner_t *)timer)->elem_cnt;
}



long MESA_timer_memsize(MESA_timer_t *timer)
{
    assert(timer != NULL);
    return ((MESA_timer_inner_t *)timer)->mem_ocupy;
}

