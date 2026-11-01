/* Original pinned LittleFS with a host-only synchronous RAM NOR model.
 * Fault injection is not a device driver, timeout or physical power-loss test. */
#include "nor_guard.h"
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MEDIA_BYTES (WF_LFS_BLOCK*WF_LFS_BLOCKS)
#define CANARY 32u
#define OLD_BYTES 8192u
#define APPEND_BYTES 8192u
enum { READ_OP, PROG_OP, ERASE_OP, SYNC_OP, OP_COUNT };
enum { NONE, NEGATIVE, SHORT_RESULT, LYING, BAD_READ, ZERO_RESULT };
static uint8_t media[MEDIA_BYTES+2*CANARY], seed[MEDIA_BYTES], payload[OLD_BYTES+APPEND_BYTES];
static uint8_t read_cache[256], prog_cache[256], file_cache[256];
static uint32_t lookahead[4];
static lfs_t fs;
static lfs_file_t file;
static struct lfs_file_config file_config;
static struct lfs_config config;
static wf_lfs_nor_t nor;
static struct {
    unsigned calls[OP_COUNT], mutations, fault_op, fault_at, fault_mode;
    unsigned cut_at, cut_phase, locks, unlocks;
    bool hit, locked;
} model;
static jmp_buf power_cut;

