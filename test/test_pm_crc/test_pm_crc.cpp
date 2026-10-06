#include <string.h>
#include <unity.h>

#include "fake_eeprom.h"
#include "preset_management/crc16.h"
#include "preset_management/layout.h"

void setUp() {}
void tearDown() {}

void test_crc16_matches_the_ccitt_false_check_value() {
    const char* text = "123456789";
    TEST_ASSERT_EQUAL_HEX16(0x29B1, crc16(reinterpret_cast<const uint8_t*>(text), strlen(text)));
}

void test_crc16_of_nothing_is_the_initial_value() {
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, crc16(nullptr, 0));
}

void test_crc16_changes_when_any_single_bit_flips() {
    uint8_t data[16];
    for (int i = 0; i < 16; ++i) data[i] = static_cast<uint8_t>(i * 7 + 3);
    const uint16_t original = crc16(data, sizeof(data));
    for (int byte = 0; byte < 16; ++byte) {
        for (int bit = 0; bit < 8; ++bit) {
            data[byte] ^= static_cast<uint8_t>(1u << bit);
            TEST_ASSERT_NOT_EQUAL_HEX16(original, crc16(data, sizeof(data)));
            data[byte] ^= static_cast<uint8_t>(1u << bit);
        }
    }
}

void test_crc16_can_be_computed_incrementally() {
    const uint8_t data[] = {1, 2, 3, 4, 5};
    uint16_t crc = kCrcInit;
    for (uint8_t b : data) crc = crc16Update(crc, b);
    TEST_ASSERT_EQUAL_HEX16(crc16(data, sizeof(data)), crc);
}

void test_fake_eeprom_starts_erased() {
    FakeEeprom eeprom;
    for (uint16_t a = 0; a < FakeEeprom::kSize; ++a) TEST_ASSERT_EQUAL_HEX8(0xFF, eeprom.read(a));
}

void test_update_writes_and_counts_only_real_changes() {
    FakeEeprom eeprom;
    eeprom.update(10, 0xFF);  // already 0xFF
    TEST_ASSERT_EQUAL_UINT32(0, eeprom.writeCount(10));
    eeprom.update(10, 0x42);
    eeprom.update(10, 0x42);
    TEST_ASSERT_EQUAL_HEX8(0x42, eeprom.read(10));
    TEST_ASSERT_EQUAL_UINT32(1, eeprom.writeCount(10));
    eeprom.update(10, 0x43);
    TEST_ASSERT_EQUAL_UINT32(2, eeprom.writeCount(10));
    TEST_ASSERT_EQUAL_UINT32(2, eeprom.totalWrites());
    TEST_ASSERT_EQUAL_UINT32(2, eeprom.maxWritesPerCell());
}

void test_out_of_range_access_is_recorded_and_harmless() {
    FakeEeprom eeprom;
    eeprom.update(FakeEeprom::kSize, 1);
    eeprom.read(FakeEeprom::kSize + 5);
    TEST_ASSERT_EQUAL_UINT32(2, eeprom.outOfRangeAccesses);
    TEST_ASSERT_EQUAL_UINT32(0, eeprom.totalWrites());
}

void test_layout_regions_fit_in_256_bytes_without_overlap() {
    TEST_ASSERT_EQUAL_UINT16(3, kLabelsAddr);
    TEST_ASSERT_EQUAL_UINT16(83, kPresetsAddr);
    TEST_ASSERT_EQUAL_UINT16(219, kConfigEnd);
    TEST_ASSERT_EQUAL_UINT16(220, kRingAddr);
    TEST_ASSERT_EQUAL_UINT16(256, kRingAddr + kRingRecords * kRingRecordSize);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_crc16_matches_the_ccitt_false_check_value);
    RUN_TEST(test_crc16_of_nothing_is_the_initial_value);
    RUN_TEST(test_crc16_changes_when_any_single_bit_flips);
    RUN_TEST(test_crc16_can_be_computed_incrementally);
    RUN_TEST(test_fake_eeprom_starts_erased);
    RUN_TEST(test_update_writes_and_counts_only_real_changes);
    RUN_TEST(test_out_of_range_access_is_recorded_and_harmless);
    RUN_TEST(test_layout_regions_fit_in_256_bytes_without_overlap);
    return UNITY_END();
}
