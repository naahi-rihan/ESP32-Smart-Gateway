#pragma once
#include <Arduino.h>

extern bool isAPMode;

bool setupWiFiProvisioning();   // true = STA connected, false = AP portal
void loopWiFiProvisioning();
void reconfigButton();