#pragma once

#include <Arduino.h>

#define ZO_LOGI(fmt, ...) Serial.printf("[I] " fmt "\n", ##__VA_ARGS__)
#define ZO_LOGW(fmt, ...) Serial.printf("[W] " fmt "\n", ##__VA_ARGS__)
#define ZO_LOGE(fmt, ...) Serial.printf("[E] " fmt "\n", ##__VA_ARGS__)

