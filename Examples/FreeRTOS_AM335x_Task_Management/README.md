# FreeRTOS AM3352 Task Management

Port of FreeRTOS for Texas Instruments AM3352 (ARM Cortex-A8) with a **pure task management demo**: no GPIO, no LED, no application interrupts — everything demonstrated through UART logging. Based on the FreeRTOS Cortex-A9 port, adapted for AM3352's AINTC instead of GIC (infrastructure copied from `FreeRTOS_AM335x_GPIO_INTERRUPT`).

![Hardware](https://img.shields.io/badge/Hardware-AM3352%20Cortex--A8-blue) ![FreeRTOS](https://img.shields.io/badge/RTOS-FreeRTOS%20v10.2.0-green) ![Build](https://img.shields.io/badge/Build-CMake%203.11+-purple)

---

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
- [Features](#features)
- [Task Management Concepts](#task-management-concepts)
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

This project runs a bare-metal FreeRTOS kernel on a TI AM3352 SoC (BeagleBone Black / Antminer L3+ hardware). The system boots directly into DDR memory via JTAG, initializes MMU, AINTC, UART, and DMTimer2, then starts two FreeRTOS tasks that demonstrate core **task management**:

- **Task1 (prio 2)** — Infinite loop that prints its iteration count and actual task state every 1000 ms. At iteration 3 it **resumes** Task2 (`vTaskResume`); after 5 iterations it **deletes Task2 cross-task** (`vTaskDelete(xTask2Handle)`) and then **deletes itself** (`vTaskDelete(NULL)`).
- **Task2 (prio 1)** — Suspends itself (`vTaskSuspend(NULL)`), gets resumed by Task1, then runs until it is deleted cross-task by Task1.
- **Idle Task (prio 0, automatic)** — `configUSE_IDLE_HOOK=1`; `vApplicationIdleHook()` prints a monotonically increasing counter **once per second** whenever no other task is Ready.

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
         │  │   Task1 (prio 2)  — Task2      │          │
         │  │   Idle Task + Idle Hook        │          │
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
                        └── xTaskCreate(Task1, prio 2)
                        └── xTaskCreate(Task2, prio 1)
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
| **Task Creation** | ✅ Tested | `xTaskCreate()` — Task1 prio 2, Task2 prio 1, created before `vTaskStartScheduler()` |
| **Infinite Loop Task** | ✅ Tested | Task1 `for(;;)` + `vTaskDelay(1000 ms)`, prints iteration + actual state |
| **Task State Reporting** | ✅ Tested | `eTaskGetState()` → Running / Ready / Blocked / Suspended via `pcTaskStateToString()` |
| **Task Self-Suspend** | ✅ Tested | Task2 calls `vTaskSuspend(NULL)` (Running → Suspended) |
| **Task Resume** | ✅ Tested | Task1 calls `vTaskResume(xTask2Handle)` at iter 3 (Suspended → Ready) |
| **Cross-Task Delete** | ✅ Tested | `vTaskDelete(xTask2Handle)` deletes Task2 from Task1 |
| **Self Delete** | ✅ Tested | Task1 ends itself with `vTaskDelete(NULL)` |
| **Idle Task / Hook** | ✅ Tested | `configUSE_IDLE_HOOK=1`; counter printed once per second when no task is Ready |
| **Serial Logger** | ✅ Active | UART 115200 8N1: task state transitions + idle counter |
| **MMU & Cache** | ✅ Enabled | Data cache on via `InitMem()` in `configure_platform()` |
| **FreeRTOS Scheduler** | ✅ Running | 1 ms tick via DMTimer2, preemptive, 10 priority levels |

---

## Task Management Concepts

### Task States (FreeRTOS)

| State | Meaning | Triggered by |
|-------|---------|--------------|
| **Running** | Task is currently executing | Scheduler picks the task |
| **Ready** | Ready to run, waiting for CPU | After resume / preempt / delay expiry |
| **Blocked** | Waiting for a delay/event | `vTaskDelay()` |
| **Suspended** | Paused, only resumable via `vTaskResume()` | `vTaskSuspend()` |
| **Deleted** | Task removed | `vTaskDelete()` |

### Task Design

| Task | Priority | Role |
|------|----------|------|
| Task1 | 2 (highest) | Infinite loop, resumes & deletes Task2, self-deletes |
| Task2 | 1 | State demo (suspends self, gets resumed, gets deleted) |
| Idle | 0 (automatic) | Runs when no task is Ready |

### Execution Flow

```
main → xTaskCreate(Task1) → xTaskCreate(Task2) → vTaskStartScheduler()
  Task1 (prio 2): for(;;) { print iter+state;
                    if iter==3 → vTaskResume(Task2); print Task2 state
                    if iter>=5 → vTaskDelete(Task2); vTaskDelete(NULL);
                    ulIter++; vTaskDelay(1000ms); }
  Task2 (prio 1): for(;;) { print state; vTaskSuspend(NULL);
                    // resumed by Task1 → vTaskResume(xTask2Handle)
                    print state; vTaskDelay(500ms); }
  Idle Hook:     runs when no task is Ready → prints counter once/sec
```

### `pcTaskStateToString` helper (`src/application/TaskMgmt.c`)

```c
const char *pcTaskStateToString(eTaskState eState)
{
    switch (eState) {
        case eRunning:   return "Running";
        case eReady:     return "Ready";
        case eBlocked:   return "Blocked";
        case eSuspended: return "Suspended";
        default:         return "Unknown";
    }
}
```

> **Note:** Always pass a valid task handle to `eTaskGetState()`. Use `xTaskGetCurrentTaskHandle()` for the running task — `eTaskGetState(NULL)` is **not** supported and returned garbage (`Unknown`) in early testing.

---

## Project Structure

```
FreeRTOS_AM335x_Task_Management/
├── CMakeLists.txt                # Root build configuration
├── ProjectIncludes.cmake         # Include paths + library definitions
├── bbb.lds                       # Linker script (DDR at 0x80000000)
├── AM335xFreeRTOS_cmake_makefile_args.py  # Auto-generates CMake includes
├── gcc-a.toolchain.ccs12.cmake   # Toolchain file for CCS12 GCC ARM
├── README.md                     # This file
│
├── src/
│   ├── application/              # Application layer
│   │   ├── main.c                # Entry point, creates Task1 (prio 2) + Task2 (prio 1)
│   │   ├── TaskMgmt.c            # vTask1, vTask2, pcTaskStateToString
│   │   └── app_utils.c           # FreeRTOS hooks (idle hook counter, malloc fail, stack overflow)
│   │
│   ├── inc/
│   │   ├── FreeRTOSConfig.h      # Kernel config (60 KB heap, 10 prios, configUSE_IDLE_HOOK=1)
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

> **Note:** `hal_bspInit.c` is a copy of the GPIO_INTERRUPT version with the GPIO1 clock module, `GPIO1ISR`, and `GPIO_INT_PRIORITY` stripped out (the `GPIO1ISR` was defined in the removed `TaskLED2.c`; leaving the reference would break the link). See `docs/architecture.md`.

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
>
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
(gdb) break vTask1
(gdb) continue
```

### Inspect FreeRTOS Task State Live

```powershell
(gdb) p pxCurrentTCB           # current running task control block
(gdb) p *(TCB_t*)pxCurrentTCB  # inspect its name, priority, state list item
(gdb) p *pxReadyTasksLists[2]  # ready list for priority 2 (Task1)
```

---

## Live UART Log

The serial logger outputs UART messages at **115200 8N1**. Connect a serial terminal (COM4) to view live output. A verified session:

```
TIMER2_CLKSEL=0x00000001
Task1 created (prio=2)
Task2 created (prio=1)
Scheduler started
Enabling timer interrupt!
Timer interrupt enabled OK
Starting first task...
First tick!
Task1 running iter=0 state=Running
Task2 running state=Running
Task2 suspending itself
Idle task running count=8621065
Task1 running iter=1 state=Running
Idle task running count=17270438
Task1 running iter=2 state=Running
Idle task running count=25919264
Task1 running iter=3 state=Running
Task1 resuming Task2...
Task2 state after resume: Ready
Task2 resumed state=Running
Task2 running state=Running
Task2 suspending itself
Idle task running count=34455183
Task1 running iter=4 state=Running
Idle task running count=43103811
Task1 running iter=5 state=Running
Task1 deleting Task2 (cross-task)
Task1 deleting itself (self)
Idle task running count=51700341
Idle task running count=60376986
Idle task running count=69053631
Idle task running count=77730271
...
```

Key observations:

- `Task1 running iter=N state=Running` — Task1 (prio 2) preempts everything; state printed while actually running is `Running`.
- `Task2 state after resume: Ready` — after `vTaskResume(xTask2Handle)` the scheduler reports Task2 as `Ready` (it has not run yet because Task1 still holds the CPU at that instant).
- `Idle task running count=N` — printed **once per second**; only the Idle Task remains after Task1 deletes Task2 and itself, and it keeps running forever.
- The idle counter is a **free-running counter** — it is not reset on boot print, so the first value is large (it increments continuously even when not printed).

---

## Known Issues

| Issue | Impact | Workaround |
|-------|--------|------------|
| `monitor reset` kills DDR state | Issuing `reset` before `load` in JTAG mode can corrupt DDR contents, causing crash | Use `load` + `continue` only; never `monitor reset` |
| J-Link sessions are not reusable | Reusing a stale J-Link GDB Server can cause target state mismatch | Always start a **fresh** JLinkGDBServer for each flash |
| No SD Card boot verified | TI image conversion and SD card boot sequence not tested | Use JTAG for reliable flashing |
| `%lu` / `%ld` not supported by `UARTPrintf` | `ConsoleUtilsPrintf` only supports `%c %d %s %u %x %X %p %%`; `%lu` prints `ERROR` and corrupts later args | Use `%u` with `(unsigned int)` casts (see `TaskMgmt.c`, `app_utils.c`) |
| `eTaskGetState(NULL)` returns garbage | Passing `NULL` to query the current task is not supported → printed `Unknown` | Use `eTaskGetState(xTaskGetCurrentTaskHandle())` |
| Parallel build fails | Multiple targets rename the same `.a` file concurrently | Always build with `-j1` |

---

## License

This project contains code from:
- **FreeRTOS** — MIT licensed
- **TI StarterWare** — Texas Instruments BSD-style license
- **Amazon FreeRTOS** — Apache License 2.0

See individual files for specific license text.
