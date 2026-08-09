# AM335x FreeRTOS StarterWare Project

<p align="center">
  <img src="Doc/bg.png" alt="AM335x Banner" width="500px">
</p>

> **Hardware Target:** This project is targeted at the **Antminer L3+** mining board, which uses the **TI AM3352** (ARM Cortex-A8) SoC. The PCB form factor is roughly similar to the BeagleBone Black / AM3356, **but without the PRU subsystems**. This makes the Antminer L3+ a **low-cost option to repurpose** retired mining hardware as a learning platform for FreeRTOS on TI's Cortex-A stack. The banner image above is the Antminer L3+ board itself.

<p align="center">
  <strong>A FreeRTOS port for the TI AM3352 (Cortex-A8), built with CMake + GCC ARM, flashable via J-Link GDB Server.</strong>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/platform-AM3352%20Cortex--A8-orange?logo=texas-instruments" alt="Platform">
  <img src="https://img.shields.io/badge/RTOS-FreeRTOS%20v10.2.0-green?logo=open-source" alt="FreeRTOS">
  <img src="https://img.shields.io/badge/language-C-green?logo=c" alt="Language">
  <img src="https://img.shields.io/badge/build-CMake%203.11%2B+-purple" alt="CMake">
  <img src="https://img.shields.io/badge/debugger-J--Link-blue" alt="J-Link">
  <img src="https://img.shields.io/badge/status-active-success" alt="Status">
</p>

---

## Overview

This repository contains a **FreeRTOS port** for the TI AM3352 SoC. It uses:

- **GNU ARM GCC 7.3.1** (`gcc-arm-none-eabi-7-2018-q2-update`)
- **CMake 3.11+** with Ninja generator
- **J-Link GDB Server** for flashing and debugging

The FreeRTOS kernel is based on Amazon FreeRTOS v10.2.0, adapted from the Cortex-A9 GIC port to use TI's **AINTC** (Advanced Interrupt Controller). Peripheral drivers come from TI's **StarterWare 02.00.01.01** package.

No SD card or external bootloader is required — the binary runs directly from DDR via JTAG.

---

## Prerequisites

