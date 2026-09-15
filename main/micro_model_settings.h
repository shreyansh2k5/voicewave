/* Copyright 2023 The TensorFlow Authors. All Rights Reserved. */

#ifndef TENSORFLOW_LITE_MICRO_EXAMPLES_MICRO_SPEECH_MICRO_MODEL_SETTINGS_H_
#define TENSORFLOW_LITE_MICRO_EXAMPLES_MICRO_SPEECH_MICRO_MODEL_SETTINGS_H_

// Audio processing and feature generation parameters
constexpr int kMaxAudioSampleSize = 480;
constexpr int kAudioSampleFrequency = 16000;
constexpr int kFeatureSize = 40;
constexpr int kFeatureCount = 49; // EXACTLY 49 to match retrained model
constexpr int kFeatureElementCount = (kFeatureSize * kFeatureCount); // 1960
constexpr int kFeatureStrideMs = 20;
constexpr int kFeatureDurationMs = 30;

// Variables for the custom 2-class model
constexpr int kCategoryCount = 2;
constexpr const char* kCategoryLabels[kCategoryCount] = {
    "background",
    "hey_vikram"
};

#endif  // TENSORFLOW_LITE_MICRO_EXAMPLES_MICRO_SPEECH_MICRO_MODEL_SETTINGS_H_