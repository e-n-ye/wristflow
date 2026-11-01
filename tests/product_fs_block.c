/* Exact Product SDK adapter, with host-only service/PM/SDK substitutes. */
#include "../apps/product/src/product_fs_block.c"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const FLASH_RDID_TYPE_T ids[]={{0x0b,0x40,0x18,0,0x1000000},{0}};
static const FLASH_RDID_TYPE_T empty[]={{0}};
const FLASH_RDID_TYPE_T *const flash_cmd_id_pool[]={ids,empty};
const SPI_FLASH_FACT_CFG_T flash_cmd_table_list[2]={{0},{0}};
static FLASH_HandleTypeDef handle;
static unsigned pm,pm_enter,pm_leave,depth,gate_enter,gate_leave,reads,metadata,ticks,delay;
static bool busy,missing,wrong_id,wrong_end,short_read,stop_on_read,reenter;
static unsigned baseline_pm;
rt_base_t rt_hw_interrupt_disable(void) { return 0; }
void rt_hw_interrupt_enable(rt_base_t level) { assert(!level); }
rt_tick_t rt_tick_get(void) { return ticks; }
void rt_pm_request(unsigned mode) { assert(mode==PM_SLEEP_MODE_IDLE);++pm;++pm_enter; }
void rt_pm_release(unsigned mode) { assert(mode==PM_SLEEP_MODE_IDLE && pm>baseline_pm);--pm;++pm_leave; }
bool wf_product_storage_gate_acquire(uint32_t ms)
{
    assert(ms==100 && pm>baseline_pm);++gate_enter;if (busy) return false;++depth;return true;
}
void wf_product_storage_gate_release(void) { assert(depth && pm>baseline_pm);--depth;++gate_leave; }
static void owned(void) { assert(depth && pm>baseline_pm && in_flight); }
void *rt_flash_get_handle_by_addr(uint32_t addr)
{
    owned();++metadata;if (missing || (wrong_end && addr==WF_FS_BLOCK_END-1))return NULL;return &handle;
}
int rt_flash_get_total_size(uint32_t addr) { owned();++metadata;assert(addr==0x12000000);return 0x1000000; }
int rt_flash_get_erase_alignment(uint32_t addr) { owned();++metadata;assert(addr==0x12000000);return 4096; }
int rt_flash_read_id(uint32_t addr) { owned();++metadata;assert(addr==0x12000000);ticks+=delay;return wrong_id ? 0x1840ef:0x18400b; }
int rt_flash_read(uint32_t addr,uint8_t *out,int n)
{
    owned();++reads;assert(addr>=WF_FS_BLOCK_START && addr<WF_FS_BLOCK_END && n==256 && (unsigned)n<=WF_FS_BLOCK_END-addr);
    if (stop_on_read) {
        wf_product_fs_block_stop();wf_product_fs_block_state_t state=wf_product_fs_block_state();
        assert(state.in_flight && state.stopped && pm>baseline_pm && depth==1);
    }
    if (reenter) {
        uint8_t nested[256];
        assert(wf_product_fs_block_read(0,0,nested,sizeof nested)==WF_FS_BLOCK_BUSY);
        assert(in_flight && depth==1 && pm==baseline_pm+1);
    }
    ticks+=delay;memset(out,255,(size_t)n);return short_read ? n-1:n;
}
static void reset(void)
{
    block_device=(wf_fs_block_t){0};claimed=ready=stopped=in_flight=false;
    baseline_pm=2;pm=baseline_pm;pm_enter=pm_leave=depth=gate_enter=gate_leave=reads=metadata=ticks=delay=0;
    busy=missing=wrong_id=wrong_end=short_read=stop_on_read=reenter=false;
    handle=(FLASH_HandleTypeDef){(void *)1,&flash_cmd_table_list[0],0x12000000,0x1000000,0,0};
}
static void balanced(void) { assert(pm==baseline_pm && pm_enter==pm_leave && !depth && !in_flight); }
int main(void)
{
    uint8_t data[4096];reset();
    assert(wf_product_fs_block_read(0,0,data,256)==WF_FS_BLOCK_NOT_READY && !pm_enter && !reads);
    assert(!wf_product_fs_block_prepare());balanced();assert(wf_product_fs_block_state().ready);
    unsigned m=metadata;assert(wf_product_fs_block_prepare()==WF_FS_BLOCK_NOT_READY && metadata==m);balanced();
    assert(!wf_product_fs_block_read(0,0,data,sizeof data) && reads==16);balanced();
    assert(wf_product_fs_block_prog(0,0,data,256)==WF_FS_BLOCK_READ_ONLY);
    assert(wf_product_fs_block_erase(0)==WF_FS_BLOCK_READ_ONLY && wf_product_fs_block_sync()==WF_FS_BLOCK_READ_ONLY);
    assert(!block_device.prog && !block_device.erase && !block_device.sync);balanced();
    for (unsigned bad=0;bad<7;++bad) {
        reset();switch (bad) {
        case 0:missing=true;break;case 1:wrong_id=true;break;case 2:wrong_end=true;break;
        case 3:handle.dualFlash=1;break;case 4:handle.isNand=1;break;
        case 5:handle.ctable=&flash_cmd_table_list[1];break;case 6:handle.size=0x800000;break;
        }
        assert(wf_product_fs_block_prepare()==WF_FS_BLOCK_NOT_READY && !reads);balanced();
        assert(!wf_product_fs_block_state().ready);
    }
    reset();busy=true;assert(wf_product_fs_block_prepare()==WF_FS_BLOCK_BUSY && !metadata);balanced();
    reset();wf_product_fs_block_stop();assert(wf_product_fs_block_prepare()==WF_FS_BLOCK_STOPPED && !metadata);balanced();
    reset();delay=100;assert(wf_product_fs_block_prepare()==WF_FS_BLOCK_BUDGET);balanced();assert(!ready);
    reset();assert(!wf_product_fs_block_prepare());busy=true;
    assert(wf_product_fs_block_read(0,0,data,256)==WF_FS_BLOCK_BUSY && !reads);balanced();
    reset();assert(!wf_product_fs_block_prepare());wf_product_fs_block_stop();
    assert(wf_product_fs_block_read(0,0,data,256)==WF_FS_BLOCK_STOPPED && !reads);balanced();
    reset();assert(!wf_product_fs_block_prepare());stop_on_read=true;reenter=true;
    /* Nested call first sees STOPPED after acquiring and detects recursive entry
     * at the gate; it must preserve the outer owner and PM references. */
    assert(wf_product_fs_block_read(0,0,data,sizeof data)==WF_FS_BLOCK_STOPPED && reads==1);balanced();
    assert(wf_product_fs_block_state().stopped);
    reset();assert(!wf_product_fs_block_prepare());delay=100;
    assert(wf_product_fs_block_read(0,0,data,sizeof data)==WF_FS_BLOCK_BUDGET && reads==1);balanced();
    reset();assert(!wf_product_fs_block_prepare());short_read=true;
    assert(wf_product_fs_block_read(0,0,data,256)==WF_FS_BLOCK_IO && block_device.fault==WF_FS_BLOCK_READ);balanced();
    reset();assert(!wf_product_fs_block_prepare());
    assert(wf_product_fs_block_read(UINT32_MAX,0,data,256)==WF_FS_BLOCK_IO && !reads);balanced();
    assert(gate_enter==gate_leave);
    puts("product_fs_block: exact SDK adapter metadata, read-only default, bounded settings gate, PM references, in-flight stop and reentry passed");
    return 0;
}
