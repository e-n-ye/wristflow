#include "../apps/product/src/product_storage_probe.c"
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
static unsigned copy_result,printed,done;
rt_base_t rt_hw_interrupt_disable(void) { return 0; }
void rt_hw_interrupt_enable(rt_base_t value) { assert(!value); }
uint32_t SystemPowerOnModeGet(void) { return 4; }
uint32_t pm_get_pwron_wakeup_src(void) { return 0x20; }
unsigned wristflow_product_services_storage_snapshot(uint8_t *out,size_t n,wf_storage_copy_stats_t *s)
{
    assert(busy && n==16384);
    memset(out,255,n);
    for(unsigned i=0;i<4;++i) {
        uint8_t *p=out+i*4096;p[0]=0x3f;p[1]=0x7f;
        p[4]=0x46;p[5]=0x44;p[6]=0x42;p[7]=0x30;
    }
    *s=(wf_storage_copy_stats_t){16384,128,25};return copy_result;
}
void rt_kprintf(const char *format,...)
{
    char line[128];va_list args;va_start(args,format);
    int n=vsnprintf(line,sizeof line,format,args);va_end(args);
    assert(n>0 && n<(int)sizeof line);++printed;
    assert(!strstr(line,"preferences") && !strstr(line,"secret"));
    if(strstr(line,"[storage-done]")) ++done;
}
int main(void)
{
    char *args[]={"wf_storage","scan"};
    assert(wf_storage(2,args)==0 && printed==9 && done==1 && !busy);
    for(unsigned i=0;i<sizeof image;++i) assert(!image[i]);
    copy_result=WF_STORAGE_COPY_UNAVAILABLE;assert(wf_storage(2,args)==-1 && !busy && done==2);
    busy=true;assert(wf_storage(2,args)==-1 && busy && done==3);busy=false;
    args[1]="erase";assert(wf_storage(2,args)==-1 && !busy);
    assert(wf_storage(1,args)==-1 && !busy);
    return 0;
}
