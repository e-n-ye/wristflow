#include "fs_block.h"
#include <string.h>

static int fail(wf_fs_block_t *n, unsigned reason)
{
    if (!n->fault) n->fault=reason;
    return WF_FS_BLOCK_IO;
}
static bool address(uint32_t block, uint32_t off, size_t size, unsigned align, uint32_t *out)
{
    if (block>=WF_FS_BLOCK_COUNT || off>WF_FS_BLOCK_SIZE || !size ||
        size>WF_FS_BLOCK_SIZE-off || off%align || size%align) return false;
    uint64_t a=(uint64_t)WF_FS_BLOCK_START+(uint64_t)block*WF_FS_BLOCK_SIZE+off;
    if (a>=WF_FS_BLOCK_END || size>WF_FS_BLOCK_END-a) return false;
    *out=(uint32_t)a;return true;
}
static int check(wf_fs_block_t *n, uint32_t started, bool mutating)
{
    int result=n->stopped(n->context) ? WF_FS_BLOCK_STOPPED:
        (uint32_t)(n->now(n->context)-started)>=n->budget_ms ? WF_FS_BLOCK_BUDGET:0;
    if (result && mutating) fail(n,WF_FS_BLOCK_INTERRUPTED);
    return result;
}
static int begin(wf_fs_block_t *n, bool writing, uint32_t *started)
{
    if (!n || !n->read || !n->acquire || !n->release || !n->stopped || !n->now || !n->budget_ms)
        return WF_FS_BLOCK_NOT_READY;
    if (!n->acquire(n->context)) return WF_FS_BLOCK_BUSY;
    *started=n->now(n->context);
    int result=check(n,*started,false);
    if (!result && writing && n->fault) result=WF_FS_BLOCK_IO;
    if (!result && writing && !n->writable) result=WF_FS_BLOCK_READ_ONLY;
    if (result) n->release(n->context);
    return result;
}
static int finish(wf_fs_block_t *n, int result)
{
    n->release(n->context);return result;
}
static int verify(wf_fs_block_t *n, uint32_t addr, const uint8_t *expected, size_t size, uint32_t started)
{
    uint8_t buffer[256];
    while (size) {
        int result=check(n,started,true);if (result) return result;
        size_t count=size<sizeof buffer ? size:sizeof buffer;
        if (n->read(n->context,addr,buffer,count)!=(int)count) return fail(n,WF_FS_BLOCK_READ);
        result=check(n,started,true);if (result) return result;
        if (expected) {
            if (memcmp(buffer,expected,count)) return fail(n,WF_FS_BLOCK_VERIFY);
            expected+=count;
        } else for (size_t i=0;i<count;++i) if (buffer[i]!=255) return fail(n,WF_FS_BLOCK_VERIFY);
        addr+=(uint32_t)count;size-=count;
    }
    return 0;
}
int wf_fs_block_read(wf_fs_block_t *n, uint32_t block, uint32_t off, void *buffer, size_t size)
{
    uint32_t started,addr;int result=begin(n,false,&started);if (result) return result;
    if (!buffer || !address(block,off,size,32,&addr)) return finish(n,fail(n,WF_FS_BLOCK_RANGE));
    /* Read diagnostics remain available after a fault. */
    uint8_t *out=buffer;
    while (size) {
        result=check(n,started,false);if (result) break;
        size_t count=size<256 ? size:256;
        if (n->read(n->context,addr,out,count)!=(int)count) { result=fail(n,WF_FS_BLOCK_READ);break; }
        result=check(n,started,false);if (result) break;
        addr+=(uint32_t)count;out+=count;size-=count;
    }
    return finish(n,result);
}
int wf_fs_block_prog(wf_fs_block_t *n, uint32_t block, uint32_t off, const void *buffer, size_t size)
{
    uint32_t started,addr;int result=begin(n,true,&started);if (result) return result;
    if (!n->prog || !buffer || !address(block,off,size,256,&addr)) return finish(n,fail(n,WF_FS_BLOCK_RANGE));
    const uint8_t *in=buffer;
    while (size) {
        result=check(n,started,false);if (result) { fail(n,WF_FS_BLOCK_INTERRUPTED);break; }
        if (n->prog(n->context,addr,in,256)!=256) { result=fail(n,WF_FS_BLOCK_PROG);break; }
        result=verify(n,addr,in,256,started);if (result) break;
        addr+=256;in+=256;size-=256;
    }
    return finish(n,result);
}
int wf_fs_block_erase(wf_fs_block_t *n, uint32_t block)
{
    uint32_t started,addr;int result=begin(n,true,&started);if (result) return result;
    if (!n->erase || !address(block,0,WF_FS_BLOCK_SIZE,WF_FS_BLOCK_SIZE,&addr)) return finish(n,fail(n,WF_FS_BLOCK_RANGE));
    if (n->erase(n->context,addr,WF_FS_BLOCK_SIZE)!=0) result=fail(n,WF_FS_BLOCK_ERASE);
    else result=verify(n,addr,NULL,WF_FS_BLOCK_SIZE,started);
    return finish(n,result);
}
int wf_fs_block_sync(wf_fs_block_t *n)
{
    uint32_t started;int result=begin(n,true,&started);if (result) return result;
    if (!n->sync || n->sync(n->context)!=0) result=fail(n,WF_FS_BLOCK_SYNC);
    else result=check(n,started,true);
    return finish(n,result);
}
int wf_fs_block_reject(wf_fs_block_t *n)
{
    uint32_t started;int result=begin(n,false,&started);if (result) return result;
    return finish(n,fail(n,WF_FS_BLOCK_RANGE));
}
