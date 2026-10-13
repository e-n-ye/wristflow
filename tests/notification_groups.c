#include "notifications.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    wf_phone_message_t messages[WF_PHONE_MESSAGES] = {0};
    for (unsigned i=0;i<WF_PHONE_MESSAGES;++i) {
        messages[i].id=(int32_t)(WF_PHONE_MESSAGES-i);
        messages[i].metadata=(wf_phone_metadata_t){.valid=true,.when_valid=true,.when_ms=1000-i};
        strcpy(messages[i].metadata.notification_key,"0123456789abcdef0123456789abcdef");
        strcpy(messages[i].metadata.package,"com.tencent.mobileqq");
    }
    unsigned latest[WF_PHONE_MESSAGES], history[WF_PHONE_MESSAGES];
    assert(wf_notification_groups(messages,10,latest)==1&&latest[0]==0);
    assert(wf_notification_history(messages,10,&messages[0],history)==10);
    assert(history[0]==9&&history[9]==0);
    messages[0].metadata.when_ms=900;
    assert(wf_notification_groups(messages,10,latest)==1&&latest[0]==1);
    assert(wf_notification_history(messages,10,&messages[0],history)==10&&history[0]==0);
    messages[0].metadata.when_ms=messages[1].metadata.when_ms;
    assert(wf_notification_groups(messages,10,latest)==1&&latest[0]==0);
    messages[5].metadata.when_valid=false;
    assert(wf_notification_history(messages,10,&messages[0],history)==10&&history[0]==9&&history[9]==0);
    strcpy(messages[2].metadata.package,"other.package");
    messages[3].metadata.package[0]=0;
    messages[4].metadata.valid=false;
    strcpy(messages[5].metadata.notification_key,"1123456789abcdef0123456789abcdef");
    assert(wf_notification_groups(messages,10,latest)==5);
    assert(!wf_notification_same_group(&messages[2],&messages[0]));
    assert(!wf_notification_same_group(&messages[3],&messages[4]));
    assert(!wf_notification_groups(messages,11,latest));
    puts("notification_groups: identity isolation, ordering, ties, fallback and bounds PASS");
    return 0;
}
