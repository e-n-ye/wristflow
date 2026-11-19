#ifndef WF_OTA_RECEIVER_IDENTITY_H
#define WF_OTA_RECEIVER_IDENTITY_H

#include <stdbool.h>
#include <stdint.h>

/* The SDK address array is least-significant octet first. */
bool wf_ota_receiver_address(const uint8_t uid[16], uint8_t address[6]);

#endif
