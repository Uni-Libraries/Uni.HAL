//
// Includes
//

#include "uni_hal_core.h"



//
// Functions
//

void uni_hal_core_irq_init(void){

}

bool uni_hal_core_irq_enable(uni_hal_core_irq_e irq, uint32_t priority_group, uint32_t priority_subgroup){
    (void)irq;
    (void)priority_group;
    (void)priority_subgroup;
    return true;
}

bool uni_hal_core_irq_disable(uni_hal_core_irq_e irq){
    (void)irq;
    return true;
}

uint32_t uni_hal_core_irq_getnum(uni_hal_core_irq_e irq){
    (void)irq;
    return INT16_MAX;
}

uint32_t uni_hal_core_irq_pause(void){
    return 0U;
}

void uni_hal_core_irq_resume(uint32_t primask){
    (void)primask;
}