| Component | Version / Detail |
|---|---|
| GNU ARM GCC | **7.3.1** (`gcc-arm-none-eabi-7-2018-q2-update`) — must be at `C:\ti\gcc-arm-none-eabi-7-2018-q2-update\bin\arm-none-eabi-gdb.exe` |
| CMake | **3.11+** (tested up to 4.x) |
| Ninja | Build system generator |
| Python | 2.x or 3.x (for CMake config script) |
| Target Board | **Antminer L3+** (TI AM3352, Cortex-A8) — repurposed mining hardware |
| Emulator / Debugger | **J-Link** — solder a JTAG cable onto the Antminer L3+ board. The JTAG footprint is on the rear edge, same as the BeagleBone Black. Follow the [BBB JTAG soldering tutorial](https://dr-kino.github.io/2020/07/22/Beaglebone-black-soldering-jyag-connector/). |

> **Note on compatibility:** The AM3352 shares the same ARM Cortex-A8 core as the BeagleBone Black (AM335x). However, the AM3352 **does not include the PRU subsystems**. Any code that depends on PRU will not work — stick to GPIO, UART, Timer, MMCSD, I2C, SPI, and Ethernet peripherals.

---

## Getting Started

### Quick Summary

1. **Install GNU ARM GCC 7.3.1** and place it at `C:\ti\gcc-arm-none-eabi-7-2018-q2-update`.
2. **Place third-party libraries** (`lib/third_party/ti/` and `lib/third_party/amazon/`) inside your project folder.
3. **Generate CMake config** by running the included Python script.
4. **Configure and build** with CMake + Ninja using the provided toolchain file.
5. **Flash via J-Link GDB Server** to run on the Antminer L3+.

Full step-by-step instructions are in each project's folder under [`Examples/`](./Examples/).

---

## Example Projects

All examples live under the [`Examples/`](./Examples/) folder.

### FreeRTOS Examples

- 🟧 [**`Examples/FreeRTOS_AM335x_GPIO_LED/`**](./Examples/FreeRTOS_AM335x_GPIO_LED/) — Basic FreeRTOS port: two LED blink tasks on AM3352 using CMake and JTAG.
- 🔘 [**`Examples/FreeRTOS_AM335x_GPIO_INTERRUPT/`**](./Examples/FreeRTOS_AM335x_GPIO_INTERRUPT/) — GPIO interrupt demo: AINTC edge interrupt to control LED D2 blink rate dynamically.
- 🔌 [**`Examples/FreeRTOS_AM335x_ISR_FromISR/`**](./Examples/FreeRTOS_AM335x_ISR_FromISR/) — ISR-safe FreeRTOS APIs: xQueueSendFromISR, xSemaphoreGiveFromISR, xTaskNotifyFromISR via UART logging.
- 📡 [**`Examples/FreeRTOS_AM335x_SPI_TX/`**](./Examples/FreeRTOS_AM335x_SPI_TX/) — SPI TX loopback demo: continuous 0xAF transmission at 100 kHz Mode 0 with manual CS control via UART.
- 🎨 [**`Examples/FreeRTOS_AM335x_SPI_ILI9341/`**](./Examples/FreeRTOS_AM335x_SPI_ILI9341/) — ILI9341 SPI LCD demo.
- ⚙️ [**`Examples/FreeRTOS_AM335x_Task_Management/`**](./Examples/FreeRTOS_AM335x_Task_Management/) — Task lifecycle: create, suspend, resume, delete, and idle hook utilities over UART.
- 🔀 [**`Examples/FreeRTOS_AM335x_Task_Scheduler/`**](./Examples/FreeRTOS_AM335x_Task_Scheduler/) — Preemption, time slicing, and priority scheduling via UART logging.
- ⏱️ [**`Examples/FreeRTOS_AM335x_Task_Timing/`**](./Examples/FreeRTOS_AM335x_Task_Timing/) — Task delays: vTaskDelay, vTaskDelayUntil, and software timer demos.
- 🧠 [**`Examples/FreeRTOS_AM335x_Memory_Debug/`**](./Examples/FreeRTOS_AM335x_Memory_Debug/) — Static vs dynamic task allocation, heap schemes, and stack/heap monitoring via UART.
- 🔐 [**`Examples/FreeRTOS_AM335x_Semaphore/`**](./Examples/FreeRTOS_AM335x_Semaphore/) — Binary, counting, ISR semaphores, event notification, and task rendezvous demos.
- 🔒 [**`Examples/FreeRTOS_AM335x_Mutex/`**](./Examples/FreeRTOS_AM335x_Mutex/) — Mutex, recursive mutex, priority inheritance, critical sections, and UART peripheral protection.
- 📬 [**`Examples/FreeRTOS_AM335x_Queue/`**](./Examples/FreeRTOS_AM335x_Queue/) — Producer-Consumer FIFO queue demo: xQueueSend/Receive with blocking and queue length monitoring.
- 🔔 [**`Examples/FreeRTOS_AM335x_Task_Notification/`**](./Examples/FreeRTOS_AM335x_Task_Notification/) — Direct-to-task notifications: xTaskNotify, wait, take, and ISR signaling demonstrations.
- 💥 [**`Examples/FreeRTOS_AM335x_Event_Group/`**](./Examples/FreeRTOS_AM335x_Event_Group/) — Event group sync: set/clear bits, wait for bit combinations, via UART logging.
- ⏲️ [**`Examples/FreeRTOS_AM335x_Software_Timer/`**](./Examples/FreeRTOS_AM335x_Software_Timer/) — One-shot and auto-reload FreeRTOS software timers with callbacks managed by daemon task.
- 📨 [**`Examples/FreeRTOS_AM335x_Message_Buffer/`**](./Examples/FreeRTOS_AM335x_Message_Buffer/) — Queue‑based message passing with CRC validation and variable-length framing via UART.
- 📈 [**`Examples/FreeRTOS_AM335x_Stream_Buffer/`**](./Examples/FreeRTOS_AM335x_Stream_Buffer/) — Stream buffer for dynamic data flow, variable-size chunks, and circular buffer semantics via UART.
- 🛰️ [**`Examples/FreeRTOS_AM335x_UART_LDS08RR/`**](./Examples/FreeRTOS_AM335x_UART_LDS08RR/) — LDS08RR LiDAR decoder: UART1 Delta-2A polling, 360° scan assembly, and ASCII radial map on UART0.
- 📡 [**`Examples/FreeRTOS_AM335x_SPI_ILI9341_LDS08RR/`**](./Examples/FreeRTOS_AM335x_SPI_ILI9341_LDS08RR/) — LiDAR LDS08RR decoder via UART1, radar visualization on ILI9341 TFT via SPI0.

---

## Folder Layout

```
AM335x-FreeRTOS-Project/
├── README.md                  ← you are here
├── .gitignore
├── Doc/
│   └── bg.png                 ← banner image
└── Examples/                  ← all portable examples live here
    ├── FreeRTOS_AM335x_GPIO_LED/    ← FreeRTOS bare-metal port for AM3352 (LED blink tasks)
    ├── FreeRTOS_AM335x_GPIO_INTERRUPT/  ← FreeRTOS GPIO interrupt demo (button → LED speed)
    ├── FreeRTOS_AM335x_ISR_FromISR/   ← FreeRTOS ISR-from-ISR API demo (GPIO + FromISR functions)
    ├── FreeRTOS_AM335x_SPI_TX/       ← FreeRTOS SPI master TX demo (loopback, manual CS control)
    ├── FreeRTOS_AM335x_SPI_ILI9341/  ← FreeRTOS ILI9341 SPI LCD demo (graphics, color fills)
    ├── FreeRTOS_AM335x_Task_Management/ ← FreeRTOS task management demo (suspend/resume/delete)
    ├── FreeRTOS_AM335x_Task_Scheduler/  ← FreeRTOS scheduler & priority demo (preemption/time slicing)
    ├── FreeRTOS_AM335x_Task_Timing/     ← FreeRTOS task delay & timing demo (vTaskDelayUntil/timer)
    ├── FreeRTOS_AM335x_Memory_Debug/   ← FreeRTOS memory management demo (static/dynamic allocation, heap schemes)
    ├── FreeRTOS_AM335x_Semaphore/       ← FreeRTOS sync primitives demo (binary/counting/ISR/notify/sync)
    ├── FreeRTOS_AM335x_Mutex/           ← FreeRTOS mutex & resource protection demo (priority inheritance)
    ├── FreeRTOS_AM335x_Queue/           ← FreeRTOS queue task-communication demo (producer/consumer)
    ├── FreeRTOS_AM335x_Task_Notification/ ← FreeRTOS task notification demo (xTaskNotify/ISR notify)
    ├── FreeRTOS_AM335x_Event_Group/   ← FreeRTOS event group sync demo (set/clear/wait bits)
    ├── FreeRTOS_AM335x_Software_Timer/ ← FreeRTOS software timer demo (one-shot / auto-reload)
    ├── FreeRTOS_AM335x_Message_Buffer/ ← FreeRTOS message buffer demo (CRC, variable-length, UART)
    ├── FreeRTOS_AM335x_Stream_Buffer/ ← FreeRTOS stream buffer demo (dynamic data flow, circular buffer)
    ├── FreeRTOS_AM335x_UART_LDS08RR/    ← FreeRTOS LDS08RR LiDAR decoder demo (UART1 polling + ASCII scan map)
    └── FreeRTOS_AM335x_SPI_ILI9341_LDS08RR/ ← FreeRTOS LiDAR + ILI9341 radar TFT demo (UART1 + SPI0)
```

---

## Special Thanks

This repository's FreeRTOS port was bootstrapped from the work in [`kryochronic/AM335X-FreeRTOS-lwip`](https://github.com/kryochronic/AM335X-FreeRTOS-lwip). Thanks to the original author for the foundational AM3352 FreeRTOS port.

---

## License & Credits

This project contains code from:
- **FreeRTOS** — MIT licensed
- **TI StarterWare** — Texas Instruments BSD-style license
- **Amazon FreeRTOS** — Apache License 2.0

See individual files for specific license text.

<p align="center"><sub>Built for the Antminer L3+ (AM3352) • Powered by FreeRTOS v10.2.0 + StarterWare 02.00.01.01</sub></p>
