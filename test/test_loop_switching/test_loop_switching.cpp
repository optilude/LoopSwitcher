#include <unity.h>

#include "fake_hardware.h"
#include "loop_switching/loop_switching.h"

static FakeHardware hw;

void setUp() { hw = FakeHardware(); }
void tearDown() {}

static int popcount(uint8_t v) {
    int n = 0;
    for (; v; v >>= 1) n += v & 1;
    return n;
}

void test_unchanged_state_causes_no_activity() {
    LoopSwitching loops(hw, hw);
    TEST_ASSERT_TRUE(loops.apply(0x00));
    TEST_ASSERT_EQUAL_UINT(0, hw.events.size());
}

void test_engaging_a_loop_pulses_set_then_releases() {
    LoopSwitching loops(hw, hw);
    TEST_ASSERT_TRUE(loops.apply(0b00000100));

    TEST_ASSERT_EQUAL_UINT(3, hw.events.size());
    TEST_ASSERT_EQUAL(HwEvent::Energize, hw.events[0].type);
    TEST_ASSERT_EQUAL_HEX8(0b00000100, hw.events[0].drive.setMask);
    TEST_ASSERT_EQUAL_HEX8(0, hw.events[0].drive.rstMask);
    TEST_ASSERT_EQUAL(HwEvent::Delay, hw.events[1].type);
    TEST_ASSERT_EQUAL_UINT32(kRelayPulseMs, hw.events[1].ms);
    TEST_ASSERT_EQUAL(HwEvent::Release, hw.events[2].type);
    TEST_ASSERT_EQUAL_HEX8(0b00000100, loops.stateMask());
}

void test_bypassing_a_loop_pulses_rst() {
    LoopSwitching loops(hw, hw);
    loops.apply(0b00000001);
    hw.events.clear();

    TEST_ASSERT_TRUE(loops.apply(0x00));
    TEST_ASSERT_EQUAL_HEX8(0, hw.events[0].drive.setMask);
    TEST_ASSERT_EQUAL_HEX8(0b00000001, hw.events[0].drive.rstMask);
    TEST_ASSERT_EQUAL_HEX8(0, loops.stateMask());
}

void test_mixed_change_in_one_group_uses_set_and_rst_on_different_loops() {
    LoopSwitching loops(hw, hw);
    loops.apply(0b00000011);
    hw.events.clear();

    TEST_ASSERT_TRUE(loops.apply(0b00000100));
    TEST_ASSERT_EQUAL_UINT(3, hw.events.size());
    TEST_ASSERT_EQUAL_HEX8(0b00000100, hw.events[0].drive.setMask);
    TEST_ASSERT_EQUAL_HEX8(0b00000011, hw.events[0].drive.rstMask);
}

void test_large_change_is_split_into_sequential_groups() {
    LoopSwitching loops(hw, hw);
    TEST_ASSERT_TRUE(loops.apply(0xFF));

    // group 1, gap, group 2
    TEST_ASSERT_EQUAL_UINT(7, hw.events.size());
    TEST_ASSERT_EQUAL(HwEvent::Energize, hw.events[0].type);
    TEST_ASSERT_EQUAL_HEX8(0x0F, hw.events[0].drive.setMask);
    TEST_ASSERT_EQUAL(HwEvent::Delay, hw.events[1].type);
    TEST_ASSERT_EQUAL(HwEvent::Release, hw.events[2].type);
    TEST_ASSERT_EQUAL(HwEvent::Delay, hw.events[3].type);
    TEST_ASSERT_EQUAL_UINT32(kRelayGroupGapMs, hw.events[3].ms);
    TEST_ASSERT_EQUAL(HwEvent::Energize, hw.events[4].type);
    TEST_ASSERT_EQUAL_HEX8(0xF0, hw.events[4].drive.setMask);
    TEST_ASSERT_EQUAL_HEX8(0xFF, loops.stateMask());
}

