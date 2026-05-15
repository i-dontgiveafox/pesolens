// pesos firmware — unit tests for speech.cpp (English number-to-words)
// Run with: pio test -e native
//
// These are pure-host tests — speech.cpp has no Arduino dependencies, so
// PlatformIO's `native` env compiles + runs them directly on the dev machine.

#include <unity.h>
#include <string.h>
#include <vector>
#include "../../src/speech.h"

void setUp(void) {}
void tearDown(void) {}

// Helper: assert the produced vector matches an expected list of clip IDs.
static void assert_clips(uint32_t pesos,
                         const std::vector<const char*>& expected) {
    std::vector<const char*> got = compose_english(pesos);
    TEST_ASSERT_EQUAL_size_t(expected.size(), got.size());
    for (size_t i = 0; i < expected.size(); i++) {
        TEST_ASSERT_EQUAL_STRING(expected[i], got[i]);
    }
}

void test_zero(void) {
    assert_clips(0, {"zero", "pesos"});
}

void test_one_uses_singular(void) {
    assert_clips(1, {"one", "peso"});
}

void test_single_digit(void) {
    assert_clips(5, {"five", "pesos"});
}

void test_teen(void) {
    assert_clips(12, {"twelve", "pesos"});
}

void test_compound_tens(void) {
    assert_clips(25, {"twenty", "five", "pesos"});
}

void test_round_hundred(void) {
    assert_clips(100, {"one", "hundred", "pesos"});
}

void test_five_hundred(void) {
    assert_clips(500, {"five", "hundred", "pesos"});
}

void test_full_under_1000(void) {
    assert_clips(521, {"five", "hundred", "twenty", "one", "pesos"});
}

void test_round_thousand(void) {
    assert_clips(1000, {"one", "thousand", "pesos"});
}

void test_thousand_with_hundred(void) {
    assert_clips(1500, {"one", "thousand", "five", "hundred", "pesos"});
}

void test_large_round(void) {
    assert_clips(2700, {"two", "thousand", "seven", "hundred", "pesos"});
}

void test_full_six_digit(void) {
    assert_clips(12345,
                 {"twelve", "thousand", "three", "hundred", "forty", "five",
                  "pesos"});
}

// Additional edge cases worth pinning down.
void test_exactly_ten(void) {
    assert_clips(10, {"ten", "pesos"});
}

void test_exactly_twenty(void) {
    assert_clips(20, {"twenty", "pesos"});
}

void test_thousand_with_teen(void) {
    assert_clips(1015, {"one", "thousand", "fifteen", "pesos"});
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_zero);
    RUN_TEST(test_one_uses_singular);
    RUN_TEST(test_single_digit);
    RUN_TEST(test_teen);
    RUN_TEST(test_compound_tens);
    RUN_TEST(test_round_hundred);
    RUN_TEST(test_five_hundred);
    RUN_TEST(test_full_under_1000);
    RUN_TEST(test_round_thousand);
    RUN_TEST(test_thousand_with_hundred);
    RUN_TEST(test_large_round);
    RUN_TEST(test_full_six_digit);
    RUN_TEST(test_exactly_ten);
    RUN_TEST(test_exactly_twenty);
    RUN_TEST(test_thousand_with_teen);
    return UNITY_END();
}
