//
// Includes
//

// ST
#include "stm32h7xx.h"
#include "stm32h7xx_hal_cortex.h"
#include "stm32h7xx_ll_cortex.h"

// Uni.HAL
#include "core/uni_hal_core_cm7.h"



//
// Functions
//

bool uni_hal_core_cm7_dcache_get() {
    return SCB->CCR & SCB_CCR_DC_Msk;
}

void uni_hal_core_cm7_dcache_set(bool enable) {
    if (enable) {
        SCB_EnableDCache();
    } else {
        SCB_DisableDCache();
    }
}

void uni_hal_core_cm7_dcache_cleaninvalidate_addr(void* ptr, int32_t len) {
    SCB_CleanInvalidateDCache_by_Addr(ptr, len);
}

void uni_hal_core_cm7_dcache_clean(void* ptr, int32_t len) {
    SCB_CleanDCache_by_Addr(ptr, len);
}

void uni_hal_core_cm7_dcache_invalidate(void* ptr, int32_t len) {
    SCB_InvalidateDCache_by_Addr(ptr, len);
}

void uni_hal_core_cm7_icache_set(bool enable) {
    if (enable) {
        SCB_EnableICache();
    } else {
        SCB_DisableICache();
    }
}

void uni_hal_core_cm7_mpu_set(bool enable) {
    if (enable) {
        LL_HANDLER_EnableFault(LL_HANDLER_FAULT_MEM);
        LL_MPU_Enable(LL_MPU_CTRL_PRIVILEGED_DEFAULT);
    } else {
        LL_HANDLER_DisableFault(LL_HANDLER_FAULT_MEM);
        LL_MPU_Disable();
    }
}

void uni_hal_core_cm7_mpu_config() {
    LL_MPU_ConfigRegion(
            LL_MPU_REGION_NUMBER0,
            0x0U,
            0x30000000,
            LL_MPU_REGION_SIZE_256KB | LL_MPU_TEX_LEVEL1 | LL_MPU_ACCESS_NOT_BUFFERABLE | LL_MPU_ACCESS_NOT_CACHEABLE |
            LL_MPU_ACCESS_NOT_SHAREABLE | LL_MPU_INSTRUCTION_ACCESS_DISABLE | LL_MPU_REGION_FULL_ACCESS);
    LL_MPU_ConfigRegion(
            LL_MPU_REGION_NUMBER1,
            0x0U,
            0x30010000,
            LL_MPU_REGION_SIZE_1KB | MPU_TEX_LEVEL0 | LL_MPU_ACCESS_BUFFERABLE | LL_MPU_ACCESS_NOT_CACHEABLE |
            LL_MPU_ACCESS_SHAREABLE | LL_MPU_INSTRUCTION_ACCESS_DISABLE | LL_MPU_REGION_FULL_ACCESS);
    LL_MPU_ConfigRegion(
            LL_MPU_REGION_NUMBER2,
            0x0U,
            0x30010400,
            LL_MPU_REGION_SIZE_1KB | MPU_TEX_LEVEL0 | LL_MPU_ACCESS_BUFFERABLE | LL_MPU_ACCESS_NOT_CACHEABLE |
            LL_MPU_ACCESS_SHAREABLE | LL_MPU_INSTRUCTION_ACCESS_DISABLE | LL_MPU_REGION_FULL_ACCESS);
    LL_MPU_ConfigRegion(
            LL_MPU_REGION_NUMBER2,
            0x0U,
            0x30010800,
            LL_MPU_REGION_SIZE_1KB | MPU_TEX_LEVEL0 | LL_MPU_ACCESS_BUFFERABLE | LL_MPU_ACCESS_NOT_CACHEABLE |
            LL_MPU_ACCESS_SHAREABLE | LL_MPU_INSTRUCTION_ACCESS_DISABLE | LL_MPU_REGION_FULL_ACCESS);
}
