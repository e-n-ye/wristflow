#include "boot_plan.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
static wf_ota_boot_layout_t layout(void)
{
    return (wf_ota_boot_layout_t){.flash=0x12000000,.flash_bytes=0x1000000,
        .slot={0x12020000,0x12420000},.slot_bytes=0x400000,
        .loader=0x12820000,.loader_bytes=0x60000,.ram=0x20000000,.ram_bytes=0x7fc00};
}
static void refused(const wf_ota_boot_layout_t *l, unsigned target, uint32_t size, uint32_t sp, uint32_t pc)
{
    wf_ota_boot_plan_t plan; memset(&plan, 0xa5, sizeof plan);
    uint32_t vectors[2] = {sp, pc};
    assert(!wf_ota_boot_plan(&plan, l, target, size, vectors));
    const uint8_t *bytes = (const uint8_t *)&plan;
    for (unsigned i=0; i<sizeof plan; ++i) assert(bytes[i] == 0);
    ++checks;
}
static void plans_and_views(void)
{
    wf_ota_boot_layout_t l = layout();
    for (unsigned target=0; target<=WF_OTA_BOOT_LOADER; ++target) {
        uint32_t linked = target==WF_OTA_BOOT_LOADER ? l.loader:l.slot[0];
        uint32_t capacity = target==WF_OTA_BOOT_LOADER ? l.loader_bytes:l.slot_bytes;
        for (uint32_t size=8; size<=capacity; size*=2) {
            uint32_t vectors[2] = {l.ram+l.ram_bytes,linked+size-1};
            wf_ota_boot_plan_t p;
            assert(wf_ota_boot_plan(&p,&l,target,size,vectors));
            assert(p.vector==linked && p.stack==vectors[0] && p.reset==vectors[1]);
            assert(p.physical==(target==WF_OTA_BOOT_LOADER ? l.loader:l.slot[target]));
            if (target==WF_OTA_BOOT_B) {
                assert(p.alias.start==0x20000 && p.alias.end==0x420000 && p.alias.offset==0x400000);
                /* Host address model only: intended half-open window; exact
                 * peripheral endpoint semantics remain device qualification. */
                assert(linked+p.alias.offset==l.slot[1]);
                assert(linked+size-2+p.alias.offset<l.slot[1]+l.slot_bytes);
                assert(l.loader-l.flash>p.alias.end);
            } else assert(!p.alias.start && !p.alias.end && !p.alias.offset);
            ++checks;
        }
        const uint32_t invalid_sp[] = {0,l.ram,l.ram+1,l.ram+l.ram_bytes+8,UINT32_MAX};
        for (unsigned i=0;i<sizeof invalid_sp/sizeof invalid_sp[0];++i)
            refused(&l,target,128,invalid_sp[i],linked+65);
        const uint32_t invalid_pc[] = {0,linked+64,linked-1,linked+129,0x60000001,UINT32_MAX};
        for (unsigned i=0;i<sizeof invalid_pc/sizeof invalid_pc[0];++i)
            refused(&l,target,128,l.ram+0x4000,invalid_pc[i]);
        refused(&l,target,7,l.ram+0x4000,linked+1);
        refused(&l,target,capacity+1,l.ram+0x4000,linked+1);
    }
}
static void bad_layouts(void)
{
    wf_ota_boot_layout_t l = layout();
    for (unsigned field=0;field<9;++field) {
        wf_ota_boot_layout_t changed=l;
        switch(field) {
        case 0: changed.flash_bytes=0; break;
        case 1: changed.flash=UINT32_MAX-15; break;
        case 2: changed.slot[0]+=1024; break;
        case 3: changed.slot[1]+=1024; break;
        case 4: changed.slot_bytes=0; break;
        case 5: changed.loader-=1024; break;
        case 6: changed.loader_bytes=0; break;
        case 7: changed.ram=UINT32_MAX-15; break;
        default: changed.ram_bytes=0; break;
        }
        refused(&changed,WF_OTA_BOOT_B,128,0x20004000,0x12020041);
    }
    for (unsigned offset=1;offset<1024;++offset) {
        wf_ota_boot_layout_t changed=l; changed.slot[1]+=offset;
        refused(&changed,WF_OTA_BOOT_B,128,0x20004000,0x12020041);
    }
    refused(&l,3,128,0x20004000,0x12020041);
    l.flash=0x11ffffff; l.flash_bytes=0x1000001;
    refused(&l,WF_OTA_BOOT_B,128,0x20004000,0x12020041);
    refused(NULL,0,128,0x20004000,0x12020041);
}
int main(void)
{
    plans_and_views(); bad_layouts();
    printf("OTA RAM boot plan: %u checks passed; structural model only\n",checks);
    return 0;
}
