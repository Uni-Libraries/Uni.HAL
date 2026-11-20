#pragma once

#include "adc/uni_hal_adc_typedef.h"

// A full-scale 16-bit sample and an unavailable channel have the same public raw value.
// Return validity alongside the sample so that a DMA fault cannot turn into a voltage.
bool uni_hal_adc_find_sample(const uni_hal_adc_context_t *ctx, uint32_t channel, uint16_t *sample);
