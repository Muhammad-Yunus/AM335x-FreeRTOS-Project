# FreeRTOS AM3352 Mutex & Resource Protection

Port of FreeRTOS for Texas Instruments AM3352 (ARM Cortex-A8) demonstrating **Mutex & Resource Protection**: Mutex, Recursive Mutex, Priority Inheritance, Shared Resource, Critical Section, and Peripheral Protection (UART). All output is UART-only — **no LED, no GPIO interrupt**. Originally based on the FreeRTOS Cortex-A9 port, adapted for AM3352's AINTC instead of GIC.

![Hardware](https://img.shields.io/badge/Hardware-AM3352%20Cortex--A8-blue) ![FreeRTOS](https://img.shields.io/badge/RTOS-FreeRTOS%20v10.2.0-green) ![Build](https://img.shields.io/badge/Build-CMake%203.11+-purple)

---

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
- [Demonstrations](#demonstrations)
- [Expected UART Log](#expected-uart-log)
- [Project Structure](#project-structure)
- [Toolchain](#toolchain)
- [Build Instructions](#build-instructions)
- [Flash Instructions](#flash-instructions)
- [Known Issues](#known-issues)
- [License](#license)

---

## Overview

This project runs a bare-metal FreeRTOS kernel on a TI AM3352 SoC (BeagleBone Black / Antminer L3+ hardware). The system boots directly into DDR memory via JTAG, initializes MMU, AINTC, UART, and DMTimer2, then starts the scheduler with **13 tasks** that demonstrate how shared resources and the UART peripheral are protected in FreeRTOS.

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
                            │     │     Heap 10 MB            │   │
                            │     │     Stack 10 MB           │   │
         ┌──────────────────┤     └───────────────────────────┘   │
         │                  └─────────────────────────────────────┘
         │       FreeRTOS Kernel                        │
         │  ┌──────────────────────────────────────────┐ │
         │  │ xSharedResourceMutex (mutex biasa)       │ │
         │  │ xRecursiveMutex      (recursive mutex)   │ │
         │  │ xPriorityInheritanceMutex (mutex biasa)  │ │
         │  │ xUARTMutex           (proteksi UART)     │ │
         │  │ xRxQueue             (simulasi RX UART)  │ │
         │  └──────────────────────────────────────────┘ │
         │       TI StarterWare Drivers                  │
         │       UART | DMA Timer | AINTC                │
         └────────────────────────────────────────────────┘
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
                        ├── Create 4 mutexes + RX queue
                        ├── Create 13 demo tasks
                        └── vTaskStartScheduler()
```

---

## Demonstrations

| # | Concept | Files | How it works |
|---|---------|-------|--------------|
| 1 | **Mutex** | `MutexDemo.c` | Writer (prio 2) & Reader (prio 1) access a shared `SharedAccount_t`; every update and dump happens inside `xSemaphoreTake/Give` |
| 2 | **Recursive Mutex** | `RecursiveMutexDemo.c` | One task takes the same mutex 3 levels deep (L1→L2→L3) via `xSemaphoreTakeRecursive` — a normal mutex would deadlock here |
| 3 | **Priority Inheritance** | `PriorityInheritanceDemo.c` | PILow (1) holds a mutex for 800 ms; PIHigh (3) blocks on it; the log shows PILow boosted to priority 3, restored to 1 after give |
| 4 | **Shared Resource** | `MutexDemo.c` | `SharedAccount_t` (balance + write counter) only touched inside the mutex region |
| 5 | **Critical Section** | `CriticalSectionDemo.c` | CS-A (2) & CS-B (1) increment a shared counter inside `taskENTER_CRITICAL`/`EXIT` (nested 2 levels); value printed after exit |
| 6 | **Peripheral Protection** | `UARTProtectionDemo.c` | UART console as a shared peripheral: multi-line frames written atomically + simulated RX→TX echo, all under `xUARTMutex` |
| + | **Task Lifecycle** | `TaskLifecycleDemo.c` | Supervisor creates/deletes workers and logs every event (self-delete and external delete) |

### Priority Inheritance demo sequence

| Task | Priority | Action |
|------|----------|--------|
| `PILow` | 1 | Takes `xPriorityInheritanceMutex`, holds it while sleeping 800 ms |
| `PIHigh` | 3 | After 300 ms tries to take the same mutex → **blocks**, boosting PILow to 3 |
| `PIMid` | 2 | Unrelated task running every 200 ms (control) |

Without priority inheritance, `PIMid` would keep preempting `PILow`, delaying the mutex release and starving `PIHigh`.

---

## Expected UART Log

Connect a serial terminal on **COM4** (115200 8N1). A verified session shows:

```
TIMER2_CLKSEL=0x00000001
First tick!
All mutexes created OK
Scheduler started
[MutexWr] task created (priority=2)
[MutexRd] task created (priority=1)
[RecMutx] task created (priority=2)
[PILow]  task created (priority=1)
[PIMid]  task created (priority=2)
[PIHigh] task created (priority=3)
[CS-A] task created (priority=2)
[CS-B] task created (priority=1)
[UART-A] task created (priority=2)
[UART-B] task created (priority=1)
[ECHO-RX] task created (priority=2)
[ECHO-TX] task created (priority=1)
[Supervisor] task created (priority=2)

[Writer] MUTEX held: balance 1000 -> 1100 (writes=1)      ← Mutex / Shared Resource
[Reader] MUTEX held: dump balance=1100 writes=1
[Recursive] RECURSIVE take L1 (count=1)                   ← Recursive Mutex
[Recursive] RECURSIVE take L2 (count=2)
[Recursive] RECURSIVE take L3 (count=3)
[Recursive] RECURSIVE give  L3 (count=2)
[Recursive] RECURSIVE give  L2 (count=1)
[Recursive] RECURSIVE give  L1 (count=0, mutex free)
[PILow]  MUTEX HELD, sleeping 800 ms (priority=1)          ← Priority Inheritance
[PIHigh] trying to take mutex (BLOCKING, this boosts PILow)...
[PILow]  woke up, priority now=3 (BOOSTED while High waits)
[PILow]  mutex released, priority after give=1
[PIHigh] MUTEX ACQUIRED after priority inheritance
[CS-A] critical section: counter=1 (update 1)             ← Critical Section
[CS-B] critical section: counter=2 (update 1)
[UART] frame BEGIN  owner=A                               ← Peripheral Protection
[UART]   A frame line 1 (protected)
[UART]   A frame line 2 (protected)
[UART]   A frame line 3 (protected)
[UART]   A frame line 4 (protected)
[UART] frame END    owner=A
[ECHO-RX] simulated UART RX byte 'A' queued               ← UART echo simulation
[ECHO-TX] echo 'A' -> A (UART TX protected)
[Supervisor] task 1 "WkSelfDel" created (xTaskCreate=1)   ← Task Lifecycle
[WkSelfDel] running iteration 1
...
[WkSelfDel] deleting itself (vTaskDelete(NULL))
[Supervisor] deleting worker 2 externally (vTaskDelete(handle))
[Supervisor] worker 2 deleted, supervisor done, deleting self
```

---

## Project Structure

```
FreeRTOS_AM335x_Mutex/
├── CMakeLists.txt                # Root build configuration
├── ProjectIncludes.cmake         # Include paths + library definitions
├── bbb.lds                       # Linker script (DDR at 0x80000000)
├── AM335xFreeRTOS_cmake_makefile_args.py  # Auto-generates CMake includes
├── gcc-a.toolchain.ccs12.cmake   # Toolchain file for CCS12 GCC ARM
│
├── src/
│   ├── application/              # Application layer
│   │   ├── main.c                # Entry point: mutexes + 13 demo tasks
│   │   ├── MutexDemo.c           # Mutex + shared resource (Writer/Reader)
│   │   ├── RecursiveMutexDemo.c  # Recursive mutex (nested L1-L3)
│   │   ├── PriorityInheritanceDemo.c  # Priority inheritance (Low/Mid/High)
│   │   ├── CriticalSectionDemo.c # Critical section (nested, CS-A/CS-B)
│   │   ├── UARTProtectionDemo.c  # UART frame + echo simulation
│   │   ├── TaskLifecycleDemo.c   # Task create/delete logging
│   │   └── app_utils.c           # FreeRTOS hooks (malloc fail, stack overflow)
│   │
│   ├── inc/
│   │   ├── FreeRTOSConfig.h      # Kernel config (10 MB heap, 10 prios, mutex ON)
│   │   ├── MutexDemo.h
│   │   ├── RecursiveMutexDemo.h
│   │   ├── PriorityInheritanceDemo.h
│   │   ├── CriticalSectionDemo.h
│   │   ├── UARTProtectionDemo.h
│   │   ├── TaskLifecycleDemo.h
│   │   └── app_utils.h
│   │
│   └── portable/                 # Platform adaptation layer
│       ├── FreeRTOS/portable/GCC/ARM_CA8_amm335x/   # port.c, portASM, portmacro
│       ├── AM335X/               # bsp_platform.c, hal_bspInit.c, hal_mmu.c, …
│       └── syscalls_minimal.c    # newlib syscalls (_sbrk, _write, etc.)
│
├── lib/
│   └── third_party/              # Cloned dependencies (StarterWare, FreeRTOS, tools)
│
├── docs/                         # vision / architecture / requirements / build / flash
├── build/                        # CMake output directory
│   ├── freeRTOSBBB.elf           # Final ELF executable
│   └── app.freeRTOSBBB.bin       # Raw binary image (objcopy)
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

#### 3. Clean Build (Recommended)

```powershell
Remove-Item -Recurse -Force build
```

#### 4. Configure and Build with Ninja

> **Penting:** Gunakan `-j1` (single-threaded build) saat linking. Project ini memiliki konflik parallel build di mana multiple target mencoba rename file `.a` yang sama secara bersamaan.
>
> **PowerShell note:** Ganti `%cd%` dengan `$PWD` pada baris `CMAKE_TOOLCHAIN_FILE`.

```powershell
cmake -S . -B build -G Ninja ^
  "-DCMAKE_TOOLCHAIN_FILE=$PWD/gcc-a.toolchain.ccs12.cmake" ^
  -DCMAKE_C_COMPILER_WORKS=1

cmake --build build -- -j1
```

> **Note:** The final build step attempts to generate both a BIN file and an SDCARD image via `tiimage.exe`. If `tiimage.exe` is missing (not included in this repo), the SDCARD image step will fail with exit code 1 but **the ELF and BIN files are already built and valid**. Check for `build/freeRTOSBBB.elf`.

#### 5. Verify Output

```
build/freeRTOSBBB.elf       — Linked ELF executable
build/app.freeRTOSBBB.bin   — Raw binary image (objcopy)
```

---

## Flash Instructions

### JTAG via J-Link GDB Server (Verified Working)

#### Terminal 1 — Start J-Link GDB Server

```powershell
Start-Process -FilePath "C:\Program Files\SEGGER\JLink_V844\JLinkGDBServer.exe" `
  -ArgumentList "-device AM3352 -if JTAG -speed 2000 -port 2331" -WindowStyle Minimized
Start-Sleep -Seconds 3
```

Verify the GDB Server is ready:

```powershell
Get-NetTCPConnection -LocalPort 2331 | Select-Object State
```

#### Terminal 2 — Flash via GDB

```powershell
C:\ti\gcc-arm-none-eabi-7-2018-q2-update\bin\arm-none-eabi-gdb.exe build/freeRTOSBBB.elf `
  -batch `
  -ex "target remote localhost:2331" `
  -ex "load" `
  -ex "monitor go 0x80000100"
```

> **Important:** Do NOT issue `monitor reset` or `monitor halt` before `load`. The AM335x DDR state must be preserved for the program to run. Always start a **fresh** JLinkGDBServer for each flash session. If the PC or board was recently restarted, you must power-cycle the board and wait 15 seconds to let U-Boot initialize the DDR controller before starting J-Link GDB Server.
>
> Serial output appears on UART at **115200 8N1** (e.g. COM4) once the program is running.

---

## Known Issues

| Issue | Impact | Workaround |
|-------|--------|------------|
| `monitor reset` kills DDR state | Issuing `reset` before `load` in JTAG mode can corrupt DDR contents | Use `load` + `continue` only; never `monitor reset` |
| J-Link sessions are not reusable | Reusing a stale J-Link GDB Server can cause target state mismatch / connection lost | Always start a **fresh** JLinkGDBServer for each flash; use speed **2000 kHz** |
| Cold Boot / PC Restart Connection Lost | Target connection lost when attempting JTAG writes because DDR controller is uninitialized | Power cycle the board, wait 15 seconds for U-Boot to boot and configure DDR, then run GDB load |
| `ERRORu` logs on UART | Length modifier `%lu` is not supported by `ConsoleUtilsPrintf` | Use `%u` and cast 32-bit arguments to `(unsigned int)` |
| No SD Card boot verified | TI image conversion and SD card boot sequence not tested | Use JTAG for reliable flashing |
| Log lines may interleave between demos | Multiple demos share one UART | Single-line logs; frame demo shows correct behavior *with* mutex |

---

## License

This project contains code from:
- **FreeRTOS** — MIT licensed
- **TI StarterWare** — Texas Instruments BSD-style license
- **Amazon FreeRTOS** — Apache License 2.0

See individual files for specific license text.
