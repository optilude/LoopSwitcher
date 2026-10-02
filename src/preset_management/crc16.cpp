#include "preset_management/crc16.h"

uint16_t crc16Update(uint16_t crc, uint8_t byte) {
    crc = static_cast<uint16_t>(crc ^ (static_cast<uint16_t>(byte) << 8));
    for (uint8_t bit = 0; bit < 8; ++bit) {
        crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021) : static_cast<uint16_t>(crc << 1);
    }
    return crc;
}

uint16_t crc16(const uint8_t* data, size_t length) {
    uint16_t crc = kCrcInit;
    for (size_t i = 0; i < length; ++i) crc = crc16Update(crc, data[i]);
    return crc;
}
