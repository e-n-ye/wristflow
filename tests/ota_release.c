#include "release.h"
#include "boot_plan.h"
#include "offline_receiver.h"
#include "mbedtls/sha256.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A public signature of the ordinary 11/08 Product build, release 2026111801. */
static const uint8_t signed_header[] = {
#include "ota_release_fixture.inc"
};
static uint8_t slot[0x400000];
static const uint32_t physical_b = 0x12420000;
static const uint8_t original_enrollment[WF_RELEASE_HEADER_BYTES] = {
#include "original_a.inc"
};

#ifdef WF_OTA_BOOT_TRACE
enum { NO_FAULT, MANIFEST_RESULT, BODY_RESULT, BLOCK_IO };
static unsigned fault;
static bool collect;
static char points[64];
static uint32_t details[64];
static size_t emitted;

void wf_boot_trace(char point, uint32_t value)
{
    if (!collect) return;
    assert(emitted + 1 < sizeof points);
    points[emitted] = point;
    details[emitted++] = value;
    points[emitted] = 0;
}
#endif

bool wf_ota_release_hash(uint32_t state[8], uint32_t previous, uint8_t *block,
                         unsigned bytes, bool final)
{
    static mbedtls_sha256_context sha;
    if (!previous) {
        mbedtls_sha256_init(&sha);
        mbedtls_sha256_starts(&sha, 0);
    }
    assert(sha.total[0] == previous);
    mbedtls_sha256_update(&sha, block, bytes);
    if (final) {
        mbedtls_sha256_finish(&sha, (uint8_t *)state);
        mbedtls_sha256_free(&sha);
#ifdef WF_OTA_BOOT_TRACE
        if ((fault == MANIFEST_RESULT && !previous) || (fault == BODY_RESULT && previous))
            state[0] ^= 1;
#endif
    }
    return true;
}
static bool ready(void *context) { (void)context; return true; }
static int read_bytes(void *context, uint32_t address, void *out, size_t bytes)
{
    (void)context;
    assert(address >= physical_b && address - physical_b <= sizeof slot - bytes);
    memcpy(out, slot + (address - physical_b), bytes);
    return (int)bytes;
}

static int read_enrolled(void *context, uint32_t address, void *out, size_t bytes)
{
    if (address >= 0x12821000) {
        assert(address - 0x12821000 <= sizeof original_enrollment - bytes);
        memcpy(out, (const uint8_t *)context + (address - 0x12821000), bytes);
    } else {
        uint32_t base = address < physical_b ? 0x12020000 : physical_b;
        assert(address >= base && address - base <= sizeof slot - bytes);
        memcpy(out, slot + (address - base), bytes);
    }
    return (int)bytes;
}

static void check_original(const char *backup, const char *journal)
{
    FILE *f = fopen(backup, "rb");
    assert(f && !fseek(f, 0x20000, SEEK_SET) && fread(slot, 1, sizeof slot, f) == sizeof slot);
    fclose(f);
    uint8_t cert[sizeof original_enrollment];
    memcpy(cert, original_enrollment, sizeof cert);
    wf_ota_physical_reader_t reader = {.context=cert,.ready=ready,.read=read_enrolled,
        .region={[WF_OTA_READ_A]={0x12020000,sizeof slot},[WF_OTA_READ_B]={physical_b,sizeof slot},
                 [WF_OTA_READ_ORIGINAL_A]={0x12821000,sizeof cert}}};
    wf_ota_ab_image_t image;
    assert(wf_ota_release_verify_slot(&reader, WF_OTA_READ_A, &image));
    assert(image.release == 2026110301 && image.bytes == 3055760);
    if (journal) {
        wf_ota_ab_state_t initial;
        uint8_t encoded[WF_OTA_AB_BYTES], sector[4096];
        assert(wf_ota_ab_provision(&initial, WF_OTA_AB_SLOT_A, &image));
        assert(wf_ota_ab_encode(&initial, encoded));
        f = fopen(journal, "rb");
        assert(f && fread(sector, 1, sizeof sector, f) == sizeof sector && fgetc(f) == EOF);
        fclose(f);
        assert(!memcmp(sector, encoded, sizeof encoded));
        for (size_t i = sizeof encoded; i < sizeof sector; ++i) assert(sector[i] == 0xff);
        puts("Initial wired journal matches firmware provision/encode from verified original A");
    }
    assert(!wf_ota_release_verify_slot(&reader, WF_OTA_READ_B, &image));
    slot[3055759] ^= 1;
    assert(!wf_ota_release_verify_slot(&reader, WF_OTA_READ_A, &image));
    slot[3055759] ^= 1;
    cert[335] ^= 1;
    assert(!wf_ota_release_verify_slot(&reader, WF_OTA_READ_A, &image));
    memcpy(cert, signed_header, sizeof cert);
    assert(!wf_ota_release_verify_slot(&reader, WF_OTA_READ_A, &image));
    puts("Original A verified from its complete physical slot; wrong slot, body, signature and role refused");
}

