//
// Includes
//

// st
#include <stm32h7xx_hal.h>
#include <stm32h7xx_hal_flash.h>
#include <stm32h7xx_hal_flash_ex.h>

// FreeRTOS
//TODO: guard with define
#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>

// Uni.HAL
#include "flash/uni_hal_flash.h"


//
// Globals
//

static SemaphoreHandle_t xFlashDoneSem;
BaseType_t flash_wake_up = pdFALSE;

//
// HAL
//

void HAL_FLASH_EndOfOperationCallback(uint32_t ReturnValue)
{
    (void)ReturnValue;
    xSemaphoreGiveFromISR(xFlashDoneSem, &flash_wake_up);   /* ✓ safe from ISRs :contentReference[oaicite:0]{index=0} */

}

void HAL_FLASH_OperationErrorCallback(uint32_t ReturnValue)
{
    (void)ReturnValue;
    xSemaphoreGiveFromISR(xFlashDoneSem, &flash_wake_up);
}



//
// IRQ
//

void FLASH_IRQHandler(void)          /* generated by CubeMX */
{
    traceISR_ENTER();
    flash_wake_up = pdFALSE;
    HAL_FLASH_IRQHandler();          /* will end up in the callbacks below */
    portYIELD_FROM_ISR(flash_wake_up);
}



//
// Private
//

bool _uni_hal_flash_lock_ob()
{
    return HAL_FLASH_OB_Lock() == HAL_OK;
}


bool _uni_hal_flash_lock()
{
    return HAL_FLASH_Lock() == HAL_OK;
}

bool _uni_hal_flash_unlock()
{
    return HAL_FLASH_Unlock() == HAL_OK;
}

bool _uni_hal_flash_unlock_ob()
{
    return HAL_FLASH_OB_Unlock() == HAL_OK;
}


//
// Public
//

void uni_hal_flash_init()
{
    xFlashDoneSem = xSemaphoreCreateBinary();
    HAL_NVIC_SetPriority(FLASH_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(FLASH_IRQn);
}

size_t uni_hal_flash_write(size_t addr, size_t size, uint8_t *dst)
{
    HAL_StatusTypeDef status;
    size_t bytes_written = 0;

    if (dst == NULL || size == 0 || size % 32 != 0 || addr < FLASH_BASE || addr >= FLASH_END) {
        return 0;
    }

    status = HAL_FLASH_Unlock();
    if (status != HAL_OK) {
        return 0;
    }

    for (size_t i = 0; i < size; i += 32) {
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, addr + i, (uint32_t)&dst[i]);
        if (status != HAL_OK) {
            HAL_FLASH_Lock();
            return bytes_written;
        }
        bytes_written += 32;
    }

    HAL_FLASH_Lock();
    return bytes_written;
}

size_t uni_hal_flash_read(size_t addr, size_t size, uint8_t *dst)
{
    if (dst == NULL || size == 0 || addr <FLASH_BASE || addr >= FLASH_END)
    {
        return 0;
    }

    uint8_t *flash_ptr = (uint8_t *)addr;

    for (size_t i = 0; i < size; i++)
    {
        dst[i] = flash_ptr[i];
    }

    return size;
}

bool uni_hal_flash_erase_bank()
{
    bool result = false;

    SCB_CleanInvalidateDCache();
    SCB_InvalidateICache();
    SCB_DisableDCache();
    SCB_DisableICache();


    HAL_FLASH_Unlock();
    __HAL_FLASH_SET_PSIZE(FLASH_PSIZE_DOUBLE_WORD, FLASH_BANK_2);

    for (size_t sector = 0 ;sector <8;sector++)
    {
        FLASH_EraseInitTypeDef ei = {
            .TypeErase    = FLASH_TYPEERASE_SECTORS,
            .Banks        = FLASH_BANK_2,
            .Sector =     sector,
            .NbSectors = 1,
            .VoltageRange = FLASH_VOLTAGE_RANGE_4      /* fastest */
        };

        if (HAL_FLASHEx_Erase_IT(&ei) == HAL_OK)
        {
            xSemaphoreTake(xFlashDoneSem, portMAX_DELAY);
            result = true;
        }
        vTaskDelay(450);
    }

    HAL_FLASH_Lock();
    SCB_EnableICache();
    SCB_EnableDCache();

    return result;
}

bool uni_hal_flash_erase(size_t addr, size_t size)
{
    HAL_StatusTypeDef status;
    uint32_t sectorError = 0;

    if (size == 0 || addr < FLASH_BASE || addr > FLASH_END) {
        return 0;
    }

    size_t start_sector = (addr - FLASH_BASE) / FLASH_SECTOR_SIZE;
    size_t nb_sector = (size + FLASH_SECTOR_SIZE - 1) / FLASH_SECTOR_SIZE;

    status = HAL_FLASH_Unlock();
    if (status != HAL_OK)
    {
        return 0;
    }

    FLASH_EraseInitTypeDef eraseInitStruct = {0};

    eraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
    eraseInitStruct.Banks = (addr < FLASH_BANK2_BASE) ? FLASH_BANK_1 : FLASH_BANK_2;
    eraseInitStruct.Sector = start_sector;
    eraseInitStruct.NbSectors = nb_sector;
    eraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    status = HAL_FLASHEx_Erase(&eraseInitStruct, &sectorError);
    if (status != HAL_OK || sectorError != 0xFFFFFFFF) {
        HAL_FLASH_Lock();
        return 0;
    }
    HAL_FLASH_Lock();
    return 1;
}


bool uni_hal_flash_swap_banks(void)
{
    bool result = false;

    taskENTER_CRITICAL();

    if (_uni_hal_flash_unlock() && _uni_hal_flash_unlock_ob())
    {
        FLASH_OBProgramInitTypeDef OBInit = {0};

        HAL_FLASHEx_OBGetConfig(&OBInit);
        OBInit.Banks = FLASH_BANK_1;
        HAL_FLASHEx_OBGetConfig(&OBInit);

        OBInit.OptionType = OPTIONBYTE_USER;
        OBInit.USERType   = OB_USER_SWAP_BANK;
        if ((OBInit.USERConfig & OB_SWAP_BANK_ENABLE) == OB_SWAP_BANK_DISABLE) {
            OBInit.USERConfig = OB_SWAP_BANK_ENABLE;
        } else {
            OBInit.USERConfig = OB_SWAP_BANK_DISABLE;
        }

        result = HAL_FLASHEx_OBProgram(&OBInit) == HAL_OK;
        if (result)
        {
            HAL_FLASH_OB_Launch();
            SCB_InvalidateICache();
            NVIC_SystemReset();
        }
    }

    // unreachable on success
    _uni_hal_flash_lock_ob();
    _uni_hal_flash_lock();
    taskEXIT_CRITICAL();

    return result;
}
