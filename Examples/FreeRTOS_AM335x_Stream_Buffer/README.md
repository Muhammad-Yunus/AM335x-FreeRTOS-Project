# FreeRTOS AM3352 Stream Buffer (SPSC Demo)

Port of FreeRTOS for Texas Instruments AM3352 (ARM Cortex-A8) platform with a **Stream Buffer** Single Producer Single Consumer (SPSC) demo. One FreeRTOS task sends variable-length byte streams to another task via the FreeRTOS Stream Buffer API. Demonstrates creation, send, receive, variable-length data, blocking (`portMAX_DELAY`), and timeout behavior. All observation is via UART logging — no GPIO interrupt, no LED blink.

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

- **Producer Task** (priority 2) — Creates a 64-byte stream buffer, sends variable-length payloads (1 B, 4 B, 16 B, 64 B), tests blocking and timeout sends, closes with a delete signal (`0xFE`).
- **Consumer Task** (priority 3) — Receives stream events with a 500 ms timeout, prints received bytes as hex, exits on delete signal.

The consumer runs at a higher priority than the producer, so it preempts the producer whenever data is available. All observation is via UART at 115200 8N1.

No SD card or external bootloader required when flashing via JTAG.

---

## Architecture

```
                            ┌─────────────────────────────────────┐
                            │        ARM Cortex-A8 (AM3352)       │
                            │         @ 600 MHz, VFP/Neon         │
                            ├─────────────────────────────────────┤
                            │          DDR SDRAM                  │
           Entry (.S)       │        0x80000000                   │
           start_boot() ──► │     ┌───────────────────────────┐   │
                            │     │     Vector Table          │   │
                            │     │     BSS Initialized       │   │
                            │     │     Heap 60 KB            │   │
                            │     │     Stack 10 MB           │   │
           ┌────────────────┤     └───────────────────────────┘   │
           │                 └─────────────────────────────────────┘
           │       FreeRTOS Kernel                        │
           │  ┌────────────────────────────────┐          │
           │  │   DMTimer2 ISR → Tick Handler  │          │
           │  │                                │          │
           │  │  Producer Task (prio 2)        │          │
           │  │    xStreamBufferCreate(64, 0)  │          │
           │  │    xStreamBufferSend() — 1, 4, │          │
           │  │      16, 64 bytes + timeout    │          │
           │  │    Delete signal 0xFE           │          │
           │  │    vTaskDelete(NULL)            │          │
           │  │                                │          │
           │  │  Consumer Task (prio 3)        │          │
           │  │    xStreamBufferReceive()      │          │
           │  │    UART hex print              │          │
           │  │    Delete on 0xFE              │          │
           │  │    vTaskDelete(NULL)            │          │
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
                         └── xStreamBufferCreate() before task creation
                              └── xTaskCreate — producer + consumer tasks
                                   └── vTaskStartScheduler()
```

### Interrupt Architecture

| Interrupt | Source | Sys IRQ # | Priority |
|-----------|--------|-----------|----------|
| DMTimer2 Tick Timer | FreeRTOS systick | `SYS_INT_TINT2` (82) | Lowest (`configMAX_IRQ_PRIORITIES − 1`) |

All ISRs are registered via `IntRegister()` into `fnRAMVectors[]` and dispatched from `vApplicationFPUSafeIRQHandler()` (`bsp_platform.c`) — no standard GIC used.

> ⚠️ **Key AINTC gotcha**: the FreeRTOS dispatcher calls `IntSystemDisable(index)` (masks the interrupt in AINTC via `MIR_SET`) before running the registered ISR. Every ISR must therefore call `IntSystemEnable()` for its own interrupt at the end, or it will only ever fire **once**. The DMTimer ISR does this (see `hal_bspInit.c`).

---

## Features

### ✅ Implemented & Verified (via JTAG + UART)

