#ifndef WF_OTA_PROBE_PORT_H
#define WF_OTA_PROBE_PORT_H
void wf_ota_loader_read_probe(void);
#include "ota_ab.h"
bool wf_ota_loader_accepting(void *context);
bool wf_ota_loader_has_stable(void);
bool wf_ota_loader_install(void *context, const uint8_t *header, const uint8_t *body, const wf_ota_ab_image_t *image);
#endif
