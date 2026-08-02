# FreeRTOS AM335x ISR FromISR Demo

Demonstrasi API FreeRTOS Interrupt-Safe (`*FromISR`) pada TI AM3352 (ARM Cortex-A8).

Port of FreeRTOS untuk Texas Instruments AM3352 (ARM Cortex-A8) platform dengan demo GPIO input interrupt: menekan pushbutton pada **P9_12** memicu edge interrupt yang mendemonstrasikan penggunaan API FreeRTOS FromISR.

![Hardware](https://img.shields.io/badge/Hardware-AM3352%20Cortex--A8-blue)
![FreeRTOS](https://img.shields.io/badge/RTOS-FreeRTOS%20v10.2.0-green)
![Build](https://img.shields.io/badge/Build-CMake%203.11+-purple)

---

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Architecture](#architecture)
- [Interrupt Handling (GPIO)](#interrupt-handling-gpio)
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

Project ini menjalankan bare-metal FreeRTOS kernel pada TI AM3352 SoC (BeagleBone Black / Antminer L3+ hardware). Sistem boot langsung ke DDR memory via JTAG, menginisialisasi MMU, AINTC, UART, dan DMTimer2, kemudian menjalankan beberapa FreeRTOS task:

- **GPIO ISR** — Menangani rising dan falling edge pada tombol P9_12 (GPIO1_28). Menggunakan 3 API FromISR:
  - `xQueueSendFromISR()` — Mengirim data event ke ConsumerTask
  - `xSemaphoreGiveFromISR()` — Memberikan binary semaphore
  - `xTaskNotifyFromISR()` — Notifikasi langsung ke MonitorTask
- **ConsumerTask** — Menerima data dari queue dan semaphore
- **MonitorTask** — Menerima notifikasi, membuat DynamicTask secara dinamis
- **DynamicTask** — Task temporer yang dibuat saat notifikasi diterima

Tidak memerlukan SD Card atau external bootloader saat flashing via JTAG.

---

## Features

### ✅ Implemented & Verified (via JTAG)

| Feature | Status | Details |
|---------|--------|---------|
| **GPIO Input Interrupt** | ✅ Tested | P9_12 (GPIO1_28) fire `GPIO1ISR` pada setiap press **dan** release (BOTH_EDGE) |
| **xQueueSendFromISR()** | ✅ Tested | ISR → ConsumerTask, mengirim `ISREventData_t` (pinLevel, timestamp) |
| **xSemaphoreGiveFromISR()** | ✅ Tested | ISR → ConsumerTask, binary semaphore signaling |
| **xTaskNotifyFromISR()** | ✅ Tested | ISR → MonitorTask, mengirimkan nilai level pin |
| **portYIELD_FROM_ISR()** | ✅ Tested | Context switch dipicu saat higher priority task woken |
| **Dynamic Task Creation** | ✅ Tested | MonitorTask membuat DynamicTask via `xTaskCreate()` |
| **Serial Logger** | ✅ Active | UART 115200 8N1: ISR + task events |
| **MMU & Cache** | ✅ Enabled | Data cache on via `InitMem()` di `configure_platform()` |
| **FreeRTOS Scheduler** | ✅ Running | 1 ms tick via DMTimer2, preemptive, 10 priority levels |

---

## Architecture

```
                            ┌─────────────────────────────────────┐
                            │        ARM Cortex-A8 (AM3352)       │
                            │         @ 600 MHz, VFP/Neon         │
                            ├─────────────────────────────────────┤
                            │          DDR SDRAM                  │
           Entry (.S)        │        0x80000000                   │
           start_boot() ──►  │     ┌───────────────────────────┐   │
                            │     │     Vector Table          │   │
                            │     │     BSS Initialized       │   │
                            │     │     Heap 60 KB            │   │
                            │     │     Stack 10 MB           │   │
           ┌──────────────────┤     └───────────────────────────┘   │
           │                  └─────────────────────────────────────┘
           │       FreeRTOS Kernel                        │
           │  ┌────────────────────────────────┐          │
           │  │   GPIO1ISR (AINTC IRQ 98)      │          │
           │  │   ├─ xQueueSendFromISR()       │          │
           │  │   ├─ xSemaphoreGiveFromISR()   │          │
           │  │   ├─ xTaskNotifyFromISR()      │          │
           │  │   └─ portYIELD_FROM_ISR()     │          │
           │  │                                │          │
           │  │   ConsumerTask (prio 3) ──────►│ Queue   │
           │  │   MonitorTask (prio 2) ──────►│ Notify  │
           │  │   DynamicTask (prio 1) ──────►│ Created │
           │  │                                │          │
           │  │   DMTimer2 ISR → Tick Handler  │          │
           │  └────────────────────────────────┘          │
           │       TI StarterWare Drivers                 │
           │  GPIO | UART | DMA Timer | AINTC             │
           └──────────────────────────────────────────────┘
```

### Button → GPIO mapping

| BBB header | Ball AM335x | Pad offset | Mux mode | GPIO bank | GPIO input # |
|------------|-------------|------------|----------|-----------|--------------|
| P9_12 | `gpmc_ben1` | `0x0878` (`GPIO_1_28`) | 7 | 1 | 60 |

- Sys interrupt: `SYS_INT_GPIOINT1A` (98) on `GPIO_INT_LINE_1`
- Trigger: `GPIO_INT_TYPE_BOTH_EDGE` (fires on press **and** release)
- Debounce: hardware debounce disabled for testing

### Level Values

| Value | Hex | Meaning |
|-------|-----|---------|
| `268435456` | `0x10000000` | Rising edge (button pressed) - bit 28 set |
| `0` | `0x0` | Falling edge (button released) |

---

## Interrupt Handling (GPIO)

### ISR Implementation (`src/application/ISR_demo.c`)

```c
void GPIO1ISR(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    unsigned int level = GPIOPinRead(SOC_GPIO_1_REGS, 28);

    // Clear interrupt
    GPIOPinIntClear(SOC_GPIO_1_REGS, GPIO_INT_LINE_1, 28);

    // Send to queue
    ISREventData_t event;
    event.pinLevel = level;
    event.timestamp = xTaskGetTickCount();
    xQueueSendFromISR (xISRQueue, &event, &xHigherPriorityTaskWoken);

    // Give semaphore
    xSemaphoreGiveFromISR (xISRSemaphore, &xHigherPriorityTaskWoken);

    // Notify task
    xTaskNotifyFromISR (xMonitorTask, level, eSetValueWithOverwrite, &xHigherPriorityTaskWoken);

    // Re-enable interrupt (CRITICAL: FreeRTOS port masks it)
    IntSystemEnable(SYS_INT_GPIOINT1A);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```

### Why this pattern (debugging history)

Interrupt initially fired **exactly once per boot**. Root cause was **not** the GPIO edge-detection registers — it was the FreeRTOS port's dispatcher (`vApplicationFPUSafeIRQHandler`, `bsp_platform.c:80`):

1. `IntActiveIrqNumGet()` reads the pending IRQ.
2. `IntSystemDisable(index)` masks it in AINTC (`MIR_SET`).
3. `IntIrqEnableNewIrqs()` writes `NEWIRQAGR` (AINTC EOI).
4. `fnRAMVectors[index]()` runs the ISR.
5. **The ISR is responsible for its own re-enable.**

The GPIO ISR must call `IntSystemEnable(SYS_INT_GPIOINT1A)` at the end, or IRQ 98 stays masked and every later press is silently dropped at the AINTC.

> ⚠️ **Key AINTC gotcha**: the FreeRTOS dispatcher calls `IntSystemDisable(index)` (masks the interrupt in AINTC via `MIR_SET`) **before** running the registered ISR. Every ISR must therefore call `IntSystemEnable()` for its own interrupt at the end, or it will only ever fire **once**.

---

## Project Structure

```
FreeRTOS_AM335x_ISR_FromISR/
├── CMakeLists.txt                # Root build configuration
├── ProjectIncludes.cmake         # Include paths + library definitions
├── bbb.lds                       # Linker script (DDR at 0x80000000)
├── AM335xFreeRTOS_cmake_makefile_args.py  # Auto-generates CMake includes
├── gcc-a.toolchain.ccs12.cmake   # Toolchain file for CCS12 GCC ARM
├── AGENT.md                      # Agent development guide
│
├── src/
│   ├── application/              # Application layer
│   │   ├── main.c                # Entry point, task creation
│   │   ├── ISR_demo.c            # GPIO1ISR + ConsumerTask + MonitorTask + DynamicTask
│   │   ├── ISR_demo.h            # Types and constants
│   │   └── app_utils.c           # FreeRTOS hooks (malloc fail, stack overflow)
│   │
│   ├── inc/
│   │   ├── FreeRTOSConfig.h      # Kernel config (60 KB heap, 10 prios)
│   │   └── app_utils.h
│   │
│   └── portable/                 # Platform adaptation layer
│       ├── FreeRTOS/
│       │   └── portable/GCC/ARM_CA8_amm335x/
│       │       ├── port.c          # FreeRTOS C port
│       │       └── portASM_CA8_am335x.S  # ARM asm: SVC, IRQ, nested interrupts, FPU
│       │
│       ├── AM335X/
│       │   ├── hal_bspInit.c       # Board init: MMU, AINTC, UART, timer, GPIO1 ISR
│       │   ├── bsp_platform.c      # vApplicationFPUSafeIRQHandler (AINTC dispatch)
│       │   ├── ported_am335x_startup.c  # Vector table, DDR/PLL declarations
│       │   ├── ported_am335x_interrupt.c # AINTC driver
│       │   ├── hal_mmu.c           # MMU setup
│       │   └── ...
│       └── syscalls_minimal.c      # newlib syscalls
│
├── lib/
│   └── third_party/
│       └── ti/                    # TI StarterWare (GPIO, UART, timers, AINTC)
│
├── docs/                          # Documentation
│   ├── vision.md                  # Project vision
│   ├── architecture.md           # System architecture
│   ├── requirements.md           # Requirements
│   ├── HOW_TO_BUILD.md           # Build instructions
│   └── HOW_TO_FLASH.md           # Flash instructions
├── build/                         # CMake output directory
│   ├── freeRTOSBBB.elf            # Final ELF executable
│   └── app.freeRTOSBBB.bin        # Raw binary image
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

### Step-by-Step

#### 1. Clone Third-Party Repositories

```powershell
New-Item -ItemType Directory -Force lib/third_party | Out-Null

git clone http://github.com/kryochronic/AM335X_StarterWare_02_00_01_01.git lib/third_party/ti
git clone https://github.com/aws/amazon-freertos.git lib/third_party/amazon
git clone http://github.com/kryochronic/c_source_tools lib/third_party/c_source_tools
```

Then checkout Amazon FreeRTOS to the commit used by this project:

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

> ⚠️ **Penting**: Project ini **wajib** memakai **GCC ARM 7.3.1** (`gcc-arm-none-eabi-7-2018-q2-update`) yang berlokasi di `C:/ti/gcc-arm-none-eabi-7-2018-q2-update`. Versi GCC ARM lain tidak kompatibel karena path dan prefix-nya beda.

`gcc-a.toolchain.ccs12.cmake` sudah disediakan dan siap dipakai tanpa edit.

#### 4. Clean Build (Recommended)

```powershell
Remove-Item -Recurse -Force build
```

#### 5. Configure and Build with Ninja

> **Penting:** Gunakan `-j1` (single-threaded build) saat linking. Project ini memiliki konflik parallel build di mana multiple target mencoba rename file `.a` yang sama secara bersamaan.

```powershell
cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE="$PWD/gcc-a.toolchain.ccs12.cmake" -DCMAKE_C_COMPILER_WORKS=1
cmake --build build -- -j1
```

> **Note:** The final build step attempts to generate both a BIN file and an SDCARD image via `tiimage.exe`. If `tiimage.exe` is missing (not included in this repo), the SDCARD image step will fail with exit code 1 but **the ELF and BIN files are already built and valid**. Check for `build/freeRTOSBBB.elf` — if it exists, the build succeeded for JTAG flashing.

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

> **Note:** `-batch` is required to prevent GDB from prompting "Quit anyway? (y or n)" when the remote session ends.

**Option B: Interactive flash** (GDB stays attached for debugging):

```powershell
C:\ti\gcc-arm-none-eabi-7-2018-q2-update\bin\arm-none-eabi-gdb.exe build/freeRTOSBBB.elf
(gdb) target remote localhost:2331
(gdb) load
(gdb) continue
```

> **Important:** Do NOT issue `monitor reset` or `monitor halt` before `load`. The AM335x DDR state must be preserved for the program to run. Always start a **fresh** JLinkGDBServer for each flash session.

Serial output appears on UART at **115200 8N1** (e.g. COM4) once the program is running.

---

## Debugging

### Attach GDB to Running Target

```powershell
"C:\ti\gcc-arm-none-eabi-7-2018-q2-update\bin\arm-none-eabi-gdb.exe"
(gdb) target extended-remote :2331
(gdb) file build/freeRTOSBBB.elf
(gdb) monitor halt
(gdb) info registers
(gdb) break GPIO1ISR
(gdb) continue
```

### Inspect GPIO1 Interrupt Registers Live

```powershell
(gdb) x/16wx 0x4804C024      # IRQSTATUS_RAW_0 … FALLINGDETECT
(gdb) p/x *(unsigned int*)0x4804C02C   # IRQSTATUS_0 (masked status)
(gdb) p/x *(unsigned int*)0x4804C034   # IRQSTATUS_SET_0 (enable mask)
```

If the status stays `0x10000000` but no ISR fires, first check that IRQ 98 is still unmasked in the AINTC (` gINTC_MIR_CLEAR3`, AINTC base `0x48200000`) — a missing `IntSystemEnable()` in the ISR is the usual cause.

---

## Live UART Log

The serial logger outputs UART messages at 115200 8N1. Connect a serial terminal (COM4) to view live output. A verified session pressing the button twice:

```
TIMER2_CLKSEL=0x00000001
[init] ISR demo queue and semaphore created.
[GPIO] Pin Mux Set
[GPIO] Dir Mode Set
[GPIO] Int Type Set
[GPIO] Int Enable
[GPIO] CTRL=0x50600801, OE=0x00000000, DEBOUNCE=0x00000000
[GPIO] ISR Registered
[GPIO] Priority Set
[GPIO] IRQ Enabled
[init] ISR FromISR demo initialized.
[main] ISR demo tasks created. Starting scheduler...
Enabling timer interrupt!
Timer interrupt enabled OK
Starting first task...
First tick!

--- Button Press (rising edge) ---
[ISR] FIRED! level=268435456
[ISR] IRQ: GPIOSTATUS=0x0, PINLEVEL=0x10000000
[ISR] Queue: level=268435456, ts=12994, woken=1
[ISR] Semaphore: woken=1
[ISR] Notify: value=268435456, woken=1
[ISR] portYIELD_FROM_ISR
[ISR] IRQ re-enabled

--- Button Release (falling edge) ---
[ISR] FIRED! level=0
[ISR] IRQ: GPIOSTATUS=0x0, PINLEVEL=0x0
[ISR] Queue: level=0, ts=13101, woken=1
[ISR] Semaphore: woken=1
[ISR] Notify: value=0, woken=1
[ISR] portYIELD_FROM_ISR
[ISR] IRQ re-enabled

[Consumer] Queue: pin=268435456, ts=12994
[Consumer] Semaphore taken
[Monitor] Notified: val=268435456
[Monitor] Creating DynTask... [DynTsk268435456] Iter 0
PASS
[DynTsk268435456] Iter 1
[DynTsk268435456] Iter 2
[DynTsk268435456] Done, deleting
```

---

## Known Issues

| Issue | Impact | Workaround |
|-------|--------|------------|
| `monitor reset` kills DDR state | Issuing `reset` before `load` in JTAG mode can corrupt DDR contents, causing crash | Use `load` + `continue` only; never `monitor reset` |
| J-Link sessions are not reusable | Reusing a stale J-Link GDB Server can cause target state mismatch | Always start a **fresh** JLinkGDBServer for each flash |
| No SD Card boot verified | TI image conversion and SD card boot sequence not tested | Use JTAG for reliable flashing |
| GPIO ISR fires only once (fixed) | Missing `IntSystemEnable()` in ISR leaves IRQ 98 masked in AINTC | Fixed — ISR re-enables its own AINTC interrupt (`ISR_demo.c`) |
| Timer clock runs at ~3 MHz instead of 24 MHz | Tick fires every 1 ms as expected, but root clock source is ~3 MHz instead of 24 MHz | Adjusted `TIMER_INITIAL_COUNT` / `TIMER_RLD_COUNT` to `0xfffff44c` (~1 ms @ ~3 MHz) in `hal_bspInit.c` |
| `tiimage.exe` not found | SDCARD image generation fails (exit code 1) | Ignore — ELF and BIN are valid for JTAG flashing |

---

## License

This project contains code from:
- **FreeRTOS** — MIT licensed
- **TI StarterWare** — Texas Instruments BSD-style license
- **Amazon FreeRTOS** — Apache License 2.0

See individual files for specific license text.
