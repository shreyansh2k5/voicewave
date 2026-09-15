#ifndef CLOUD_STREAMING_TASK_H_
#define CLOUD_STREAMING_TASK_H_

// Starts the background task that waits for a wake-word detection, then
// drains the 1.5s pre-roll ring buffer and hands each chunk to
// stream_audio_chunk() for actual transmission (WiFi/TLS to be added later).
//
// Call this once from setup(), after audio_pipeline_init() has already
// created g_audio_event_group and the ring buffer.
void start_cloud_streaming_task(void);

#endif  // CLOUD_STREAMING_TASK_H_