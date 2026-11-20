# Public API by target

Use this table to select functions provided by the Uni.HAL target. Each name is
`uni_hal_<prefix>_<suffix>`. For example, the `flash` row with `read` covers
`uni_hal_flash_read`. A plus means a consumer can compile and link the function
with the normal target libraries. A dash means the target does not supply the
complete call, including its dependencies. Shared headers also declare functions
for other targets.

## Driver modes

- **STM32H743** provides blocking SPI and asynchronous SPI with DMA, classic CAN
  and optional CAN FD, sector and bank Flash operations, and the CM7 cache API.
  I²C1–4 support 7-bit master and memory transfers by polling. The TIM driver
  implements base timers with update callbacks on TIM2 and TIM15–17.
  Applications can supply their own TIM2 interrupt handler. Other
  instances and configurations with nonzero `channel_count` fail initialization.
  Channel configuration and input capture measurements
  use the L496 driver.
- **STM32L496** provides blocking SPI, classic CAN, DMA channel configuration,
  TIM1 input capture (CH1–4) and update callbacks on TIM1, TIM2 and TIM15–17.
  I²C1–4 use polling for master transfers and probing. Memory transfers also
  support interrupts. SPI1–3 support 8-bit full-duplex master and slave modes.
  SPI1 uses PCLK2 and SPI2/3 use PCLK1. Leave `clock_source` at zero to use the
  instance clock. The master prescaler supports divisors 2 through 256. Supply
  SCK, MISO and MOSI GPIO configurations, plus an alternate-function NSS pin
  for hardware chip select. With software NSS, the application controls external
  chip select. A supplied NSS GPIO is initialized using its configuration.
  SPI initialization rejects CRC and DMA requests. The SPI abort entry point
  returns false because transfers are blocking. The Flash driver is absent.
- **Zynq-7000** provides CPU0/GIC startup, the system time base, PS UART, PS
  reset and basic SEGGER RTT. Generic timing calculators, context queries and
  I/O buffer helpers can be used independently. They do not provide a Zynq
  ADC, CAN, I²C, SPI or STM32-style RCC driver. RTT-to-stdio needs a HAL timer.
- **PC** provides host time in milliseconds and real FreeRTOS I/O buffers.
  CRC computes an independent remainder for each context using its seed and odd
  polynomial at widths 7, 8, 16 and 32. It processes bytes most-significant bit
  first without reflection or a final XOR. RNG reads the operating system's
  random source. Discard the buffer after a failed read because it may be
  partially filled. Flash reports zero capacity and SPI transfers return failure.

  A zero-initialized PC I²C configuration represents an empty bus. To attach a
  device model, set `config.pc_exchange` and `config.pc_user` on the bus before
  initialization. The callback receives normalized 7-bit device addresses. It
  returns zero for an acknowledged probe, the transferred byte count for data,
  or a negative value for failure. A successful read fills the requested buffer.
  The callback runs synchronously and must return within the supplied timeout.
  When several tasks share a model, the model owns synchronization of its device
  state. Reset preserves the model.

## Link contract

The PC and firmware consumer targets take a reference to every function marked
for their platform. The clients include `uni_hal.h` and the public I/O tunnel,
RTT and platform power headers. UART buffer convenience macros use the I/O
functions. Board-provided hooks and vendor APIs such as the USB device stack
have their own integration requirements.

Hosted CI links the PC and STM32 clients on each build. The same firmware
project builds `public_api_link` for Zynq with a board's Vitis standalone BSP.
To enable that job on pushes to `main`, set the repository variable
`UNI_HAL_ZYNQ7000_RUNNER` to the label of a Linux runner with the Vitis Arm
compiler on its path. Set `UNI_HAL_ZYNQ7000_BSP_DIR` in the runner environment
to the exported BSP directory.

