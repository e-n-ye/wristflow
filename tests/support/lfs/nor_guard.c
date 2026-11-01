#include "nor_guard.h"
#include <string.h>

static int fail(wf_lfs_nor_t *n, unsigned reason)
{
    if (n && !n->fault) n->fault=reason;
    return LFS_ERR_IO;
}
static bool address(const struct lfs_config *c, lfs_block_t block, lfs_off_t off,
                    lfs_size_t size, unsigned alignment, uint32_t *out)
{
    /* Check before multiplication/addition or any low-level call. */
    if (!c || c->block_size!=WF_LFS_BLOCK || c->block_count!=WF_LFS_BLOCKS ||
        c->read_size!=32 || c->prog_size!=256 || block>=WF_LFS_BLOCKS ||
        off>WF_LFS_BLOCK || !size || size>WF_LFS_BLOCK-off ||
        off%alignment || size%alignment) return false;
    uint64_t a=(uint64_t)WF_LFS_START+(uint64_t)block*WF_LFS_BLOCK+off;
    if (a>=WF_LFS_END || size>WF_LFS_END-a) return false;
    *out=(uint32_t)a;
    return true;
}
static wf_lfs_nor_t *owner(const struct lfs_config *c)
{
    return c ? c->context : NULL;
}
static bool verify(wf_lfs_nor_t *n, uint32_t addr, const uint8_t *expected, size_t size)
{
    uint8_t buffer[256];
    while (size) {
        size_t count=size<sizeof buffer ? size:sizeof buffer;
        if (n->read(n->context,addr,buffer,count)!=(int)count) {
            fail(n,WF_LFS_READ);return false;
        }
        if (expected) {
            if (memcmp(buffer,expected,count)) return false;
            expected+=count;
        } else {
            for (size_t i=0;i<count;++i) if (buffer[i]!=255) return false;
        }
        addr+=(uint32_t)count;size-=count;
    }
    return true;
}
int wf_lfs_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off,
                void *buffer, lfs_size_t size)
{
    wf_lfs_nor_t *n=owner(c);uint32_t addr;
    if (!n || !n->read || !buffer || !address(c,block,off,size,32,&addr)) return fail(n,WF_LFS_RANGE);
    /* Reads remain possible after a failure for diagnosis. */
    return n->read(n->context,addr,buffer,size)==(int)size ? 0:fail(n,WF_LFS_READ);
}
int wf_lfs_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off,
                const void *buffer, lfs_size_t size)
{
    wf_lfs_nor_t *n=owner(c);uint32_t addr;
    if (!n || n->fault) return LFS_ERR_IO;
    if (!n->read || !n->prog || !buffer || !address(c,block,off,size,256,&addr)) return fail(n,WF_LFS_RANGE);
    if (n->prog(n->context,addr,buffer,size)!=(int)size) return fail(n,WF_LFS_PROG);
    return verify(n,addr,buffer,size) ? 0:fail(n,WF_LFS_VERIFY);
}
int wf_lfs_erase(const struct lfs_config *c, lfs_block_t block)
{
    wf_lfs_nor_t *n=owner(c);uint32_t addr;
    if (!n || n->fault) return LFS_ERR_IO;
    if (!n->read || !n->erase || !address(c,block,0,WF_LFS_BLOCK,WF_LFS_BLOCK,&addr)) return fail(n,WF_LFS_RANGE);
    if (n->erase(n->context,addr,WF_LFS_BLOCK)!=0) return fail(n,WF_LFS_ERASE);
    return verify(n,addr,NULL,WF_LFS_BLOCK) ? 0:fail(n,WF_LFS_VERIFY);
}
int wf_lfs_sync(const struct lfs_config *c)
{
    wf_lfs_nor_t *n=owner(c);
    if (!n || n->fault) return LFS_ERR_IO;
    if (!n->sync || n->sync(n->context)!=0) return fail(n,WF_LFS_SYNC);
    return 0;
}
