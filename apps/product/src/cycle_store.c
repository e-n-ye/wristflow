#if defined(WF_CYCLE_LOG) || defined(WF_CYCLE_HOST)
#include "cycle_store.h"
#include <string.h>
static int rd(const struct lfs_config *c,lfs_block_t b,lfs_off_t o,void *p,lfs_size_t n)
{return wf_fs_block_read(c->context,b,o,p,n) ? LFS_ERR_IO:0;}
static int pg(const struct lfs_config *c,lfs_block_t b,lfs_off_t o,const void *p,lfs_size_t n)
{return wf_fs_block_prog(c->context,b,o,p,n) ? LFS_ERR_IO:0;}
static int er(const struct lfs_config *c,lfs_block_t b)
{return wf_fs_block_erase(c->context,b) ? LFS_ERR_IO:0;}
static int sy(const struct lfs_config *c)
{return wf_fs_block_sync(c->context) ? LFS_ERR_IO:0;}
/* Only the dedicated owner calls LFS; transport commands queue to that owner. */
static int lk(const struct lfs_config *c) {(void)c;return 0;}
void wf_cycle_store_init(wf_cycle_store_t *s,wf_fs_block_t *device)
{
    memset(s,0,sizeof *s);
    s->cfg=(struct lfs_config){.context=device,.read=rd,.prog=pg,.erase=er,.sync=sy,.lock=lk,.unlock=lk,
        .read_size=32,.prog_size=256,.block_size=4096,.block_count=1024,.block_cycles=100,
        .cache_size=256,.lookahead_size=16,.read_buffer=s->read_cache,.prog_buffer=s->prog_cache,.lookahead_buffer=s->ahead};
    s->fcfg=(struct lfs_file_config){.buffer=s->file_cache};
}
int wf_cycle_store_mount(wf_cycle_store_t *s,bool format_blank)
{
    if(s->mounted)return LFS_ERR_INVAL;
    if(format_blank) {
        uint8_t data[256];
        for(unsigned b=0;b<1024;++b)for(unsigned off=0;off<4096;off+=256) {
            if(wf_fs_block_read(s->cfg.context,b,off,data,sizeof data))return LFS_ERR_IO;
            for(unsigned i=0;i<sizeof data;++i)if(data[i]!=255)return LFS_ERR_EXIST;
        }
        int result=lfs_format(&s->fs,&s->cfg);if(result)return result;
    }
    int result=lfs_mount(&s->fs,&s->cfg);s->mounted=result==0;return result;
}
static const char *name(bool smoke) {return smoke ? "smoke.bin":"cycle.bin";}
int wf_cycle_store_create(wf_cycle_store_t *s,bool smoke,uint32_t session)
{
    if(!s->mounted || s->opened || !session)return LFS_ERR_INVAL;
    int result=lfs_file_opencfg(&s->fs,&s->file,name(smoke),LFS_O_WRONLY|LFS_O_CREAT|LFS_O_EXCL,&s->fcfg);
    if(!result) {s->opened=true;s->smoke=smoke;s->session=session;s->rows=0;}
    return result;
}
int wf_cycle_store_append(wf_cycle_store_t *s,const uint8_t row[64])
{
    wf_cycle_row_t decoded;
    if(!s->opened || !wf_cycle_decode(row,&decoded) || decoded.session!=s->session || decoded.sequence!=s->rows ||
        s->rows>=WF_CYCLE_MAX_ROWS)return LFS_ERR_INVAL;
    if(lfs_file_write(&s->fs,&s->file,row,64)!=64)return LFS_ERR_IO;
    ++s->rows;return 0;
}
int wf_cycle_store_commit(wf_cycle_store_t *s) {return s->opened ? lfs_file_sync(&s->fs,&s->file):LFS_ERR_INVAL;}
int wf_cycle_store_close(wf_cycle_store_t *s)
{
    if(!s->opened)return 0;
    int result=lfs_file_close(&s->fs,&s->file);s->opened=false;return result;
}
int wf_cycle_store_reload(wf_cycle_store_t *s)
{
    if(s->opened)return LFS_ERR_INVAL;
    wf_fs_block_t *device=s->cfg.context;
    if(s->mounted)lfs_unmount(&s->fs);
    wf_cycle_store_init(s,device);return wf_cycle_store_mount(s,false);
}
int wf_cycle_store_info(wf_cycle_store_t *s,bool smoke,wf_cycle_info_t *info)
{
    if(!s->mounted || s->opened)return LFS_ERR_INVAL;
    int result=lfs_file_opencfg(&s->fs,&s->file,name(smoke),LFS_O_RDONLY,&s->fcfg);if(result)return result;
    *info=(wf_cycle_info_t){0};lfs_soff_t size=lfs_file_size(&s->fs,&s->file);
    if(size<=0 || size%64 || size>(lfs_soff_t)WF_CYCLE_MAX_ROWS*64)result=LFS_ERR_CORRUPT;
    uint8_t row[64];wf_cycle_row_t decoded;uint64_t elapsed=0;uint32_t crc=UINT32_MAX;
    for(unsigned i=0;!result && i<(unsigned)size/64;++i) {
        if(lfs_file_read(&s->fs,&s->file,row,64)!=64 || !wf_cycle_decode(row,&decoded) ||
            decoded.sequence!=i || (i && decoded.session!=info->session) ||
            decoded.elapsed<elapsed || (!i && decoded.kind!=WF_CYCLE_START) ||
            (i && decoded.kind==WF_CYCLE_START) || info->terminal) {result=LFS_ERR_CORRUPT;break;}
        info->session=decoded.session;info->terminal=decoded.kind==WF_CYCLE_END;elapsed=decoded.elapsed;
        for(unsigned j=0;j<64;++j) {crc^=row[j];for(unsigned k=0;k<8;++k)crc=(crc>>1)^((crc&1)?0xedb88320u:0);}
    }
    if(!result) {info->bytes=(uint32_t)size;info->rows=(uint32_t)size/64;info->crc=crc^UINT32_MAX;}
    int close=lfs_file_close(&s->fs,&s->file);return result ? result:close;
}
int wf_cycle_store_read(wf_cycle_store_t *s,bool smoke,uint32_t session,uint32_t index,uint8_t row[64])
{
    if(!s->mounted || s->opened || index>=WF_CYCLE_MAX_ROWS)return LFS_ERR_INVAL;
    int result=lfs_file_opencfg(&s->fs,&s->file,name(smoke),LFS_O_RDONLY,&s->fcfg);if(result)return result;
    wf_cycle_row_t decoded;
    if(lfs_file_seek(&s->fs,&s->file,index*64,LFS_SEEK_SET)!=(lfs_soff_t)index*64 ||
        lfs_file_read(&s->fs,&s->file,row,64)!=64 || !wf_cycle_decode(row,&decoded) ||
        decoded.session!=session || decoded.sequence!=index) result=LFS_ERR_CORRUPT;
    int close=lfs_file_close(&s->fs,&s->file);return result ? result:close;
}
#endif
