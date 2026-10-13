// pesos firmware - weak model data fallback.
// A generated model_data.cpp provides strong symbols when present.

#include "model_data.h"

#if defined(__GNUC__)
#define MODEL_DATA_WEAK __attribute__((weak))
#else
#define MODEL_DATA_WEAK
#endif

extern "C" {
extern MODEL_DATA_WEAK const unsigned char tflite_model[] = { 0 };
extern MODEL_DATA_WEAK const unsigned int tflite_model_len = 0;
}
