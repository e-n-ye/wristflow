#ifndef WF_CYCLE_STORE_H
#define WF_CYCLE_STORE_H
#include "cycle_record.h"
#include "fs_block.h"
#include "lfs.h"
typedef struct {
    lfs_t fs;lfs_file_t file;struct lfs_config cfg;struct lfs_file_config fcfg;
    uint8_t read_cache[256],prog_cache[256],file_cache[256];uint32_t ahead[4];
    bool mounted,opened,smoke;uint32_t rows,session,crc;
} wf_cycle_store_t;
typedef struct {uint32_t session,rows,bytes,crc;bool terminal;} wf_cycle_info_t;
void wf_cycle_store_init(wf_cycle_store_t *,wf_fs_block_t *);
/* Explicit initialization only, full blank scan before formatting. Never
 * auto-format an existing/corrupt filesystem. Owner serializes all calls. */
int wf_cycle_store_mount(wf_cycle_store_t *,bool format_blank);
int wf_cycle_store_create(wf_cycle_store_t *,bool smoke,uint32_t session);
/* New formal runs use immutable session filenames. Legacy APIs stay read-compatible.
 * Admission reserves maximum row bytes plus 128KiB metadata/GC allowance; it is
 * a conservative software budget, not a guarantee against physical I/O failure. */
int wf_cycle_store_create_session(wf_cycle_store_t *,uint32_t session);
int wf_cycle_store_append(wf_cycle_store_t *,const uint8_t row[64]);
int wf_cycle_store_commit(wf_cycle_store_t *);
int wf_cycle_store_close(wf_cycle_store_t *);
int wf_cycle_store_reload(wf_cycle_store_t *); /* Drop volatile FS caches, never format. */
int wf_cycle_store_info(wf_cycle_store_t *,bool smoke,wf_cycle_info_t *);
int wf_cycle_store_read(wf_cycle_store_t *,bool smoke,uint32_t session,uint32_t index,uint8_t row[64]);
/* Strict named-file access; never falls back to a legacy file during recovery. */
int wf_cycle_store_info_session(wf_cycle_store_t *,uint32_t session,wf_cycle_info_t *);
int wf_cycle_store_read_session(wf_cycle_store_t *,uint32_t session,uint32_t index,uint8_t row[64]);
#endif
