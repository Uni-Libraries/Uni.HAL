/*
 * Vector table for FreeRTOS GCC/ARM_CA9 on top of the Xilinx standalone BSP.
 *
 * The BSP boot code sets VBAR to its own _vector_table, whose IRQ/SVC entries go to the
 * Xilinx handlers. The FreeRTOS port needs IRQ and SVC to enter FreeRTOS_IRQ_Handler and
 * FreeRTOS_SWI_Handler directly. uni_hal_core_zynq7000_init() points VBAR here; the other
 * exceptions are forwarded to the BSP table.
 */
    .text
    .arm
    .balign 32                          /* VBAR: the low 5 bits are zero */
    .global uni_hal_core_vector_table_ca9
uni_hal_core_vector_table_ca9:
    b       _boot                       /* reset */
    b       _vector_table + 0x04        /* undefined instruction -> BSP */
    b       FreeRTOS_SWI_Handler        /* SVC -> FreeRTOS (yield) */
    b       _vector_table + 0x0C        /* prefetch abort -> BSP */
    b       _vector_table + 0x10        /* data abort -> BSP */
    nop                                 /* reserved */
    b       FreeRTOS_IRQ_Handler        /* IRQ -> FreeRTOS -> vApplicationIRQHandler */
    b       _vector_table + 0x1C        /* FIQ -> BSP */
