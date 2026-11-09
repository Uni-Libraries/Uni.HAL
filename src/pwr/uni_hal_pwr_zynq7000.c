//
// Zynq-7000: power/reset
//

//
// Includes
//

// stdlib
#include <stddef.h>

// Xilinx BSP
#include <xil_io.h>
#include <xil_misc_psreset_api.h>

// uni_hal
#include "pwr/uni_hal_pwr.h"



//
// Globals
//

extern uni_hal_pwr_context_t g_uni_hal_pwr_ctx;



//
// Functions
//

/**
 * PS software reset (SLCR PSS_RST_CTRL.SOFT_RST): the whole PS and the PL restart, the BootROM
 * boots again according to the boot mode pins. In the JTAG boot mode it waits for the debugger.
 */
void uni_hal_pwr_reset(void) {
    Xil_Out32(XSLCR_UNLOCK_ADDR, XSLCR_UNLOCK_CODE);
    Xil_Out32(XSLCR_BASEADDR + 0x200U /* PSS_RST_CTRL */, 0x1U);
    for (;;) {
    }
}


bool uni_hal_pwr_init(void) {
    g_uni_hal_pwr_ctx.inited = true;
    return g_uni_hal_pwr_ctx.inited;
}


bool uni_hal_pwr_set_battery_charging(bool value) {
    g_uni_hal_pwr_ctx.battery_charging = value;
    return true;
}
