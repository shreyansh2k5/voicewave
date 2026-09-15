#ifndef WAKE_WORD_LOGIC_H_
#define WAKE_WORD_LOGIC_H_

#include "tensorflow/lite/c/common.h"
#include <cstdint>

void init_wake_word_logic();
void process_wake_word(TfLiteTensor* output, int32_t current_time);

#endif // WAKE_WORD_LOGIC_H_