#ifndef DESKMATE_VOICE_INPUT_H
#define DESKMATE_VOICE_INPUT_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "voice_contracts.h"

void vc_input_begin(QueueHandle_t event_queue);
void vc_input_post_pmic_short();
bool vc_input_queue_healthy();

#endif
