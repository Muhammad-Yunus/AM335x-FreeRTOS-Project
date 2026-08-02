# FreeRTOS AM3352 Software Timer Demo

Port of FreeRTOS for Texas Instruments AM3352 (ARM Cortex-A8) platform with a **Software Timer** demonstration: creating and managing FreeRTOS Software Timers (One-shot and Auto-reload), using `xTimerCreate()`, `xTimerStart()`, and callback functions. Output is logged via UART console.

![Hardware](https://img.shields.io/badge/Hardware-AM3352%20Cortex--A8-blue) ![FreeRTOS](https://img.shields.io/badge/RTOS-FreeRTOS%20v10.2.0-green) ![Build](https://img.shields.io/badge/Build-CMake%203.11+-purple)

---

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
- [Features](#features)
- [Software Timer Configuration](#software-timer-configuration)
- [Project Structure](#project-structure)
- [Toolchain](#toolchain)
- [Build Instructions](#build-instructions)
- [Flash Instructions](#flash-instructions)
- [Debugging](#debugging)
- [Live UART Log](#live-uart-log)
- [Known Issues](#known-issues)
- [License](#license)

---

## Overview

This project runs a bare-metal FreeRTOS kernel on a TI AM3352 SoC (BeagleBone Black / Antminer L3+ hardware). The system boots directly into DDR memory via JTAG, initializes MMU, AINTC, UART, and DMTimer2, then starts a single FreeRTOS task:

- **Timer Monitor Task** — Creates and starts two FreeRTOS Software Timers (One-shot and Auto-reload), logs timer lifecycle events to UART.
- **One-shot Timer** — Fires once after 5 seconds (5000 ms), triggers a callback, then stops.
- **Auto-reload Timer** — Fires every 2 seconds (2000 ms) periodically, triggers a callback with an execution counter.

No SD card or external bootloader required when flashing via JTAG.

---

## Architecture

```
                            ┌─────────────────────────────────────┐
                            │        ARM Cortex-A8 (AM3352)       │
                            │         @ 600 MHz, VFP/Neon         │
                            ├─────────────────────────────────────┤
                            │          DDR SDRAM                  │
          Entry (.S)         │        0x80000000                   │
          start_boot() ──►   │     ┌───────────────────────────┐   │
                            │     │     Vector Table          │   │
                            │     │     BSS Initialized       │   │
                            │     │     Heap 60 KB            │   │
                            │     │     Stack 10 MB           │   │
          ┌──────────────────┤     └───────────────────────────┘   │
          │                  └─────────────────────────────────────┘
          │       FreeRTOS Kernel                        │
          │  ┌────────────────────────────────┐          │
          │  │   Timer Service Task (Daemon)  │          │
          │  │   - Manages One-shot Timer     │          │
          │  │   - Manages Auto-reload Timer  │          │
          │  │   xTaskCreate — Timer_Monitor  │          │
          │  │   DMTimer2 ISR → Tick Handler  │          │
          │  └────────────────────────────────┘          │
          │       TI StarterWare Drivers                 │
          │  UART | DMTimer | AINTC                      │
          └──────────────────────────────────────────────┘
```

### Boot Flow

```
Reset ─► .S (ported_amm335x_init.S)
          ├── Setup stacks for all 6 ARM modes
          ├── Enable Neon/VFP FPU
          ├── Clear BSS
          └── Call start_boot()
               ├── Copy vector table to 0x80000000
               ├── Set up IRQ/FIQ exception handlers
               └── Call main()
                    └── halBspInit() — MMU, AINTC, UART, DMTimer2
                         └── Task Created (Timer_Monitor)
                              └── vTaskStartScheduler()
                                   └── Timer Service Task auto-created
```

---

## Features

### Implemented & Verified (via JTAG)

| Feature | Status | Details |
|---------|--------|---------|
| **Timer Service Task** | Tested | FreeRTOS Daemon Task auto-created on scheduler start (`configUSE_TIMERS=1`) |
| **xTimerCreate()** | Tested | One-shot Timer (5000 ms) and Auto-reload Timer (2000 ms) created successfully |
| **xTimerStart()** | Tested | Both timers started from Timer_Monitor task |
| **One-shot Timer** | Tested | Fires once at T=5s, callback executed, timer stops |
| **Auto-reload Timer** | Tested | Fires every 2s periodically, callback executed with execution counter |
| **Timer Callback** | Tested | Callback functions print status to UART (non-blocking) |
| **Serial Logger** | Active | UART 115200 8N1: Timer lifecycle events |
| **MMU & Cache** | Enabled | Data cache on via `InitMem()` in `configure_platform()` |
| **FreeRTOS Scheduler** | Running | 1 ms tick via DMTimer2, preemptive, 10 priority levels |

---

## Software Timer Configuration

### FreeRTOSConfig.h Settings

```c
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               3
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            configMINIMAL_STACK_SIZE
```

### Timer Parameters

| Timer | Type | Period | Auto-reload | Callback |
|-------|------|--------|-------------|----------|
| One-shot Timer | `pdFALSE` | 5000 ms | No | `prvOneShotTimerCallback` |
| Auto-reload Timer | `pdTRUE` | 2000 ms | Yes | `prvAutoReloadTimerCallback` |

### Timer Lifecycle

```
Timer Monitor Task
    ├── xTimerCreate("OneShot", 5000ms, pdFALSE, ...)
    ├── xTimerCreate("AutoReload", 2000ms, pdTRUE, ...)
    ├── xTimerStart(OneShot, 0)
    └── xTimerStart(AutoReload, 0)
         │
         ▼
    Timer Service Task (Daemon)
         ├── Waits for timer queue commands
         ├── Processes xTimerStart commands
         ├── Manages timer expiry
         └── Calls callbacks in daemon context
```

---

## Project Structure

```
FreeRTOS_AM335x_Software_Timer/
├── CMakeLists.txt                # Root build configuration
├── ProjectIncludes.cmake         # Include paths + library definitions
├── bbb.lds                       # Linker script (DDR at 0x80000000)
├── AM335xFreeRTOS_cmake_makefile_args.py  # Auto-generates CMake includes
├── gcc-a.toolchain.ccs12.cmake   # Toolchain file for CCS12 GCC ARM
│
├── src/
│   ├── application/              # Application layer
│   │   ├── main.c                # Entry point, task creation (Timer_Monitor)
│   │   ├── TaskTimerDemo.c       # Timer setup, callbacks, monitoring task
│   │   └── app_utils.c           # FreeRTOS hooks (malloc fail, stack overflow)
│   │
│   ├── inc/
│   │   ├── FreeRTOSConfig.h      # Kernel config (60 KB heap, 10 prios, timers enabled)
│   │   └── app_utils.h
│   │
│   └── portable/                 # Platform adaptation layer
│       ├── FreeRTOS/
│       │   └── portable/GCC/ARM_CA8_amm335x/
│       │       ├── port.c          # FreeRTOS C port (context switch, interrupts)
│       │       └── portASM_CA8_am335x.S  # ARM asm: SVC, IRQ, nested interrupts, FPU
│       │
│       ├── AM335X/
│       │   ├── hal_bspInit.c       # Board init: MMU, AINTC, UART, DMTimer2
│       │   ├── bsp_platform.c      # vApplicationFPUSafeIRQHandler (AINTC dispatch)
│       │   ├── ported_am335x_startup.c  # Vector table, DDR/PLL declarations
│       │   ├── ported_am335x_interrupt.c # AINTC driver (register/unregister ISR)
│       │   ├── hal_mmu.c           # MMU setup
│       │   ├── ported_am335x_clock_data.c
│       │   └── ported_amm335x_init.S  # Reset handler, stack setup, BSS clear
│       └── syscalls_minimal.c      # newlib syscalls (_sbrk, _write, etc.)
│
├── docs/
│   ├── vision.md                # Project vision and goals
│   ├── requirements.md          # Functional and non-functional requirements
│   └── architecture.md          # System architecture and design
│
├── lib/                         # Third-party libraries
│   └── third_party/
│       ├── ti/                  # TI StarterWare (UART, timers, AINTC)
│       ├── amazon/              # Amazon FreeRTOS v10.2.0 (kernel)
│       └── c_source_tools/      # CMake utility
│
├── build/                       # CMake output directory
│   ├── freeRTOSBBB.elf          # Final ELF executable
│   └── app.freeRTOSBBB.bin      # Raw binary image (objcopy)
│
└── AGENT.md                     # Agent configuration for this project
```

---

## Toolchain

| Component | Version | Path Example |
|-----------|---------|-------------|
| **Compiler** | GCC ARM 7.3.1 | `gcc-arm-none-eabi-7-2018-q2-update` |
| **Build System** | CMake 3.11+ | `cmake` |
| **Flash/Debug** | J-Link Software | `JLinkGDBServer V8.44` |
| **RTOS** | FreeRTOS v10.2.0 (Amazon fork) | — |
| **Peripheral Lib** | TI AM335x StarterWare | — |
| **Target CPU** | ARM Cortex-A8 (AM3352) | `-mcpu=cortex-a8 -march=armv7-a` |
| **FPU** | VFPv3-SP + NEON | `-mfpu=neon -mfloat-abi=hard` |

### Compiler Flags

```
-mcpu=cortex-a8 -march=armv7-a -marm -mfloat-abi=hard -mfpu=neon
-mlong-calls -fdata-sections -funsigned-char -ffunction-sections -Wall
-O0 -g3 -Dgcc -Dam335x -Dbeaglebone -DMMCSD -DUARTCONSOLE -DSUPPORT_UNALIGNED
```

---

## Build Instructions

### Prerequisites

1. **Git** — with submodule support
2. **CMake** 3.11+ (tested up to 4.x)
3. **Python** 2.x or 3.x
4. **GNU ARM GCC** 7.3.1 (recommended: `gcc-arm-none-eabi-7-2018-q2-update`)
5. **Ninja** build system
6. **TI StarterWare** cloned into `lib/third_party/ti`

### Step-by-Step

#### 1. Clone Third-Party Repositories

```powershell
New-Item -ItemType Directory -Force lib/third_party | Out-Null
git clone http://github.com/kryochronic/AM335X_StarterWare_02_00_01_01.git lib/third_party/ti
git clone https://github.com/aws/amazon-freertos.git lib/third_party/amazon
git clone http://github.com/kryochronic/c_source_tools lib/third_party/c_source_tools
```

Then checkout Amazon FreeRTOS to the commit used by this project and initialize its internal submodules:

```powershell
cd lib/third_party/amazon
git checkout 2bb3154718ecf0346e3236eef7039a27977d46d9
git submodule update --init --recursive
cd ../..
```

#### 2. Generate CMake Configuration

```powershell
python AM335xFreeRTOS_cmake_makefile_args.py
```

#### 3. Configure Toolchain

> Note: `gcc-a.toolchain.ccs12.cmake` is already configured and ready to use without edits.

#### 4. Clean Build (Recommended)

```powershell
Remove-Item -Recurse -Force build
```

#### 5. Configure and Build with Ninja

> Note: Use `-j1` (single-threaded build) to avoid parallel build conflicts.

```powershell
cmake -S . -B build -G Ninja "-DCMAKE_TOOLCHAIN_FILE=gcc-a.toolchain.ccs12.cmake" -DCMAKE_C_COMPILER_WORKS=1
cmake --build build -- -j1
```

> Note: The final build step attempts to generate both a BIN file and an SDCARD image via `tiimage.exe`. If `tiimage.exe` is missing, the SDCARD image step will fail but **the ELF and BIN files are already built and valid**.

#### 6. Verify Output

```
build/freeRTOSBBB.elf       — Linked ELF executable
build/app.freeRTOSBBB.bin   — Raw binary image (objcopy)
```

---

## Flash Instructions

### JTAG via J-Link GDB Server (Verified Working)

Connect J-Link to the AM335x JTAG header and start **two** terminals:

#### Terminal 1 — Start J-Link GDB Server

```powershell
Start-Process -FilePath "C:\Program Files\SEGGER\JLink_V844\JLinkGDBServer.exe" `
  -ArgumentList "-if JTAG -device AM3352 -endian little -speed 12000 -port 2331"
```

Verify the GDB Server is ready before proceeding:

```powershell
Get-NetTCPConnection -LocalPort 2331 | Select-Object State
```

You should see `State = Listen`.

#### Terminal 2 — Flash via GDB

**Option A: One-shot flash** (GDB exits after loading):

```powershell
C:\ti\gcc-arm-none-eabi-7-2018-q2-update\bin\arm-none-eabi-gdb.exe build/freeRTOSBBB.elf `
  -batch `
  -ex "target remote localhost:2331" `
  -ex "load" `
  -ex "monitor go 0x80000100"
```

**Option B: Interactive flash** (GDB stays attached for debugging):

```powershell
C:\ti\gcc-arm-none-eabi-7-2018-q2-update\bin\arm-none-eabi-gdb.exe build/freeRTOSBBB.elf
(gdb) target remote localhost:2331
(gdb) load
(gdb) continue
```

> Important: Do NOT issue `monitor reset` or `monitor halt` before `load`. Always start a fresh JLinkGDBServer for each flash.

Serial output appears on UART at **115200 8N1** (e.g. COM4) once the program is running.

---

## Debugging

### Attach GDB to Running Target

While J-Link GDB Server is running:

```powershell
(gdb) target extended-remote :2331
(gdb) file build/freeRTOSBBB.elf
(gdb) monitor halt
(gdb) info registers
(gdb) break prvOneShotTimerCallback
(gdb) continue
```

---

## Live UART Log

The serial logger outputs UART messages at 115200 8N1. Connect a serial terminal (COM4) to view live output. A verified session:

```
TIMER2_CLKSEL=0x00000001
First tick!
Scheduler started
Timer Monitor Task created!
One-shot Timer created!
Auto-reload Timer created!
Starting both timers...
Auto-reload Timer Callback triggered! (Count: 1)
Auto-reload Timer Callback triggered! (Count: 2)
One-shot Timer Callback triggered!
Auto-reload Timer Callback triggered! (Count: 3)
Auto-reload Timer Callback triggered! (Count: 4)
Auto-reload Timer Callback triggered! (Count: 5)
Auto-reload Timer Callback triggered! (Count: 6)
Auto-reload Timer Callback triggered! (Count: 7)
Auto-reload Timer Callback triggered! (Count: 8)
Auto-reload Timer Callback triggered! (Count: 9)
Auto-reload Timer Callback triggered! (Count: 10)
```

- `Auto-reload Timer Callback triggered! (Count: N)` — fires every 2 seconds, counter increments
- `One-shot Timer Callback triggered!` — fires once at T=5s, then stops

---

## Known Issues

| Issue | Impact | Workaround |
|-------|--------|------------|
| `monitor reset` kills DDR state | Issuing `reset` before `load` in JTAG mode can corrupt DDR contents | Use `load` + `continue` only; never `monitor reset` |
| J-Link sessions are not reusable | Reusing a stale J-Link GDB Server can cause target state mismatch | Always start a fresh JLinkGDBServer for each flash |
| No SD Card boot verified | TI image conversion and SD card boot sequence not tested | Use JTAG for reliable flashing |
| `tiimage.exe` missing | SDCARD image generation fails | ELF & BIN files are valid; ignore SDCARD generation error |

---

## License

This project contains code from:
- **FreeRTOS** — MIT licensed
- **TI StarterWare** — Texas Instruments BSD-style license
- **Amazon FreeRTOS** — Apache License 2.0

See individual files for specific license text.
