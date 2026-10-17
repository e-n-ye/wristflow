#ifndef TEST_POWER_RTTHREAD_H
#define TEST_POWER_RTTHREAD_H
#include <stdint.h>
#include <stddef.h>
#define RT_EOK 0
#define RT_ERROR 1
#define RT_EINVAL 2
#define MSH_CMD_EXPORT(function, description)
typedef uint32_t rt_uint32_t;
typedef int32_t rt_int32_t;
typedef uint32_t rt_tick_t;
typedef intptr_t rt_base_t;
struct rt_event { unsigned bits; };
struct rt_mutex { unsigned held; };
struct rt_thread { void (*entry)(void *); void *context; };
typedef struct rt_thread *rt_thread_t;
#define RT_NULL NULL
#define RT_TICK_PER_SECOND 1000
#define RT_WAITING_FOREVER -1
#define RT_IPC_FLAG_PRIO 1
#define RT_EVENT_FLAG_OR 1
#define RT_EVENT_FLAG_CLEAR 2
rt_base_t rt_hw_interrupt_disable(void);
void rt_hw_interrupt_enable(rt_base_t);
rt_tick_t rt_tick_get(void);
rt_tick_t rt_tick_from_millisecond(unsigned);
int rt_event_init(struct rt_event *, const char *, unsigned);
int rt_event_detach(struct rt_event *);
int rt_event_send(struct rt_event *, unsigned);
int rt_event_recv(struct rt_event *, unsigned, unsigned, int, rt_uint32_t *);
rt_thread_t rt_thread_create(const char *, void (*)(void *), void *, unsigned, unsigned, unsigned);
int rt_thread_startup(rt_thread_t);
int rt_thread_delete(rt_thread_t);
int rt_thread_mdelay(rt_int32_t);
void rt_kprintf(const char *, ...);
int rt_mutex_init(struct rt_mutex *, const char *, unsigned);
int rt_mutex_take(struct rt_mutex *, int);
int rt_mutex_release(struct rt_mutex *);
int rt_mutex_detach(struct rt_mutex *);
#endif
