#include <unity.h>

#include "fake_i2c_bus.h"
#include "loop_switching/clock.h"
#include "loop_switching/loop_switching.h"
#include "loop_switching/mcp23017_relay_port.h"

void setUp() {}
void tearDown() {}

constexpr uint8_t kU3 = 0x21;
constexpr uint8_t kOlatA = 0x14;
constexpr uint8_t kIodirA = 0x00;

static void assertTransfer(const FakeI2cBus& bus, size_t index, uint8_t reg, uint8_t a, uint8_t b) {
    TEST_ASSERT_TRUE(index < bus.transfers.size());
    const Transfer& t = bus.transfers[index];
    TEST_ASSERT_EQUAL_HEX8(kU3, t.address);
    TEST_ASSERT_EQUAL_UINT(3, t.bytes.size());
    TEST_ASSERT_EQUAL_HEX8(reg, t.bytes[0]);
    TEST_ASSERT_EQUAL_HEX8(a, t.bytes[1]);
    TEST_ASSERT_EQUAL_HEX8(b, t.bytes[2]);
}

void test_begin_clears_the_latches_before_making_the_pins_outputs() {
    FakeI2cBus bus;
    Mcp23017RelayPort port(bus);
    TEST_ASSERT_TRUE(port.begin());

    TEST_ASSERT_EQUAL_UINT(2, bus.transfers.size());
    assertTransfer(bus, 0, kOlatA, 0x00, 0x00);   // latches low first
    assertTransfer(bus, 1, kIodirA, 0x00, 0x00);  // then all pins are outputs
}

void test_begin_stops_when_clearing_the_latches_fails() {
    FakeI2cBus bus;
    bus.failOnCall = 1;
    Mcp23017RelayPort port(bus);
    TEST_ASSERT_FALSE(port.begin());
    TEST_ASSERT_EQUAL_UINT(1, bus.transfers.size());  // the pins were never made outputs
}

void test_begin_reports_failure_when_the_direction_write_fails() {
    FakeI2cBus bus;
    bus.failOnCall = 2;
    Mcp23017RelayPort port(bus);
    TEST_ASSERT_FALSE(port.begin());
}

void test_every_coil_maps_to_the_pin_in_the_spec_table() {
    // loop (0-based): port, RST bit, SET bit; written out independently of coilPin()
    const uint8_t table[8][3] = {{0, 0, 1}, {0, 2, 3}, {0, 4, 5}, {0, 6, 7},
                                 {1, 0, 1}, {1, 2, 3}, {1, 4, 5}, {1, 6, 7}};
    for (uint8_t loop = 0; loop < 8; ++loop) {
        for (int coil = 0; coil < 2; ++coil) {
            FakeI2cBus bus;
            Mcp23017RelayPort port(bus);
            const bool set = coil == 1;
            const RelayDrive drive = set ? RelayDrive{static_cast<uint8_t>(1u << loop), 0}
                                         : RelayDrive{0, static_cast<uint8_t>(1u << loop)};
            TEST_ASSERT_TRUE(port.energize(drive));

            const uint8_t bit = static_cast<uint8_t>(1u << (set ? table[loop][2] : table[loop][1]));
            const uint8_t a = table[loop][0] == 0 ? bit : 0;
            const uint8_t b = table[loop][0] == 1 ? bit : 0;
            TEST_ASSERT_EQUAL_UINT(1, bus.transfers.size());
            assertTransfer(bus, 0, kOlatA, a, b);
        }
    }
}

void test_loop_one_set_is_gpa1_and_reset_is_gpa0() {
    FakeI2cBus bus;
    Mcp23017RelayPort port(bus);
    port.energize({0b1, 0});
    port.energize({0, 0b1});
    assertTransfer(bus, 0, kOlatA, 0b10, 0x00);  // SET = GPA1 engages loop 1
    assertTransfer(bus, 1, kOlatA, 0b01, 0x00);  // RST = GPA0 bypasses it
}

void test_several_loops_are_written_in_one_transfer() {
    FakeI2cBus bus;
    Mcp23017RelayPort port(bus);
    // SET on loops 1 and 3, RST on loops 2 and 5
    TEST_ASSERT_TRUE(port.energize({0b00000101, 0b00010010}));

    TEST_ASSERT_EQUAL_UINT(1, bus.transfers.size());
    // A: loop1 SET bit1 + loop3 SET bit5 + loop2 RST bit2 = 0x26;  B: loop5 RST bit0
    assertTransfer(bus, 0, kOlatA, 0x26, 0x01);
}

void test_set_and_reset_of_one_relay_are_refused_and_nothing_is_written() {
    FakeI2cBus bus;
    Mcp23017RelayPort port(bus);
    TEST_ASSERT_FALSE(port.energize({0b00000100, 0b00000100}));
    TEST_ASSERT_EQUAL_UINT(0, bus.transfers.size());
}

