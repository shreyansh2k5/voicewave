#ifndef AUDIO_PIPELINE_BUFFER_H_
#define AUDIO_PIPELINE_BUFFER_H_

#include <cstdint>
#include <cstddef>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/ringbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WAKE_WORD_DETECTED_BIT     (1 << 0)
#define PRE_ROLL_BUFFER_SIZE_BYTES (48 * 1024)

extern EventGroupHandle_t g_audio_event_group;

void audio_pipeline_init(void);
void audio_ringbuf_write(const uint8_t* data, size_t size_bytes);
bool audio_ringbuf_read(uint8_t* out_buffer, size_t max_bytes,
                        size_t* actual_bytes_read, TickType_t wait_ticks);
void signal_wake_word_detected(void);
uint32_t audio_ringbuf_drop_count(void);

#ifdef __cplusplus
}
#endif

#endif  // AUDIO_PIPELINE_BUFFER_H_