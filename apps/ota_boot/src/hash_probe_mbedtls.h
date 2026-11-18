/* This diagnostic verifies the same RSA-2048/SHA-256 format as the production
 * boot. Exclude the unrelated SHA-512 family from its digest dispatcher. */
#include "mbedtls/config.h"
#undef MBEDTLS_SHA512_C