void test_an_empty_drive_writes_nothing() {
    FakeI2cBus bus;
    Mcp23017RelayPort port(bus);
    TEST_ASSERT_TRUE(port.energize({0, 0}));
    TEST_ASSERT_EQUAL_UINT(0, bus.transfers.size());
}

void test_release_clears_both_latches() {
    FakeI2cBus bus;
    Mcp23017RelayPort port(bus);
    TEST_ASSERT_TRUE(port.releaseAll());
    assertTransfer(bus, 0, kOlatA, 0x00, 0x00);
}

void test_bus_failures_are_reported() {
    FakeI2cBus bus;
    bus.failOnCall = 1;
    Mcp23017RelayPort port(bus);
    TEST_ASSERT_FALSE(port.energize({0b1, 0}));

    FakeI2cBus bus2;
    bus2.failOnCall = 1;
    Mcp23017RelayPort port2(bus2);
    TEST_ASSERT_FALSE(port2.releaseAll());
}

void test_a_different_address_can_be_given() {
    FakeI2cBus bus;
    Mcp23017RelayPort port(bus, 0x27);
    port.releaseAll();
    TEST_ASSERT_EQUAL_HEX8(0x27, bus.transfers[0].address);
}

class NoDelay : public Clock {
public:
    uint32_t total = 0;
    void delayMs(uint32_t ms) override { total += ms; }
};

void test_switching_all_loops_on_pulses_two_groups_of_four_coils() {
    FakeI2cBus bus;
    NoDelay clock;
    Mcp23017RelayPort port(bus);
    LoopSwitching loops(port, clock);

    TEST_ASSERT_TRUE(loops.apply(0xFF));
    TEST_ASSERT_EQUAL_UINT(4, bus.transfers.size());
    assertTransfer(bus, 0, kOlatA, 0xAA, 0x00);  // loops 1-4 SET (GPA1, 3, 5, 7)
    assertTransfer(bus, 1, kOlatA, 0x00, 0x00);  // released
    assertTransfer(bus, 2, kOlatA, 0x00, 0xAA);  // loops 5-8 SET (GPB1, 3, 5, 7)
    assertTransfer(bus, 3, kOlatA, 0x00, 0x00);
    TEST_ASSERT_EQUAL_UINT32(kRelayPulseMs * 2 + kRelayGroupGapMs, clock.total);
}

void test_boot_pulses_reset_on_all_eight_relays() {
    FakeI2cBus bus;
    NoDelay clock;
    Mcp23017RelayPort port(bus);
    LoopSwitching loops(port, clock);

    TEST_ASSERT_TRUE(port.begin());
    bus.transfers.clear();
    TEST_ASSERT_TRUE(loops.begin(0x00));
    assertTransfer(bus, 0, kOlatA, 0x55, 0x00);  // loops 1-4 RST (GPA0, 2, 4, 6)
    assertTransfer(bus, 2, kOlatA, 0x00, 0x55);  // loops 5-8 RST
}

void test_a_bus_failure_during_switching_is_reported_by_the_loop_layer() {
    FakeI2cBus bus;
    bus.failOnCall = 3;  // the second group's energize write
    NoDelay clock;
    Mcp23017RelayPort port(bus);
    LoopSwitching loops(port, clock);

    TEST_ASSERT_FALSE(loops.apply(0xFF));
    TEST_ASSERT_EQUAL_HEX8(0x0F, loops.stateMask());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_begin_clears_the_latches_before_making_the_pins_outputs);
    RUN_TEST(test_begin_stops_when_clearing_the_latches_fails);
    RUN_TEST(test_begin_reports_failure_when_the_direction_write_fails);
    RUN_TEST(test_every_coil_maps_to_the_pin_in_the_spec_table);
    RUN_TEST(test_loop_one_set_is_gpa1_and_reset_is_gpa0);
    RUN_TEST(test_several_loops_are_written_in_one_transfer);
    RUN_TEST(test_set_and_reset_of_one_relay_are_refused_and_nothing_is_written);
    RUN_TEST(test_an_empty_drive_writes_nothing);
    RUN_TEST(test_release_clears_both_latches);
    RUN_TEST(test_bus_failures_are_reported);
    RUN_TEST(test_a_different_address_can_be_given);
    RUN_TEST(test_switching_all_loops_on_pulses_two_groups_of_four_coils);
    RUN_TEST(test_boot_pulses_reset_on_all_eight_relays);
    RUN_TEST(test_a_bus_failure_during_switching_is_reported_by_the_loop_layer);
    return UNITY_END();
}
