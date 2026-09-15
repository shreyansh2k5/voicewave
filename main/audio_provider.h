/* Copyright 2018 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#ifndef TENSORFLOW_LITE_MICRO_EXAMPLES_MICRO_SPEECH_AUDIO_PROVIDER_H_
#define TENSORFLOW_LITE_MICRO_EXAMPLES_MICRO_SPEECH_AUDIO_PROVIDER_H_

#include "tensorflow/lite/c/common.h"

// Abstraction around an audio source (microphone). Returns 16-bit PCM samples.
TfLiteStatus GetAudioSamples(int start_ms, int duration_ms,
                             int* audio_samples_size, int16_t** audio_samples);

// Time that audio data was last captured, in milliseconds.
int32_t LatestAudioTimestamp();

#endif  // TENSORFLOW_LITE_MICRO_EXAMPLES_MICRO_SPEECH_AUDIO_PROVIDER_H_