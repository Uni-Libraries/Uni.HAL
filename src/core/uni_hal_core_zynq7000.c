//
// Includes
//

// stdlib
#include <limits.h>

// Xilinx BSP
#include <sleep.h>
#include <xparameters.h>
#include <xparameters_ps.h>
#include <xpseudo_asm.h>
#include <xscutimer.h>

// FreeRTOS
#include <FreeRTOS.h>
#include <task.h>

// uni_hal
#include "uni_hal_core.h"



//
// Globals
//

/* configCPU_CLOCK_HZ of the generated FreeRTOSConfig.h */
uint32_t SystemCoreClock = XPAR_CPU_CORE_CLOCK_FREQ_HZ;

XScuGic g_uni_hal_core_zynq7000_gic;

extern const uint32_t uni_hal_core_vector_table_ca9[];



//
// Private
//

static bool _uni_hal_core_zynq7000_gic_ready = false;
static Xil_InterruptHandler global_timer_default_handler;

static uint32_t _uni_hal_core_zynq7000_irq_id(uni_hal_core_irq_e irq) {
    switch (irq) {
        case UNI_HAL_CORE_IRQ_ETH:       return XPS_GEM0_INT_ID;
        case UNI_HAL_CORE_IRQ_I2C_1_IRQ1: return XPS_I2C0_INT_ID;
        case UNI_HAL_CORE_IRQ_I2C_2_IRQ1: return XPS_I2C1_INT_ID;
        case UNI_HAL_CORE_IRQ_UART_1:    return XPS_UART0_INT_ID;
        case UNI_HAL_CORE_IRQ_UART_2:    return XPS_UART1_INT_ID;
        default:                         return INT16_MAX;
    }
}



//
// Public: Zynq-7000 specific
//

void uni_hal_core_zynq7000_init(void) {
    // the global timer (XTime) is started lazily by xiltimer on the first sleep, which also
    // zeroes the counter: start it now, so that XTime runs and never steps back later
    usleep(1);

    uni_hal_core_irq_init();
}


bool uni_hal_core_zynq7000_irq_connect(uint32_t irq_id, Xil_InterruptHandler handler, void *ctx, uint32_t priority,
                                       bool rising_edge) {
    bool result = false;
    if (_uni_hal_core_zynq7000_gic_ready && handler != nullptr && irq_id < XSCUGIC_MAX_NUM_INTR_INPUTS) {
        XScuGic_SetPriorityTriggerType(&g_uni_hal_core_zynq7000_gic, irq_id, (uint8_t)((priority & 0x1FU) << 3U),
                                       rising_edge ? 3U : 1U);
        if (XScuGic_Connect(&g_uni_hal_core_zynq7000_gic, irq_id, handler, ctx) == XST_SUCCESS) {
            XScuGic_Enable(&g_uni_hal_core_zynq7000_gic, irq_id);
            result = true;
        }
    }
    return result;
}



//
// Public: uni_hal_core
//

void uni_hal_core_irq_init(void) {
    if (!_uni_hal_core_zynq7000_gic_ready) {
        // VBAR -> vector table with the FreeRTOS IRQ/SVC entries
        __asm volatile("mcr p15, 0, %0, c12, c0, 0" ::"r"(uni_hal_core_vector_table_ca9) : "memory");
        __asm volatile("isb" ::: "memory");

        XScuGic_Config *cfg = XScuGic_LookupConfig(XPAR_XSCUGIC_0_BASEADDR);
        // CfgInitialize installs its private stub only into unclaimed vector slots.
        const bool timer_unclaimed = cfg != nullptr && cfg->HandlerTable[XPS_GLOBAL_TMR_INT_ID].Handler == nullptr;
        if (cfg != nullptr &&
            XScuGic_CfgInitialize(&g_uni_hal_core_zynq7000_gic, cfg, cfg->CpuBaseAddress) == XST_SUCCESS) {
            _uni_hal_core_zynq7000_gic_ready = true;
            if (timer_unclaimed) {
                global_timer_default_handler = cfg->HandlerTable[XPS_GLOBAL_TMR_INT_ID].Handler;
            }
        }
    }
}


bool uni_hal_core_zynq7000_global_timer_irq_available(void) {
    return global_timer_default_handler != nullptr &&
           g_uni_hal_core_zynq7000_gic.Config->HandlerTable[XPS_GLOBAL_TMR_INT_ID].Handler == global_timer_default_handler &&
           !(XScuGic_DistReadReg(&g_uni_hal_core_zynq7000_gic, XSCUGIC_ENABLE_SET_OFFSET) & (1U << XPS_GLOBAL_TMR_INT_ID));
}


