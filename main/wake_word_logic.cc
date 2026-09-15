#include "wake_word_logic.h"
#include "recognize_commands.h"
#include "driver/gpio.h"
#include "tensorflow/lite/micro/micro_log.h"
#include "audio_pipeline_buffer.h"
#include <cstring>

#define GREEN_LED_PIN GPIO_NUM_48

static RecognizeCommands* recognizer = nullptr;
static int32_t green_led_turnoff_time = 0;

void init_wake_word_logic() {
    gpio_reset_pin(GREEN_LED_PIN);
    gpio_set_direction(GREEN_LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(GREEN_LED_PIN, 0);

    // 500ms window, 50% threshold, 1500ms suppression, min 2 frames
    static RecognizeCommands static_recognizer(600, 0.85, 1500, 1);
    recognizer = &static_recognizer;
}

void process_wake_word(TfLiteTensor* output, int32_t current_time) {
    if (current_time > green_led_turnoff_time) {
        gpio_set_level(GREEN_LED_PIN, 0);
    }

    const char* found_command = nullptr;
    float score = 0.0;
    bool is_new_command = false;
    TfLiteStatus process_status = recognizer->ProcessLatestResults(
        output, current_time, &found_command, &score, &is_new_command);
    if (process_status != kTfLiteOk) return;

    MicroPrintf("[CMD] found=%s score=%d%% is_new=%d",
                found_command ? found_command : "NULL",
                (int)(score * 100), is_new_command);

    if (is_new_command && strcmp(found_command, "hey_vikram") == 0) { 
        gpio_set_level(GREEN_LED_PIN, 1);
        green_led_turnoff_time = current_time + 2000;
        MicroPrintf(">>> WAKE WORD DETECTED: %s (score: %d%%) <<<", found_command, (int)(score * 100));
        signal_wake_word_detected();
    }
}