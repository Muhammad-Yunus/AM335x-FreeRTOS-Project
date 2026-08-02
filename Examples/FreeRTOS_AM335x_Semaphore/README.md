# FreeRTOS AM3352 Semaphore Demo (Binary | Counting | ISR | Notification | Sync)

Port of FreeRTOS for Texas Instruments AM3352 (ARM Cortex-A8) demonstrating the core **FreeRTOS synchronization primitives** — Binary Semaphore, Counting Semaphore, ISR Semaphore, Event Notification (task notifications) and Task Synchronization (rendezvous) — plus task lifecycle (create / suspend / resume / delete). Debug output is **UART-only**: no LED, no GPIO, no new hardware interrupt. ISR Semaphore is shown using `vApplicationTickHook()` (the DMTimer2 tick ISR the RTOS already needs). Originally based on the FreeRTOS Cortex-A9 port, adapted for AM3352's AINTC instead of GIC.

![Hardware](https://img.shields.io/badge/Hardware-AM3352%20Cortex--A8-blue) ![FreeRTOS](https://img.shields.io/badge/RTOS-FreeRTOS%20v10.2.0-green) ![Build](https://img.shields.io/badge/Build-CMake%203.11+-purple) ![Status](https://img.shields.io/badge/Status-VERIFIED%20on%20HW-brightgreen)

---

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
- [Demo Phases](#demo-phases)
- [Features](#features)
- [Project Structure](#project-structure)
- [Toolchain](#toolchain)
- [Build Instructions](#build-instructions)
- [Flash Instructions](#flash-instructions)
- [Live UART Log](#live-uart-log)
- [Known Issues](#known-issues)
- [License](#license)

---

## Overview

This project runs a bare-metal FreeRTOS kernel on a TI AM3352 SoC (BeagleBone Black / Antminer L3+ hardware). The system boots directly into DDR memory via JTAG, initializes MMU, AINTC, UART, and DMTimer2, then starts a single **demo director task** (`SemDemo`) that runs six demo phases in a loop:

1. **Binary Semaphore** — director `give`s, worker `take`s (1→1 sync)
2. **Counting Semaphore** — producer fills the counter to 5, consumer drains it to 0
3. **ISR Semaphore** — `vApplicationTickHook()` gives from the tick ISR every 1 s
4. **Event Notification** — `xTaskNotifyGive` / `ulTaskNotifyTake` between two tasks
5. **Synchronization** — 2-task rendezvous / barrier
6. **Task Lifecycle** — create / suspend / resume / delete a worker task

Each phase creates the worker tasks it needs (with UART log), exercises the mechanism, then deletes them. The whole demo loops every ~few seconds so the UART log keeps updating. No SD card or external bootloader required when flashing via JTAG.

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                      ARM Cortex-A8 (AM3352) @ 600 MHz               │
│                         DDR 0x80000000                              │
│                                                                      │
│  FreeRTOS Kernel                                                     │
│  ├── SemDemo director task (prio 1)                                 │
│  │     └── 6 phase: create → exercise → delete worker tasks         │
│  ├── Worker tasks per phase (prio 1 / 2, temporary)                 │
│  └── DMTimer2 ISR @ 1 ms → FreeRTOS_Tick_Handler                    │
│        └── xTaskIncrementTick → vApplicationTickHook (ISR ctx)      │
│              └── xSemaphoreGiveFromISR(xIsrSem)  ← ISR Semaphore    │
│                                                                      │
│  Shared objects (created in main.c):                                │
│  ├── xBinarySem, xCountingSem (max 5), xIsrSem                      │
│  ├── xPhaseDoneSem, xSyncArriveA, xSyncArriveB                      │
│  └── task notifications (direct to listener handle)                 │
│                                                                      │
│  TI StarterWare Drivers: UART | DMTimer2 | AINTC (tick only)        │
└─────────────────────────────────────────────────────────────────────┘
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
                        └── Semaphores + SemDemo task created
                             └── vTaskStartScheduler()
```

> **No GPIO / no LED / no extra interrupt.** The only interrupt is the DMTimer2 tick (required by FreeRTOS). `hal_bspInit.c` has all GPIO setup removed.

---

## Demo Phases

| # | Phase | FreeRTOS API | Tasks |
|---|-------|--------------|-------|
| 1 | **Binary Semaphore** | `xSemaphoreCreateBinary`, `xSemaphoreGive`, `xSemaphoreTake` | `BinWorker` |
| 2 | **Counting Semaphore** | `xSemaphoreCreateCounting(5,0)`, `uxSemaphoreGetCount` | `CntProducer`, `CntConsumer` |
| 3 | **ISR Semaphore** | `configUSE_TICK_HOOK`, `xSemaphoreGiveFromISR`, `portYIELD_FROM_ISR` | `IsrConsumer` |
| 4 | **Event Notification** | `xTaskNotifyGive`, `ulTaskNotifyTake` | `EventListener`, `EventNotifier` |
| 5 | **Synchronization** | two binary sems (rendezvous) | `SyncA`, `SyncB` |
| 6 | **Task Lifecycle** | `vTaskSuspend`, `vTaskResume`, `vTaskDelete` | `LifecycleWorker` |

### ISR Semaphore (phase 3)

The FreeRTOS tick interrupt already exists (DMTimer2 → `FreeRTOS_Tick_Handler` → `xTaskIncrementTick`). With `configUSE_TICK_HOOK=1`, `vApplicationTickHook()` runs inside that ISR, so it is a real interrupt context:

```c
/* app_utils.c */
void vApplicationTickHook( void )
{
    vSemaphoreDemoTickHook();
}

/* SemaphoreDemo.c — ISR context */
void vSemaphoreDemoTickHook(void)
{
    static TickType_t ulTickCounter = 0U;
    if (gIsrDemoActive != 0U)
    {
        ulTickCounter++;
        if (ulTickCounter >= ISR_SEM_GIVE_INTERVAL)   /* 1000 ticks = 1 s */
        {
            ulTickCounter = 0U;
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xSemaphoreGiveFromISR(xIsrSem, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }
}
```

---

## Features

### ✅ Implemented & Verified (via JTAG)

| Feature | Status | Details |
|---------|--------|---------|
| **Binary Semaphore** | ✅ Tested | Director gives 3×, worker takes 3× (blocking) |
| **Counting Semaphore** | ✅ Tested | Counter fills to 5, drains to 0 (`uxSemaphoreGetCount`) |
| **ISR Semaphore** | ✅ Tested | Given from tick ISR every 1 s via `vApplicationTickHook` |
| **Event Notification** | ✅ Tested | `xTaskNotifyGive` → `ulTaskNotifyTake` (3×) |
| **Synchronization** | ✅ Tested | 2-task rendezvous barrier |
| **Task Lifecycle** | ✅ Tested | create / suspend / resume / delete with UART log |
| **Serial Logger** | ✅ Active | UART 115200 8N1: phase headers + per-task events |
| **No GPIO / LED** | ✅ | All GPIO init removed from BSP |
| **MMU & Cache** | ✅ Enabled | Data cache on via `InitMem()` |
| **FreeRTOS Scheduler** | ✅ Running | 1 ms tick via DMTimer2, preemptive, 10 priority levels |

> **Note on logging:** TI's `ConsoleUtilsPrintf` does **not** support `%lu` — it prints `ERRORu`. All log formats use `%u` (verified fix; see Known Issues).

---

## Project Structure

```
FreeRTOS_AM335x_Semaphore/
├── CMakeLists.txt                # Root build configuration
├── ProjectIncludes.cmake         # Include paths + library definitions
├── bbb.lds                       # Linker script (DDR at 0x80000000)
├── AM335xFreeRTOS_cmake_makefile_args.py  # Auto-generates CMake includes
├── gcc-a.toolchain.ccs12.cmake   # Toolchain file for CCS12 GCC ARM
│
├── src/
│   ├── application/              # Application layer
│   │   ├── main.c                # Entry point, semaphores + SemDemo task
│   │   ├── SemaphoreDemo.c       # 6 demo phases (binary/counting/ISR/notify/sync/lifecycle)
│   │   └── app_utils.c           # FreeRTOS hooks (tick hook → ISR semaphore)
│   │
│   ├── inc/
│   │   ├── FreeRTOSConfig.h      # Kernel config (tick hook, notifications, counting sems)
│   │   ├── SemaphoreDemo.h       # Demo API + shared handles
│   │   └── app_utils.h
│   │
│   └── portable/                 # Platform adaptation layer (unchanged)
│       ├── FreeRTOS/
│       │   └── portable/GCC/ARM_CA8_amm335x/
│       │       ├── port.c
│       │       └── portASM_CA8_am335x.S
│       ├── AM335X/
│       │   ├── hal_bspInit.c       # Board init (GPIO setup removed)
│       │   ├── bsp_platform.c      # vApplicationFPUSafeIRQHandler (AINTC dispatch)
│       │   ├── ported_am335x_startup.c
│       │   ├── ported_am335x_interrupt.c
│       │   ├── hal_mmu.c
│       │   ├── ported_am335x_clock_data.c
│       │   └── ported_amm335x_init.S
│       └── syscalls_minimal.c
│
├── docs/                          # vision / architecture / requirements / build / flash
├── AGENT.md                       # Agent guide for this project
├── lib/
│   └── third_party/               # TI StarterWare, Amazon FreeRTOS, CMake utils
└── build/
    ├── freeRTOSBBB.elf            # Final ELF executable
    └── app.freeRTOSBBB.bin        # Raw binary image (objcopy)
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

> **DDR is volatile:** the program runs from RAM. If the board is power-cycled or reset, the code is gone and the UART banner stops — simply re-flash (run the `load` + `monitor go` again).

---

## Live UART Log

The serial logger outputs UART messages at 115200 8N1. Connect a serial terminal (COM4) to view live output. Verified session on hardware (one full loop; values are real — note the `%lu`→`%u` fix):

```
TIMER2_CLKSEL=0x00000001
============================================================
 FreeRTOS AM3352 Semaphore Demo
 Binary | Counting | ISR | Notification | Sync | Lifecycle
============================================================
[main] creating binary semaphore (init 0) ... ok (0x800412c0)
[main] creating counting semaphore (max 5, init 0) ... ok (0x80041318)
[main] creating ISR semaphore (given from tick hook) ... ok (0x80041370)
[main] creating phase-done sync semaphore ... ok (0x800413c8)
[main] creating rendezvous semaphores (SyncA / SyncB) ... ok
[main] creating demo director task: SemDemo ... created
[main] all semaphores & tasks created, starting scheduler
Enabling timer interrupt!
Timer interrupt enabled OK
Starting first task...
First tick!
[Director] demo director started

=== [1/6] Binary Semaphore ===
[Director] creating worker task: BinWorker ... created
[Director] give binary semaphore #1/3
[BinWorker] binary semaphore TAKEN #1 (count=0)
[Director] give binary semaphore #2/3
[BinWorker] binary semaphore TAKEN #2 (count=0)
[Director] give binary semaphore #3/3
[BinWorker] binary semaphore TAKEN #3 (count=0)
[BinWorker] work done, waiting again (deleted by director)
[Director] deleting worker task: BinWorker ... deleted

=== [2/6] Counting Semaphore (max=5) ===
[Director] creating producer task: CntProducer ... created
[Director] creating consumer task: CntConsumer ... created
[CntProducer] give counting sem #1 -> count=1
[CntProducer] give counting sem #2 -> count=2
[CntProducer] give counting sem #3 -> count=3
[CntProducer] give counting sem #4 -> count=4
[CntProducer] give counting sem #5 -> count=5
[CntProducer] counter full (5 tokens), signalling phase-done
[CntConsumer] take counting sem #1 -> count=4
[CntConsumer] take counting sem #2 -> count=3
[CntConsumer] take counting sem #3 -> count=2
[CntConsumer] take counting sem #4 -> count=1
[CntConsumer] take counting sem #5 -> count=0
[CntConsumer] counter drained
[Director] deleting tasks: CntProducer & CntConsumer ... deleted

=== [3/6] ISR Semaphore (via tick hook) ===
[Director] vApplicationTickHook will give xIsrSem every 1000 ms
[Director] creating consumer task: IsrConsumer ... created
[IsrConsumer] ISR semaphore TAKEN #1 (given in vApplicationTickHook)
[IsrConsumer] ISR semaphore TAKEN #2 (given in vApplicationTickHook)
[IsrConsumer] ISR semaphore TAKEN #3 (given in vApplicationTickHook)
[Director] deleting consumer task: IsrConsumer ... deleted

=== [4/6] Event Notification (task notifications) ===
[Director] creating listener task: EventListener ... created
[Director] creating notifier task: EventNotifier ... created
[EventNotifier] xTaskNotifyGive -> EventListener
[EventListener] notification received #1/3
[EventNotifier] xTaskNotifyGive -> EventListener
[EventListener] notification received #2/3
[EventNotifier] xTaskNotifyGive -> EventListener
[EventListener] notification received #3/3
[Director] deleting tasks: EventListener & EventNotifier ... deleted

=== [5/6] Synchronization (rendezvous / barrier) ===
[Director] creating sync tasks: SyncA & SyncB ... created
[SyncA] reached rendezvous point
[SyncB] reached rendezvous point
[SyncA] SyncB also arrived — both proceed together
[SyncB] SyncA also arrived — both proceed together
[Director] deleting sync tasks: SyncA & SyncB ... deleted

=== [6/6] Task Lifecycle: create / suspend / resume / delete ===
[Director] creating worker task: LifecycleWorker ... created
[LifecycleWorker] running, iteration=1
[LifecycleWorker] running, iteration=2
[LifecycleWorker] running, iteration=3
[Director] suspending LifecycleWorker ... suspended
[Director] resuming LifecycleWorker ... resumed
[LifecycleWorker] running, iteration=4
[LifecycleWorker] running, iteration=5
[LifecycleWorker] running, iteration=6
[Director] deleting LifecycleWorker ... deleted

=== Demo complete — restarting in 3 s ===
```

> On the wire, lines printed concurrently by different tasks can interleave mid-line (e.g. `[CntConsumerted] started`) because `ConsoleUtilsPrintf` writes char-by-char without a UART lock. This is cosmetic — all values/logic are correct.

---

## Known Issues

| Issue | Impact | Workaround |
|-------|--------|------------|
| `ConsoleUtilsPrintf` does not support `%lu` | TI console_utils prints `ERRORu` for `%lu`/`%ld` | Use `%u`/`%d`/`%x`/`%s` only; keep `(unsigned long)` casts (both 32-bit) |
| Program lives in DDR (RAM) | Power cycle / board reset clears the program — UART banner stops | Re-flash via JTAG (`load` + `monitor go`) |
| UART lines interleave under concurrency | Two tasks printing at once interleave mid-line (`ConsoleUtilsPrintf` is char-by-char, no lock) | Cosmetic only — values stay correct |
| `InitMem` / `CP15BranchPredictionEnable` implicit-declaration warnings | Prototypes not included in `hal_bspInit.c` | Harmless — symbols resolve in port lib, link succeeds |
| `monitor reset` kills DDR state | Issuing `reset` before `load` in JTAG mode can corrupt DDR contents, causing crash | Use `load` + `continue` only; never `monitor reset` |
| J-Link sessions are not reusable | Reusing a stale J-Link GDB Server can cause target state mismatch; the server also exits after one load session | Always start a **fresh** JLinkGDBServer for each flash |
| No SD Card boot verified | TI image conversion and SD card boot sequence not tested; `tiimage.exe` not shipped (Linux binary) | Use JTAG for reliable flashing |
| Timer clock runs at ~3 MHz instead of 24 MHz | Tick fires every 1 ms as expected, but root clock source is ~3 MHz instead of 24 MHz | Adjusted `TIMER_INITIAL_COUNT` / `TIMER_RLD_COUNT` to `0xfffff44c` (~1 ms @ ~3 MHz) in `hal_bspInit.c` |

---

## License

This project contains code from:
- **FreeRTOS** — MIT licensed
- **TI StarterWare** — Texas Instruments BSD-style license
- **Amazon FreeRTOS** — Apache License 2.0

See individual files for specific license text.
