#include <unity.h>

#include "loop_switching/relay_map.h"

void setUp() {}
void tearDown() {}

void test_coil_pin_matches_spec_table() {
    // loop (0-based): port, RST bit, SET bit
    const uint8_t expected[8][3] = {
        {0, 0, 1}, {0, 2, 3}, {0, 4, 5}, {0, 6, 7},
        {1, 0, 1}, {1, 2, 3}, {1, 4, 5}, {1, 6, 7},
    };
    for (uint8_t loop = 0; loop < 8; ++loop) {
        CoilPin pin = coilPin(loop);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(expected[loop][0], pin.port, "port");
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(expected[loop][1], pin.rstBit, "rst bit");
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(expected[loop][2], pin.setBit, "set bit");
    }
}

void test_newly_engaged_loops_go_to_set_only() {
    RelayDrive d = computeDrive(0b00000000, 0b00000101);
    TEST_ASSERT_EQUAL_HEX8(0b00000101, d.setMask);
    TEST_ASSERT_EQUAL_HEX8(0, d.rstMask);
}

void test_newly_bypassed_loops_go_to_rst_only() {
    RelayDrive d = computeDrive(0b11111111, 0b11111010);
    TEST_ASSERT_EQUAL_HEX8(0, d.setMask);
    TEST_ASSERT_EQUAL_HEX8(0b00000101, d.rstMask);
}

void test_unchanged_loops_are_in_neither_mask() {
    RelayDrive d = computeDrive(0b00001111, 0b00001111);
    TEST_ASSERT_EQUAL_HEX8(0, d.setMask);
    TEST_ASSERT_EQUAL_HEX8(0, d.rstMask);
}

void test_set_and_rst_never_overlap_for_any_transition() {
    for (int current = 0; current < 256; ++current) {
        for (int target = 0; target < 256; ++target) {
            RelayDrive d = computeDrive(current, target);
            TEST_ASSERT_EQUAL_HEX8(0, d.setMask & d.rstMask);
            TEST_ASSERT_EQUAL_HEX8(current ^ target, d.setMask | d.rstMask);
        }
    }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_coil_pin_matches_spec_table);
    RUN_TEST(test_newly_engaged_loops_go_to_set_only);
    RUN_TEST(test_newly_bypassed_loops_go_to_rst_only);
    RUN_TEST(test_unchanged_loops_are_in_neither_mask);
    RUN_TEST(test_set_and_rst_never_overlap_for_any_transition);
    return UNITY_END();
}
