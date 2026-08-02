# FreeRTOS AM3352 Scheduler & Task Priority Demo

Port of FreeRTOS for Texas Instruments AM3352 (ARM Cortex-A8) platform that demonstrates the **core scheduler & task-priority concepts** purely through UART logging: preemptive/cooperative scheduling, priority, context switching, time slicing, tick interrupt, and tick rate. Originally based on the FreeRTOS Cortex-A9 port, adapted for AM3352's AINTC instead of GIC.

![Hardware](https://img.shields.io/badge/Hardware-AM3352%20Cortex--A8-blue) ![FreeRTOS](https://img.shields.io/badge/RTOS-FreeRTOS%20v10.2.0-green) ![Build](https://img.shields.io/badge/Build-CMake%203.11+-purple)

---

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
- [Scheduler Concepts](#scheduler-concepts)
- [Demo Task Lifecycle](#demo-task-lifecycle)
- [Project Structure](#project-structure)
- [Toolchain](#toolchain)
- [Build Instructions](#build-instructions)
- [Flash Instructions](#flash-instructions)
- [Live UART Log](#live-uart-log)
- [Cooperative Mode](#cooperative-mode)
- [Known Issues](#known-issues)
- [License](#license)

---

## Overview

This project runs a bare-metal FreeRTOS kernel on a TI AM3352 SoC (BeagleBone Black / Antminer L3+ hardware). The system boots directly into DDR memory via JTAG, initializes MMU, AINTC, UART, and DMTimer2, then starts **3 demo tasks** that make every scheduler decision visible on the UART console:

- **Task1 "HiPrio"** (priority 3) — Orchestrator + preemption proof. Creates the other tasks, then periodically wakes from `vTaskDelay()` and **preempts** lower-priority tasks; performs the full task lifecycle (create → suspend → resume → delete).
- **Task2 "LowPrio"** (priority 1) — Busy-loop task (never blocks) that is the constant preemption target; also demonstrates an explicit cooperative-style `taskYIELD()`.
- **Task3 "SlicePrio"** (priority 1) — Busy-loop at the *same* priority as Task2, demonstrating **time slicing** (round-robin per tick).

No GPIO, no LED, no button — the demo is observed **entirely through UART 115200 8N1**.

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
         │  ┌────────────────────────────────┐          │
         │  │   Task1 HiPrio    (prio 3)     │          │
         │  │   Task2 LowPrio   (prio 1)     │          │
         │  │   Task3 SlicePrio (prio 1)     │          │
         │  │   Idle            (prio 0)     │          │
         │  │   DMTimer2 ISR → Tick Handler  │          │
         │  └────────────────────────────────┘          │
         │       TI StarterWare Drivers                 │
         │  UART | DMA Timer | AINTC                     │
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
                   └── halBspInit() — MMU, AINTC, UART, DMTimer2 tick
                        └── vSchedulerDemoPrintConfig()
                        └── Task1 Created (vSchedulerDemoInit)
                        └── vTaskStartScheduler()
```

### Interrupt Architecture

Unlike the standard FreeRTOS GIC-based ARM ports, this project uses TI's **Advanced Interrupt Controller (AINTC)**:

| Interrupt | Source | Sys IRQ # | Priority |
|-----------|--------|-----------|----------|
| DMTimer2 Tick Timer | FreeRTOS systick | `SYS_INT_TINT2` (82) | Lowest (configMAX_IRQ_PRIORITIES − 1) |

All ISRs are registered via `IntRegister()` into `fnRAMVectors[]` and dispatched from `vApplicationFPUSafeIRQHandler()` (`bsp_platform.c`) — no standard GIC used.

> ⚠️ **Key AINTC gotcha**: the FreeRTOS dispatcher calls `IntSystemDisable(index)` (masks the interrupt in AINTC via `MIR_SET`) **before** running the registered ISR. Every ISR must therefore call `IntSystemEnable()` for its own interrupt at the end, or it will only ever fire **once**. The DMTimer ISR already does this (`hal_bspInit.c`).

### Tick → Scheduler Path

```
DMTimer2 overflow @ 1 ms
  → DMTimerIsr()                    (hal_bspInit.c)
  → FreeRTOS_Tick_Handler()          (port.c)
  → xTaskIncrementTick()
      → tick++ (configTICK_RATE_HZ = 1000)
      → ready task berprioritas lebih tinggi → ulPortYieldRequired = pdTRUE
      → portASM → context switch (preemption)
```

---

## Scheduler Concepts

| Concept | Config | How it's shown on UART |
|---------|--------|------------------------|
| **Preemptive Scheduler** | `configUSE_PREEMPTION = 1` | Task1 (prio 3) wakes from delay → logs `PREEMPT ... (context switch)` over the busy-looping lower-priority tasks |
| **Cooperative Scheduler** | `configUSE_PREEMPTION = 0` | Task2 calls `taskYIELD()` → logs `yield (taskYIELD)`; tasks only switch when they yield/block |
| **Priority** | 3 levels (3, 2, 1) | `created: ... prio=X` + `uxTaskPriorityGet` in every run log; higher always runs first |
| **Context Switching** | — | Every task logs `tick=%u` when it runs; the interleaving is the visible context switch |
| **Time Slicing** | `configUSE_TIME_SLICING = 1` | Task2 & Task3 (same prio 1) both busy-loop → `[Task2]`/`[Task3]` prints alternate every tick |
| **Tick Interrupt** | DMTimer2 @ 1 ms | `First tick!` from the ISR + all timestamps |
| **Tick Rate** | `configTICK_RATE_HZ = 1000` | Boot banner prints the rate; Task1 verifies `delay 1000 ms = 1000 ticks` |

## Demo Task Lifecycle

The demo performs a deterministic lifecycle and logs **every** event:

```
[main]  Task1 created                     (before scheduler starts)
[Task1] Task2 created                     (created from inside a task)
[Task1] Task3 created                     (same priority as Task2)
[Task1] Task2 suspended   → resumed
[Task1] Task3 deleted
[Task1] Task1 self-suspend → resumed by Task2
[Task1] Task2 deleted
[Task1] Task1 deleted                     (vTaskDelete(NULL))
```

---

## Project Structure

```
FreeRTOS_AM335x_Task_Scheduler/
├── CMakeLists.txt                # Root build configuration
├── ProjectIncludes.cmake         # Include paths + library definitions
├── bbb.lds                       # Linker script (DDR at 0x80000000)
├── AM335xFreeRTOS_cmake_makefile_args.py  # Auto-generates CMake includes
├── gcc-a.toolchain.ccs12.cmake   # Toolchain file for CCS12 GCC ARM
│
├── src/
│   ├── application/              # Application layer
│   │   ├── main.c                # Entry point, config banner, Task1 creation
│   │   ├── scheduler_demo.c      # Demo tasks + lifecycle + UART logs
│   │   └── app_utils.c           # FreeRTOS hooks (malloc fail, stack overflow)
│   │
│   ├── inc/
│   │   ├── FreeRTOSConfig.h      # Kernel config (tick 1000 Hz, preemption, time slicing)
│   │   ├── scheduler_demo.h      # Demo API
│   │   └── app_utils.h
│   │
│   └── portable/                 # Platform adaptation layer (unchanged)
│       ├── FreeRTOS/
│       │   └── portable/GCC/ARM_CA8_amm335x/
│       │       ├── port.c          # FreeRTOS C port (context switch, interrupts)
│       │       └── portASM_CA8_am335x.S  # ARM asm: SVC, IRQ, nested interrupts, FPU
│       │
│       ├── AM335X/
│       │   ├── hal_bspInit.c       # Board init: MMU, AINTC, UART, DMTimer2 tick
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
│       └── ti/                    # TI StarterWare (UART, timers, AINTC)
│           ├── include/           # Headers (hw_*.h, uart.h, interrupt.h)
│           ├── drivers/           # Peripheral drivers (uart.c, dmtimer.c, …)
│           └── …
│
├── docs/                          # Vision / architecture / requirements / how-to
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
6. **TI StarterWare** cloned into `lib/third_party/ti` (same as the base project)

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

## Live UART Log

Connect a serial terminal (COM4) at 115200 8N1. A representative session:

```
TIMER2_CLKSEL=0x00000001
========================================
 FreeRTOS Scheduler & Task Priority Demo
========================================
[Config] configTICK_RATE_HZ     = 1000 Hz (1 ms per tick)
[Config] configUSE_PREEMPTION   = 1 (Preemptive scheduler)
[Config] configUSE_TIME_SLICING = 1 (round-robin utk priority sama)
[Config] configMAX_PRIORITIES   = 10
[main] Task1 created: name='HiPrio' prio=3
Scheduler started
Enabling timer interrupt!
Timer interrupt enabled OK
Starting first task...
First tick!
[Task1] Running (tick=1) - preemptive scheduler aktif, prio=3
[Task1] tick=1001 - membuat Task2 (task kedua)
[Task1] Task2 created: name='LowPrio' prio=1
[Task2] Running (tick=1001) - prio=1, busy-loop TANPA blokir (kandidat preempt)
[Task1] tick=1501 - membuat Task3 (prio sama dgn Task2)
[Task1] Task3 created: name='SlicePrio' prio=1 (time slicing)
[Task3] Running (tick=1501) - prio=1 SAMA dgn Task2 -> time slicing (round-robin per tick)
[Task2] tick=2501 counter=1048576 - jalan, lalu yield (taskYIELD)
[Task3] tick=2502 counter=1048576 - time slice
[Task2] tick=2503 counter=2097152 - jalan, lalu yield (taskYIELD)
[Task3] tick=2504 counter=2097152 - time slice
...
[Task1] tick=3001 - Task1(prio=3) bangun & PREEMPT task prio lebih rendah (context switch)
...
[Task1] Tick check: delay 1000 ms menghabiskan 1000 tick (configTICK_RATE_HZ=1000)
[Task1] tick=... - vTaskSuspend(Task2) -> Task2 berhenti berjalan
[Task1] Task2 suspended
[Task1] tick=... - vTaskResume(Task2) -> Task2 jalan lagi
[Task1] Task2 resumed
[Task1] tick=... - vTaskDelete(Task3)
[Task1] Task3 deleted
[Task1] tick=... - Task1 self-suspend (vTaskSuspend(NULL)), Task2 akan me-resume
[Task2] tick=... - me-resume Task1 (vTaskResume)
[Task1] tick=... - Task1 resumed oleh Task2
[Task1] tick=... - vTaskDelete(Task2)
[Task1] Task2 deleted
[Task1] tick=... - vTaskDelete(NULL) -> Task1 dihapus
```

- `[Task2]`/`[Task3]` interleaving at the same priority → **time slicing**
- `[Task1] ... PREEMPT ...` → **preemption** when the higher-priority task wakes
- `tick=%u` deltas → **context switching** and **tick rate** verification
- `First tick!` → **tick interrupt** from DMTimer2

## Cooperative Mode

To observe **cooperative scheduling**, set in `src/inc/FreeRTOSConfig.h`:

```c
#define configUSE_PREEMPTION  0
#define configUSE_TIME_SLICING 0
```

Rebuild and re-flash. The log banner will print `Cooperative scheduler`, and tasks will only switch to each other when they explicitly call `taskYIELD()` or a blocking API (`vTaskDelay`, `vTaskSuspend`) — even if the higher-priority Task1 is ready. Task2 already contains an explicit `taskYIELD()` call to make this visible.

---

## Known Issues

| Issue | Impact | Workaround |
|-------|--------|------------|
| `monitor reset` kills DDR state | Issuing `reset` before `load` in JTAG mode can corrupt DDR contents, causing crash | Use `load` + `continue` only; never `monitor reset` |
| J-Link sessions are not reusable | Reusing a stale J-Link GDB Server can cause target state mismatch | Always start a **fresh** JLinkGDBServer for each flash |
| No SD Card boot verified | TI image conversion and SD card boot sequence not tested | Use JTAG for reliable flashing |
| Timer clock runs at ~3 MHz instead of 24 MHz | Tick fires every 1 ms as expected, but root clock source is ~3 MHz instead of 24 MHz | Adjusted `TIMER_INITIAL_COUNT` / `TIMER_RLD_COUNT` to `0xfffff44c` (~1 ms @ ~3 MHz) in `hal_bspInit.c:50-51` |
| Busy-loop tasks print heavily | UART can be flooded by `[Task2]`/`[Task3]` during time-slicing demo | Increase `DEMO_PRINT_INTERVAL` (2^20) in `scheduler_demo.c` |

---

## License

This project contains code from:
- **FreeRTOS** — MIT licensed
- **TI StarterWare** — Texas Instruments BSD-style license
- **Amazon FreeRTOS** — Apache License 2.0

See individual files for specific license text.
