# Uni.HAL

A hardware abstraction layer in C23 for STM32 microcontrollers, with a PC target for host tests.

| Target | `UNI_HAL_TARGET_MCU` | Notes |
|---|---|---|
| STM32H743 | `STM32H743` | Cortex-M7 |
| STM32L496 | `STM32L496` | Cortex-M4F |
| Zynq-7000 | `ZYNQ7000` | Cortex-A9 #0 of the PS, on the Vitis standalone BSP of the board (`UNI_HAL_ZYNQ7000_BSP_DIR`) |
| Host | `PC` | stand-ins for the drivers, used by the unit tests |

The [public API matrix](docs/api-support.md) lists the functions and driver modes
available on each target, including the PC stand-ins. The library bundles CMSIS,
the ST HAL/LL drivers, the FreeRTOS kernel and SEGGER RTT under `3rdparty/`.

On Zynq-7000, the PS UART driver reserves CPU0's global timer comparator and its private
interrupt (PPI 27) when the first UART is initialized. Leave both available when using a
PS UART. Initialization fails if the alarm or interrupt is already in use. The driver
checks the shift register after the FIFO empties, including before FreeRTOS starts when
interrupts are enabled. The BSP keeps its free-running time counter and FreeRTOS uses
the separate private tick timer.

## Using it from CMake

Place `Uni.HAL/` next to the application's top-level `CMakeLists.txt` and `main.c`,
as in the [minimal firmware project](src_tests/cmake_consumer/CMakeLists.txt).
Select the MCU when configuring so the toolchain can choose a compiler before
`project()` enables C, C++ and ASM. For an STM32H743 board with an 8 MHz HSE,
run these commands from the application directory with Arm GCC on the path.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=Uni.HAL/cmake/toolchain.cmake \
  -DUNI_HAL_TARGET_MCU=STM32H743 -DUNI_HAL_HSE_VALUE=8000000
