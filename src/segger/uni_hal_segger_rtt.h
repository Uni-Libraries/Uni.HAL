#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

//
// Includes
//

// stdlib
#include <stdbool.h>



//
// Public
//

void uni_hal_segger_rtt_init(void);

bool uni_hal_segger_rtt_is_inited(void);

#if defined(__cplusplus)
}
#endif