| Feature | Status | Details |
|---------|--------|---------|
| **Stream Buffer creation** | ✅ Tested | `xStreamBufferCreate(64, 0)` — 64-byte stream mode buffer |
| **Send 1 byte (blocking)** | ✅ Tested | `0xAB` → 1 byte sent, consumer receives it |
| **Send 4 bytes (blocking)** | ✅ Tested | `0xDEADBEEF` → 4 bytes sent in little-endian order |
| **Send 16 bytes (blocking)** | ✅ Tested | `0x00..0x0F` → 16 bytes sent sequentially |
| **Rapid fill 64 bytes** | ✅ Tested | Consumer prints 4 × 16-byte chunks via UART |
| **Non-blocking send (timeout = 0)** | ✅ Tested | Shows SUCCESS/timeout depending on buffer state |
| **Delete signal (0xFE)** | ✅ Tested | Consumer detects signal and exits |
| **Task self-delete** | ✅ Tested | Both producer and consumer call `vTaskDelete(NULL)` |
| **Serial Logger** | ✅ Active | UART 115200 8N1: all events logged |
| **MMU & Cache** | ✅ Enabled | Data cache on via `InitMem()` in `configure_platform()` |
| **FreeRTOS Scheduler** | ✅ Running | 1 ms tick via DMTimer2, preemptive, 10 priority levels |

---

## Project Structure

```
FreeRTOS_AM335x_Stream_Buffer/
├── CMakeLists.txt                     # Root build configuration
├── ProjectIncludes.cmake              # Include paths + library definitions
├── bbb.lds                            # Linker script (DDR at 0x80000000)
├── AM335xFreeRTOS_cmake_makefile_args.py  # Auto-generates CMake includes
├── gcc-a.toolchain.ccs12.cmake        # Toolchain file for CCS12 GCC ARM
├──flash.jlink                         # J-Link flash script (reference)
├── AGENT.md                           # Agent instructions
├── README.md                          # This file
├── docs/
│   ├── vision.md                      # Project vision
│   ├── requirements.md                 # Requirements
│   ├── architecture.md                 # Architecture design
│   ├── HOW_TO_BUILD.md                 # Build steps
│   └── HOW_TO_FLASH.md                 # Flash steps
├── src/
│   ├── application/
│   │   ├── main.c                     # Entry point; creates stream buffer & tasks
│   │   ├── stream_buffer_demo.c       # Producer + consumer task implementations
│   │   ├── app_utils.c                # FreeRTOS hooks (malloc fail, stack overflow)
│   │   └── CMakeLists.txt             # Added stream_buffer_demo.c
│   ├── inc/
│   │   ├── FreeRTOSConfig.h           # Kernel config (60 KB heap, 10 prios)
│   │   ├── stream_buffer_demo.h       # Shared stream buffer handle & prototypes
│   │   └── app_utils.h
│   └── portable/
│       ├── FreeRTOS/portable/GCC/ARM_CA8_amm335x/
│       │   ├── port.c                 # FreeRTOS C port (context switch, interrupts)
│       │   └── portASM_CA8_am335x.S   # ARM asm: SVC, IRQ, nested interrupts, FPU
│       ├── AM335X/
│       │   ├── hal_bspInit.c          # Board init: MMU, AINTC, UART, DMTimer2
│       │   ├── bsp_platform.c         # vApplicationFPUSafeIRQHandler (AINTC dispatch)
│       │   ├── ported_am335x_startup.c    # Vector table, DDR/PLL declarations
│       │   ├── ported_am335x_interrupt.c  # AINTC driver (register/unregister ISR)
│       │   ├── hal_mmu.c              # MMU setup
│       │   ├── ported_am335x_clock_data.c
│       │   └── ported_amm335x_init.S  # Reset handler, stack setup, BSS clear
│       └── syscalls_minimal.c         # newlib syscalls (_sbrk, _write, etc.)
├── lib/
│   └── third_party/
│       ├── ti/             — TI StarterWare 02.00.01.01
│       ├── amazon/         — Amazon FreeRTOS v10.2.0
│       └── c_source_tools/ — CMake utility
└── build/
    ├── freeRTOSBBB.elf            — Linked ELF executable
    └── app.freeRTOSBBB.bin        — Raw binary image (objcopy)
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

Checkout Amazon FreeRTOS to the commit used by this project and initialize submodules:

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

`gcc-a.toolchain.ccs12.cmake` sudah disediakan dan siap pakai tanpa edit.

#### 4. Clean Build (Recommended)

```powershell
Remove-Item -Recurse -Force build
```

#### 5. Configure and Build with Ninja

> **Penting:** Gunakan `-j1` (single-threaded build) saat linking. Project ini memiliki konflik parallel build di mana multiple target mencoba rename file `.a` yang sama secara bersamaan.

> **PowerShell note:** Gunakan `$PWD`, bukan `%cd%`. `%cd%` adalah syntax **cmd.exe**, tidak didukung di PowerShell.

```powershell
cmake -S . -B build -G Ninja "-DCMAKE_TOOLCHAIN_FILE=$PWD/gcc-a.toolchain.ccs12.cmake" -DCMAKE_C_COMPILER_WORKS=1
cmake --build build -- -j1
```

> **Note:** Step akhir mencoba membuat file BIN dan SDCARD image via `tiimage.exe`. Jika `tiimage.exe` tidak ada (tidak termasuk repo), langkah SDCARD akan gagal dengan exit code 1 tetapi **ELF dan BIN sudah terbangun dengan benar**. Periksa `build/freeRTOSBBB.elf` — jika ada, build sukses untuk JTAG flash.

#### 6. Verify Output

```
build/freeRTOSBBB.elf       — Linked ELF executable
build/app.freeRTOSBBB.bin   — Raw binary image (objcopy)
```

---

## Flash Instructions

### JTAG via J-Link GDB Server (Verified Working)

Connect J-Link probe to the AM335x JTAG header and open **two** terminals:

#### Terminal 1 — Start J-Link GDB Server

```powershell
Start-Process -FilePath "C:\Program Files\SEGGER\JLink_V844\JLinkGDBServer.exe" `
  -ArgumentList "-device AM3352 -if JTAG -speed 12000 -port 2331" -WindowStyle Minimized
Start-Sleep -Seconds 3
Get-NetTCPConnection -LocalPort 2331 | Select-Object State
```

