#include <assert.h>
#include <stdbool.h>
static bool assertions_enabled;
/* Match RT_ASSERT's disabled semantics: do not evaluate its argument. */
#define RT_ASSERT(condition) do { if (assertions_enabled) assert(condition); } while (0)
#define WF_STORAGE_GATE_HOST
#include "../apps/product/src/product_services.c"

static unsigned takes,releases;
static bool unavailable;
rt_tick_t rt_tick_from_millisecond(unsigned ms) { assert(ms && ms<=100);return ms; }
int rt_mutex_take(struct rt_mutex *mutex,int timeout)
{
    assert(mutex==&database_lock && timeout>0 && timeout<=100);++takes;
    if (unavailable)return -1;
    ++mutex->held;return RT_EOK;
}
int rt_mutex_release(struct rt_mutex *mutex)
{
    assert(mutex==&database_lock && mutex->held);--mutex->held;++releases;return RT_EOK;
}
int main(void)
{
    for (unsigned enabled=0;enabled<2;++enabled) {
        assertions_enabled=enabled!=0;takes=releases=0;unavailable=false;service_ready=false;
        assert(!wf_product_storage_gate_acquire(100) && !takes);
        service_ready=true;
        assert(!wf_product_storage_gate_acquire(0) && !wf_product_storage_gate_acquire(101) && !takes);
        unavailable=true;assert(!wf_product_storage_gate_acquire(100) && takes==1 && !database_lock.held);
        unavailable=false;assert(wf_product_storage_gate_acquire(100) && database_lock.held==1);
        wf_product_storage_gate_release();assert(releases==1 && !database_lock.held);
    }
    return 0;
}
