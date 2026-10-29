#if defined(WF_STORAGE_PROBE) || defined(WF_STORAGE_METADATA_HOST)
#include "storage_snapshot.h"
#include <string.h>
unsigned wf_storage_source_copy(const wf_storage_source_t *source, const wf_storage_copy_io_t *io,
    uint8_t *out, size_t size, wf_storage_probe_info_t *info)
{
    if (!info) return WF_STORAGE_COPY_ARGUMENT;
    memset(info,0,sizeof *info);
    if (!source) return WF_STORAGE_COPY_ARGUMENT;
    info->source=*source;
    if (!source->ready) info->guard|=WF_STORAGE_GUARD_READY;
    if (!source->initialized) info->guard|=WF_STORAGE_GUARD_INIT;
    if (source->file_mode) info->guard|=WF_STORAGE_GUARD_FILE;
    if (!source->has_partition) info->guard|=WF_STORAGE_GUARD_PART;
    if (source->partition_bytes!=WF_STORAGE_BYTES) info->guard|=WF_STORAGE_GUARD_SIZE;
    if (source->sector_bytes!=WF_STORAGE_SECTOR) info->guard|=WF_STORAGE_GUARD_SECTOR;
    if (info->guard) return WF_STORAGE_COPY_UNAVAILABLE;
    return wf_storage_copy(io,out,size,&info->copy);
}
unsigned wf_storage_copy(const wf_storage_copy_io_t *io, uint8_t *out, size_t size, wf_storage_copy_stats_t *stats)
{
    if (!stats) return WF_STORAGE_COPY_ARGUMENT;
    memset(stats,0,sizeof *stats);
    if (!io || !out || size!=WF_STORAGE_BYTES || !io->lock || !io->unlock || !io->read || !io->now)
        return WF_STORAGE_COPY_ARGUMENT;
    uint32_t start=io->now(io->context);
    if (!io->lock(io->context)) return WF_STORAGE_COPY_LOCK;
    unsigned result=WF_STORAGE_COPY_OK;
    for (uint32_t off=0; off<size; off+=128) {
        if (io->now(io->context)-start>=2000U) { result=WF_STORAGE_COPY_TIMEOUT; break; }
        ++stats->calls;
        if (io->read(io->context,off,out+off,128)!=128) { result=WF_STORAGE_COPY_READ; break; }
        stats->bytes+=128;
    }
    if (result==WF_STORAGE_COPY_OK && io->now(io->context)-start>=2000U) result=WF_STORAGE_COPY_TIMEOUT;
    if (!io->unlock(io->context)) result=WF_STORAGE_COPY_UNLOCK;
    stats->elapsed_ms=io->now(io->context)-start;
    if (result!=WF_STORAGE_COPY_OK) memset(out,0,size);
    return result;
}
#endif
