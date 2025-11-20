#include <Arduino.h>
#include <unity.h>
#include "main.cpp"

void some_test_function(void) {
    Serial.println("Hi");
}

void setup() {
    UNITY_BEGIN();
    RUN_TEST(some_test_function);
    UNITY_END();
}
void loop() {

}