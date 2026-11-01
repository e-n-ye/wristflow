#ifndef WF_HOST_LFS_NOR_GUARD_H
#define WF_HOST_LFS_NOR_GUARD_H
#include "lfs.h"
#include "fs_block.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Test-only LittleFS facade over the exact reusable Product block guard. */
#define WF_LFS_START WF_FS_BLOCK_START
#define WF_LFS_END WF_FS_BLOCK_END
#define WF_LFS_BLOCK WF_FS_BLOCK_SIZE
#define WF_LFS_BLOCKS WF_FS_BLOCK_COUNT
#define WF_LFS_VERIFY WF_FS_BLOCK_VERIFY
typedef wf_fs_block_t wf_lfs_nor_t;

int wf_lfs_read(const struct lfs_config *, lfs_block_t, lfs_off_t, void *, lfs_size_t);
int wf_lfs_prog(const struct lfs_config *, lfs_block_t, lfs_off_t, const void *, lfs_size_t);
int wf_lfs_erase(const struct lfs_config *, lfs_block_t);
int wf_lfs_sync(const struct lfs_config *);
#endif
