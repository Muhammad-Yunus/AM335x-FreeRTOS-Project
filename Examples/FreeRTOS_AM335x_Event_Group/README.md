# FreeRTOS AM3352 GPIO Interrupt (Button → LED Speed)

Port of FreeRTOS for Texas Instruments AM3352 (ARM Cortex-A8) platform with a GPIO input interrupt demo: pressing a pushbutton on **P9_12** fires an edge interrupt that speeds up the on-board **LED D2** (GPIO1_21 / USR0) blink. Originally based on the FreeRTOS Cortex-A9 port, adapted for AM3352's AINTC instead of GIC.

![Hardware](https://img.shields.io/badge/Hardware-AM3352%20Cortex--A8-blue) ![FreeRTOS](https://img.shields.io/badge/RTOS-FreeRTOS%20v10.2.0-green) ![Build](https://img.shields.io/badge/Build-CMake%203.11+-purple)

---

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
- [Features](#features)
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

This project runs a bare-metal FreeRTOS kernel on a TI AM3352 SoC (BeagleBone Black / Antminer L3+ hardware). The system boots directly into DDR memory via JTAG, initializes MMU, AINTC, UART, and DMTimer2, then starts a single FreeRTOS task:

- **GPIO Interrupt Task** — Blinks the on-board LED **D2** (GPIO1_21 / USR0) at a symmetric base rate of ~1000 ms ON / 1000 ms OFF. Each **rising edge** on the button (P9_12 = GPIO1_28) decrements the blink delay by 100 ms (down to 0 ms, then resets to 1000 ms).
- **GPIO ISR** — Handles both rising and falling edges (`BOTH_EDGE`), clears the GPIO status, and re-arms the interrupt in the AINTC.

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
         │  │   GPIO1ISR (AINTC IRQ 98)      │          │
         │  │   xTaskCreate — gpio_int task  │          │
         │  │   DMTimer2 ISR → Tick Handler  │          │
         │  └────────────────────────────────┘          │
         │       TI StarterWare Drivers                 │
         │  GPIO | UART | DMA Timer | AINTC             │
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
                   └── halBspInit() — MMU, AINTC, UART, DMTimer2,
                       GPIO1 clock + module + GPIO1_28 interrupt config
                        └── Task Created (gpio_interrupt_task)
                             └── vTaskStartScheduler()
```

### Interrupt Architecture

Unlike the standard FreeRTOS GIC-based ARM ports, this project uses TI's **Advanced Interrupt Controller (AINTC)**:

| Interrupt | Source | Sys IRQ # | Priority |
|-----------|--------|-----------|----------|
| DMTimer2 Tick Timer | FreeRTOS systick | `SYS_INT_TINT2` (82) | Lowest (configMAX_IRQ_PRIORITIES − 1) |
| GPIO1 Button | P9_12 → GPIO1_28, `GPIO_INT_LINE_1` | `SYS_INT_GPIOINT1A` (98) | 0x10 (higher than tick) |

All ISRs are registered via `IntRegister()` into `fnRAMVectors[]` and dispatched from `vApplicationFPUSafeIRQHandler()` (`bsp_platform.c`) — no standard GIC used.

> ⚠️ **Key AINTC gotcha**: the FreeRTOS dispatcher calls `IntSystemDisable(index)` (masks the interrupt in AINTC via `MIR_SET`) **before** running the registered ISR. Every ISR must therefore call `IntSystemEnable()` for its own interrupt at the end, or it will only ever fire **once**. The DMTimer ISR already does this; the GPIO ISR must too (see below).

### GPIO Register Map (used here)

GPIO1 base `0x4804C000`. `set 0` = interrupt line 0.

| Offset | Register | Usage |
|--------|----------|-------|
| `0x024` | `GPIO_IRQSTATUS_RAW_0` | Raw event latch (read) |
| `0x02C` | `GPIO_IRQSTATUS_0` | Masked status — **write 1 to clear** |
| `0x034` | `GPIO_IRQSTATUS_SET_0` | Interrupt enable set |
| `0x03C` | `GPIO_IRQSTATUS_CLR_0` | Interrupt enable clear |
| `0x148` | `GPIO_RISINGDETECT` | Rising edge detect enable |
| `0x14C` | `GPIO_FALLINGDETECT` | Falling edge detect enable |
| `0x134` | `GPIO_OE` | Direction (1 = input) |
| `0x138` | `GPIO_DATAIN` | Read input level |

---

## Features

### ✅ Implemented & Verified (via JTAG)

| Feature | Status | Details |
|---------|--------|---------|
| **GPIO Input Interrupt** | ✅ Tested | P9_12 (GPIO1_28) fires `GPIO1ISR` on every press **and** release (BOTH_EDGE) |
| **LED Blink (D2, Pin 21)** | ✅ Tested | GPIO1_21 (USR0), symmetric delay: 1000 ms ON/OFF default, −100 ms per press down to 0 ms, then reset to 1000 ms |
| **Button → Speed-up** | ✅ Tested | Each rising edge decrements the blink delay by 100 ms; resets to 1000 ms at 0 ms |
| **Serial Logger** | ✅ Active | UART 115200 8N1: ISR + button events |
| **MMU & Cache** | ✅ Enabled | Data cache on via `InitMem()` in `configure_platform()` |
| **FreeRTOS Scheduler** | ✅ Running | 1 ms tick via DMTimer2, preemptive, 10 priority levels |

---

## Interrupt Handling (GPIO)

### Button → GPIO mapping

| BBB header | Ball AM335x | Pad offset | Mux mode | GPIO bank | GPIO input # |
|------------|-------------|------------|----------|-----------|--------------|
| P9_12 | `gpmc_ben1` | `0x0878` (`GPIO_1_28`) | 7 | 1 | 60 |

- Sys interrupt: `SYS_INT_GPIOINT1A` (98) on `GPIO_INT_LINE_1`
- Trigger: `GPIO_INT_TYPE_BOTH_EDGE` (fires on press **and** release; only the rising edge decrements the delay)
- Debounce: hardware debounce removed for testing — software bounces are visible in the log as repeated `ISR fired!` lines

### On-board LED silkscreen mapping (this board)

The 4 user LEDs map to the BeagleBone USRx signals as follows (verified by observation — USR2 = GPIO1_23 physically lights **D4**):

| Silkscreen | USRx | GPIO |
|------------|------|------|
| **D2** | USR0 | GPIO1_21 |
| D3 | USR1 | GPIO1_22 |
| D4 | USR2 | GPIO1_23 |
| D5 | USR3 | GPIO1_24 |

### ISR (`src/application/TaskLED2.c`)

```c
void GPIO1ISR(void)
{
    unsigned int level = GPIOPinRead(SOC_GPIO_1_REGS, 28);

    GPIOPinIntClear(SOC_GPIO_1_REGS, GPIO_INT_LINE_1, 28);

    if (level != 0) {   /* rising edge = button pressed */
        gButtonPressed = 1;
    }
    ConsoleUtilsPrintf("ISR fired! pin_level=%u\r\n", (unsigned int)level);

    /* FreeRTOS port masks the interrupt in AINTC before dispatch, so each
       ISR must re-enable itself (same pattern as DMTimerIsr). Without this
       the GPIO interrupt only ever fires once. */
    IntSystemEnable(SYS_INT_GPIOINT1A);

    portYIELD_FROM_ISR(pdTRUE);
}
```

### Why this pattern (debugging history)

The interrupt initially fired **exactly once per boot**. Root cause was **not** the GPIO edge-detection registers — it was the FreeRTOS port's dispatcher (`vApplicationFPUSafeIRQHandler`, `bsp_platform.c:80`):

1. `IntActiveIrqNumGet()` reads the pending IRQ.
2. `IntSystemDisable(index)` masks it in AINTC (`MIR_SET`).
3. `IntIrqEnableNewIrqs()` writes `NEWIRQAGR` (AINTC EOI).
4. `fnRAMVectors[index]()` runs the ISR.
5. **The ISR is responsible for its own re-enable.**

The GPIO ISR was missing `IntSystemEnable(SYS_INT_GPIOINT1A)`, so after the first press IRQ 98 stayed masked and every later press was silently dropped at the AINTC. A working bare-metal TI-CCS/StarterWare reference (`AM3352_GPIO_INTERRUPT/main.c`) has no `IntSystemDisable` in its dispatcher at all, which is why it worked without any re-enable.

> **Note:** A redundant "force-release + re-arm RISING/FALLINGDETECT" workaround in the ISR (added under the wrong hypothesis about `IRQSTATUS_RAW`) was **removed** once the AINTC re-enable was identified — the detector dance is unnecessary and can create spurious events.

---

## Project Structure

```
FreeRTOS_AM335x_GPIO_INTERRUPT/
├── CMakeLists.txt                # Root build configuration
├── ProjectIncludes.cmake         # Include paths + library definitions
├── bbb.lds                       # Linker script (DDR at 0x80000000)
├── AM335xFreeRTOS_cmake_makefile_args.py  # Auto-generates CMake includes
├── gcc-a.toolchain.ccs12.cmake   # Toolchain file for CCS12 GCC ARM
│
├── src/
│   ├── application/              # Application layer
│   │   ├── main.c                # Entry point, task creation (GPIO_INT)
│   │   ├── TaskLED2.c            # GPIO1ISR + gpio_interrupt_task (blink)
│   │   └── app_utils.c           # FreeRTOS hooks (malloc fail, stack overflow)
│   │
│   ├── inc/
│   │   ├── FreeRTOSConfig.h      # Kernel config (60 KB heap, 10 prios)
│   │   ├── TaskLED2.h            # Task param typedefs
│   │   └── app_utils.h
│   │
│   └── portable/                 # Platform adaptation layer
│       ├── FreeRTOS/
│       │   └── portable/GCC/ARM_CA8_amm335x/
│       │       ├── port.c          # FreeRTOS C port (context switch, interrupts)
│       │       └── portASM_CA8_am335x.S  # ARM asm: SVC, IRQ, nested interrupts, FPU
│       │
│       ├── AM335X/
│       │   ├── hal_bspInit.c       # Board init: MMU, AINTC, UART, timer, GPIO1 ISR
│       │   ├── bsp_platform.c      # vApplicationFPUSafeIRQHandler (AINTC dispatch)
│       │   ├── ported_am335x_startup.c  # Vector table, DDR/PLL declarations
│       │   ├── ported_am335x_interrupt.c # AINTC driver (register/unregister ISR)
│       │   ├── hal_mmu.c           # MMU setup
│       │   ├── ported_am335x_clock_data.c
│       │   └── ported_amm335x_init.S  # Reset handler, stack setup, BSS clear
│       └── syscalls_minimal.c      # newlib syscalls (_sbrk, _write, etc.)
│
├── lib/
│   └── third_party/
│       └── ti/                    # TI StarterWare (GPIO, UART, timers, AINTC)
│           ├── include/           # Headers (hw_*.h, gpio_v2.h, interrupt.h)
│           ├── drivers/           # Peripheral drivers (gpio_v2.c, …)
│           └── …
│
├── docs/                          # Vision / architecture / build / flash notes
├── build/                         # CMake output directory
│   ├── freeRTOSBBB.elf            # Final ELF executable
│   └── app.freeRTOSBBB.bin        # Raw binary image (objcopy)
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
6. **TI StarterWare** cloned into `lib/third_party/ti` (same as the GPIO_LED project)

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

> ⚠️ **Penting**: Project ini **wajib** memakai **GCC ARM 7.3.1** (`gcc-arm-none-eabi-7-2018-q2-update`) yang berlokasi di `C:/ti/gcc-arm-none-eabi-7-2018-q2-update`. Versi GCC ARM lain tidak kompatibel karena path dan prefix-nya beda — project memakai `arm-none-eabi-`, bukan `arm-eabi-`.

`gcc-a.toolchain.ccs12.cmake` sudah disediakan dan siap dipakai tanpa edit.

#### 4. Clean Build (Recommended)

```powershell
Remove-Item -Recurse -Force build
```

#### 5. Configure and Build with Ninja

> **Penting:** Gunakan `-j1` (single-threaded build) saat linking. Project ini memiliki konflik parallel build di mana multiple target mencoba rename file `.a` yang sama secara bersamaan.

> **PowerShell note:** Ganti `%cd%` dengan `$PWD` pada baris `CMAKE_TOOLCHAIN_FILE`. `%cd%` adalah syntax **cmd.exe**, tidak didukung di PowerShell.

```powershell
cmake -S . -B build -G Ninja ^
  "-DCMAKE_TOOLCHAIN_FILE=$PWD/gcc-a.toolchain.ccs12.cmake" ^
  -DCMAKE_C_COMPILER_WORKS=1

cmake --build build -- -j1
```

> **Note:** The final build step attempts to generate both a BIN file and an SDCARD image via `tiimage.exe`. If `tiimage.exe` is missing (not included in this repo), the SDCARD image step will fail with exit code 1 but **the ELF and BIN files are already built and valid**. Check for `build/freeRTOSBBB.elf` — if it exists, the build succeeded for JTAG flashing.

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

While J-Link GDB Server is running:

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

If the status stays `0x10000000` but no ISR fires, first check that IRQ 98 is still unmasked in the AINTC (`INTC_MIR_CLEAR3`, AINTC base `0x48200000`) — a missing `IntSystemEnable()` in the ISR is the usual cause.

---

## Live UART Log

The serial logger outputs UART messages at 115200 8N1. Connect a serial terminal (COM4) to view live output. A verified session pressing the button repeatedly:

```
TIMER2_CLKSEL=0x00000001
Scheduler started
Enabling timer interrupt!
Timer interrupt enabled OK
Starting first task...
First tick!
ISR fired! pin_level=268435456
ISR fired! pin_level=268435456
Button pressed, delay=900 ms
ISR fired! pin_level=0
ISR fired! pin_level=268435456
ISR fired! pin_level=0
Button pressed, delay=800 ms
...
Button pressed, delay=100 ms
ISR fired! pin_level=268435456
ISR fired! pin_level=0
Button pressed, delay=1000 ms
```

- `pin_level=268435456` (0x10000000) → rising edge (button pressed) → decrements delay
- `pin_level=0` → falling edge (button released) → no decrement
- `Button pressed, delay=XXX ms` → the task applying the new blink rate (ON and OFF share the same delay)
- Delay wraps: 0 ms → 1000 ms (reset at minimum)

---

## Known Issues

| Issue | Impact | Workaround |
|-------|--------|------------|
| `monitor reset` kills DDR state | Issuing `reset` before `load` in JTAG mode can corrupt DDR contents, causing crash | Use `load` + `continue` only; never `monitor reset` |
| J-Link sessions are not reusable | Reusing a stale J-Link GDB Server can cause target state mismatch | Always start a **fresh** JLinkGDBServer for each flash |
| No SD Card boot verified | TI image conversion and SD card boot sequence not tested | Use JTAG for reliable flashing |
| GPIO ISR fires only once (fixed) | Missing `IntSystemEnable()` in ISR leaves IRQ 98 masked in AINTC | Fixed — ISR re-enables its own AINTC interrupt (`TaskLED2.c`) |
| Timer clock runs at ~3 MHz instead of 24 MHz | Tick fires every 1 ms as expected, but root clock source is ~3 MHz instead of 24 MHz | Adjusted `TIMER_INITIAL_COUNT` / `TIMER_RLD_COUNT` to `0xfffff44c` (~1 ms @ ~3 MHz) in `hal_bspInit.c:55-56` |

---

## License

This project contains code from:
- **FreeRTOS** — MIT licensed
- **TI StarterWare** — Texas Instruments BSD-style license
- **Amazon FreeRTOS** — Apache License 2.0

See individual files for specific license text.
