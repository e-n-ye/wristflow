#ifndef WF_HOST_LFS_NOR_GUARD_H
#define WF_HOST_LFS_NOR_GUARD_H
#include "lfs.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Host candidate only. No Product registration, driver or format entry. */
#define WF_LFS_START UINT32_C(0x129a0000)
#define WF_LFS_END UINT32_C(0x12da0000)
#define WF_LFS_BLOCK 4096u
#define WF_LFS_BLOCKS 1024u
enum { WF_LFS_RANGE=1, WF_LFS_READ, WF_LFS_PROG, WF_LFS_ERASE,
       WF_LFS_VERIFY, WF_LFS_SYNC };
typedef struct {
    void *context;
    int (*read)(void *, uint32_t, void *, size_t);
    int (*prog)(void *, uint32_t, const void *, size_t);
    int (*erase)(void *, uint32_t, size_t); /* SDK convention: zero success. */
    int (*sync)(void *);
    unsigned fault; /* First error latched until the owner is reconstructed. */
} wf_lfs_nor_t;

int wf_lfs_read(const struct lfs_config *, lfs_block_t, lfs_off_t, void *, lfs_size_t);
int wf_lfs_prog(const struct lfs_config *, lfs_block_t, lfs_off_t, const void *, lfs_size_t);
int wf_lfs_erase(const struct lfs_config *, lfs_block_t);
int wf_lfs_sync(const struct lfs_config *);
#endif