| Prefix | Function suffixes | STM32H743 | STM32L496 | PC | ZYNQ7000 |
|---|---|:---:|:---:|:---:|:---:|
| `adc` | `get_channel_raw, get_rank_raw, has_channel, is_inited` | + | + | + | + |
| `adc` | `get_channel_mv, init` | + | + | + | - |
| `adc` | `stm32l4_get_mcutemp, stm32l4_get_vdda` | - | + | - | - |
| `adc` | `stm32h7_get_mcutemp, stm32h7_get_vdda` | + | - | - | - |
| `ads1015` | `is_inited` | + | + | + | + |
| `ads1015` | `configure, get_raw, get_voltage_mv, init, is_ready` | + | + | + | - |
| `can` | `dlc_from_length, dlc_to_length, is_available, is_inited, receive, set_error_callback, timing_calc, timing_calc_data` | + | + | + | + |
| `can` | `bus_status_get, init, set_filter, set_tx_callback, start, stop, transmit, transmit_abort, transmit_free, transmit_nowait` | + | + | + | - |
| `core` | `irq_disable, irq_enable, irq_getnum, irq_init, irq_pause, irq_resume` | + | + | + | + |
| `core` | `zynq7000_global_timer_irq_available, zynq7000_init, zynq7000_irq_connect` | - | - | - | + |
| `core` | `cm7_dcache_clean, cm7_dcache_cleaninvalidate_addr, cm7_dcache_get, cm7_dcache_invalidate, cm7_dcache_set, cm7_icache_set, cm7_mpu_config, cm7_mpu_set, stm32h7_revision_get, stm32h7_uid_0, stm32h7_uid_1, stm32h7_uid_2` | + | - | - | - |
| `crc` | `is_inited` | + | + | + | + |
| `crc` | `append, calculate, calculate_ex, get_value, init, reset, reset_ex` | + | + | + | - |
| `cy15b064j` | `is_inited` | + | + | + | + |
| `cy15b064j` | `init, read, write` | + | + | + | - |
| `dma` | `is_inited` | + | + | + | + |
| `dma` | `init` | + | + | + | - |
| `dma` | `enable, set_fifo_mode, set_mode, set_priority` | + | + | - | - |
| `dma` | `set_address, set_direction, set_length, set_memory_align, set_memory_increment, set_periph_align, set_periph_increment, set_request, stm32l4_get_channel, stm32l4_get_module, stm32l4_has_transfer_error` | - | + | - | - |
| `dma` | `stm32h7_get_channel, stm32h7_get_module` | + | - | - | - |
| `dwt` | `compare` | + | + | + | + |
| `dwt` | `convert_ms, convert_ns, convert_us, delay_ms, delay_us, get_ms, get_tick, get_us, init, is_inited, timeout_ms_to_tick` | + | + | + | - |
| `flash` | `configure_latency_auto, erase, erase_bank, erase_sector, init, read, swap_banks, write` | + | - | + | - |
| `flash` | `get_size` | - | - | + | - |
| `gpio` | `pin_deinit, pin_get, pin_init, pin_is_inited, pin_set, pin_speed_get` | + | + | + | - |
| `gpio` | `pin_set_interrupt_callback` | + | + | - | - |
| `gpio` | `pin_get_bankaddr` | - | + | - | - |
| `i2c` | `is_inited, timing_calc` | + | + | + | + |
| `i2c` | `deinit, init, isready, master_receive, master_transmit, mem_read, mem_write, recover, reset` | + | + | + | - |
| `io` | `buffer_is_empty, buffer_pop_isr, buffer_push_isr, deinit, init, receive_available, receive_clear, receive_data, receive_sync, receive_sync_reset, stdio_init, stdio_printf, stdio_snprintf, stdio_vprintf, transmit_data, tunnel_init, tunnel_is_inited, tunnel_transmit` | + | + | + | + |
| `iwdg` | `is_inited` | + | + | + | + |
| `iwdg` | `deinit, init, reload` | + | + | + | - |
| `mcp23017` | `is_inited` | + | + | + | + |
| `mcp23017` | `get_gpio, init, set_gpio, set_iodir` | + | + | + | - |
| `pwr` | `init, is_battery_charging, is_inited, reset, set_battery_charging` | + | + | + | + |
| `pwr` | `stm_set_backup_access` | + | + | - | - |
| `pwr` | `stm_l4_set_vddio2` | - | + | - | - |
| `rcc` | `clk_get, clk_get_freq, clk_set, clksrc_get, clksrc_set, init, is_inited, reset` | + | + | + | - |
| `rcc` | `get_status_reg, get_status_reg_flag` | - | + | + | - |
| `rcc` | `css_callback, stm32_mco_enable` | + | + | - | - |
| `rcc` | `stm32l4_config_set, stm32l4_status_get` | - | + | - | - |
| `rcc` | `stm32h7_clk_get_hclk, stm32h7_config_set, stm32h7_reset_flags_get, stm32h7_status_get` | + | - | - | - |
| `rng` | `get, get_16u, get_32u, get_8u, init, is_inited` | + | + | + | - |
| `rtc` | `backup_read, backup_write, clock_source_get, get, init, is_inited, set` | + | + | + | - |
| `segger` | `rtt_init, rtt_is_inited, rtt_stdio_is_inited` | + | + | + | + |
| `segger` | `rtt_stdio_init` | + | + | + | - |
| `spi` | `abort, bitrate_get, get_last_errors, init, is_inited, receive, set_callback, transmit, transmitreceive` | + | + | + | - |
| `spi` | `is_busy, set_prescaler, transceive_async` | + | - | + | - |
| `systick` | `get_ms` | + | + | + | + |
| `systick` | `delay, get_freq, get_tick, init` | + | + | - | + |
| `tim` | `is_inited` | + | + | + | + |
| `tim` | `period_elapsed` | + | + | + | - |
| `tim` | `init, register_callback, start, stop` | + | + | - | - |
| `tim` | `clear, get_chan_age, get_chan_freq_hz, get_chan_freq_mhz, get_chan_period_ns, get_chan_period_us, get_chan_state, get_tick_period_ns, get_tick_period_us, is_channel_valid, set_arrpreload, set_dmarequest` | - | + | - | - |
| `uart` | `io_get, is_inited` | + | + | + | + |
| `usart` | `baudrate_get, baudrate_set, init, irq_enable, receive_enable, set_callback, transmit_busy, transmit_enable` | + | + | + | + |