static uint8_t staging[WF_DFU_ENVELOPE + WF_DFU_SLOT_BYTES];
static int read_nor_file(void *context, uint32_t addr, void *out, size_t n)
{
    FILE *f = context;
#ifdef WF_OTA_BOOT_TRACE
    if (fault == BLOCK_IO && addr == 0x12020100) return -1;
#endif
    if (addr < 0x12000000 || n > 0x1000000 || addr - 0x12000000 > 0x1000000 - n ||
        fseek(f, (long)(addr - 0x12000000), SEEK_SET)) return -1;
    return (int)fread(out, 1, n, f);
}

static void check_ordinary_a(const char *path)
{
    FILE *f = fopen(path, "rb");
    assert(f);
    wf_ota_physical_reader_t reader = {.context=f,.ready=ready,.read=read_nor_file,
        .region={[WF_OTA_READ_A]={0x12020000,sizeof slot},[WF_OTA_READ_B]={physical_b,sizeof slot},
                 [WF_OTA_READ_ORIGINAL_A]={0x1286c404,sizeof original_enrollment}}};
    uint8_t records[2][WF_OTA_AB_BYTES];
    for (unsigned i=0; i<2; ++i)
        assert(read_nor_file(f,0x12da0000+i*8192,records[i],sizeof records[i])==(int)sizeof records[i]);
    wf_ota_ab_state_t state;
    assert(wf_ota_ab_select(&state,records[0],sizeof records[0],records[1],sizeof records[1])==0);
    wf_ota_ab_image_t checked[2] = {0};
    assert(wf_ota_release_verify_slot(&reader,WF_OTA_READ_A,&checked[0]));
    wf_ota_ab_plan_t plan;
    assert(wf_ota_ab_plan(&plan,&state,checked) && !plan.persist && plan.target==WF_OTA_AB_SLOT_A);
    assert(checked[0].release==2026110301 && checked[0].bytes==3055760);
    const wf_ota_boot_layout_t layout = {
        .flash=0x12000000, .flash_bytes=0x1000000, .slot={0x12020000,0x12420000},
        .slot_bytes=0x400000, .loader=0x12820000, .loader_bytes=0x60000,
        .ram=0x20000000, .ram_bytes=0x7fc00
    };
    uint32_t vectors[2];
    wf_ota_boot_plan_t boot;
    assert(read_nor_file(f,layout.slot[0],vectors,sizeof vectors)==sizeof vectors);
    assert(wf_ota_boot_plan(&boot,&layout,WF_OTA_BOOT_A,checked[0].bytes,vectors));
    assert(boot.stack==0x20002000 && boot.reset==0x1202baa1 && boot.vector==layout.slot[0]);
#ifdef WF_OTA_BOOT_TRACE
    const char *prefix = "rrimmmmmmmmnnnnnnnnkw";
    collect = true;
    for (fault = NO_FAULT; fault <= BLOCK_IO; ++fault) {
        emitted = 0;
        bool ok = wf_ota_release_verify_slot(&reader, WF_OTA_READ_A, &checked[0]);
        assert(ok == (fault == NO_FAULT));
        assert(!strncmp(points, prefix, strlen(prefix)) && details[0] == 0x1241feb0 && details[1] == 0x1286c404);
        assert(details[2] == 3055760 && details[19] == 0);
        if (fault == MANIFEST_RESULT) {
            assert(details[3] != details[11] && (int32_t)details[20] < 0 && !strcmp(points+21, "x") && details[21] == 5);
        } else if (fault == BLOCK_IO) {
            assert(!strcmp(points+21, "d") && details[21] == 256);
        } else {
            assert(!memcmp(details+3, details+11, 32) && !details[20]);
            assert(!strncmp(points+21, "bbbbbbbbcccccccc", 16));
            assert(!memcmp(details+29, original_enrollment+48, 32));
            assert((memcmp(details+21, details+29, 32) != 0) == (fault == BODY_RESULT));
            if (ok) assert(emitted == 37);
            else assert(!strcmp(points+37, "x") && details[37] == 6);
        }
    }
    collect = false;
    fault = NO_FAULT;
    puts("Traced original A: signed input accepted; manifest and body peripheral discrepancies and short read rejected");
#endif
    fclose(f);
    puts("Ordinary A authenticated, selected and admitted by its vector plan on host");
}

