#include "notifications.h"
#include <string.h>

static bool groupable(const wf_phone_metadata_t *m)
{ return m->valid && m->notification_key[0] && m->package[0]; }
bool wf_notification_group_key_equal(const wf_phone_metadata_t *a, const wf_phone_metadata_t *b)
{
    if (!groupable(a) || !groupable(b)) return !groupable(a) && !groupable(b);
    return !strcmp(a->notification_key, b->notification_key) && !strcmp(a->package, b->package);
}
bool wf_notification_same_group(const wf_phone_message_t *a, const wf_phone_message_t *b)
{
    if (!a || !b) return false;
    if (a->id == b->id) return true;
    return groupable(&a->metadata) && groupable(&b->metadata) &&
        wf_notification_group_key_equal(&a->metadata, &b->metadata);
}

unsigned wf_notification_history(const wf_phone_message_t *messages, unsigned count,
    const wf_phone_message_t *anchor, unsigned indices[WF_PHONE_MESSAGES])
{
    unsigned n = 0;
    bool timed = true;
    if (count > WF_PHONE_MESSAGES) return 0;
    /* Storage is newest reception first. Reverse it for stable arrival order. */
    for (unsigned i = count; i > 0; --i) {
        if (!wf_notification_same_group(anchor, &messages[i - 1])) continue;
        indices[n++] = i - 1;
        if (!messages[i - 1].metadata.when_valid) timed = false;
    }
    /* Missing times retain arrival order for the whole group; no synthetic time
     * and no non-transitive mixture of timestamp and arrival comparisons. */
    if (timed) for (unsigned i = 1; i < n; ++i) {
        unsigned index = indices[i], j = i;
        while (j && messages[indices[j - 1]].metadata.when_ms > messages[index].metadata.when_ms) {
            indices[j] = indices[j - 1]; --j;
        }
        indices[j] = index;
    }
    return n;
}

unsigned wf_notification_groups(const wf_phone_message_t *messages, unsigned count,
    unsigned latest[WF_PHONE_MESSAGES])
{
    unsigned n = 0;
    if (count > WF_PHONE_MESSAGES) return 0;
    for (unsigned i = 0; i < count; ++i) {
        bool seen = false;
        for (unsigned j = 0; j < n; ++j)
            if (wf_notification_same_group(&messages[i], &messages[latest[j]])) { seen = true; break; }
        if (seen) continue;
        unsigned history[WF_PHONE_MESSAGES];
        unsigned size = wf_notification_history(messages, count, &messages[i], history);
        latest[n++] = history[size - 1];
    }
    return n;
}
