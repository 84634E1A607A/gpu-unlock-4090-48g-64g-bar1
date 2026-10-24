/* SPDX-License-Identifier: GPL-2.0-only */
#include "compat.h"
#define SHA256_DIGEST_SIZE 32
void sha256(const u8 *data, size_t size, u8 digest[SHA256_DIGEST_SIZE]);