static unsigned installs;
static uint32_t mpeg(void *context, const uint8_t *buf, size_t size)
{
    (void)context;
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < size; i++) {
        crc ^= (uint32_t)buf[i] << 24;
        for (unsigned bit = 0; bit < 8; bit++)
            crc = (crc << 1) ^ ((crc >> 31) ? 0x04c11db7u : 0);
    }
    return crc;
}
static bool install(void *context, const uint8_t *header, const uint8_t *body, const wf_ota_ab_image_t *image)
{
    wf_ota_ab_image_t readback;
    ++installs;
    memcpy(slot, body, image->bytes);
    memcpy(slot + sizeof slot - WF_RELEASE_HEADER_BYTES, header, WF_RELEASE_HEADER_BYTES);
    return wf_ota_release_verify_slot(context, WF_OTA_READ_B, &readback) &&
           readback.release == image->release && !memcmp(readback.sha256, image->sha256, 32);
}
static void put32(uint8_t *out, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i) out[i] = (uint8_t)(value >> (i * 8));
}
static void android_file(const char *path, wf_ota_physical_reader_t *reader)
{
    FILE *f = fopen(path, "rb");
    assert(f && !fseek(f, 0, SEEK_END));
    long size = ftell(f);
    assert(size >= 354 && (size_t)size <= sizeof staging && !fseek(f, 0, SEEK_SET));
    uint8_t *file = malloc(size);
    assert(file && fread(file, 1, size, f) == (size_t)size);
    fclose(f);
    uint32_t count = ((uint32_t)size + WF_DFU_BLOCK - 1) / WF_DFU_BLOCK;
    uint8_t begin[20] = {38,0,16,0}, reply[12], end[6] = {42,0,2,0};
    put32(begin + 4, size); put32(begin + 8, count); memcpy(begin + 12, file + 8, 4); begin[16] = 1;
    wf_dfu_receiver_t r = {.file=staging, .capacity=sizeof staging, .context=reader, .can_start=ready, .crc32_mpeg2=mpeg, .install=install};
    for (unsigned pass = 0; pass < 3; ++pass) {
        if (pass) {
            /* On pass 1, even self-consistent transport CRCs cannot admit a
             * modified image. Pass 2 restores the actual signed payload. */
            file[size - 1] ^= 1;
            put32(file + 8, mpeg(NULL, file + WF_DFU_ENVELOPE, size - WF_DFU_ENVELOPE));
            memcpy(begin + 12, file + 8, 4);
        }
        assert(wf_dfu_request(&r, begin, sizeof begin, reply) == 12 && reply[0] == 39 && !reply[4] && reply[6] == 1);
        for (uint32_t index = 1; index <= count; ++index) {
            size_t offset = (index - 1) * WF_DFU_BLOCK;
            size_t n = (size_t)size - offset;
            if (n > WF_DFU_BLOCK) n = WF_DFU_BLOCK;
            uint8_t data[16 + WF_DFU_BLOCK] = {40,0};
            data[2] = (uint8_t)(12 + n); data[3] = (uint8_t)((12 + n) >> 8);
            put32(data + 4, index); put32(data + 8, n); put32(data + 12, mpeg(NULL, file + offset, n));
            memcpy(data + 16, file + offset, n);
            if (index == 1) {
                put32(data + 4, 2);
                assert(wf_dfu_request(&r, data, 16+n, reply) == 12 && reply[4] == 13 && reply[6] && !r.accepted);
                put32(data + 4, 1);
                data[12] ^= 1;
                assert(wf_dfu_request(&r, data, 16+n, reply) == 12 && reply[4] && reply[6] && !r.accepted);
                data[12] ^= 1;
            }
            assert(wf_dfu_request(&r, data, 16+n, reply) == 12 && !reply[4] && r.accepted == index);
            if (!pass && index == 1) {
                wf_dfu_disconnect(&r);
                assert(wf_dfu_request(&r, end, sizeof end, reply) == 6 && reply[4] && !installs);
                break;
            }
        }
        if (pass == 1) {
            assert(wf_dfu_request(&r, end, sizeof end, reply) == 6 && reply[4] && !installs);
            assert(!wf_dfu_install_received(&r));
        }
    }
    assert(wf_dfu_request(&r, end, sizeof end, reply) == 6 && !reply[4] && !installs);
    assert(wf_dfu_install_received(&r) && installs == 1);
    assert(!wf_dfu_install_received(&r) && installs == 1);
    assert(wf_dfu_request(&r, end, sizeof end, reply) == 6 && reply[4] && installs == 1);
    free(file);
}

