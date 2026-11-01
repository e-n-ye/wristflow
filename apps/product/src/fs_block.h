#ifndef WF_FS_BLOCK_H
#define WF_FS_BLOCK_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WF_FS_BLOCK_START UINT32_C(0x129a0000)
#define WF_FS_BLOCK_END UINT32_C(0x12da0000)
#define WF_FS_BLOCK_SIZE 4096u
#define WF_FS_BLOCK_COUNT 1024u
enum { WF_FS_BLOCK_OK=0, WF_FS_BLOCK_IO=-1, WF_FS_BLOCK_BUSY=-2,
       WF_FS_BLOCK_STOPPED=-3, WF_FS_BLOCK_BUDGET=-4, WF_FS_BLOCK_READ_ONLY=-5,
       WF_FS_BLOCK_NOT_READY=-6 };
enum { WF_FS_BLOCK_RANGE=1, WF_FS_BLOCK_READ, WF_FS_BLOCK_PROG,
       WF_FS_BLOCK_ERASE, WF_FS_BLOCK_VERIFY, WF_FS_BLOCK_SYNC,
       WF_FS_BLOCK_INTERRUPTED };
typedef struct {
    void *context;
    int (*read)(void *, uint32_t, void *, size_t);
    int (*prog)(void *, uint32_t, const void *, size_t);
    int (*erase)(void *, uint32_t, size_t); /* SDK convention: zero success. */
    int (*sync)(void *);
    bool (*acquire)(void *); /* Bounded owner gate + required PM hold. */
    void (*release)(void *);
    bool (*stopped)(void *); /* Safe asynchronous stop observation. */
    uint32_t (*now)(void *);
    uint32_t budget_ms;
    bool writable;
    unsigned fault;
} wf_fs_block_t;

/* Construct once before use; every state access belongs under acquire/release.
 * No reset/unfreeze API. Buffers stay alive until synchronous calls return.
 * Budget/stop checks cannot preempt a blocked driver; never release its PM/gate
 * or declare a timed-out operation complete while it is still executing. */
int wf_fs_block_read(wf_fs_block_t *, uint32_t block, uint32_t off, void *, size_t);
int wf_fs_block_prog(wf_fs_block_t *, uint32_t block, uint32_t off, const void *, size_t);
int wf_fs_block_erase(wf_fs_block_t *, uint32_t block);
int wf_fs_block_sync(wf_fs_block_t *);
int wf_fs_block_reject(wf_fs_block_t *); /* Invalid external geometry. */
#endif
