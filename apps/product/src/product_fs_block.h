#ifndef WF_PRODUCT_FS_BLOCK_H
#define WF_PRODUCT_FS_BLOCK_H
#include "fs_block.h"
typedef struct { bool ready,stopped,in_flight;unsigned fault; } wf_product_fs_block_state_t;
/* No auto-init, registration, mount, format or FinSH command. Prepare once in
 * thread context after services startup, before publishing to consumers.
 * This candidate stays read-only; no SDK write/erase callback is installed. */
int wf_product_fs_block_prepare(void);
int wf_product_fs_block_read(uint32_t block, uint32_t off, void *, size_t);
int wf_product_fs_block_prog(uint32_t block, uint32_t off, const void *, size_t);
int wf_product_fs_block_erase(uint32_t block);
int wf_product_fs_block_sync(void);
/* Monotonic stop request. In-flight SDK calls retain gate/PM until return.
 * No reset/rearm API; a new boot is required. */
void wf_product_fs_block_stop(void);
wf_product_fs_block_state_t wf_product_fs_block_state(void);
#ifdef WF_CYCLE_LOG
/* Dedicated CycleLog build only: explicit worker admission after prepare.
 * No mount/format on admission. Caller first proves a bounded operation on
 * the blank candidate sector; physical authorization is external to this API. */
bool wf_product_fs_block_admit(void);
wf_fs_block_t *wf_product_fs_block_device(void);
#endif
#endif
