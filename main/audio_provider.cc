#include "audio_provider.h"
#include "i2s_setup.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>
#include <cstdlib>

#define RED_LED_PIN GPIO_NUM_47
#define HISTORY_SIZE 16000 // Exact 1-second history buffer

static Microphone* mic = nullptr;
static int16_t history_buffer[HISTORY_SIZE];
static volatile int32_t latest_audio_timestamp = 0;
static volatile int32_t total_samples_written = 0;
int16_t g_audio_output_buffer[512];

void i2s_capture_task(void* arg) {
    size_t bytes_read = 0;
    int32_t i2s_buffer[800]; 
    
    while (true) {
        mic->i2s_readsamples(i2s_buffer, &bytes_read);
        if (bytes_read > 0) {
            int num_samples = bytes_read / sizeof(int32_t);
            int32_t energy = 0;
            
            for(int i = 0; i < num_samples; i++) {
                // Convert 32-bit to 16-bit and apply 8x software gain
                int32_t val = (i2s_buffer[i] >> 16);
                if (val > 32767) val = 32767;
                if (val < -32768) val = -32768;
                
                // Write directly to our circular history buffer
                int write_index = total_samples_written % HISTORY_SIZE;
                history_buffer[write_index] = (int16_t)val;
                total_samples_written = total_samples_written + 1;
                energy += abs(val);
            }
            
            // Advance the global system timestamp natively via sample count
            latest_audio_timestamp = (total_samples_written * 1000) / 16000;
            
            // Voice Activity Detection -> Red LED
            if ((energy / num_samples) > 800) { 
                gpio_set_level(RED_LED_PIN, 1);
            } else {
                gpio_set_level(RED_LED_PIN, 0);
            }
        }
    }
}

TfLiteStatus InitAudioRecording() {
    gpio_reset_pin(RED_LED_PIN);
    gpio_set_direction(RED_LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(RED_LED_PIN, 0);
    
    memset(history_buffer, 0, sizeof(history_buffer));

    mic = new Microphone(GPIO_NUM_36, GPIO_NUM_37, GPIO_NUM_38);
    mic->i2s_setup();
    
    xTaskCreatePinnedToCore(i2s_capture_task, "i2s_capture", 8192, NULL, 10, NULL, 0);
    return kTfLiteOk;
}

TfLiteStatus GetAudioSamples(int start_ms, int duration_ms,
                             int* audio_samples_size, int16_t** audio_samples) {
    
    int start_sample = (start_ms * 16000) / 1000;
    int num_samples = (duration_ms * 16000) / 1000;
    
    // Safety check to prevent buffer overflows
    if (num_samples > 512) {
        num_samples = 512;
    }
    
    // Block until the circular buffer has reached this timestamp
    while (total_samples_written < start_sample + num_samples) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    // Extract the exact requested window (whether 480, 512, or anything else)
    for (int i = 0; i < num_samples; i++) {
        int read_index = (start_sample + i) % HISTORY_SIZE;
        g_audio_output_buffer[i] = history_buffer[read_index];
    }
    
    *audio_samples_size = num_samples;
    *audio_samples = g_audio_output_buffer;
    return kTfLiteOk;
}

int32_t LatestAudioTimestamp() {
    return latest_audio_timestamp;
}