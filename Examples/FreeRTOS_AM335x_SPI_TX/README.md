# FreeRTOS AM3352 SPI TX Demo

Port of FreeRTOS for Texas Instruments AM3352 (ARM Cortex-A8) platform with an **SPI TX** demo: transmits `0xAF` continuously via SPI0 (100 kHz, Mode 0) with manual CS control and loopback verification (MISO jumpered to MOSI). Also demonstrates GPIO DC/RST control for LCD initialization sequence.

![Hardware](https://img.shields.io/badge/Hardware-AM3352%20Cortex--A8-blue)
![FreeRTOS](https://img.shields.io/badge/RTOS-FreeRTOS%20v10.2.0-green)
![Build](https://img.shields.io/badge/Build-CMake%203.11+-purple)

---

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
- [Features](#features)
- [Project Structure](#project-structure)
- [Toolchain](#toolchain)
- [Build Instructions](#build-instructions)
- [Flash Instructions](#flash-instructions)
- [Hardware Setup](#hardware-setup)
- [Expected UART Log](#expected-uart-log)
- [Verification](#verification)
- [Known Issues](#known-issues)
- [License](#license)

---

## Overview

This project runs a bare-metal FreeRTOS kernel on a TI AM3352 SoC (BeagleBone Black / Antminer L3+ hardware). The system boots directly into DDR memory via JTAG, initializes MMU, AINTC, UART, DMTimer2, and SPI0, then starts a single FreeRTOS task:

- **SPI TX Task**: Transmits `0xAF` byte continuously via SPI0 at 100 kHz, Mode 0 (CPOL=0, CPHA=0), with manual CS control. Loopback verified by jumpering MISO (P9_21) to MOSI (P9_18).
- **GPIO DC/RST**: Controls DC (P8_26 = GPIO1_29) and RST (P8_19 = GPIO0_22) for LCD initialization pattern.
- **UART Logging**: Prints SPI TX/RX values every 1 second.

No SD card or external bootloader required when flashing via JTAG.

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                      ARM Cortex-A8 (AM3352)                        │
│                         @ 600 MHz                                  │
├─────────────────────────────────────────────────────────────────────┤
│                         DDR 0x80000000                             │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │  Vector Table @ 0x80000000                                  │   │
│  │  .text (code)                                               │   │
│  │  .data / .bss                                               │   │
│  │  Heap 10 MB                                                 │   │
│  │  Stack 10 MB                                                │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                      │
│  FreeRTOS Kernel                                                     │
│  ├── Task: spi_tx (SPI0 TX loop, priority 1)                        │
│  │     ├── Manual CS control per byte                                │
│  │     ├── McSPICSAssert → McSPITransmitData → poll TXS             │
│  │     ├── McSPIReceiveData (loopback)                               │
│  │     └── McSPICSDeAssert                                          │
│  │                                                                   │
│  └── DMTimer2 ISR @ 1 ms → FreeRTOS_Tick_Handler                    │
│                                                                      │
│  Board Init (hal_bspInit):                                          │
│  ├── MMU init (DDR + peripheral)                                     │
│  ├── AINTC init                                                      │
│  ├── DMTimer2 → FreeRTOS tick                                        │
│  ├── UART0 console                                                   │
│  ├── SPI0: 100 kHz, Mode 0, 8-bit, manual CS                        │
│  └── GPIO: DC (GPIO1_29), RST (GPIO0_22)                            │
│                                                                      │
│  SPI Pin Mapping:                                                   │
│  ├── P9_22 (SCLK) - CONTROL_CONF_SPI0_SCLK                          │
│  ├── P9_18 (MOSI) - CONTROL_CONF_SPI0_D1                            │
│  ├── P9_21 (MISO) - CONTROL_CONF_SPI0_D0  [FIXED]                   │
│  └── P9_17 (CS)   - CONTROL_CONF_SPI0_CS0                           │
└─────────────────────────────────────────────────────────────────────┘
```

### Boot Flow

```
Reset → .S (ported_amm335x_init.S)
           ├── Setup stacks for all 6 ARM modes
           ├── Enable Neon/VFP FPU
           ├── Clear BSS
           └── Call start_boot()
                ├── Copy vector table to 0x80000000
                ├── Set up IRQ/FIQ exception handlers
                └── Call main()
                     └── halBspInit()
                          ├── MMU init (DDR + peripheral)
                          ├── AINTC init
                          ├── DMTimer2 → FreeRTOS tick
                          ├── UART0 console
                          ├── SPI0: pinmux, reset, clock config
                          ├── GPIO1: DC pin (P8_26)
                          └── GPIO0: RST pin (P8_19)
                               └── Tasks Created
                                    └── vTaskStartScheduler()
```

---

## Features

### ✅ Implemented & Verified (via JTAG + Logic Analyzer)

| Feature | Status | Details |
|---------|--------|---------|
| **SPI0 TX Loop** | ✅ Verified | Continuous transmission of `0xAF` at 100 kHz, Mode 0 |
| **Manual CS Control** | ✅ Verified | `McSPICSAssert`/`McSPICSDeAssert` per byte |
| **Loopback RX** | ✅ Verified | MISO (P9_21) receives `0xAF` when jumpered to MOSI (P9_18) |
| **UART Logging** | ✅ Active | Prints TX/RX values every 1 second |
| **GPIO DC/RST** | ✅ Verified | DC=P8_26 (GPIO1_29), RST=P8_19 (GPIO0_22), mode 7 |
| **MMU & Cache** | ✅ Enabled | Data cache on via `InitMem()` in `configure_platform()` |
| **FreeRTOS Scheduler** | ✅ Running | 1 ms tick via DMTimer2, preemptive, 10 priority levels |

---

## Project Structure

```
FreeRTOS_AM335x_SPI_TX/
├── CMakeLists.txt                # (copy from base — not modified)
├── ProjectIncludes.cmake         # (auto-generated by Python script)
├── bbb.lds                       # (copy from base — not modified)
├── AM335xFreeRTOS_cmake_makefile_args.py  # (copy — not modified)
├── gcc-a.toolchain.ccs12.cmake   # (copy — not modified)
├── README.md                     # ← THIS FILE
├── docs/
│   ├── vision.md                 # Project vision & goals
│   ├── architecture.md           # System architecture details
│   ├── requirements.md           # Hardware/software requirements
│   ├── HOW_TO_BUILD.md           # Build instructions
│   └── HOW_TO_FLASH.md           # Flash instructions
├── src/
│   ├── application/
│   │   ├── main.c                # ← NEW: SPI task creation
│   │   ├── TaskSPI_TX.c          # ← NEW: SPI TX loop + UART logging
│   │   ├── TaskSPI_TX.h          # ← NEW: Task header
│   │   └── app_utils.c           # (copy — not modified)
│   ├── inc/
│   │   └── FreeRTOSConfig.h      # (copy — not modified)
│   └── portable/
│       ├── FreeRTOS/portable/GCC/ARM_CA8_amm335x/
│       │   ├── port.c            # (copy — not modified)
│       │   ├── portmacro.h       # (copy — not modified)
│       │   └── portASM_CA8_am335x.S  # (copy — not modified)
│       └── AM335X/
│           ├── hal_bspInit.c     # ← MODIFIED: SPI0 + GPIO DC/RST init
│           ├── bsp_platform.c    # (copy — not modified)
│           ├── hal_mmu.c         # (copy — not modified)
│           ├── ported_am335x_startup.c
│           ├── ported_am335x_interrupt.c
│           ├── ported_am335x_clock_data.c
│           ├── ported_amm335x_init.S
│           └── CMakeLists.txt    # ← MODIFIED: added portASM_CA8_am335x.S
└── lib/third_party/              # (clone same as base project)
    ├── ti/           — TI StarterWare 02.00.01.01
    ├── amazon/       — Amazon FreeRTOS v10.2.0
    └── c_source_tools/ — CMake utility
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

### Compiler_FLAGS

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
6. **TI StarterWare** cloned into `lib/third_party/ti` (same as the GPIO_INTERRUPT project)

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

> **Important**: This project **requires** **GCC_ARM 7.3.1** (`gcc-arm-none-eabi-7-2018-q2-update`) located at `C:/ti/gcc-arm-none-eabi-7-2018-q2-update`. Other GCC ARM versions (e.g., GCC 13.x from STM32CubeIDE) are **not compatible** because the path and prefix differ — this project uses `arm-none-eabi-`, not `arm-eabi-`.

`gcc-a.toolchain.ccs12.cmake` is provided and ready to use without editing.

#### 4. Clean Build (Recommended)

```powershell
Remove-Item -Recurse -Force build
```

#### 5. Configure and Build with Ninja

> **Important**: Use `-j1` (single-threaded build) during linking. This project has a parallel build conflict where multiple targets try to rename the same `.a` file simultaneously. Sequential build avoids this issue.

> **PowerShell note**: Replace `%cd%` with `$PWD` in the `CMAKE_TOOLCHAIN_FILE` line. `%cd%` is **cmd.exe** syntax and is not supported in PowerShell.

```powershell
cmake -S . -B build -G Ninja ^
  "-DCMAKE_TOOLCHAIN_FILE=$PWD/gcc-a.toolchain.ccs12.cmake" ^
  -DCMAKE_C_COMPILER_WORKS=1

cmake --build build -- -j1
```

> **Note**: The final build step attempts to generate both a BIN file and an SDCARD image via `tiimage.exe`. If `tiimage.exe` is missing (not included in this repo), the SDCARD image step will fail with exit code 1 but **the ELF and BIN files are already built and valid**. Check for `build/freeRTOSBBB.elf` — if it exists, the build succeeded for JTAG flashing.

#### 6. Verify Output

```
build/freeRTOSBBB.elf       — Linked ELF executable (~8.7 MB with debug symbols)
build/app.freeRTOSBBB.bin   — Raw binary image (objcopy)
```

---

## Flash Instructions

### JTAG via J-Link GDB Server (Verified Working)

Connect J-Link to the AM335x JTAG header and start **two** terminals:

#### Terminal 1 — Start J-Link GDB Server

```powershell
Start-Process -FilePath "C:\Program Files\SEGGER\JLink_V844\JLinkGDBServer.exe" `
  -ArgumentList "-device AM3352 -if JTAG -speed 12000 -port 2331" -WindowStyle Minimized
Start-Sleep -Seconds 3
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

> **Note**: `-batch` is required to prevent GDB from prompting "Quit anyway? (y or n)" when the remote session ends.

**Option B: Interactive flash** (GDB stays attached for debugging):

```powershell
C:\ti\gcc-arm-none-eabi-7-2018-q2-update\bin\arm-none-eabi-gdb.exe build/freeRTOSBBB.elf
(gdb) target remote localhost:2331
(gdb) load
(gdb) continue
```

> **Important**: Do **NOT** issue `monitor reset` or `monitor halt` before `load`. The AM335x DDR state must be preserved for the program to run. Always start a **fresh** JLinkGDBServer for each flash session.

Serial output appears on UART at **115200 8N1** (e.g. COM4) once the program is running.

---

## Hardware Setup

### Pin Mapping

#### SPI Pins (BeagleBone Header)

| Signal | Pin Header | Pad Control Register | Offset | Function |
|--------|-----------|---------------------|--------|----------|
| **CLK** | **P9_22** | `CONTROL_CONF_SPI0_SCLK` | 0x950 | SPI0 clock (CPOL=0, CPHA=0) |
| **MOSI** | **P9_18** | `CONTROL_CONF_SPI0_D1` | 0x958 | SPI0 data line 1 → output enabled by MCSPI_DATA_LINE_COMM_MODE_1 |
| **MISO** | **P9_21** | `CONTROL_CONF_SPI0_D0` | 0x954 | SPI0 data line 0 — input |
| **CS** | **P9_17** | `CONTROL_CONF_SPI0_CS0` | 0x95C | Hardware chip-select 0, active-low |

#### GPIO DC/RST Pins

| Signal | Pin Header | Register | GPIO Bank | GPIO # | Function |
|--------|-----------|----------|-----------|--------|----------|
| **DC** | **P8_26** | `CONTROL_CONF_GPMC_CSN(0)` | GPIO1 | 29 | Data/Command select (active HIGH) |
| **RST** | **P8_19** | `CONTROL_CONF_GPMC_AD(8)` | GPIO0 | 22 | Reset (active HIGH) |

### Loopback Setup

For TX/RX verification, jumper MISO to MOSI:

```
P9_18 (MOSI) ─── jumper ─── P9_21 (MISO)
```

### Logic Analyzer Probes

| Probe | Signal | Expected Waveform |
|-------|--------|------------------|
| CH0 | CS (P9_17) | LOW during transfer, HIGH idle |
| CH1 | SCLK (P9_22) | 100 kHz square wave during TX |
| CH2 | MOSI (P9_18) | 0xAF pattern (10101111) |
| CH3 | MISO (P9_21) | Same as MOSI (loopback) |

---

## Expected UART Log

```
TIMER2_CLKSEL=0x00000001
First tick!
[BSP] GPIO DC (GPIO1_29) initialized
[BSP] GPIO RST (GPIO0_22) initialized
[BSP] SPI0 initialized @ 100kHz, Mode 0
[TASK] Created: SPI TX Task
[TASK] SPI TX loop started
[TASK] SPI TX=0xaf, RX=0xaf
[TASK] SPI TX=0xaf, RX=0xaf
[TASK] SPI TX=0xaf, RX=0xaf
... (every 1 second)
```

---

## Verification

1. **Build succeeds** → `build/freeRTOSBBB.elf` and `build/app.freeRTOSBBB.bin` exist
2. **UART log** → appears on COM4 (115200 8N1) with `[TASK] SPI TX=0xaf, RX=0xaf`
3. **Logic Analyzer**:
   - **CS** goes LOW during each transfer
   - **SCLK** toggles at 100 kHz
   - **MOSI** and **MISO** show 0xAF pattern (same data)
4. **GPIO DC/RST** — verify with scope if LCD init pattern is needed

---

## Known Issues

| Issue | Impact | Workaround |
|-------|--------|------------|
| `tiimage.exe` missing | SDCARD image generation fails | Ignore — ELF and BIN are valid for JTAG flash |
| `CMAKE_TOOLCHAIN_FILE` warning | CMake warning during configure | Safe to ignore |
| Parallel build fails | `ren` conflict on `.a` files | Use `-j1` for single-threaded build |
| Cross-compiler ABI test fails | Bare-metal toolchain | Use `-DCMAKE_C_COMPILER_WORKS=1` |
| COM port locked | Cannot read UART via software | Check with your own serial terminal |
| MISO reads 0x00 initially | **FIXED** — missing pinmux for D0 | Added `CONTROL_CONF_SPI0_D0` to `hal_bspInit.c` |

### SPI Configuration Details

```c
/* SPI0 Configuration */
McSPIMasterModeConfig(SPI_BASE,
                      MCSPI_SINGLE_CH,              /* Single channel mode */
                      MCSPI_TX_RX_MODE,             /* TX/RX mode (not TX-only) */
                      MCSPI_DATA_LINE_COMM_MODE_1,  /* D1=MOSI, D0=MISO */
                      SPI_CH);
McSPIClkConfig(SPI_BASE,
               MCSPI_IN_CLK,    /* Input clock: 48 MHz (parent of SPI) */
               MCSPI_OUT_FREQ,  /* Output: 100 kHz */
               SPI_CH,
               MCSPI_CLK_MODE_0);  /* CPOL=0, CPHA=0 */
McSPICSPolarityConfig(SPI_BASE, MCSPI_CS_POL_LOW, SPI_CH);  /* Active-low CS */
McSPITxFIFOConfig(SPI_BASE, MCSPI_TX_FIFO_ENABLE, SPI_CH);  /* TX FIFO on */
McSPIRxFIFOConfig(SPI_BASE, MCSPI_RX_FIFO_ENABLE, SPI_CH);  /* RX FIFO on */
```

---

## License

This project contains code from:
- **FreeRTOS** — MIT licensed
- **TI StarterWare** — Texas Instruments BSD-style license
- **Amazon FreeRTOS** — Apache License 2.0

See individual files for specific license text.
