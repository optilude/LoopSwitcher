#pragma once

#include <stddef.h>
#include <stdint.h>

constexpr uint16_t kCrcInit = 0xFFFF;

// CRC-16/CCITT-FALSE (polynomial 0x1021, initial value 0xFFFF).
uint16_t crc16Update(uint16_t crc, uint8_t byte);
uint16_t crc16(const uint8_t* data, size_t length);
