//
// Includes
//

// uni.hal
#include "flash/uni_hal_flash.h"



//
// Functions
//

void uni_hal_flash_init(void){
    
}


size_t uni_hal_flash_get_size(void){
    return 0U;
}


size_t uni_hal_flash_read(size_t addr, size_t size, uint8_t *dst){
    (void)addr;
    (void)size;
    (void)dst;
    return 0U;
}


size_t uni_hal_flash_write(size_t addr, size_t size, uint8_t *dst){
    (void)addr;
    (void)size;
    (void)dst;
    return 0U;
}

bool uni_hal_flash_erase(size_t addr, size_t size){
    (void)addr;
    (void)size;
    return false;
}


bool uni_hal_flash_erase_bank(uni_hal_flash_bank_e bank){
    (void)bank;
    return false;
}


bool uni_hal_flash_erase_sector(uni_hal_flash_bank_e bank, uni_hal_flash_sector_e sector){
    (void)bank;
    (void)sector;
    return false;
}


bool uni_hal_flash_swap_banks(void){
    return false;
}
