# AM335X FreeRTOS GPIO LED Blink

Port of FreeRTOS for Texas Instruments AM3352 (ARM Cortex-A8) platform with GPIO LED blink tasks. Originally based on the FreeRTOS Cortex-A9 port, adapted for AM3352's AINTC instead of GIC.

![Hardware](https://img.shields.io/badge/Hardware-AM3352%20Cortex--A8-blue) ![FreeRTOS](https://img.shields.io/badge/RTOS-FreeRTOS%20v10.2.0-green) ![Build](https://img.shields.io/badge/Build-CMake%203.11+-purple)

---

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
- [Features](#features)
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

This project runs a bare-metal FreeRTOS kernel on a TI AM3352 SoC (BeagleBone Black / Antminer L3+ hardware). The system boots directly into DDR memory via JTAG, initializes MMU and peripherals, then starts two FreeRTOS tasks:

- **LED Blink** — Two GPIO toggling tasks (Pin 23 at ~50ms ON/1s OFF, Pin 24 at ~250ms ON/1s OFF)
- **Serial Logger** — UART output with timer register dump and tick confirmation

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
         start_boot() ───►  │     ┌───────────────────────────┐   │
                            │     │     Vector Table          │   │
                            │     │     BSS Initialized       │   │
                            │     │     Heap 10MB             │   │
                            │     │     Stack 10MB            │   │
         ┌──────────────────┤     └───────────────────────────┘   │
         │                  └─────────────────────────────────────┘
         │       FreeRTOS Kernel                        │
         │  ┌────────────────────────────────┐          │
         │  │   xTaskCreate — LED23 (Pin 23) │          │
         │  │   xTaskCreate — LED24 (Pin 24) │          │
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
                   └── halBspInit() — MMU, AINTC, UART, DMTimer2
                        └── Tasks Created
                             └── vTaskStartScheduler()
```

### Interrupt Architecture

Unlike the standard FreeRTOS GIC-based ARM ports, this project uses TI's **Advanced Interrupt Controller (AINTC)**:

| Interrupt | Source | Priority |
|-----------|--------|----------|
| DMTimer2 Tick Timer | FreeRTOS systick | Lowest (0x3E) |

All ISRs are registered via custom `portAINTCRegisterISRHandler()` — no standard GIC used.

### Memory Map

| Address | Region | Size |
|---------|--------|------|
| `0x80000000` | Vector Table + Code (.text) | ~470 KB |
| `0x8006E000` | .data, .init, .fini | ~22 KB |
| `0x80070000` | Heap (10 MB) | 10 MB |
| `0x80B70000` | Stack (10 MB) | 10 MB |
| DDR0 | Total external memory | 1 GB |

---

## Features

### ✅ Implemented & Verified (via JTAG)

| Feature | Status | Details |
|---------|--------|---------|
| **LED Blink (Pin 23)** | ✅ Tested | GPIO Pin 23 high for ~50ms, low for ~1s |
| **LED Blink (Pin 24)** | ✅ Tested | GPIO Pin 24 high for ~250ms, low for ~1s |
| **Serial Logger** | ✅ Active | Timestamped output via UART 115200 8N1 |
| **MMU & Cache** | ✅ Enabled | Data cache on via `InitMem()` in `configure_platform()` |
| **FreeRTOS Scheduler** | ✅ Running | 1ms tick via DMTimer2, preemptive, 10 priority levels |

---

## Project Structure

```
AM335X-FreeRTOS-lwip/
├── CMakeLists.txt                # Root build configuration
├── ProjectIncludes.cmake         # Include paths + library definitions
├── bbb.lds                       # Linker script (DDR at 0x80000000)
├── AM335xFreeRTOS_cmake_makefile_args.py  # Auto-generates CMake includes
│
├── src/
│   ├── application/              # Application layer
│   │   ├── main.c                # Entry point, task creation (LED23, LED24)
│   │   ├── TaskLED2.c            # LED blink tasks (GPIO 23, 24)
│   │   └── app_utils.c           # FreeRTOS hooks (malloc fail, stack overflow)
│   │
│   ├── portable/                 # Platform adaptation layer
│   │   ├── FreeRTOS/
│   │   │   └── portable/GCC/ARM_CA8_amm335x/
│   │   │       ├── port.c          # FreeRTOS C port (context switch, interrupts)
│   │   │       └── portASM_CA8_am335x.S  # ARM asm: SVC, IRQ, nested interrupts, FPU
│   │   │
│   │   ├── AM335X/
│   │   │   ├── hal_bspInit.c       # Board init: MMU, AINTC, UART, timer
│   │   │   ├── ported_am335x_startup.c  # Vector table, DDR/PLL declarations
│   │   │   ├── ported_am335x_interrupt.c # AINTC driver (register/unregister ISR)
│   │   │   ├── hal_mmu.c           # MMU setup
│   │   │   ├── ported_am335x_clock_data.c
│   │   │   └── syscalls_minimal.c  # newlib syscalls (_sbrk, _write, etc.)
│   │   │
│   │   ├── AM335XInit/
│   │   │   └── ported_amm335x_init.S  # Reset handler, stack setup, BSS clear
│   │   │
│   └── inc/
│       ├── FreeRTOSConfig.h       # Kernel config (10MB heap, 10 prios)
│       ├── lwipopts.h             # (dead code — not compiled)
│       └── lwipopts_freertos.h    # (dead code — not compiled)
│
├── lib/
│   └── third_party/
│       ├── ti/                    # TI StarterWare (GPIO, UART, timers, AINTC)
│       │   ├── include/           # Headers (hw_*.h, clock.h, pinmux.h)
│       │   ├── drivers/           # Peripheral drivers
│       │   ├── board/             # Board abstraction
│       │   ├── mmcsdlib/          # SD card library (unused)
│       │   └── nandlib/           # NAND flash library (unused)
│       │
│       └── amazon/                # FreeRTOS v10.2.0 (Amazon fork)
│           ├── freertos_kernel/   # Kernel core
│           ├── demos/             # Demo apps (unused in this project)
│           └── libraries/         # BufferPool, FFS (unused)
│
├── scripts/                       # Build/utility scripts
├── build/                         # CMake output directory
│   ├── freeRTOSBBB.elf            # Final ELF executable
│   └── app.freeRTOSBBB.bin        # Raw binary image (objcopy)
│
├── gcc-a.toolchain.ccs12.cmake        # Toolchain file for CCS12 GCC ARM
└── gcc-a.toolchain.win10.sample.cmake # Sample toolchain file (template)
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

The build depends on three repositories that must be cloned into `lib/third_party/`. This project does not use Git submodules — clone each repo manually:

```powershell
New-Item -ItemType Directory -Force lib/third_party | Out-Null

git clone http://github.com/kryochronic/AM335X_StarterWare_02_00_01_01.git lib/third_party/ti

git clone https://github.com/aws/amazon-freertos.git lib/third_party/amazon

git clone http://github.com/kryochronic/c_source_tools lib/third_party/c_source_tools
```

Then checkout Amazon FreeRTOS to the specific commit used by this project and initialize its internal submodules:

```powershell
cd lib/third_party/amazon
git checkout 2bb3154718ecf0346e3236eef7039a27977d46d9
git submodule update --init --recursive
cd ../..
```

These directories are expected:

| Directory | Contents |
|---|---|
| `lib/third_party/ti` | TI StarterWare — GPIO, UART, timers, AINTC drivers |
| `lib/third_party/amazon` | Amazon FreeRTOS v10.2.0 kernel |
| `lib/third_party/c_source_tools` | CMake utility library for C source/header handling |
#### 2. Generate CMake Configuration

```powershell
python AM335xFreeRTOS_cmake_makefile_args.py
```

Script ini secara otomatis menghasilkan include paths dan referensi library untuk semua komponen TI dan FreeRTOS. Script juga melakukan post-processing untuk memperbaiki folder header-only dan menambahkan manual target sources untuk fungsi ARM CP15.

#### 3. Configure Toolchain

> ⚠️ **Penting**: Project ini **wajib** memakai **GCC ARM 7.3.1** (`gcc-arm-none-eabi-7-2018-q2-update`) yang berlokasi di `C:/ti/gcc-arm-none-eabi-7-2018-q2-update`. Versi GCC ARM lain (contoh: GCC 13.x dari STM32CubeIDE di `C:/ST/...`) **tidak kompatibel** karena path dan prefix-nya beda — project memakai `arm-none-eabi-`, bukan `arm-eabi-`. Jika direktori `C:/ti/gcc-arm-none-eabi-7-2018-q2-update` belum ada di sistem, install toolchain tersebut dulu sebelum lanjut ke Step 5.

Dua file toolchain disediakan:

- **`gcc-a.toolchain.ccs12.cmake`** (recommended) — untuk TI CCS 12 bundled toolchain di `C:/ti/gcc-arm-none-eabi-7-2018-q2-update`
- **`gcc-a.toolchain.win10.sample.cmake`** — sample file menggunakan prefix `arm-eabi-` (sesuaikan path sesuai kebutuhan)

Jika menggunakan CCS12 toolchain, tidak perlu editing. Jika tidak, copy dan edit sample:

```powershell
Copy-Item gcc-a.toolchain.win10.sample.cmake gcc-a.toolchain.cmake
```

Edit `gcc-a.toolchain.cmake` agar mengarah ke path compiler GCC ARM kamu:

```cmake
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER "C:/ti/gcc-arm-none-eabi-7-2018-q2-update/bin/arm-none-eabi-gcc.exe")
```

#### 4. Clean Build (Recommended)

Selalu hapus direktori build sebelum mengkonfigurasi ulang untuk menghindari state yang stale:

```powershell
Remove-Item -Recurse -Force build
```

#### 5. Configure and Build with Ninja

> **Penting:** Gunakan `-j1` (single-threaded build) saat linking. Project ini memiliki konflik parallel build di mana multiple target mencoba rename file `.a` yang sama secara bersamaan. Sequential build menghindari masalah ini.

> **PowerShell note:** Jika menggunakan PowerShell, ganti `%cd%` dengan `$PWD` pada baris `CMAKE_TOOLCHAIN_FILE`. Contoh: `"-DCMAKE_TOOLCHAIN_FILE=$PWD/gcc-a.toolchain.ccs12.cmake"`. `%cd%` adalah syntax **cmd.exe**, tidak didukung di PowerShell.

```powershell
cmake -S . -B build -G Ninja ^
  "-DCMAKE_TOOLCHAIN_FILE=%cd%/gcc-a.toolchain.ccs12.cmake" ^
  -DCMAKE_C_COMPILER_WORKS=1

cmake --build build -- -j1
```

> **Note:** `-DCMAKE_C_COMPILER_WORKS=1` skips the compiler ABI detection test which fails for bare-metal cross-compilers. This is safe for embedded targets.
>
> **Note:** You may see a CMake warning `Manually-specified variables were not used by the project: CMAKE_TOOLCHAIN_FILE`. This is harmless and can be ignored — the toolchain file is applied correctly despite the warning.

> **Note:** The final build step attempts to generate both a BIN file and an SDCARD image via `tiimage.exe`. If `tiimage.exe` is missing (it is not included in this repo), the SDCARD image step will fail with exit code 1 but **the ELF and BIN files are already built and valid**. Check for `build/freeRTOSBBB.elf` — if it exists, the build succeeded for JTAG flashing. To regenerate the BIN manually if needed:
> ```powershell
> arm-none-eabi-objcopy -O binary -B arm build/freeRTOSBBB.elf build/app.freeRTOSBBB.bin
> ```

#### 6. Verify Output

After a successful build, you should have:

```
build/freeRTOSBBB.elf       — Linked ELF executable (~8.1 MB with debug symbols)
build/app.freeRTOSBBB.bin   — Raw binary image (objcopy)
```

> **Note:** The SD card image (`freeRTOSBBB.sdcard`) is NOT generated automatically. The `tiimage.exe` tool required for TI SDCARD image conversion is not included in the repository (only source `.c` is present). See Step 5 for details on handling this.

---

## Flash Instructions

### JTAG via J-Link GDB Server (Verified Working)

Connect J-Link to the AM335x JTAG header and start **two** terminals:

#### Terminal 1 — Start J-Link GDB Server

Using the GUI application (recommended):

```
"C:\Program Files\SEGGER\JLink_V844\JLinkGDBServer.exe"
```

Configure in the GUI:
- **Device**: `AM3352`
- **Target interface**: `JTAG`
- **Speed**: `12000 kHz`
- **Port**: `2331`

Or launch from command line (optional):

```powershell
Start-Process -FilePath "C:\Program Files\SEGGER\JLink_V844\JLinkGDBServer.exe" `
  -ArgumentList "-if JTAG -device AM3352 -endian little -speed 12000 -port 2331"
```

Verify the GDB Server is ready before proceeding:

```powershell
Get-NetTCPConnection -LocalPort 2331 | Select-Object State
```

You should see `State = Listen`. Then wait for `"Connected to target"` in the GDB Server console/log **before** running the flash command from Terminal 2. If port 2331 shows `Listen` but no "Connected to target", check that J-Link hardware is properly connected to the AM335x JTAG header.

#### Terminal 2 — Flash via GDB

**Option A: One-shot flash** (GDB exits after loading):

```powershell
C:\ti\gcc-arm-none-eabi-7-2018-q2-update\bin\arm-none-eabi-gdb.exe build/freeRTOSBBB.elf `
  -batch `
  -ex "target remote localhost:2331" `
  -ex "load" `
  -ex "monitor go 0x80000100"
```

> **Note:** `-batch` is required to prevent GDB from prompting "Quit anyway? (y or n)" when the remote session ends. Using `-ex "quit"` without `-batch` will hang the terminal.

**Option B: Interactive flash** (GDB stays attached for debugging):

```powershell
C:\ti\gcc-arm-none-eabi-7-2018-q2-update\bin\arm-none-eabi-gdb.exe build/freeRTOSBBB.elf
(gdb) target remote localhost:2331
(gdb) load
(gdb) continue
```

Press **Ctrl+C** to break back to GDB for debugging.

> **Important:** Do NOT issue `monitor reset` or `monitor halt` before `load`. The AM335x DDR state must be preserved for the program to run. Always start a **fresh** JLinkGDBServer for each flash session. Reusing a previous instance can leave the target in an inconsistent state.

Serial output appears on UART at **115200 8N1** once the program is running.

> **Note:** Use `localhost:2331` (the JLink GDB Server runs on the development PC, not on the target's network IP).

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
(gdb) list main.c:main
(gdb) break main
(gdb) continue
```

---

## Live UART Log

The serial logger outputs UART messages at 115200 8N1. Connect a serial terminal to view live output:

```
TIMER2_CLKSEL=0x00000001
First tick!
```

- `"TIMER2_CLKSEL=..."` prints the timer clock select register value immediately after DMTimer setup
- `"First tick!"` prints once on the first DMTimer2 overflow interrupt (1ms tick)

---

## Known Issues

| Issue | Impact | Workaround |
|-------|--------|------------|
| `monitor reset` kills DDR state | Issuing `reset` before `load` in JTAG mode can corrupt DDR contents, causing crash | Use `load` + `continue` only; never `monitor reset` |
| J-Link sessions are not reusable | Reusing a stale J-Link GDB Server can cause target state mismatch | Always start a **fresh** JLinkGDBServer for each flash |
| No SD Card boot verified | TI image conversion and SD card boot sequence not tested | Use JTAG for reliable flashing |
| Timer clock runs at ~3 MHz instead of 24 MHz | Tick fires every 1ms as expected, but root clock source is at ~3 MHz instead of 24 MHz | Adjusted `TIMER_INITIAL_COUNT` and `TIMER_RLD_COUNT` to `0xfffff44c` (~1ms @ ~3MHz timer clock) in `hal_bspInit.c:54-55`. CLKSEL confirms CLK_M_OSC is selected but actual timer clock runs at ~3 MHz instead of 24 MHz. |

---

## License

This project contains code from:
- **FreeRTOS** — MIT licensed
- **TI StarterWare** — Texas Instruments BSD-style license
- **Amazon FreeRTOS** — Apache License 2.0

See individual files for specific license text.
