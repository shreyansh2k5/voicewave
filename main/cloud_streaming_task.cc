#include "cloud_streaming_task.h"
#include "audio_pipeline_buffer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char* TAG = "CLOUD_STREAM";

#define STREAM_CHUNK_BUFFER_BYTES 2048

// TODO(you): replace this with real WiFi/TLS transmission.
static void stream_audio_chunk(const uint8_t* data, size_t size_bytes) {
  ESP_LOGI(TAG, "stream_audio_chunk: %u bytes (implement WiFi/TLS send here)",
           (unsigned)size_bytes);
}

static void drain_ring_buffer_to_stream(void) {
  uint8_t chunk[STREAM_CHUNK_BUFFER_BYTES];
  size_t bytes_read = 0;

  while (audio_ringbuf_read(chunk, sizeof(chunk), &bytes_read, 0)) {
    if (bytes_read > 0) {
      stream_audio_chunk(chunk, bytes_read);
    }
  }

  const int32_t kPostWakeWindowMs = 2000;
  const TickType_t kReadTimeout = pdMS_TO_TICKS(100);
  const int64_t window_start_us = esp_timer_get_time();
  while ((esp_timer_get_time() - window_start_us) < (kPostWakeWindowMs * 1000)) {
    if (audio_ringbuf_read(chunk, sizeof(chunk), &bytes_read, kReadTimeout)) {
      if (bytes_read > 0) {
        stream_audio_chunk(chunk, bytes_read);
      }
    }
  }

  ESP_LOGI(TAG, "stream complete (dropped chunks so far: %u)",
           (unsigned)audio_ringbuf_drop_count());
}

static void cloud_streaming_task(void* arg) {
  while (true) {
    EventBits_t bits = xEventGroupWaitBits(
        g_audio_event_group,
        WAKE_WORD_DETECTED_BIT,
        pdTRUE,
        pdFALSE,
        portMAX_DELAY);

    if (bits & WAKE_WORD_DETECTED_BIT) {
      ESP_LOGI(TAG, "wake word detected -> draining pre-roll buffer");
      drain_ring_buffer_to_stream();
    }
  }
}

void start_cloud_streaming_task(void) {
  xTaskCreatePinnedToCore(cloud_streaming_task, "cloud_stream", 4096, NULL,
                           5, NULL, 0);
}