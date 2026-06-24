#pragma once
#include <Arduino.h>

void setupMQTT();
void loopMQTT();

// Publish online/offline availability status
void publishStatus(const char* availability);

// Publish standard sensor telemetry (called on interval timer)
void publishTelemetry(float temp, float humid, float nh3, float co2,
                      float h2s, float pm25, float pm10, float thi);

// Publish custom parameter payload to cfg_cust_topic
// data_int and test_par come from cfg_cust_* globals (set by web portal / NVS)
void publishCustomParams();