void test_invariants_hold_for_every_transition() {
    for (int current = 0; current < 256; ++current) {
        for (int target = 0; target < 256; ++target) {
            hw = FakeHardware();
            LoopSwitching loops(hw, hw);
            loops.apply(static_cast<uint8_t>(current));
            hw.events.clear();

            TEST_ASSERT_TRUE(loops.apply(static_cast<uint8_t>(target)));
            TEST_ASSERT_EQUAL_HEX8(target, loops.stateMask());

            bool energized = false;
            for (const HwEvent& e : hw.events) {
                if (e.type == HwEvent::Energize) {
                    TEST_ASSERT_FALSE_MESSAGE(energized, "energize twice without release");
                    energized = true;
                    TEST_ASSERT_EQUAL_HEX8(0, e.drive.setMask & e.drive.rstMask);
                    TEST_ASSERT_TRUE(popcount(e.drive.setMask | e.drive.rstMask) <=
                                     kMaxSimultaneousRelays);
                } else if (e.type == HwEvent::Release) {
                    energized = false;
                }
            }
            TEST_ASSERT_FALSE_MESSAGE(energized, "coils left energized");
        }
    }
}

void test_energize_failure_stops_releases_and_keeps_earlier_state() {
    hw.failEnergizeOnCall = 2;
    LoopSwitching loops(hw, hw);

    TEST_ASSERT_FALSE(loops.apply(0xFF));
    TEST_ASSERT_EQUAL_HEX8(0x0F, loops.stateMask());  // only the first group completed
    TEST_ASSERT_EQUAL(HwEvent::Release, hw.events.back().type);
    TEST_ASSERT_EQUAL_INT(2, hw.releaseCalls);  // normal release + best-effort after failure
}

void test_release_failure_leaves_that_groups_state_unchanged() {
    hw.failReleaseOnCall = 1;
    LoopSwitching loops(hw, hw);

    TEST_ASSERT_FALSE(loops.apply(0b00000001));
    TEST_ASSERT_EQUAL_HEX8(0, loops.stateMask());
}

void test_begin_pulses_all_eight_relays_even_when_all_bypassed() {
    LoopSwitching loops(hw, hw);
    TEST_ASSERT_TRUE(loops.begin(0x00));

    uint8_t rstSeen = 0;
    for (const HwEvent& e : hw.events) {
        if (e.type == HwEvent::Energize) {
            TEST_ASSERT_EQUAL_HEX8(0, e.drive.setMask);
            rstSeen |= e.drive.rstMask;
        }
    }
    TEST_ASSERT_EQUAL_HEX8(0xFF, rstSeen);
    TEST_ASSERT_EQUAL_HEX8(0x00, loops.stateMask());
}

void test_begin_drives_each_relay_to_its_initial_state() {
    LoopSwitching loops(hw, hw);
    TEST_ASSERT_TRUE(loops.begin(0b10100101));

    uint8_t setSeen = 0;
    uint8_t rstSeen = 0;
    for (const HwEvent& e : hw.events) {
        if (e.type == HwEvent::Energize) {
            setSeen |= e.drive.setMask;
            rstSeen |= e.drive.rstMask;
        }
    }
    TEST_ASSERT_EQUAL_HEX8(0b10100101, setSeen);
    TEST_ASSERT_EQUAL_HEX8(0b01011010, rstSeen);
    TEST_ASSERT_EQUAL_HEX8(0b10100101, loops.stateMask());
}

void test_begin_failure_leaves_tracked_state_unchanged() {
    hw.failEnergizeOnCall = 2;
    LoopSwitching loops(hw, hw);

    TEST_ASSERT_FALSE(loops.begin(0xFF));
    TEST_ASSERT_EQUAL_HEX8(0x00, loops.stateMask());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_begin_pulses_all_eight_relays_even_when_all_bypassed);
    RUN_TEST(test_begin_drives_each_relay_to_its_initial_state);
    RUN_TEST(test_begin_failure_leaves_tracked_state_unchanged);
    RUN_TEST(test_unchanged_state_causes_no_activity);
    RUN_TEST(test_engaging_a_loop_pulses_set_then_releases);
    RUN_TEST(test_bypassing_a_loop_pulses_rst);
    RUN_TEST(test_mixed_change_in_one_group_uses_set_and_rst_on_different_loops);
    RUN_TEST(test_large_change_is_split_into_sequential_groups);
    RUN_TEST(test_invariants_hold_for_every_transition);
    RUN_TEST(test_energize_failure_stops_releases_and_keeps_earlier_state);
    RUN_TEST(test_release_failure_leaves_that_groups_state_unchanged);
    return UNITY_END();
}
