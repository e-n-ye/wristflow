#include "../apps/product/src/product_fs_probe.c"
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
static const FLASH_RDID_TYPE_T ids[]={{0xef,0x40,0x18,0,0x1000000},{0}};
static const FLASH_RDID_TYPE_T empty[]={{0}};
const FLASH_RDID_TYPE_T *const flash_cmd_id_pool[]={ids,empty};
const SPI_FLASH_FACT_CFG_T flash_cmd_table_list[2]={{0},{0}};
static FLASH_HandleTypeDef handle={(void *)1,&flash_cmd_table_list[0],0x12000000,0x1000000,0,1};
static unsigned reads,short_at,ticks,printed;
static bool missing,last_missing,unknown,total_bad,change,emit;
rt_base_t rt_hw_interrupt_disable(void) { return 0; }
void rt_hw_interrupt_enable(rt_base_t x) { assert(!x); }
rt_tick_t rt_tick_get(void) { return ticks; }
int rt_thread_mdelay(rt_int32_t x) { assert(x==1);++ticks;return 0; }
void *rt_flash_get_handle_by_addr(uint32_t addr)
{
    if(missing || (last_missing && addr==0x12d9ffff)) return NULL;
    return &handle;
}
int rt_flash_get_total_size(uint32_t addr) { assert(addr==0x12000000);return total_bad ? 0 : (int)handle.size; }
int rt_flash_read_id(uint32_t addr) { assert(addr==0x12000000);return unknown ? 0x1840aa : 0x1840ef; }
int rt_flash_get_erase_alignment(uint32_t addr) { assert(addr==0x12000000);return 4096<<handle.dualFlash; }
int rt_flash_read(uint32_t addr,uint8_t *buffer,int n)
{
    assert(fs_busy && addr>=0x129a0000 && addr<0x12da0000 && n==1024 && (unsigned)n<=0x12da0000-addr);
    ++reads;
    if(short_at==reads) return n-1;
    memset(buffer,255,(size_t)n);
    if(change && reads==4097) buffer[0]=0;
    return n;
}
void rt_kprintf(const char *format,...)
{
    char buffer[512];va_list args;va_start(args,format);
    int n=vsnprintf(buffer,sizeof buffer,format,args);va_end(args);
    assert(n>0 && n<(int)sizeof buffer);++printed;
    if(emit) fputs(buffer,stdout);
}
int main(int argc,char **argv)
{
    char *args[]={"wf_fs","scan"};emit=argc==2 && !strcmp(argv[1],"--emit");
    assert(!wf_fs(2,args) && reads==8192 && printed==9 && !fs_busy);
    if(emit) return 0;
    unsigned before=reads;fs_busy=true;assert(wf_fs(2,args)==-1 && reads==before);fs_busy=false;
    args[1]="erase";assert(wf_fs(2,args)==-1 && reads==before);args[1]="scan";
    missing=true;assert(wf_fs(2,args)==-1 && reads==before);missing=false;
    last_missing=true;assert(wf_fs(2,args)==-1 && reads==before);last_missing=false;
    unknown=true;assert(wf_fs(2,args)==-1 && reads==before);unknown=false;
    total_bad=true;assert(wf_fs(2,args)==-1 && reads==before);total_bad=false;
    handle.ctable=&flash_cmd_table_list[1];assert(wf_fs(2,args)==-1 && reads==before);handle.ctable=&flash_cmd_table_list[0];
    handle.dualFlash=255;assert(wf_fs(2,args)==-1 && reads==before);handle.dualFlash=1;
    reads=0;short_at=1;assert(wf_fs(2,args)==-1 && reads==1 && !fs_busy);short_at=0;
    reads=0;change=true;assert(wf_fs(2,args)==-1 && reads==8192 && !fs_busy);
    return 0;
}
