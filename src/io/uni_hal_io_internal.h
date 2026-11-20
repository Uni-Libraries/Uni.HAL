#pragma once

#include "uni_hal_io.h"

// With require_full, enqueue the entire block or return zero without queuing any bytes.
size_t uni_hal_io_transmit_internal(uni_hal_io_context_t *ctx, const uint8_t *data, size_t data_len, bool require_full);
