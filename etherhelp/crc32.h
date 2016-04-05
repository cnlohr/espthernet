#ifndef _CRC32_H
#define _CRC32_H

#include <eth_config.h>

//Accepts "TABLE_CRC" which takes 512 extra bytes, but is wwaaaayyyy faster

uint32_t crc32(const uint8_t *buf, size_t size);

#endif

