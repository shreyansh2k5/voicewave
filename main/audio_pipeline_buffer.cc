#include "audio_pipeline_buffer.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <cstring>

static const char* TAG = "AUDIO_PIPELINE";

static RingbufHandle_t s_audio_ringbuf = nullptr;
EventGroupHandle_t g_audio_event_group = nullptr;
static uint32_t s_drop_count = 0;

void audio_pipeline_init(void) {
  ESP_LOGI(TAG, "internal heap free: %u",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

  g_audio_event_group = xEventGroupCreate();
  if (g_audio_event_group == nullptr) {
    ESP_LOGE(TAG, "FATAL: event group allocation failed");
  }

  s_audio_ringbuf = xRingbufferCreate(PRE_ROLL_BUFFER_SIZE_BYTES, RINGBUF_TYPE_BYTEBUF);
  if (s_audio_ringbuf == nullptr) {
    ESP_LOGE(TAG, "FATAL: %u-byte ring buffer alloc failed",
             (unsigned)PRE_ROLL_BUFFER_SIZE_BYTES);
  } else {
    ESP_LOGI(TAG, "pre-roll ring buffer OK (%u B); internal heap free now: %u",
             (unsigned)PRE_ROLL_BUFFER_SIZE_BYTES,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  }
}

void audio_ringbuf_write(const uint8_t* data, size_t size_bytes) {
  if (s_audio_ringbuf == nullptr || data == nullptr || size_bytes == 0) return;

  BaseType_t ok = xRingbufferSend(s_audio_ringbuf, data, size_bytes, 0);
  if (ok == pdTRUE) return;                    // fast path: space available

  // Slow path: full -> drop OLDEST data until the new chunk fits (max 8 tries)
  for (int i = 0; i < 8 && ok != pdTRUE; ++i) {
    size_t dropped_size = 0;
    uint8_t* old = (uint8_t*)xRingbufferReceiveUpTo(s_audio_ringbuf, &dropped_size,
                                                    0, size_bytes);
    if (old == nullptr) break;
    vRingbufferReturnItem(s_audio_ringbuf, (void*)old);   // discard oldest
    ++s_drop_count;
    ok = xRingbufferSend(s_audio_ringbuf, data, size_bytes, 0);
  }

  if (ok != pdTRUE) {
    // Chunk LOST entirely (not a routine tail-drop). Rare => OK to be loud.
    ESP_LOGE(TAG, "chunk of %u bytes dropped entirely - consumer stalled?",
             (unsigned)size_bytes);
  }
}

bool audio_ringbuf_read(uint8_t* out_buffer, size_t max_bytes,
                        size_t* actual_bytes_read, TickType_t wait_ticks) {
  if (s_audio_ringbuf == nullptr || out_buffer == nullptr || actual_bytes_read == nullptr) {
    return false;
  }
  size_t item_size = 0;
  uint8_t* item = (uint8_t*)xRingbufferReceiveUpTo(s_audio_ringbuf, &item_size,
                                                   wait_ticks, max_bytes);
  if (item != nullptr) {
    memcpy(out_buffer, item, item_size);
    *actual_bytes_read = item_size;
    vRingbufferReturnItem(s_audio_ringbuf, (void*)item);
    return true;
  }
  *actual_bytes_read = 0;
  return false;
}

void signal_wake_word_detected(void) {
  if (g_audio_event_group != nullptr) {
    xEventGroupSetBits(g_audio_event_group, WAKE_WORD_DETECTED_BIT);
    ESP_LOGI(TAG, ">>> WAKE_WORD_DETECTED_BIT set <<<");
  }
}

uint32_t audio_ringbuf_drop_count(void) { return s_drop_count; }