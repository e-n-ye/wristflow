#ifndef WRISTFLOW_TEST_QUEUE_RTTHREAD_H
#define WRISTFLOW_TEST_QUEUE_RTTHREAD_H
#include <stddef.h>
typedef long rt_base_t;
typedef struct test_queue *rt_mq_t;
#define RT_EOK 0
#define RT_EFULL 3
rt_base_t rt_hw_interrupt_disable(void);
void rt_hw_interrupt_enable(rt_base_t level);
int rt_mq_send(rt_mq_t queue, const void *buffer, size_t size);
void *rt_memcpy(void *destination, const void *source, size_t size);
#endif