int main(int argc, char **argv)
{
    if (argc==3 && !strcmp(argv[1],"--ordinary-a")) {
        check_ordinary_a(argv[2]);
        return 0;
    }
    wf_ota_ab_image_t image;
    assert(!wf_ota_release_open(original_enrollment, sizeof original_enrollment, sizeof slot, &image));
    if ((argc == 3 || argc == 4) && !strcmp(argv[1], "--original-a")) {
        check_original(argv[2], argc == 4 ? argv[3] : NULL);
        return 0;
    }
    assert(wf_ota_release_open(signed_header, sizeof signed_header, sizeof slot, &image));
    assert(image.bytes == 3028888 && image.release == 2026111801);
    assert(!wf_ota_release_open(signed_header, sizeof signed_header - 1, sizeof slot, &image));
    assert(!wf_ota_release_open(signed_header, sizeof signed_header, 0x800000, &image));
    uint8_t altered[sizeof signed_header];
    for (size_t i = 0; i < sizeof altered; i++) {
        memcpy(altered, signed_header, sizeof altered);
        altered[i] ^= 1;
        assert(!wf_ota_release_open(altered, sizeof altered, sizeof slot, &image));
        assert(!image.bytes && !image.release);
    }
    wf_ota_physical_reader_t reader = {.ready = ready, .read = read_bytes,
        .region = {[WF_OTA_READ_B] = {physical_b, sizeof slot}}};
    assert(!wf_ota_release_verify_slot(&reader, WF_OTA_READ_B, &image));
    assert(!wf_ota_release_verify_slot(&reader, WF_OTA_READ_META0, &image));
    if (argc >= 2) {
        FILE *release = fopen(argv[1], "rb");
        assert(release);
        uint8_t *tail = slot + sizeof slot - WF_RELEASE_HEADER_BYTES;
        assert(fread(tail, 1, WF_RELEASE_HEADER_BYTES, release) == WF_RELEASE_HEADER_BYTES);
        assert(wf_ota_release_open(tail, WF_RELEASE_HEADER_BYTES, sizeof slot, &image));
        assert(fread(slot, 1, image.bytes, release) == image.bytes && fgetc(release) == EOF);
        fclose(release);
        assert(wf_ota_release_verify_slot(&reader, WF_OTA_READ_B, &image));
        printf("verified release %u, %u bytes\n", image.release, image.bytes);
        slot[image.bytes - 1] ^= 1;
        assert(!wf_ota_release_verify_slot(&reader, WF_OTA_READ_B, &image));
        assert(!image.bytes && !image.release);
    }
    if (argc == 3) android_file(argv[2], &reader);
    return 0;
}
