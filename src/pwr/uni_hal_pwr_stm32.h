#pragma once

#if defined(__cplusplus)
extern "C" {
#endif



//
// Public
//

/**
 * Set access to the backup registers
 * @param ctx PWR context
 * @return true on success
 */
void uni_hal_pwr_stm_set_backup_access(bool val);

#if defined(__cplusplus)
}
#endif
