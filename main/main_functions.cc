#include "esp_heap_caps.h"
#include <algorithm>
#include <cstdint>
#include <cstring>

#include "esp_timer.h"
#include "esp_log.h"
#include "main_functions.h"

#include "audio_pipeline_buffer.h"
#include "audio_provider.h"
#include "cloud_streaming_task.h"
#include "feature_provider.h"
#include "micro_model_settings.h"
#include "model.h"
#include "wake_word_logic.h"

#include "tensorflow/lite/micro/system_setup.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/core/c/common.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_log.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"

static const char* TAG = "BENCHMARK";
extern TfLiteStatus InitAudioRecording();

namespace {
const tflite::Model* model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor* model_input = nullptr;
FeatureProvider* feature_provider = nullptr;
int32_t previous_time = 0;

constexpr int kTensorArenaSize = 140 * 1024;
uint8_t tensor_arena[kTensorArenaSize];
int8_t feature_buffer[kFeatureElementCount];
int8_t* model_input_buffer = nullptr;
}  // namespace

void setup() {
  printf("\n=== MEMORY PROFILING ===\n");
  printf("Free System RAM at startup: %lu bytes\n", 
         (unsigned long)heap_caps_get_free_size(MALLOC_CAP_8BIT));

  audio_pipeline_init();
  InitAudioRecording();
  init_wake_word_logic();
  start_cloud_streaming_task();

  model = tflite::GetModel(g_model);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    MicroPrintf("Model provided is schema version %d not equal to supported version %d.",
                model->version(), TFLITE_SCHEMA_VERSION);
    while (1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
  }

  // 12 slots to accommodate DS-CNN, BatchNormalization, and INT8 quant ops
  static tflite::MicroMutableOpResolver<12> micro_op_resolver;
  micro_op_resolver.AddConv2D();
  micro_op_resolver.AddDepthwiseConv2D();
  micro_op_resolver.AddAveragePool2D();
  micro_op_resolver.AddMean();
  micro_op_resolver.AddFullyConnected();
  micro_op_resolver.AddSoftmax();
  micro_op_resolver.AddReshape();
  micro_op_resolver.AddMul();
  micro_op_resolver.AddAdd();
  micro_op_resolver.AddSub();
  micro_op_resolver.AddQuantize();
  micro_op_resolver.AddDequantize();

  static tflite::MicroInterpreter static_interpreter(
      model, micro_op_resolver, tensor_arena, kTensorArenaSize);
  interpreter = &static_interpreter;

  if (interpreter->AllocateTensors() != kTfLiteOk) {
    MicroPrintf("AllocateTensors() failed! Check arena size or unsupported ops.");
    while (1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
  }

  ESP_LOGI(TAG, "=== TENSOR ARENA ALLOCATION ===");
  ESP_LOGI(TAG, "Arena Used: %zu / %d bytes",
           interpreter->arena_used_bytes(),
           kTensorArenaSize);

  model_input = interpreter->input(0);
  model_input_buffer = tflite::GetTensorData<int8_t>(model_input);

  static FeatureProvider static_feature_provider(kFeatureElementCount, feature_buffer);
  feature_provider = &static_feature_provider;

  previous_time = LatestAudioTimestamp();
}

void loop() {
  const int32_t current_time = LatestAudioTimestamp();

  int how_many_new_slices = 0;
  TfLiteStatus feature_status = feature_provider->PopulateFeatureData(
      previous_time, current_time, &how_many_new_slices);
  if (feature_status != kTfLiteOk) {
    MicroPrintf("Feature generation failed");
    return;
  }
  previous_time = current_time;

  if (how_many_new_slices == 0) {
    return;
  }

  // Transpose frame-major feature_buffer [49 frames][40 bins]
  // into bin-major model_input_buffer [40 bins][49 frames]
  for (int frame = 0; frame < kFeatureCount; frame++) {
    for (int bin = 0; bin < kFeatureSize; bin++) {
      model_input_buffer[bin * kFeatureCount + frame] =
          feature_buffer[frame * kFeatureSize + bin];
    }
  }

  // -----------------------------------------------------------------
  // INFERENCE & LATENCY MEASUREMENT
  // -----------------------------------------------------------------
  int64_t invoke_start = esp_timer_get_time();
  
  if (interpreter->Invoke() != kTfLiteOk) {
    MicroPrintf("Invoke failed");
    return;
  }
  
  int64_t invoke_us = esp_timer_get_time() - invoke_start;
  MicroPrintf("[LATENCY] Invoke: %lld us", invoke_us);
  // -----------------------------------------------------------------

  // Live score monitor
  TfLiteTensor* raw_output = interpreter->output(0);
  int8_t raw0 = raw_output->data.int8[0];
  int8_t raw1 = raw_output->data.int8[1];
  float raw_scale = raw_output->params.scale;
  int raw_zp = raw_output->params.zero_point;
  float p0 = (raw0 - raw_zp) * raw_scale;
  float p1 = (raw1 - raw_zp) * raw_scale;

  static int log_counter = 0;
  if (++log_counter % 5 == 0) {
    MicroPrintf("[RAW] background=%d%% hey_vikram=%d%%", (int)(p0 * 100), (int)(p1 * 100));
  }

  process_wake_word(interpreter->output(0), current_time);
}