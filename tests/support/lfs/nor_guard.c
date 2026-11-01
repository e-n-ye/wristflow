#include "nor_guard.h"
static wf_lfs_nor_t *owner(const struct lfs_config *c)
{
    return c ? c->context : NULL;
}
static bool valid(const struct lfs_config *c)
{
    return c && c->block_size==WF_LFS_BLOCK && c->block_count==WF_LFS_BLOCKS &&
        c->read_size==32 && c->prog_size==256;
}
int wf_lfs_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off,
                void *buffer, lfs_size_t size)
{
    int result=valid(c) ? wf_fs_block_read(owner(c),block,off,buffer,size):wf_fs_block_reject(owner(c));
    return result ? LFS_ERR_IO:0;
}
int wf_lfs_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off,
                const void *buffer, lfs_size_t size)
{
    int result=valid(c) ? wf_fs_block_prog(owner(c),block,off,buffer,size):wf_fs_block_reject(owner(c));
    return result ? LFS_ERR_IO:0;
}
int wf_lfs_erase(const struct lfs_config *c, lfs_block_t block)
{
    int result=valid(c) ? wf_fs_block_erase(owner(c),block):wf_fs_block_reject(owner(c));
    return result ? LFS_ERR_IO:0;
}
int wf_lfs_sync(const struct lfs_config *c)
{
    return wf_fs_block_sync(owner(c)) ? LFS_ERR_IO:0;
}
