#ifndef WF_TEST_FDB_RTTHREAD_H
#define WF_TEST_FDB_RTTHREAD_H
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#define RT_ASSERT(x) assert(x)
#define rt_malloc malloc
#define rt_free free
int rt_kprintf(const char *, ...);
uint32_t rt_tick_get_millisecond(void);
#endif
