#ifndef WF_OTA_JOURNAL_H
#define WF_OTA_JOURNAL_H
#include "ota_ab.h"

enum { WF_OTA_JOURNAL_OK = 0, WF_OTA_JOURNAL_IO = -1,
       WF_OTA_JOURNAL_NO_STATE = -2, WF_OTA_JOURNAL_STALE = -3,
       WF_OTA_JOURNAL_READ_ONLY = -4, WF_OTA_JOURNAL_BUSY = -5,
       WF_OTA_JOURNAL_ARGUMENT = -6 };
typedef struct {
    void *context;
    /* Unit is 0 or 1, never a physical address. The platform must bind each
     * unit to its own exclusively owned, aligned erase region. Exact byte
     * counts are required; erase/sync return zero on success. */
    int (*read)(void *, unsigned unit, uint32_t offset, void *, size_t);
    int (*program)(void *, unsigned unit, const void *, size_t);
    int (*erase)(void *, unsigned unit);
    int (*sync)(void *);
    bool (*acquire)(void *);
    void (*release)(void *);
    uint32_t (*now_ms)(void *);
    uint32_t erase_bytes, budget_ms;
    bool writable, fault;
} wf_ota_journal_t;

/* All callbacks execute under acquire/release. Software deadlines cannot
 * preempt a blocked platform call; ownership persists until it returns.
 * Failed write/erase/verification permanently freezes writes in this instance;
 * read-only inspection remains available. No implicit format or reset API. */
int wf_ota_journal_load(wf_ota_journal_t *, wf_ota_ab_state_t *, unsigned *unit);
/* Explicit initial provisioning is accepted only when both entire units are
 * erased and initial is a generation-1 STABLE state. Never called on load failure. */
int wf_ota_journal_provision(wf_ota_journal_t *, const wf_ota_ab_state_t *initial);
/* next must come from the boot model. Reloads current under the same lock,
 * rejects stale generations/identities, and writes only the other unit.
 * readback is usable for boot resolve only when this function returns OK. */
int wf_ota_journal_commit(wf_ota_journal_t *, const wf_ota_ab_state_t *current,
                          const wf_ota_ab_state_t *next, uint8_t readback[WF_OTA_AB_BYTES]);
#endif
