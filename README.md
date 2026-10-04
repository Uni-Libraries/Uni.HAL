# Uni.HAL

A hardware abstraction layer in C23 for STM32 microcontrollers, with a PC target for host tests.

| Target | `UNI_HAL_TARGET_MCU` | Notes |
|---|---|---|
| STM32H743 | `STM32H743` | Cortex-M7 |
| STM32L496 | `STM32L496` | Cortex-M4F |
| Host | `PC` | stand-ins for the drivers, used by the unit tests |

Not every module exists for every target; the header of a module says what it covers. The
library bundles CMSIS, the ST HAL/LL drivers, the FreeRTOS kernel and SEGGER RTT under
`3rdparty/`.

## Using it from CMake

```cmake
set(UNI_HAL_TARGET_MCU "STM32H743")
set(UNI_HAL_HSE_VALUE  8000000)        # HSE frequency of the board, in Hz
add_subdirectory(Uni.HAL)

include(Uni.HAL/uni.hal.cmake)
uni_hal_add_executable(firmware)
target_sources(firmware PRIVATE main.c)
```

Configure with `-DCMAKE_TOOLCHAIN_FILE=Uni.HAL/cmake/toolchain.cmake`. The dependencies
Uni.Common and nanoprintf are downloaded by CPM during the configuration.

| Option | Default | Meaning |
|---|---|---|
| `UNI_HAL_TARGET_MCU` | none | target, required |
| `UNI_HAL_HSE_VALUE` | none | HSE frequency in Hz, required for the MCU targets |
| `UNI_HAL_USE_FREERTOS` | `ON` | build on FreeRTOS; `OFF` for a main loop without an RTOS |
| `UNI_HAL_CAN_USE_FREERTOS` | `ON` | CAN receive queue is a FreeRTOS queue instead of a ring buffer |
| `UNI_HAL_I2C_USE_FREERTOS` | `ON` | interrupt-driven I2C transfers wait on a task notification |
| `UNI_HAL_RTOS_HEAP_SIZE` | `100*1024` | `configTOTAL_HEAP_SIZE` |
| `UNI_HAL_RTOS_HEAP_APP` | `OFF` | the application defines `ucHeap` itself |
| `UNI_HAL_RTOS_MAX_PRIORITIES` | `5` | `configMAX_PRIORITIES` |
| `UNI_HAL_RTOS_MINIMAL_STACK_SIZE` | `512` | `configMINIMAL_STACK_SIZE` of the MCU targets, in words |
| `UNI_HAL_RTOS_CONFIG_EXTRA` | empty | header included at the end of the generated `FreeRTOSConfig.h` |

## What the application has to get right

**Start-up order.** `uni_hal_pwr_init()`, then `uni_hal_core_irq_init()`, then
`uni_hal_rcc_stmXX_config_set()` and `uni_hal_rcc_init()`. Check the result of
`uni_hal_rcc_init()`: when it is `false` the MCU runs from its internal oscillator and no
peripheral clock has its nominal value. SysTick runs from `uni_hal_rcc_init()` on, and the
drivers use it for their timeouts.

**Interrupt priorities.** A handler that uses a FreeRTOS `...FromISR()` function must not have
a priority above `configMAX_SYSCALL_INTERRUPT_PRIORITY`, which is 4 here. Where a driver takes
the priority from its context (`isr_priority`, `irq_priority`), set it to 4 or a larger number.

**Timeouts.** Blocking functions return `false` when their timeout runs out instead of waiting
for ever. Take that as a failed transfer: the peripheral may need
`uni_hal_i2c_recover()`, `uni_hal_spi_abort()` or a new initialisation.

**DMA buffers on the STM32H7.** With the data cache on, a buffer written by DMA must either be
in a region the MPU marks as not cacheable, or be aligned to 32 bytes with a size that is a
multiple of 32 so that it shares no cache line with other data. The asynchronous SPI transfer
cleans and invalidates the cache for its buffers; the ADC driver does not touch the cache, so
its sample buffer has to be in a non-cacheable region. `uni_hal_core_cm7_mpu_config()` sets up
such a region at `0x30000000`; it is a weak function, replace it when the linker script of the
project places the buffers elsewhere.

**I2C.** Device addresses are 7-bit and not shifted. The bus timing is computed from the kernel
clock; `config.timing` takes a raw `I2C_TIMINGR` value for a bus that needs something else.

**Backup domain.** A failure of LSE to start does not reset the backup domain unless the RCC
configuration sets `lse_backup_reset`, because that erases the RTC and the backup registers.

## Without an RTOS

With `-DUNI_HAL_USE_FREERTOS=OFF` the kernel is not built. The IO buffers become ring buffers
that are accessed with the interrupts masked, and every wait is a polling loop bounded by its
timeout. Two things behave differently:

- `uni_hal_io_receive_data()` and `uni_hal_io_receive_sync()` occupy the CPU until their
  timeout; call them with a timeout of 0 from a main loop.
- interrupt-driven I2C transfers (`irq_enable`) return before the transfer has finished; use
  the blocking mode.

## Tests

```sh
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain.cmake -DUNI_HAL_TARGET_MCU=PC
cmake --build build
ctest --test-dir build
```

The tests cover the logic that does not need hardware: DWT tick arithmetic, the I2C timing and
the CAN bit timing calculations.