bool uni_hal_core_irq_enable(uni_hal_core_irq_e irq, uint32_t priority_group, uint32_t priority_subgroup) {
    (void)priority_subgroup;
    bool result = false;
    uint32_t irq_id = _uni_hal_core_zynq7000_irq_id(irq);
    if (_uni_hal_core_zynq7000_gic_ready && irq_id != INT16_MAX) {
        XScuGic_SetPriorityTriggerType(&g_uni_hal_core_zynq7000_gic, irq_id,
                                       (uint8_t)((priority_group & 0x1FU) << 3U), 1U);
        XScuGic_Enable(&g_uni_hal_core_zynq7000_gic, irq_id);
        result = true;
    }
    return result;
}


bool uni_hal_core_irq_disable(uni_hal_core_irq_e irq) {
    bool result = false;
    uint32_t irq_id = _uni_hal_core_zynq7000_irq_id(irq);
    if (_uni_hal_core_zynq7000_gic_ready && irq_id != INT16_MAX) {
        XScuGic_Disable(&g_uni_hal_core_zynq7000_gic, irq_id);
        result = true;
    }
    return result;
}


uint32_t uni_hal_core_irq_getnum(uni_hal_core_irq_e irq) {
    return _uni_hal_core_zynq7000_irq_id(irq);
}


uint32_t uni_hal_core_irq_pause(void) {
    uint32_t cpsr = mfcpsr();
    __asm volatile("cpsid i" ::: "memory");
    return cpsr & XIL_EXCEPTION_IRQ;
}


void uni_hal_core_irq_resume(uint32_t primask) {
    if (primask == 0U) {
        __asm volatile("cpsie i" ::: "memory");
    }
}



//
// FreeRTOS GCC/ARM_CA9 port hooks
//

static XScuTimer _uni_hal_core_zynq7000_tick_timer;

extern void FreeRTOS_Tick_Handler(void);

void vConfigureTickInterrupt(void) {
    XScuTimer_Config *cfg = XScuTimer_LookupConfig(XPAR_XSCUTIMER_0_BASEADDR);
    configASSERT(cfg != nullptr);
    s32 status = XScuTimer_CfgInitialize(&_uni_hal_core_zynq7000_tick_timer, cfg, cfg->BaseAddr);
    configASSERT(status == XST_SUCCESS);

    // the lowest priority usable by the port; PPI, the trigger type is ignored
    XScuGic_SetPriorityTriggerType(&g_uni_hal_core_zynq7000_gic, XPS_SCU_TMR_INT_ID,
                                   portLOWEST_USABLE_INTERRUPT_PRIORITY << portPRIORITY_SHIFT, 3U);
    status = XScuGic_Connect(&g_uni_hal_core_zynq7000_gic, XPS_SCU_TMR_INT_ID,
                             (Xil_ExceptionHandler)FreeRTOS_Tick_Handler, nullptr);
    configASSERT(status == XST_SUCCESS);
    (void)status;

    // clocked at CPU_CLK/2, the period is load + 1 counts
    XScuTimer_EnableAutoReload(&_uni_hal_core_zynq7000_tick_timer);
    XScuTimer_LoadTimer(&_uni_hal_core_zynq7000_tick_timer,
                        (XPAR_CPU_CORE_CLOCK_FREQ_HZ / 2U) / configTICK_RATE_HZ - 1U);
    XScuTimer_EnableInterrupt(&_uni_hal_core_zynq7000_tick_timer);
    XScuTimer_Start(&_uni_hal_core_zynq7000_tick_timer);
    XScuGic_Enable(&g_uni_hal_core_zynq7000_gic, XPS_SCU_TMR_INT_ID);
}


/* the Cortex-M ports provide it, GCC/ARM_CA9 does not; used by Uni.HAL io */
BaseType_t xPortIsInsideInterrupt(void) {
    extern volatile uint32_t ulPortInterruptNesting;
    return ulPortInterruptNesting != 0U ? pdTRUE : pdFALSE;
}


void vClearTickInterrupt(void) {
    XScuTimer_ClearInterruptStatus(&_uni_hal_core_zynq7000_tick_timer);
}


/* called by the weak vApplicationIRQHandler of the port after saving the FPU context */
void vApplicationFPUSafeIRQHandler(uint32_t ulICCIAR) {
    const uint32_t id = ulICCIAR & 0x3FFU;
    if (id < XSCUGIC_MAX_NUM_INTR_INPUTS) {
        const XScuGic_VectorTableEntry *entry = &XScuGic_ConfigTable[0].HandlerTable[id];
        if (entry->Handler != nullptr) {
            entry->Handler(entry->CallBackRef);
        }
    }
}
