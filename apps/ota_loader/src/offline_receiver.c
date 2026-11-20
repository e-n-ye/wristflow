#include "offline_receiver.h"
#include <string.h>

enum { START = 38, DATA = WF_DFU_DATA, END = 42, OK = 0, ERROR = 1, SIZE = 3, NOT_READY = WF_DFU_NOT_READY,
       INCOMPLETE = 8, INDEX = 13 };

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static void put32(uint8_t *p, uint32_t v)
{
    for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (i * 8));
}
void wf_dfu_disconnect(wf_dfu_receiver_t *r)
{
    r->phase = WF_DFU_IDLE;
    r->total = r->accepted = r->blocks = r->crc = 0;
    memset(&r->image, 0, sizeof r->image);
}

uint8_t wf_dfu_percent(const wf_dfu_receiver_t *r)
{
    if (!r->total) return 0;
    if (r->accepted == r->blocks) return 100;
    /* START bounds total to 4 MiB + envelope. Only stored blocks count. */
    return (uint8_t)(r->accepted * WF_DFU_BLOCK * 100u / r->total);
}

bool wf_dfu_is_cancel(const uint8_t *wire, size_t size)
{
    return size == 6 && wire[0] == WF_DFU_CANCEL && wire[1] == 0 && wire[2] == 2 && wire[3] == 0;
}

static unsigned start(wf_dfu_receiver_t *r, const uint8_t *p, size_t n)
{
    if (r->phase == WF_DFU_INSTALLED || r->phase == WF_DFU_VERIFIED) return NOT_READY;
    wf_dfu_disconnect(r);
    if (n != 12 && (n != 16 || le32(p + 12) != 1)) return ERROR;
    uint32_t total = le32(p), blocks = le32(p + 4);
    if (total < WF_DFU_ENVELOPE + WF_RELEASE_HEADER_BYTES + 8 ||
        total > WF_DFU_ENVELOPE + WF_DFU_SLOT_BYTES || total > r->capacity ||
        blocks != (total + WF_DFU_BLOCK - 1) / WF_DFU_BLOCK) return SIZE;
    if (!r->can_start(r->context)) return NOT_READY;
    r->total = total;
    r->blocks = blocks;
    r->crc = le32(p + 8);
    r->phase = WF_DFU_RECEIVING;
    return OK;
}

static bool admit(wf_dfu_receiver_t *r, const uint8_t *p)
{
    static const uint8_t prefix[] = {'F','C','E','S',1,0xff,1,0};
    return !memcmp(p, prefix, sizeof prefix) && le32(p + 8) == r->crc &&
        p[12] == 0 && p[13] == 0 && le32(p + 14) == r->total - WF_DFU_ENVELOPE &&
        wf_ota_release_open(p + WF_DFU_ENVELOPE, WF_RELEASE_HEADER_BYTES, WF_DFU_SLOT_BYTES, &r->image) &&
        r->image.bytes == r->total - WF_DFU_ENVELOPE - WF_RELEASE_HEADER_BYTES;
}

static unsigned packet(wf_dfu_receiver_t *r, const uint8_t *p, size_t n, uint8_t *retry)
{
    if (r->phase != WF_DFU_RECEIVING) return NOT_READY;
    if (n < 12 || r->accepted >= r->blocks) return ERROR;
    if (le32(p) != r->accepted + 1) { *retry = 1; return INDEX; }
    uint32_t offset = r->accepted * WF_DFU_BLOCK;
    uint32_t bytes = r->total - offset;
    if (bytes > WF_DFU_BLOCK) bytes = WF_DFU_BLOCK;
    if (le32(p + 4) != bytes || n - 12 != bytes) return SIZE;
    if (r->crc32_mpeg2(r->context, p + 12, bytes) != le32(p + 8)) { *retry = 1; return ERROR; }
    if (!r->accepted && !admit(r, p + 12)) return ERROR;
    memcpy(r->file + offset, p + 12, bytes);
    ++r->accepted;
    return OK;
}

static unsigned finish(wf_dfu_receiver_t *r, const uint8_t *p, size_t n)
{
    if (n != 2 || p[0] || p[1] || r->phase != WF_DFU_RECEIVING) return NOT_READY;
    if (r->accepted != r->blocks) return INCOMPLETE;
    if (r->verifying) r->verifying(r->context);
    uint8_t *header = r->file + WF_DFU_ENVELOPE;
    if (r->crc32_mpeg2(r->context, header, r->total - WF_DFU_ENVELOPE) != r->crc ||
        !admit(r, r->file)) return ERROR;
    uint8_t *body = header + WF_RELEASE_HEADER_BYTES;
    uint32_t hash[8], aligned[64];
    for (uint32_t offset = 0; offset < r->image.bytes;) {
        unsigned bytes = r->image.bytes - offset;
        if (bytes > sizeof aligned) bytes = sizeof aligned;
        memcpy(aligned, body + offset, bytes);
        if (!wf_ota_release_hash(hash, offset, (uint8_t *)aligned, bytes, offset + bytes == r->image.bytes))
            return ERROR;
        offset += bytes;
    }
    if (memcmp(hash, r->image.sha256, sizeof hash)) return ERROR;
    r->phase = WF_DFU_VERIFIED;
    return OK;
}

bool wf_dfu_install_received(wf_dfu_receiver_t *r)
{
    if (!r || r->phase != WF_DFU_VERIFIED) return false;
    const uint8_t *header = r->file + WF_DFU_ENVELOPE;
    r->phase = WF_DFU_IDLE;
    if (!r->install(r->context, header, header + WF_RELEASE_HEADER_BYTES, &r->image)) return false;
    r->phase = WF_DFU_INSTALLED;
    return true;
}

size_t wf_dfu_request(wf_dfu_receiver_t *r, const uint8_t *wire, size_t n, uint8_t response[12])
{
    if (!r || !r->file || !r->crc32_mpeg2 || !r->can_start || !r->install || !wire || !response || n < 4) return 0;
    unsigned id = wire[0] | (unsigned)wire[1] << 8;
    unsigned size = wire[2] | (unsigned)wire[3] << 8;
    if (wf_dfu_is_cancel(wire, n) && r->phase != WF_DFU_INSTALLED) {
        wf_dfu_disconnect(r);
        return 0;
    }
    if (id != START && id != DATA && id != END) return 0;
    memset(response, 0, 12);
    response[0] = (uint8_t)(id + 1);
    response[2] = id == END ? 2 : 8;
    unsigned status = ERROR;
    if (size == n - 4) {
        if (id == START) { status = start(r, wire + 4, size); response[6] = 1; }
        if (id == DATA) status = packet(r, wire + 4, size, response + 6);
        if (id == END) status = finish(r, wire + 4, size);
    }
    response[4] = (uint8_t)status;
    if (id != END) put32(response + 8, r->accepted);
    if (status != OK && !(id == DATA && response[6]) && r->phase != WF_DFU_INSTALLED)
        wf_dfu_disconnect(r);
    return 4u + response[2];
}
