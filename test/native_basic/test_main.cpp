#include <unity.h>

void test_addition(void) {
    TEST_ASSERT_EQUAL(2, 1 + 1);
}

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_addition);
    UNITY_END();
}

void loop() {}