State should show **`Listen`**.

#### Terminal 2 — Flash via GDB

**Option A — One-shot flash** (GDB exits after loading):

```powershell
C:\ti\gcc-arm-none-eabi-7-2018-q2-update\bin\arm-none-eabi-gdb.exe build/freeRTOSBBB.elf `
  -batch `
  -ex "target remote localhost:2331" `
  -ex "load" `
  -ex "monitor go 0x80000100"
```

> **Note:** `-batch` diperlukan agar GDB tidak bertanya "Quit anyway? (y or n)" saat sesi remote berakhir.

**Option B — Interactive flash** (GDB tetap terhubung untuk debugging):

```powershell
C:\ti\gcc-arm-none-eabi-7-2018-q2-update\bin\arm-none-eabi-gdb.exe build/freeRTOSBBB.elf
(gdb) target remote localhost:2331
(gdb) load
(gdb) continue
```

> **Important:** Jangan panggil `monitor reset` atau `monitor halt` **sebelum** `load`. State DDR AM335x harus preserved agar program bisa berjalan. Selalu mulai **JLinkGDBServer baru** untuk setiap sesi flash.

Serial output muncul di UART **115200 8N1** (COM4) setelah program berjalan.

---

## Debugging

### Attach GDB to Running Target

While J-Link GDB Server is running:

```powershell
"C:\ti\gcc-arm-none-eabi-7-2018-q2-update\bin\arm-none-eabi-gdb.exe" build/freeRTOSBBB.elf
(gdb) target extended-remote :2331
(gdb) file build/freeRTOSBBB.elf
(gdb) break vProducerTask
(gdb) continue
```

### Inspect Stream Buffer State

```powershell
(gdb) p/x gStreamBuffer
(gdb) p *(size_t *)0x80a44000   # gStreamBuffer BSS address
```

---

## Live UART Log

The serial logger outputs UART messages at 115200 8N1. Connect a serial terminal (COM4) to view live output. A verified session:

```
TIMER2_CLKSEL=0x00000001
Producer: Stream Buffer created (64 bytes)
Scheduler started
Enabling timer interrupt!
Timer interrupt enabled OK
Starting first task...
First tick!
Producer: send 1 byte 0xAB (blocking)
Consumer: recv 1 bytes 0xab
Producer: -> 1 bytes sent
Producer: send 4 bytes 0xDEADBEEF (blocking)
Consumer: recv 4 bytes 0xef 0xbe 0xad 0xde
Producer: -> 4 bytes sent
Producer: send 16 bytes (0x00..0x0F) (blocking)
Consumer: recv 16 bytes 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0a 0x0b 0x0c 0x0d 0x0e 0x0f
Producer: -> 16 bytes sent
Producer: rapid fill 64 bytes (consumer slow-prints)
Consumer: recv 16 bytes 0x10 0x11 0x12 0x13 0x14 0x15 0x16 0x17 0x18 0x19 0x1a 0x1b 0x1c 0x1d 0x1e 0x1f
Consumer: recv 16 bytes 0x20 0x21 0x22 0x23 0x24 0x25 0x26 0x27 0x28 0x29 0x2a 0x2b 0x2c 0x2d 0x2e 0x2f
Consumer: recv 16 bytes 0x30 0x31 0x32 0x33 0x34 0x35 0x36 0x37 0x38 0x39 0x3a 0x3b 0x3c 0x3d 0x3e 0x3f
Consumer: recv 16 bytes 0x40 0x41 0x42 0x43 0x44 0x45 0x46 0x47 0x48 0x49 0x4a 0x4b 0x4c 0x4d 0x4e 0x4f
Producer: -> 64 bytes sent
Consumer: recv 1 bytes 0xff
Producer: non-blocking SUCCESS (1 bytes sent, buffer had space)
Producer: sending delete signal (0xFE)
Consumer: recv 1 bytes 0xFE
Consumer: received delete signal
Consumer: deleting self
Producer: deleting self
```

**Reading the log:**

- `TIMER2_CLKSEL=0x00000001` — Timer clock is configured (DMTimer2 enabled)
- `First tick!` — FreeRTOS tick ISR fired for the first time
- `Scheduler started` — `vTaskStartScheduler()` called successfully
- Each `Producer: send ...` line is followed immediately by the corresponding `Consumer: recv` line, proving the stream buffer transferred data correctly
- `non-blocking SUCCESS (1 bytes sent, buffer had space)` — Timeout test confirmed buffer still had space after consumer drained it
- `recv 1 bytes 0xFE` — Delete signal received; consumer exits cleanly
- Both tasks call `vTaskDelete(NULL)` — no orphaned tasks remain

### Stream Buffer API Reference

| API | Purpose |
|-----|---------|
| `xStreamBufferCreate(64, 0)` | Create 64-byte stream buffer (stream mode, `0` = stream, `1` = message buffer) |
| `xStreamBufferSend(sb, data, len, timeout)` | Send bytes; `portMAX_DELAY` = block forever, `pdMS_TO_TICKS(ms)` = timeout |
| `xStreamBufferReceive(sb, buf, maxLen, timeout)` | Receive bytes; returns number of bytes actually received |
| `pdMS_TO_TICKS(ms)` | Convert milliseconds to FreeRTOS tick count |
| `portMAX_DELAY` | Indefinite block (0xFFFFFFFF) |
| `vTaskDelete(NULL)` | Delete the calling task |

---

## Known Issues

| Issue | Impact | Workaround |
|-------|--------|------------|
| `monitor reset` kills DDR state | Issuing `reset` before `load` in JTAG mode can corrupt DDR, causing crash | Use `load` + `continue` only; never `monitor reset` |
| J-Link sessions are not reusable | Reusing a stale J-Link GDB Server can cause target state mismatch | Always start a **fresh** JLinkGDBServer for each flash |
| No SD Card boot verified | TI image conversion and SD card boot not tested | Use JTAG for reliable flashing |
| COM4 access denied | Some app holds COM4 (PuTTY, MoTTY, etc.) | Close the app, or use a different COM port |
| `tiimage.exe` missing | SDCARD image generation step fails | Ignore — ELF & BIN are valid for JTAG |
| Consumer reads stale buffer before producer creates it | If stream buffer created *inside* producer task, consumer may read NULL | **Fixed** — buffer is created in `main()` before tasks are created |

---

## License

- **FreeRTOS** — MIT licensed
- **TI StarterWare** — Texas Instruments BSD-style license
- **Amazon FreeRTOS** — Apache License 2.0

See individual files for specific license text.
