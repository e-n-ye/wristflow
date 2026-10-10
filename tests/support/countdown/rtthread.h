#ifndef TEST_COUNTDOWN_RTTHREAD_H
#define TEST_COUNTDOWN_RTTHREAD_H
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
typedef uint32_t rt_tick_t;
typedef uint32_t rt_uint32_t;
typedef int32_t rt_int32_t;
typedef int rt_err_t;
struct rt_mutex { bool held; };
struct rt_timer { void (*callback)(void *); void *context; uint32_t ticks, at; bool active; };
#define RT_EOK 0
#define RT_ERROR 1
#define RT_WAITING_FOREVER -1
#define RT_IPC_FLAG_PRIO 1
#define RT_TIMER_FLAG_ONE_SHOT 0
#define RT_TIMER_FLAG_SOFT_TIMER 4
#define RT_TIMER_CTRL_SET_TIME 0
#define RT_TICK_PER_SECOND 1000
#define RT_ASSERT(x) assert(x)
rt_tick_t rt_tick_get(void);
rt_err_t rt_mutex_init(struct rt_mutex *, const char *, unsigned);
rt_err_t rt_mutex_take(struct rt_mutex *, int32_t);
rt_err_t rt_mutex_release(struct rt_mutex *);
void rt_timer_init(struct rt_timer *, const char *, void (*)(void *), void *, uint32_t, unsigned);
rt_err_t rt_timer_stop(struct rt_timer *);
rt_err_t rt_timer_control(struct rt_timer *, int, void *);
rt_err_t rt_timer_start(struct rt_timer *);
/* Deterministic RT timer thread independent from any LVGL timer. */
void test_countdown_advance(uint32_t ms, bool dispatch);
void test_countdown_selected_callback(void);
uint32_t test_countdown_wait(void);
uint32_t test_countdown_events(void);
unsigned test_countdown_sends(void);
#endif
