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
| `UNI_HAL_CAN_USE_FD` | `OFF` | CAN messages hold up to 64 data bytes, for CAN FD on the STM32H7 |
| `UNI_HAL_RTOS_HEAP_SIZE` | `100*1024` | `configTOTAL_HEAP_SIZE` |
| `UNI_HAL_RTOS_HEAP_APP` | `OFF` | the application defines `ucHeap` itself |
| `UNI_HAL_RTOS_MAX_PRIORITIES` | `5` | `configMAX_PRIORITIES` |
| `UNI_HAL_RTOS_MINIMAL_STACK_SIZE` | `512` | `configMINIMAL_STACK_SIZE` of the MCU targets, in words |
| `UNI_HAL_RTOS_CONFIG_EXTRA` | empty | header included at the end of the generated `FreeRTOSConfig.h` |

## What the application has to get right

**Start-up order.** `uni_hal_pwr_init()`, then `uni_hal_core_irq_init()`, then
`uni_hal_rcc_stmXX_config_set()` and `uni_hal_rcc_init()`. Check the result of
`uni_hal_rcc_init()`: when it is `false` the MCU runs from its internal oscillator and no
peripheral clock has its nominal value. The STM32L4 driver does not fail there: without HSE it
feeds the PLL from HSI, and `uni_hal_rcc_stm32l4_status_get()` tells which oscillators came up.
SysTick runs from `uni_hal_rcc_init()` on, and the drivers use it for their timeouts.

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

**GPIO speed.** A pin gets the output speed of its context, `gpio_speed`, whose zero value is
the slowest one. That suits a discrete output; the pins of a UART, of SPI or of CAN are
configured by the application as well and need a faster setting.

**I2C.** Device addresses are 7-bit and not shifted. The bus timing is computed from the kernel
clock; `config.timing` takes a raw `I2C_TIMINGR` value for a bus that needs something else.

**CAN.** The STM32L4 (bxCAN) and the STM32H7 (FDCAN) have the same interface. Both drivers work
on the registers; neither uses the ST HAL. Set `bitrate` in the configuration: the timing is
computed from the CAN clock and the initialisation fails when the clock cannot give the rate
exactly. Acceptance filters are written in the bxCAN register layout on both families.

CAN FD exists on the STM32H7 only. Build with `-DUNI_HAL_CAN_USE_FD=ON`, set `fd` in the
configuration and, for frames that switch the bit rate, `bitrate_data`. A message then holds up
to 64 bytes, and so does every slot of the receive queue: 32 slots take about 2.4 KiB of the
FreeRTOS heap instead of 0.6 KiB. `dlc` of a message is its number of data bytes; a CAN FD frame
can carry 0..8, 12, 16, 20, 24, 32, 48 or 64.

`UNI_HAL_CAN_MODE_LOOPBACK_SILENT` runs the transmit and receive paths without a bus.

**ADC.** The channels are converted continuously into the `data` array of the configuration,
by DMA in circular mode. `clock_mode`, `clock_divider`, `sampling_cycles` and `resolution_bits`
set the conversion clock, the sampling time and the resolution; left at zero they give an
asynchronous clock, the longest sampling time and the full resolution. The initialisation fails
for a value the ADC does not have. It waits up to `timeout` ms, 500 when left at zero, for the
calibration and again for the ADC to become ready: keep that below the period of a watchdog
that is already running.

**UART output.** `uni_hal_usart_transmit_data()` and with it `printf()` queue the data for the
interrupt handler and return; what does not fit into the TX buffer is dropped. Output that has
to be complete, e.g. before a reset, is followed by a loop on `uni_hal_usart_transmit_busy()`.
An application that wants all of its `printf()` output that way defines its own `_write()`,
which replaces the one of the library.

**Backup domain.** A failure of LSE to start does not reset the backup domain unless the RCC
configuration sets `lse_backup_reset`, because that erases the RTC and the backup registers.
For the same reason the RTC keeps the clock it was given on an earlier boot;
`clock_source_change` in its context makes `uni_hal_rtc_init()` move it to the configured one.

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

The tests cover the logic that does not need hardware: DWT tick arithmetic, the I2C timing, the
CAN bit timings for both phases and the CAN FD data length codes.
