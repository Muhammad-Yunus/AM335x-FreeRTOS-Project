# FreeRTOS AM3352 Task Delay & Timing

Port of FreeRTOS for Texas Instruments AM3352 (ARM Cortex-A8) with a **pure task delay & timing demo**: no GPIO, no LED, no application interrupts — everything demonstrated through UART logging. Based on the FreeRTOS Cortex-A9 port, adapted for AM3352's AINTC instead of GIC (infrastructure copied from `FreeRTOS_AM335x_GPIO_INTERRUPT`).

![Hardware](https://img.shields.io/badge/Hardware-AM3352%20Cortex--A8-blue) ![FreeRTOS](https://img.shields.io/badge/RTOS-FreeRTOS%20v10.2.0-green) ![Build](https://img.shields.io/badge/Build-CMake%203.11+-purple)

---

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
- [Features](#features)
- [Timing Concepts](#timing-concepts)
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

This project runs a bare-metal FreeRTOS kernel on a TI AM3352 SoC (BeagleBone Black / Antminer L3+ hardware). The system boots directly into DDR memory via JTAG, initializes MMU, AINTC, UART, and DMTimer2 (1 ms tick), then demonstrates 6 FreeRTOS **delay & timing** concepts through UART:

- **Task1 `DelayUntil` (prio 2)** — *Periodic Task* using `vTaskDelayUntil()` with a fixed 2000 ms absolute period (drift-free). Also the lifecycle orchestrator: suspends/resumes/deletes Task2, stops & deletes the software timer, then self-deletes.
- **Task2 `DelayTask` (prio 1)** — *Relative Delay* using `vTaskDelay(1500 ms)`; prints tick before/after and elapsed ticks.
- **Software Timer** — *Software Timing*: `xTimerCreate()` auto-reload 1000 ms; the callback runs in the **timer daemon** and keeps firing while the two tasks are Blocked → proof of *non-blocking delay*.
- **Tick Count** — every event logs `xTaskGetTickCount()` (1 tick = 1 ms); a tick-rate check measures 1000 ms ≈ 1005 ticks (≈5 ticks of scheduling/tick-boundary overhead).

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
         │  ┌────────────────────────────────┐          │
         │  │   DelayUntil (prio 2)          │          │
         │  │   DelayTask (prio 1)           │          │
         │  │   Timer Daemon (prio 3)        │          │
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
                   └── halBspInit() — MMU, AINTC, UART, DMTimer2
                       (GPIO clock + interrupt config stripped — not needed)
                        ├── xTaskCreate(DelayUntil, prio 2)
                        ├── xTaskCreate(DelayTask, prio 1)
                        ├── xTimerCreate + xTimerStart (software timer)
                        └── vTaskStartScheduler()
```

### Interrupt Architecture

Unlike the standard FreeRTOS GIC-based ARM ports, this project uses TI's **Advanced Interrupt Controller (AINTC)**. The only active interrupt is the FreeRTOS tick:

| Interrupt | Source | Sys IRQ # | Priority |
|-----------|--------|-----------|----------|
| DMTimer2 Tick Timer | FreeRTOS systick | `SYS_INT_TINT2` (82) | Lowest (configMAX_IRQ_PRIORITIES − 1) |

ISRs are registered via `IntRegister()` into `fnRAMVectors[]` and dispatched from `vApplicationFPUSafeIRQHandler()` (`bsp_platform.c`) — no standard GIC used. No application (GPIO) ISR is used in this project.

---

## Features

### ✅ Implemented & Verified (via JTAG)

| Feature | Status | Details |
|---------|--------|---------|
| **vTaskDelay (relative)** | ✅ Tested | Task2 `vTaskDelay(1500 ms)`; logs tick before/after, elapsed ≈ 1506-1507 ticks measured |
| **vTaskDelayUntil (absolute)** | ✅ Tested | Task1 fixed 2000 ms period, no cumulative drift |
| **Tick Count** | ✅ Tested | `xTaskGetTickCount()` on every event; tick-rate check: 1000 ms ≈ 1005 ticks measured |
| **Software Timing (timer)** | ✅ Tested | `xTimerCreate` auto-reload 1000 ms; callback fired by timer daemon |
| **Periodic Task** | ✅ Tested | Task1 constant 2000-tick period via `vTaskDelayUntil` |
| **Non-blocking delay** | ✅ Tested | Task2 + software timer keep running while tasks are Blocked |
| **Task Lifecycle Logging** | ✅ Tested | create / suspend / resume / delete printed via UART |
| **Serial Logger** | ✅ Active | UART 115200 8N1: timing + lifecycle events |
| **MMU & Cache** | ✅ Enabled | Data cache on via `InitMem()` in `configure_platform()` |
| **FreeRTOS Scheduler** | ✅ Running | 1 ms tick via DMTimer2, preemptive, 10 priority levels |

---

## Timing Concepts

### `vTaskDelay()` — Relative Delay

Delay counted from the tick **when called**. Wake time = `tick_now + delay`. Because wake-up is never perfectly aligned (jitter/preemption), the absolute period drifts slightly each cycle.

```c
vTaskDelay(pdMS_TO_TICKS(1500));   /* Task2: Running → Blocked → Ready */
```

### `vTaskDelayUntil()` — Absolute (Drift-Free) Delay

Delay counted from `xLastWakeTime` (the previous wake time) + period. Wake time does **not** drift → ideal for fixed-rate periodic tasks. Requires `INCLUDE_vTaskDelayUntil=1` (enabled).

```c
TickType_t xLastWakeTime = xTaskGetTickCount();
vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(2000));   /* Task1: fixed 2000 ms period */
```

### Tick Count

`xTaskGetTickCount()` returns the number of ticks since the scheduler started (`configTICK_RATE_HZ = 1000` → 1 tick = 1 ms). Every log line carries the current tick so the timing behavior is explicit.

### Software Timing (Software Timer)

`xTimerCreate()` + `xTimerStart()` send commands to the **timer command queue**; the **timer daemon** (`configTIMER_TASK_PRIORITY = 3`) executes the callback when the period elapses. The callback runs in daemon context, not in the calling task.

```c
xSoftwareTimer = xTimerCreate("swTimer", pdMS_TO_TICKS(1000), pdTRUE, NULL, vSoftwareTimerCallback);
xTimerStart(xSoftwareTimer, 0);
```

### Periodic Task

Task1 (`DelayUntil`, prio 2) is a periodic task: fixed 2000-tick period via `vTaskDelayUntil`, printing `[Task1] Periodic iter=N tick=...` each cycle. The period stays constant because the wake target is absolute.

### Non-blocking Delay

Both `vTaskDelay()` and `vTaskDelayUntil()` move the task to the **Blocked** state, freeing the CPU for other Ready tasks, the timer daemon, and the Idle task. In the log, `[Task2]` and `[Timer]` lines appear **between** `[Task1]` lines — proof the CPU is not held by the delay. (Contrast with a busy-wait `for(;;){}` which consumes 100% CPU.)

---

## Project Structure

```
FreeRTOS_AM335x_Task_Timing/
├── CMakeLists.txt                # Root build configuration
├── ProjectIncludes.cmake         # Include paths + library definitions
├── bbb.lds                       # Linker script (DDR at 0x80000000)
├── AM335xFreeRTOS_cmake_makefile_args.py  # Auto-generates CMake includes
├── gcc-a.toolchain.ccs12.cmake   # Toolchain file for CCS12 GCC ARM
├── README.md                     # This file
│
├── src/
│   ├── application/              # Application layer
│   │   ├── main.c                # Entry point, creates tasks + software timer
│   │   ├── task_timing.c         # vTaskDelayUntilTask, vTaskDelayTask, vSoftwareTimerCallback
│   │   └── app_utils.c           # FreeRTOS hooks (malloc fail, stack overflow)
│   │
│   ├── inc/
│   │   ├── task_timing.h         # Prototypes + shared timing config
│   │   ├── FreeRTOSConfig.h      # Kernel config (10 MB heap, 10 prios, 1 ms tick, timers on)
│   │   └── app_utils.h
│   │
│   └── portable/                 # Platform adaptation layer
│       ├── FreeRTOS/
│       │   └── portable/GCC/ARM_CA8_amm335x/
│       │       ├── port.c          # FreeRTOS C port (context switch, interrupts)
│       │       └── portASM_CA8_am335x.S  # ARM asm: SVC, IRQ, nested interrupts, FPU
│       │
│       ├── AM335X/
│       │   ├── hal_bspInit.c       # Board init: MMU, AINTC, UART, timer (GPIO config stripped)
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
│       ├── ti/                    # TI StarterWare (UART, timers, AINTC)
│       ├── amazon/                # Amazon FreeRTOS v10.2.0
│       └── c_source_tools/        # CMake utility
│
├── docs/                          # Vision / requirements / architecture / build / flash
└── build/                         # CMake output directory
    ├── freeRTOSBBB.elf            # Final ELF executable
    └── app.freeRTOSBBB.bin        # Raw binary image (objcopy)
```

> **Note:** `hal_bspInit.c` is a copy of the GPIO_INTERRUPT version with the GPIO1 clock module, `GPIO1ISR`, and `GPIO_INT_PRIORITY` stripped out (the `GPIO1ISR` was defined in the removed `TaskLED2.c`; leaving the reference would break the link). Same pattern as `FreeRTOS_AM335x_Task_Management`. See `docs/architecture.md`.

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

> Full step-by-step guide in [`docs/HOW_TO_BUILD.md`](docs/HOW_TO_BUILD.md).

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

Then checkout Amazon FreeRTOS to the commit used by this project:

```powershell
cd lib/third_party/amazon
git checkout 2bb3154718ecf0346e3236eef7039a27977d46d9
cd ../..
```

> **Verified:** commit `2bb3154` has **no submodules** (no `.gitmodules` — `freertos_kernel` is a plain directory), so `git submodule update --init --recursive` is **not needed**. A full clone already contains the required commit; bandwidth-saving alternative (verified working, GitHub serves the SHA):
> ```powershell
> git init lib/third_party/amazon
> git -C lib/third_party/amazon remote add origin https://github.com/aws/amazon-freertos.git
> git -C lib/third_party/amazon fetch --depth 1 origin 2bb3154718ecf0346e3236eef7039a27977d46d9
> git -C lib/third_party/amazon checkout FETCH_HEAD
> ```

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
>
> **PowerShell note:** Ganti `%cd%` dengan `$PWD` pada baris `CMAKE_TOOLCHAIN_FILE`. `%cd%` adalah syntax **cmd.exe**, tidak didukung di PowerShell.

```powershell
cmake -S . -B build -G Ninja ^
  "-DCMAKE_TOOLCHAIN_FILE=$PWD/gcc-a.toolchain.ccs12.cmake" ^
  -DCMAKE_C_COMPILER_WORKS=1

cmake --build build -- -j1
```

> **Note (verified):** The final build step also tries to generate a TI SDCARD image via `tiimage.exe`. The StarterWare repo only contains `tiimage` (a **Linux ELF**) + `tiimage.c` — there is **no Windows `tiimage.exe`**, so on Windows this step fails (`'tiimage.exe' is not recognized as an internal or external command`, exit code 1) but **the ELF and BIN files are already built and valid** for JTAG flashing. Check for `build/freeRTOSBBB.elf` — if it exists, the build succeeded for JTAG flashing.

#### 6. Verify Output

```
build/freeRTOSBBB.elf       — Linked ELF executable
build/app.freeRTOSBBB.bin   — Raw binary image (objcopy)
```

---

## Flash Instructions

> Full step-by-step guide in [`docs/HOW_TO_FLASH.md`](docs/HOW_TO_FLASH.md).

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
(gdb) break vTaskDelayUntilTask
(gdb) continue
```

### Inspect FreeRTOS Timing State Live

```powershell
(gdb) p xTaskGetTickCount()            # current tick count
(gdb) p pxCurrentTCB                   # current running task control block
(gdb) p xSoftwareTimer                 # software timer handle
```

---

## Live UART Log

The serial logger outputs UART messages at **115200 8N1**. Connect a serial terminal (COM4) to view live output. A representative session:

```
TIMER2_CLKSEL=0x00000001
========================================
 FreeRTOS Task Delay & Timing Demo
========================================
[Config] configTICK_RATE_HZ = 1000 Hz (1 ms per tick)
[main] Task1 created: name='DelayUntil' prio=2 (vTaskDelayUntil / periodic)
[main] Task2 created: name='DelayTask' prio=1 (vTaskDelay / relative)
[main] Software timer created: period=1000 ms (auto-reload)
[main] Software timer started (callback via timer daemon)
Scheduler started
Enabling timer interrupt!
Timer interrupt enabled OK
Starting first task...
First tick!
[Task2] Running tick=1 — vTaskDelay(1500 ms) dimulai (relative)
[Timer] Software timer fired tick=1000 (callback via timer daemon)
[Task1] Tick check: delay 1000 ms menghabiskan 1005 tick (configTICK_RATE_HZ=1000)
[Task2] vTaskDelay selesai tick=1507 elapsed=1506 tick (1 tick = 1 ms)
[Task2] Running tick=1513 — vTaskDelay(1500 ms) dimulai (relative)
[Timer] Software timer fired tick=2000 (callback via timer daemon)
[Timer] Software timer fired tick=3000 (callback via timer daemon)
[Task1] Periodic iter=0 tick=3013 period=2000 tick (vTaskDelayUntil, absolut)
[Task2] vTaskDelay selesai tick=3020 elapsed=1507 tick (1 tick = 1 ms)
[Task2] Running tick=3026 — vTaskDelay(1500 ms) dimulai (relative)
[Timer] Software timer fired tick=4000 (callback via timer daemon)
[Task2] vTaskDelay selesai tick=4532 elapsed=1506 tick (1 tick = 1 ms)
[Task2] Running tick=4538 — vTaskDelay(1500 ms) dimulai (relative)
[Timer] Software timer fired tick=5000 (callback via timer daemon)
[Task1] Periodic iter=1 tick=5013 period=2000 tick (vTaskDelayUntil, absolut)
[Timer] Software timer fired tick=6000 (callback via timer daemon)
[Task2] vTaskDelay selesai tick=6044 elapsed=1506 tick (1 tick = 1 ms)
[Task2] Running tick=6050 — vTaskDelay(1500 ms) dimulai (relative)
[Timer] Software timer fired tick=7000 (callback via timer daemon)
[Task1] Periodic iter=2 tick=7013 period=2000 tick (vTaskDelayUntil, absolut)
[Task1] vTaskSuspend(Task2)
[Task1] Task2 suspended
[Timer] Software timer fired tick=8000 (callback via timer daemon)
[Timer] Software timer fired tick=9000 (callback via timer daemon)
[Task1] Periodic iter=3 tick=9013 period=2000 tick (vTaskDelayUntil, absolut)
[Timer] Software timer fired tick=10000 (callback via timer daemon)
[Timer] Software timer fired tick=11000 (callback via timer daemon)
[Task1] Periodic iter=4 tick=11013 period=2000 tick (vTaskDelayUntil, absolut)
[Task1] vTaskResume(Task2)
[Task1] Task2 resumed
[Task2] vTaskDelay selesai tick=11024 elapsed=4974 tick (1 tick = 1 ms)
[Task2] Running tick=11031 — vTaskDelay(1500 ms) dimulai (relative)
[Timer] Software timer fired tick=12000 (callback via timer daemon)
[Task2] vTaskDelay selesai tick=12537 elapsed=1506 tick (1 tick = 1 ms)
[Task2] Running tick=12543 — vTaskDelay(1500 ms) dimulai (relative)
[Timer] Software timer fired tick=13000 (callback via timer daemon)
[Task1] Periodic iter=5 tick=13013 period=2000 tick (vTaskDelayUntil, absolut)
[Timer] Software timer fired tick=14000 (callback via timer daemon)
[Task2] vTaskDelay selesai tick=14049 elapsed=1506 tick (1 tick = 1 ms)
[Task2] Running tick=14055 — vTaskDelay(1500 ms) dimulai (relative)
[Timer] Software timer fired tick=15000 (callback via timer daemon)
[Task1] Periodic iter=6 tick=15013 period=2000 tick (vTaskDelayUntil, absolut)
[Task1] vTaskDelete(Task2) — cross-task delete
[Task1] Task2 deleted
[Timer] Software timer fired tick=16000 (callback via timer daemon)
[Timer] Software timer fired tick=17000 (callback via timer daemon)
[Task1] Periodic iter=7 tick=17013 period=2000 tick (vTaskDelayUntil, absolut)
[Task1] xTimerStop + xTimerDelete(software timer)
[Task1] Software timer deleted
[Task1] Periodic iter=8 tick=19013 period=2000 tick (vTaskDelayUntil, absolut)
[Task1] vTaskDelete(NULL) — self delete, demo selesai
```

Key observations (measured on hardware):

- **Drift-free `vTaskDelayUntil`** — Task1 wakes at tick **3013, 5013, 7013, 9013, 11013, 13013, 15013, 17013, 19013** — exactly **2000 ticks apart** every cycle, regardless of the timer daemon preempting in between → `vTaskDelayUntil` is drift-free.
- **Non-blocking delay** — `[Timer]` fires every 1000 ticks and `[Task2]` runs repeatedly **between** `[Task1]` cycles → the CPU is never held by a delay (both tasks are in the Blocked state).
- **`vTaskDelay` is relative** — `[Task2] elapsed ≈ 1506-1507` (not exactly 1500): the +6-7 ticks are tick-boundary alignment + preemption by higher-priority tasks (timer daemon prio 3, Task1 prio 2) before Task2 gets the CPU. Because the wake target is re-armed from *now* each cycle, this offset is the "relative" behavior.
- **Tick check** — 1000 ms → **1005 ticks** (≈5 ticks of scheduling/tick-boundary overhead), confirming 1 tick ≈ 1 ms.
- **Suspend/resume** — Task2 was suspended at iter=2; at resume (iter=4) it wakes almost immediately (`elapsed=4974`) because its absolute wake target was computed at delay start and had already passed during the suspension.
- After Task1 self-deletes (iter=8), only the Idle Task remains → demo complete.

---

## Known Issues

| Issue | Impact | Workaround |
|-------|--------|------------|
| `monitor reset` kills DDR state | Issuing `reset` before `load` in JTAG mode can corrupt DDR contents, causing crash | Use `load` + `continue` only; never `monitor reset` |
| J-Link sessions are not reusable | Reusing a stale J-Link GDB Server can cause target state mismatch | Always start a **fresh** JLinkGDBServer for each flash |
| No SD Card boot verified | TI image conversion and SD card boot sequence not tested | Use JTAG for reliable flashing |
| `%lu` / `%ld` not supported by `UARTPrintf` | `ConsoleUtilsPrintf` only supports `%c %d %s %u %x %X %p %%`; `%lu` prints `ERROR` and corrupts later args | Use `%u` with `(unsigned int)` casts (see `task_timing.c`, `main.c`) |
| Parallel build fails | Multiple targets rename the same `.a` file concurrently | Always build with `-j1` |

---

## License

This project contains code from:
- **FreeRTOS** — MIT licensed
- **TI StarterWare** — Texas Instruments BSD-style license
- **Amazon FreeRTOS** — Apache License 2.0

See individual files for specific license text.
