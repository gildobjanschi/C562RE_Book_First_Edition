# C562RE Book — First Edition Source Code

This repository contains the source code accompanying the first edition of the **STM32 Projects with FreeRTOS/Practical STM32C5 Projects with STM32CubeMX2** book — a collection of hands-on peripheral examples for the **STM32C562RE** microcontroller (STM32C5 series, Arm® Cortex®-M33 core), targeting the **NUCLEO-C562RE** development board.

Each folder is a self-contained example project demonstrating a specific peripheral or feature of the STM32C562RE, built with STM32CubeIDE and debugged/flashed via SEGGER J-Link.

## Examples

| Folder | Description |
|---|---|
| `ADC_DMA` | Analog-to-digital conversion using DMA transfers |
| `Blink_LED` | Minimal GPIO example — blinking an LED |
| `DAC_DMA` | Digital-to-analog conversion using DMA transfers |
| `FDCAN` | FDCAN (Flexible Data-rate CAN) peripheral example |
| `Flash_EDATA` | Flash memory emulated data / EEPROM emulation |
| `I2C_FRAM` | I2C communication with an external FRAM device |
| `I2C_VL53L1X` | I2C communication with the VL53L1X time-of-flight sensor |
| `I3C_Controller` | I3C bus operating in Controller (master) mode |
| `I3C_Target` | I3C bus operating in Target (slave) mode |
| `Low_Power` | Low-power mode configuration and usage |
| `Memory` | Memory configuration and access examples |
| `PWR_Standby` | Standby mode and wake-up handling |
| `RAM_ECC` | RAM Error-Correcting Code (ECC) handling |
| `RAM_WP` | RAM write-protection configuration |
| `RTC` | Real-Time Clock configuration and usage |
| `SPI_FRAM` | SPI communication with an external FRAM device |
| `Shared` | Shared code/utilities used across multiple examples |
| `Timers_DMA` | Timer peripheral combined with DMA |
| `Timers_Debounce` | Button debouncing using timers |
| `Timers_Debounce_ST` | Button debouncing using ST's approach/library |
| `Timers_IR_RX` | Infrared signal reception using timers |
| `Timers_IR_TX` | Infrared signal transmission using timers |
| `Timers_PWM` | PWM generation using timers |
| `UART_DMA` | UART communication using DMA transfers |
| `VU_Meter` | VU meter application example |
| `WDG` | Watchdog (WDG) configuration and usage |

## Hardware

- **Board:** [NUCLEO-C562RE](https://www.st.com/en/evaluation-tools/nucleo-c562re.html)
- **MCU:** STM32C562RET6 (Arm Cortex-M33, 512 KB Flash, 128 KB SRAM, LQFP64)
- Some examples require additional peripherals/sensors (e.g., FRAM, VL53L1X) — see the individual example folder for details.

## Requirements

- [STM32CubeIDE](https://www.st.com/en/development-tools/stm32cubeide.html) (or another Eclipse-based STM32 toolchain)
- [SEGGER J-Link](https://www.segger.com/products/debug-probes/j-link/) tools for flashing/debugging (`.jlink` scripts are included)
- ST-LINK drivers (the Nucleo board's onboard ST-LINK can also be used)

## Getting Started

1. Clone the repository:
   ```bash
   git clone https://github.com/gildobjanschi/C562RE_Book_First_Edition.git
   ```
2. Open STM32CubeIDE and import the example project you want to run (**File → Import → Existing Projects into Workspace**).
3. Connect your NUCLEO-C562RE board.
4. Build and flash the project to the board.

Each example is designed to be built and run independently; check the `Shared` folder if a project references common code.

## License

This project is licensed under the [MIT License](LICENSE).

## About

Companion source code for the first edition of the C562RE book.
