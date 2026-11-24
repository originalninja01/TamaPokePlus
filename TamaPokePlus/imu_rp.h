#pragma once
#include <Arduino.h>

// Minimal QMI8658 accelerometer reader for the step counter and shake
// detection (TamaPoke Plus). Polled from the main loop on the shared I2C bus;
// no library, no interrupts, no gyro.
bool imuBegin();                     // true if the sensor answered
bool imuOk();
void imuPoll(uint32_t nowMs);        // call every loop pass (self-limits to ~25 Hz)
uint16_t imuTakeSteps();             // steps counted since the last call
bool imuShakeEdge();                 // one-shot shake event
float imuLastG();                    // last acceleration magnitude in g (for HEALTH)