cmake --build build --config Release
```

The build produces `firmware.elf` and `firmware.bin`. The dependencies Uni.Common
and nanoprintf are downloaded by CPM during the configuration.
All MCU targets and the PC target use the bundled FreeRTOS kernel.

| Option | Default | Meaning |
|---|---|---|
| `UNI_HAL_TARGET_MCU` | none | target, required |
| `UNI_HAL_ZYNQ7000_BSP_DIR` | none | ZYNQ7000: `<platform>/export/<platform>/sw/<domain>` of a Vitis platform (`include/`, `lib/`, `Xilinx.spec`); build with the Xilinx `arm-none-eabi-gcc` |
| `UNI_HAL_HSE_VALUE` | none | HSE frequency of the board in Hz, required for the STM32 targets |
| `UNI_HAL_CAN_USE_FD` | `OFF` | CAN messages hold up to 64 data bytes, for CAN FD on the STM32H7 |
| `UNI_HAL_IO_PRINTF_LARGE_FORMAT` | `OFF` | `uni_hal_io_stdio_*printf` understand `ll`, `j`, `z`, `t` (`%llu`, `%zu`) |
| `UNI_HAL_RTOS_HEAP_SIZE` | `100*1024` | `configTOTAL_HEAP_SIZE` |
| `UNI_HAL_LIBC_HEAP_SIZE` | `8192` | STM32 C library heap size in bytes |
| `UNI_HAL_RTOS_HEAP_APP` | `OFF` | the application defines `ucHeap` itself |
| `UNI_HAL_RTOS_MAX_PRIORITIES` | `5` | `configMAX_PRIORITIES` |
| `UNI_HAL_RTOS_MINIMAL_STACK_SIZE` | `512` | `configMINIMAL_STACK_SIZE` of the MCU targets, in words |
| `UNI_HAL_RTOS_CONFIG_EXTRA` | empty | header included at the end of the generated `FreeRTOSConfig.h` |

## What the application has to get right

**STM32 start-up order.** Call `uni_hal_pwr_init()`, then `uni_hal_core_irq_init()`, then
`uni_hal_rcc_stm32h7_config_set()` or `uni_hal_rcc_stm32l4_config_set()` for your target,
followed by `uni_hal_rcc_init()`. Check the result before starting
peripherals. On STM32H7, a failure leaves the nominal peripheral clocks unavailable.
On STM32L4, loss of HSE triggers an attempt to reproduce the configured PLL
frequency from HSI. If the PLL cannot deliver that frequency, the driver can use HSI directly.
An application that requires the PLL should check `pll_inited` as well as `sys_inited` in
`uni_hal_rcc_stm32l4_status_get()`. The CSS callback runs in NMI context. If HSE fails
during initialization, `uni_hal_rcc_init()` finishes recovery after the handler returns.
The drivers use SysTick for their timeouts.

**Zynq-7000 startup.** Start with the board's PS7 configuration and standalone BSP.
Call `uni_hal_core_zynq7000_init()` in `main` to install the exception vectors,
start the global timer and initialize the GIC before starting peripherals and FreeRTOS.

**Dynamic memory with FreeRTOS.** Blocks from the C library (`malloc`, `calloc`,
`realloc`, aligned allocations and the newlib reentrant functions) belong to the C
library and are released with `free`. FreeRTOS allocations use `pvPortMalloc` and
`vPortFree`. When upgrading firmware that mixed these APIs, update each deallocation to
match the allocator that supplied the block. Call these APIs at startup or from task context.
On STM32, size the C library pool with `UNI_HAL_LIBC_HEAP_SIZE` independently of the RTOS heap. The pool
uses ordinary RAM on L496 and section `.RAM_D1` on H743. An application supplying
its own `_sbrk` (or picolibc `sbrk`) controls its own pool. On Zynq, the BSP uses
the linker script's `_HEAP_SIZE` (64 KiB by default).

**Task-local libc state.** MCU builds using newlib enable FreeRTOS's
`configUSE_NEWLIB_REENTRANT`. Each task gets its own `errno` and newlib state,
including its `strtok` cursor. FreeRTOS reclaims that state on task deletion.
Budget for the larger task control block even for tasks that never call libc.
With the default project configuration, `sizeof(StaticTask_t)` grows from 104 to
180 bytes on STM32 with Arm GCC 14.2 and newlib-nano, and from 104 to 392 bytes on
Zynq with the Vitis GCC 13.4 standalone BSP. Individual libc calls can also
allocate from the C heap. The STM32 GCC toolchain selects the nano headers for
C and C++ sources so their structure layout matches the linked library.

With `configSUPPORT_STATIC_ALLOCATION=0`, the RX and TX stream buffers created by
`uni_hal_io_init()` use the FreeRTOS heap. Each configured size includes one reserved
byte. Stop tasks and interrupt handlers that use the context before calling
`uni_hal_io_deinit()` to release both buffers.

The default FreeRTOS malloc-failure hook traps when `heap_4` cannot satisfy a request.
To handle exhaustion in the application, define `void vApplicationMallocFailedHook(void)`
in an application source file. A returning hook lets `pvPortMalloc` return `NULL`.
Keep the hook allocation-free. Applications can also disable the hook by setting
`configUSE_MALLOC_FAILED_HOOK` to 0 in `UNI_HAL_RTOS_CONFIG_EXTRA` after undefining
the default. In either returning configuration, a failed dynamic `uni_hal_io_init()`
releases a partially created buffer pair and can be retried once memory is available.

**Interrupt priorities.** On STM32, pass an unshifted NVIC priority to `isr_priority` or
`irq_priority`. The default FreeRTOS configuration permits levels 4 through 15 for
handlers calling `...FromISR()`. On Zynq the GIC uses a different scale. The PS UART
accepts levels `configMAX_API_CALL_INTERRUPT_PRIORITY` through `portLOWEST_USABLE_INTERRUPT_PRIORITY`
(18 through 30 by default). Zero selects the default UART priority of 20, raised to the
configured FreeRTOS threshold if necessary. The shared TX alarm uses the same default.

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
On STM32L4, an interrupt-driven memory transfer blocks the calling task on a FreeRTOS
semaphore until the interrupt signals completion or the timeout expires. Before the
scheduler starts, the driver polls for completion with interrupts enabled.
`uni_hal_i2c_mem_read()` and `uni_hal_i2c_mem_write()` finish using the caller's
buffer before they return, including on timeout. A zero timeout returns
`false` without starting a transfer. A timed-out write may already have sent bytes to the device.

**CAN.** The STM32L4 (bxCAN) and the STM32H7 (FDCAN) have the same interface. Both drivers work
on the registers; neither uses the ST HAL. Set `bitrate` in the configuration: the timing is
computed from the CAN clock and the initialisation fails when the clock cannot give the rate
exactly. Acceptance filters are written in the bxCAN register layout on both families.
The receive interrupt delivers accepted frames to a FreeRTOS queue.

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

On STM32L496 a DMA transfer error stops the affected channel. The ADC getters then report
unavailable data, and `uni_hal_dma_stm32l4_has_transfer_error(dma)` reports the fault.
Discard the old buffer contents, correct the transfer setup and reload its length before
enabling the channel again. Starting the new transfer clears the fault indication.

**UART output.** `uni_hal_usart_transmit_data()` and with it `printf()` queue the data for the
interrupt handler and return; what does not fit into the TX buffer is dropped. Output that has
to be complete, e.g. before a reset, is followed by a loop on `uni_hal_usart_transmit_busy()`.
An application that wants all of its `printf()` output that way defines its own `_write()`,
which replaces the one of the library.

**Backup domain.** A failure of LSE to start does not reset the backup domain unless the RCC
configuration sets `lse_backup_reset`, because that erases the RTC and the backup registers.
For the same reason the RTC keeps the clock it was given on an earlier boot;
`clock_source_change` in its context makes `uni_hal_rtc_init()` move it to the configured one.

## Tests

```sh
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain.cmake -DUNI_HAL_TARGET_MCU=PC
cmake --build build
ctest --test-dir build
```

The host suite covers DWT tick arithmetic, I2C timing and CAN timings and data lengths. It
also runs the STM32L496 I2C driver with controlled interrupts and deadlines, and exercises
clock recovery against an RCC register model with a frozen SysTick.

The allocator regression links a firmware executable and checks the providers of the libc
and FreeRTOS symbols in its linker map. With Arm GCC and `qemu-system-arm` on the path, run
the allocation, resize, alignment and out-of-memory checks at startup and from a FreeRTOS task:

```sh
cmake -S src_tests/allocator -B build-allocator \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/toolchain.cmake" \
  -DUNI_HAL_TARGET_MCU=STM32L496 -DUNI_HAL_HSE_VALUE=8000000 -DUNI_HAL_TEST_ALLOCATOR_QEMU=ON
cmake --build build-allocator
ctest --test-dir build-allocator --output-on-failure
```

The same emulated task checks RX and TX capacity, data transfer, reinitialisation
and cleanup. To exercise the dynamic-only configuration, use a fresh build directory
and add `-DUNI_HAL_RTOS_CONFIG_EXTRA="$PWD/src_tests/config/freertos_dynamic_only.h"`
to the configure command. This also tests that a failed TX allocation releases RX
and that the context can be initialised again.