static void canaries(void)
{
    for (unsigned i=0;i<CANARY;++i) assert(media[i]==0xa5 && media[CANARY+MEDIA_BYTES+i]==0xa5);
}
static uint8_t *region(uint32_t addr, size_t size)
{
    assert(addr>=WF_LFS_START && addr<WF_LFS_END && size<=WF_LFS_END-addr);
    return media+CANARY+(addr-WF_LFS_START);
}
static unsigned enter(unsigned op)
{
    ++model.calls[op];
    if (op!=READ_OP) {
        ++model.mutations;
        if (model.cut_at==model.mutations && model.cut_phase==0) longjmp(power_cut,1);
    }
    if (model.fault_mode && model.fault_op==op && model.fault_at==model.calls[op]) {
        model.hit=true;return model.fault_mode;
    }
    return NONE;
}
static void cut(unsigned phase)
{
    if (model.cut_at==model.mutations && model.cut_phase==phase) longjmp(power_cut,1);
}
static int raw_read(void *context, uint32_t addr, void *buffer, size_t size)
{
    (void)context;unsigned fault=enter(READ_OP);
    uint8_t *p=region(addr,size);
    if (fault==NEGATIVE) return -1;
    if (fault==ZERO_RESULT) return 0;
    memcpy(buffer,p,size);
    if (fault==BAD_READ) ((uint8_t *)buffer)[size-1]^=1;
    return fault==SHORT_RESULT ? (int)size-1:(int)size;
}
static int raw_prog(void *context, uint32_t addr, const void *buffer, size_t size)
{
    (void)context;unsigned fault=enter(PROG_OP);
    uint8_t *p=region(addr,size);const uint8_t *in=buffer;
    assert(!(addr%256) && !(size%256));
    if (fault==NEGATIVE) return -1;
    if (fault==ZERO_RESULT) return 0;
    for (size_t i=0;i<size;++i) if ((p[i]&in[i])!=in[i]) return -1; /* NOR cannot set bits. */
    size_t count=fault==SHORT_RESULT ? size/2:size;
    for (size_t i=0;i<count;++i) {
        p[i]&=in[i];
        if (i+1==size/2) cut(1);
    }
    if (fault==LYING) p[size-1]^=1; /* Reports full success with wrong last byte. */
    cut(2);
    return fault==SHORT_RESULT ? (int)size-1:(int)size;
}
static int raw_erase(void *context, uint32_t addr, size_t size)
{
    (void)context;unsigned fault=enter(ERASE_OP);
    uint8_t *p=region(addr,size);
    assert(!(addr%WF_LFS_BLOCK) && size==WF_LFS_BLOCK);
    if (fault==NEGATIVE) return -1;
    memset(p,255,size/2);cut(1);
    if (fault!=SHORT_RESULT) memset(p+size/2,255,size-size/2);
    if (fault==LYING) p[size-1]=0; /* Error only at the final readback chunk. */
    cut(2);
    return fault==SHORT_RESULT ? -1:0;
}
static int raw_sync(void *context)
{
    (void)context;unsigned fault=enter(SYNC_OP);
    cut(1);cut(2);
    return fault ? -1:0; /* Model writes are immediately durable. */
}
static int lock(const struct lfs_config *c)
{
    assert(c==&config && !model.locked);model.locked=true;++model.locks;return 0;
}
static int unlock(const struct lfs_config *c)
{
    assert(c==&config && model.locked);model.locked=false;++model.unlocks;return 0;
}
static void boot(void)
{
    memset(&model,0,sizeof model);memset(&fs,0,sizeof fs);memset(&file,0,sizeof file);
    memset(read_cache,0,sizeof read_cache);memset(prog_cache,0,sizeof prog_cache);
    memset(file_cache,0,sizeof file_cache);memset(lookahead,0,sizeof lookahead);
    nor=(wf_lfs_nor_t){NULL,raw_read,raw_prog,raw_erase,raw_sync,0};
    config=(struct lfs_config){.context=&nor,.read=wf_lfs_read,.prog=wf_lfs_prog,
        .erase=wf_lfs_erase,.sync=wf_lfs_sync,.lock=lock,.unlock=unlock,
        .read_size=32,.prog_size=256,.block_size=WF_LFS_BLOCK,.block_count=WF_LFS_BLOCKS,
        .block_cycles=100,.cache_size=256,.lookahead_size=sizeof lookahead,
        .read_buffer=read_cache,.prog_buffer=prog_cache,.lookahead_buffer=lookahead};
    file_config=(struct lfs_file_config){.buffer=file_cache};
}
static void blank(void)
{
    memset(media,0xa5,sizeof media);memset(media+CANARY,255,MEDIA_BYTES);boot();
    for (unsigned i=0;i<sizeof payload;++i) payload[i]=(uint8_t)(i*29u+i/251u);
}
static void calls_reset(void)
{
    assert(!model.locked && model.locks==model.unlocks);
    memset(&model,0,sizeof model);
}
static void open_append(void)
{
    assert(lfs_mount(&fs,&config)==0);
    assert(lfs_file_opencfg(&fs,&file,"cycle.bin",LFS_O_WRONLY|LFS_O_APPEND,&file_config)==0);
    calls_reset();
}
static int append(void)
{
    lfs_ssize_t n=lfs_file_write(&fs,&file,payload+OLD_BYTES,APPEND_BYTES);
    return n==APPEND_BYTES ? lfs_file_sync(&fs,&file):(n<0 ? (int)n:LFS_ERR_IO);
}
static void make_seed(void)
{
    blank();assert(lfs_format(&fs,&config)==0);assert(lfs_mount(&fs,&config)==0);
    assert(lfs_file_opencfg(&fs,&file,"cycle.bin",LFS_O_CREAT|LFS_O_WRONLY,&file_config)==0);
    assert(lfs_file_write(&fs,&file,payload,OLD_BYTES)==OLD_BYTES);
    assert(lfs_file_sync(&fs,&file)==0);assert(lfs_file_close(&fs,&file)==0);
    assert(lfs_unmount(&fs)==0);canaries();memcpy(seed,media+CANARY,MEDIA_BYTES);
}
static void restore_seed(void)
{
    memcpy(media+CANARY,seed,MEDIA_BYTES);boot();open_append();
}
static void recovered(bool require_append)
{
    /* Reconstruct all volatile owner/cache/file state, without clean close. */
    boot();canaries();assert(lfs_mount(&fs,&config)==0);
    assert(lfs_file_opencfg(&fs,&file,"cycle.bin",LFS_O_RDONLY,&file_config)==0);
    lfs_soff_t size=lfs_file_size(&fs,&file);
    assert(size==OLD_BYTES || size==OLD_BYTES+APPEND_BYTES);
    if (require_append) assert(size==OLD_BYTES+APPEND_BYTES);
    uint8_t buffer[256];
    for (unsigned off=0;off<(unsigned)size;off+=sizeof buffer) {
        assert(lfs_file_read(&fs,&file,buffer,sizeof buffer)==sizeof buffer);
        assert(!memcmp(buffer,payload+off,sizeof buffer));
    }
    assert(lfs_file_read(&fs,&file,buffer,1)==0);
    assert(lfs_file_close(&fs,&file)==0);assert(lfs_unmount(&fs)==0);
    assert(model.locks==model.unlocks && !model.locked);canaries();
}
static void frozen(void)
{
    /* Returned API failures must unlock before volatile state is discarded.
     * Abrupt cuts deliberately do not call this helper. */
    assert(!model.locked && model.locks==model.unlocks);
    unsigned p=model.calls[PROG_OP],e=model.calls[ERASE_OP],s=model.calls[SYNC_OP],reason=nor.fault;
    assert(reason);
    assert(wf_lfs_prog(&config,0,0,payload,256)==LFS_ERR_IO);
    assert(wf_lfs_erase(&config,0)==LFS_ERR_IO && wf_lfs_sync(&config)==LFS_ERR_IO);
    assert(model.calls[PROG_OP]==p && model.calls[ERASE_OP]==e && model.calls[SYNC_OP]==s && nor.fault==reason);
    canaries();
}
static void bounds(void)
{
    uint8_t b[WF_LFS_BLOCK];memset(b,0,sizeof b);
    for (unsigned test=0;test<12;++test) {
        blank();lfs_block_t block=0;lfs_off_t off=0;lfs_size_t size=256;
        const void *in=b;void *out=b;
        switch (test) {
        case 0:block=WF_LFS_BLOCKS;break;case 1:block=UINT32_MAX;break;
        case 2:off=UINT32_MAX;break;case 3:off=WF_LFS_BLOCK;break;
        case 4:off=WF_LFS_BLOCK-128;break;case 5:size=UINT32_MAX;break;
        case 6:size=0;break;case 7:off=1;break;case 8:size=255;break;
        case 9:config.block_count++;break;case 10:in=NULL;out=NULL;break;
        case 11:config.block_size=UINT32_MAX;break;
        }
        assert(wf_lfs_prog(&config,block,off,in,size)==LFS_ERR_IO);
        assert(wf_lfs_read(&config,block,off,out,size)==LFS_ERR_IO);
        assert(!model.calls[PROG_OP] && !model.calls[READ_OP]);frozen();
    }
    blank();assert(wf_lfs_erase(&config,UINT32_MAX)==LFS_ERR_IO && !model.calls[ERASE_OP]);
    blank();assert(!wf_lfs_prog(&config,WF_LFS_BLOCKS-1,WF_LFS_BLOCK-256,b,256));
    assert(!wf_lfs_read(&config,WF_LFS_BLOCKS-1,WF_LFS_BLOCK-256,b,256));
    memset(b,255,256);assert(wf_lfs_prog(&config,WF_LFS_BLOCKS-1,WF_LFS_BLOCK-256,b,256)==LFS_ERR_IO);
    frozen();boot();assert(!wf_lfs_erase(&config,WF_LFS_BLOCKS-1));canaries();
    /* Returned lengths/errors and final-byte readback failures latch errors. */
    for (unsigned op=READ_OP;op<=ERASE_OP;++op) {
        for (unsigned mode=NEGATIVE;mode<=LYING;++mode) {
            if (op==READ_OP && mode==LYING) continue;
            blank();memset(region(WF_LFS_START,WF_LFS_BLOCK),0,WF_LFS_BLOCK);
            if (op==PROG_OP) memset(region(WF_LFS_START,WF_LFS_BLOCK),255,WF_LFS_BLOCK);
            model.fault_op=op;model.fault_at=1;model.fault_mode=mode;
            int result=op==READ_OP ? wf_lfs_read(&config,0,0,b,256):
                op==PROG_OP ? wf_lfs_prog(&config,0,0,payload,WF_LFS_BLOCK):wf_lfs_erase(&config,0);
            assert(result==LFS_ERR_IO && model.hit);frozen();
        }
    }
    for (unsigned op=PROG_OP;op<=ERASE_OP;++op) {
        blank();if (op==ERASE_OP) memset(region(WF_LFS_START,WF_LFS_BLOCK),0,WF_LFS_BLOCK);
        model.fault_op=READ_OP;model.fault_at=16;model.fault_mode=BAD_READ;
        int result=op==PROG_OP ? wf_lfs_prog(&config,0,0,payload,WF_LFS_BLOCK):wf_lfs_erase(&config,0);
        assert(result==LFS_ERR_IO && model.hit && nor.fault==WF_LFS_VERIFY);frozen();
    }
    for (unsigned op=READ_OP;op<=PROG_OP;++op) {
        blank();model.fault_op=op;model.fault_at=1;model.fault_mode=ZERO_RESULT;
        int result=op==READ_OP ? wf_lfs_read(&config,0,0,b,256):wf_lfs_prog(&config,0,0,payload,256);
        assert(result==LFS_ERR_IO && model.hit);frozen();
    }
    printf("bounds: 12 invalid requests, exact end, NOR 1-to-0, return/readback faults passed\n");
}
static void lifecycle(void)
{
    for (unsigned unknown=0;unknown<2;++unknown) {
        blank();if (unknown) memset(media+CANARY,0x5a,MEDIA_BYTES);
        memcpy(seed,media+CANARY,MEDIA_BYTES);
        assert(lfs_mount(&fs,&config)<0);
        assert(!model.calls[PROG_OP] && !model.calls[ERASE_OP] && !memcmp(seed,media+CANARY,MEDIA_BYTES));
        canaries();
    }
    /* Formatting is explicit, only on this host RAM image. */
    make_seed();restore_seed();assert(append()==0);assert(lfs_file_close(&fs,&file)==0);
    assert(lfs_unmount(&fs)==0);recovered(true);
    printf("lifecycle: blank/unknown mount leaves image unchanged; explicit RAM format/append/sync/remount passed\n");
}
static void faults(void)
{
    make_seed();restore_seed();assert(append()==0);
    unsigned baseline[OP_COUNT];memcpy(baseline,model.calls,sizeof baseline);
    unsigned cases=0;
    for (unsigned op=READ_OP;op<OP_COUNT;++op) {
        unsigned modes=op==READ_OP ? SHORT_RESULT:op==SYNC_OP ? NEGATIVE:LYING;
        for (unsigned mode=NEGATIVE;mode<=modes;++mode) {
            for (unsigned at=1;at<=baseline[op];++at) {
                restore_seed();model.fault_op=op;model.fault_at=at;model.fault_mode=mode;
                int result=append();assert(model.hit && nor.fault && result<0);frozen();
                recovered(false);++cases;
            }
        }
    }
    printf("faults: %u injected operation failures; committed prefix intact after volatile reset; baseline read/prog/erase/sync=%u/%u/%u/%u\n",
        cases,baseline[0],baseline[1],baseline[2],baseline[3]);
}
static void cuts(void)
{
    make_seed();restore_seed();assert(append()==0);unsigned count=model.mutations;
    unsigned cases=0;
    /* setjmp locals changed after longjmp are avoided: loop control is global. */
    static unsigned at,phase;
    for (at=1;at<=count;++at) for (phase=0;phase<3;++phase) {
        restore_seed();model.cut_at=at;model.cut_phase=phase;
        if (!setjmp(power_cut)) { (void)append();assert(!"cut was not reached"); }
        recovered(false);++cases;
    }
    printf("cuts: %u before/half/after program/erase/sync interruptions; committed prefix and atomic append intact\n",cases);
}
static void capacity(void)
{
    blank();assert(lfs_format(&fs,&config)==0);assert(lfs_mount(&fs,&config)==0);
    assert(lfs_file_opencfg(&fs,&file,"full.bin",LFS_O_CREAT|LFS_O_WRONLY,&file_config)==0);
    /* Previous 48h/5s/64B estimate is payload, not a production record schema. */
    const unsigned budget=(48u*60u*60u/5u)*64u;
    unsigned written=0,committed=0;bool full=false;
    while (written<=MEDIA_BYTES) {
        lfs_ssize_t result=lfs_file_write(&fs,&file,payload,4096);
        if (result<0) { assert(result==LFS_ERR_NOSPC);full=true;break; }
        assert(result==4096);written+=4096;
        int sync=lfs_file_sync(&fs,&file);
        if (sync<0) { assert(sync==LFS_ERR_NOSPC);full=true;break; }
        committed=written;
        if (committed>=budget && committed-4096<budget) printf("capacity: 48h payload budget %u bytes committed\n",budget);
    }
    assert(full && committed>=budget && committed<MEDIA_BYTES && !nor.fault);canaries();
    boot();assert(lfs_mount(&fs,&config)==0);
    assert(lfs_file_opencfg(&fs,&file,"full.bin",LFS_O_RDONLY,&file_config)==0);
    assert(lfs_file_size(&fs,&file)==(lfs_soff_t)committed);
    uint8_t buffer[256];
    for (unsigned off=0;off<committed;off+=sizeof buffer) {
        assert(lfs_file_read(&fs,&file,buffer,sizeof buffer)==sizeof buffer);
        assert(!memcmp(buffer,payload+(off%4096),sizeof buffer));
    }
    assert(lfs_file_close(&fs,&file)==0 && lfs_unmount(&fs)==0);canaries();
    printf("capacity: NOSPC bounded, %u committed bytes preserved; RAM-only layout, not field capacity promise\n",committed);
}
int main(int argc, char **argv)
{
    assert(argc==2);
    if (!strcmp(argv[1],"bounds")) bounds();
    else if (!strcmp(argv[1],"lifecycle")) lifecycle();
    else if (!strcmp(argv[1],"faults")) faults();
    else if (!strcmp(argv[1],"cuts")) cuts();
    else if (!strcmp(argv[1],"capacity")) capacity();
    else return 2;
    return 0;
}
