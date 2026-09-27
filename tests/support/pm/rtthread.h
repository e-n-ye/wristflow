#ifndef TEST_PM_RTTHREAD_H
#define TEST_PM_RTTHREAD_H
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
typedef uint32_t rt_tick_t;
typedef uint32_t rt_uint32_t;
typedef int32_t rt_int32_t;
typedef int rt_err_t;
typedef int rt_base_t;
struct rt_event { uint32_t pending; };
struct rt_timer { void (*callback)(void *); void *context; uint32_t ticks; };
#define RT_USING_PM
#define RT_EOK 0
#define RT_ERROR 1
#define RT_ETIMEOUT 2
#define RT_EINVAL 3
#define RT_ENOSYS 4
#define RT_WAITING_FOREVER -1
#define RT_IPC_FLAG_FIFO 0
#define RT_EVENT_FLAG_OR 1
#define RT_EVENT_FLAG_CLEAR 2
#define RT_TIMER_FLAG_ONE_SHOT 0
#define RT_TIMER_FLAG_SOFT_TIMER 4
#define RT_TICK_PER_SECOND 1000
#define RT_ASSERT(x) assert(x)
#define MSH_CMD_EXPORT(fn, description)
rt_base_t rt_hw_interrupt_disable(void);
void rt_hw_interrupt_enable(rt_base_t level);
rt_tick_t rt_tick_get(void);
rt_tick_t rt_tick_from_millisecond(int ms);
rt_err_t rt_event_init(struct rt_event *, const char *, unsigned);
rt_err_t rt_event_send(struct rt_event *, uint32_t);
rt_err_t rt_event_recv(struct rt_event *, uint32_t, unsigned, int32_t, uint32_t *);
void rt_timer_init(struct rt_timer *, const char *, void (*)(void *), void *, uint32_t, unsigned);
rt_err_t rt_timer_start(struct rt_timer *);
void rt_kprintf(const char *, ...);
#endif
