#include "fs_block.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

static uint8_t media[4096],data[4096];
static unsigned reads,progs,erases,syncs,enters,leaves,ticks,delay;
static bool held,busy,stop,stop_in_call,short_read,lie;
static wf_fs_block_t device;
static bool take(void *ctx) { (void)ctx;++enters;if (busy || held) return false;held=true;return true; }
static void give(void *ctx) { (void)ctx;assert(held);held=false;++leaves; }
static bool stopped(void *ctx) { (void)ctx;return stop; }
static uint32_t now(void *ctx) { (void)ctx;return ticks; }
static void during_call(void)
{
    assert(held);unsigned released_before=leaves;ticks+=delay;
    if (stop_in_call) {
        stop=true;
        /* The outer operation still owns its gate; nested work cannot release it. */
        assert(wf_fs_block_read(&device,0,0,data,32)==WF_FS_BLOCK_BUSY);
        assert(held && leaves==released_before);
    }
}
static int read_bytes(void *ctx,uint32_t addr,void *out,size_t n)
{
    (void)ctx;assert(held && addr>=WF_FS_BLOCK_START && n<=sizeof media-(addr-WF_FS_BLOCK_START));
    ++reads;during_call();memcpy(out,media+(addr-WF_FS_BLOCK_START),n);
    return short_read ? (int)n-1:(int)n;
}
static int program(void *ctx,uint32_t addr,const void *in,size_t n)
{
    (void)ctx;assert(held && n==256 && addr>=WF_FS_BLOCK_START && n<=sizeof media-(addr-WF_FS_BLOCK_START));
    ++progs;during_call();memcpy(media+(addr-WF_FS_BLOCK_START),in,n);
    if (lie) media[addr-WF_FS_BLOCK_START+n-1]^=1;
    return (int)n;
}
static int erase(void *ctx,uint32_t addr,size_t n)
{
    (void)ctx;assert(held && addr==WF_FS_BLOCK_START && n==4096);
    ++erases;during_call();memset(media,255,sizeof media);if (lie)media[4095]=0;return 0;
}
static int sync_bytes(void *ctx) { (void)ctx;++syncs;during_call();return 0; }
static void reset(void)
{
    reads=progs=erases=syncs=enters=leaves=ticks=delay=0;
    held=busy=stop=stop_in_call=short_read=lie=false;
    memset(media,255,sizeof media);memset(data,42,sizeof data);
    device=(wf_fs_block_t){.read=read_bytes,.prog=program,.erase=erase,.sync=sync_bytes,
        .acquire=take,.release=give,.stopped=stopped,.now=now,.budget_ms=100,.writable=true};
}
static void balanced(void) { assert(!held && enters==leaves); }
int main(void)
{
    reset();assert(!wf_fs_block_prog(&device,0,0,data,sizeof data));
    assert(progs==16 && reads==16 && !memcmp(data,media,sizeof data));balanced();
    reset();assert(!wf_fs_block_erase(&device,0));assert(erases==1 && reads==16);balanced();
    reset();assert(!wf_fs_block_read(&device,0,0,data,sizeof data));assert(reads==16);balanced();
    reset();assert(!wf_fs_block_sync(&device) && syncs==1);balanced();
    reset();device.writable=false;
    assert(wf_fs_block_prog(&device,0,0,data,256)==WF_FS_BLOCK_READ_ONLY);
    assert(wf_fs_block_erase(&device,0)==WF_FS_BLOCK_READ_ONLY && wf_fs_block_sync(&device)==WF_FS_BLOCK_READ_ONLY);
    assert(!progs && !erases && !syncs && !device.fault);balanced();
    reset();busy=true;assert(wf_fs_block_read(&device,0,0,data,32)==WF_FS_BLOCK_BUSY);
    assert(!reads && !held && !leaves && !device.fault);
    reset();stop=true;assert(wf_fs_block_erase(&device,0)==WF_FS_BLOCK_STOPPED && !erases && !device.fault);balanced();
    reset();stop_in_call=true;
    assert(wf_fs_block_read(&device,0,0,data,sizeof data)==WF_FS_BLOCK_STOPPED && reads==1);
    assert(!device.fault && enters==2 && leaves==1 && !held);
    reset();delay=100;assert(wf_fs_block_read(&device,0,0,data,sizeof data)==WF_FS_BLOCK_BUDGET && reads==1);balanced();
    reset();delay=100;assert(wf_fs_block_prog(&device,0,0,data,sizeof data)==WF_FS_BLOCK_BUDGET);
    assert(progs==1 && !reads && device.fault==WF_FS_BLOCK_INTERRUPTED);balanced();
    reset();stop_in_call=true;assert(wf_fs_block_prog(&device,0,0,data,sizeof data)==WF_FS_BLOCK_STOPPED);
    assert(progs==1 && !reads && device.fault==WF_FS_BLOCK_INTERRUPTED && !held);
    reset();lie=true;assert(wf_fs_block_erase(&device,0)==WF_FS_BLOCK_IO);
    assert(device.fault==WF_FS_BLOCK_VERIFY && reads==16);balanced();
    unsigned old=progs;assert(wf_fs_block_prog(&device,0,0,data,256)==WF_FS_BLOCK_IO && progs==old);balanced();
    /* Fault freezes only this owner. Reads and unrelated work remain possible. */
    lie=false;assert(!wf_fs_block_read(&device,0,0,data,256));balanced();
    reset();short_read=true;assert(wf_fs_block_read(&device,0,0,data,256)==WF_FS_BLOCK_IO);
    assert(device.fault==WF_FS_BLOCK_READ);balanced();
    reset();ticks=UINT32_MAX-50;delay=60;assert(wf_fs_block_read(&device,0,0,data,256)==0);balanced();
    reset();device.acquire=NULL;assert(wf_fs_block_read(&device,0,0,data,256)==WF_FS_BLOCK_NOT_READY && !reads);
    puts("fs_block_guard: reusable page/verify guard, gate, budget, cooperative stop and error freeze passed");
    return 0;
}
