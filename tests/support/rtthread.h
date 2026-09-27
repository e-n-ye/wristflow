#ifndef WF_HOST_RTTHREAD_H
#define WF_HOST_RTTHREAD_H
/* Only the allocator/memory calls used by the pinned SDK cJSON source. */
#include <stdlib.h>
#include <string.h>
#define rt_malloc malloc
#define rt_free free
#define rt_realloc realloc
#define rt_memcpy memcpy
#define rt_memset memset
#endif
