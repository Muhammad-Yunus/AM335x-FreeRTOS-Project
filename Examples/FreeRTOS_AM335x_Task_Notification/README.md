# FreeRTOS AM3352 Task Notification

Port of FreeRTOS for Texas Instruments AM3352 (ARM Cortex-A8) platform demonstrating **Direct-to-Task Notification**. The demo is pure software — **no LED, no button, no GPIO interrupt** — all observability is via **UART 115200 8N1**. Originally based on the FreeRTOS Cortex-A9 port, adapted for AM3352's AINTC instead of GIC.

![Hardware](https://img.shields.io/badge/Hardware-AM3352%20Cortex--A8-blue) ![FreeRTOS](https://img.shields.io/badge/RTOS-FreeRTOS%20v10.2.0-green) ![Build](https://img.shields.io/badge/Build-CMake%203.11+-purple)

---

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
- [Features](#features)
- [Task Notification (demonstrated APIs)](#task-notification-demonstrated-apis)
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

This project runs a bare-metal FreeRTOS kernel on a TI AM3352 SoC (BeagleBone Black / Antminer L3+ hardware). The system boots directly into DDR memory via JTAG, initializes MMU, AINTC, UART, and DMTimer2, then starts a single orchestrator task:

- **NotifyDemo Task** — Creates the two worker tasks, sends direct-to-task notifications, then suspends/resumes/deletes them, logging every step over UART.
- **NotifyWait Task** — Blocks on `xTaskNotifyWait()` and logs the received 32-bit notification value (also receives ISR notifications).
- **NotifyTake Task** — Blocks on `ulTaskNotifyTake()` and logs the pending notification count (notification as counting semaphore).
- **Tick ISR** — The existing DMTimer2 RTOS tick ISR calls `xTaskNotifyFromISR()` roughly once per second, demonstrating ISR-to-task notification without adding any new hardware interrupt.

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
         │  │   NotifyDemo (controller)      │          │
         │  │   NotifyWait (xTaskNotifyWait) │          │
         │  │   NotifyTake (ulTaskNotifyTake)│          │
         │  │   DMTimer2 ISR → Tick + ISR    │          │
         │  │   notification (xTaskNotifyFromISR)       │
         │  └────────────────────────────────┘          │
         │       TI StarterWare Drivers                 │
         │  UART | DMA Timer | AINTC                    │
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
                        └── Task Created (vTaskNotifyDemo)
                             └── vTaskStartScheduler()
```

### Notification Flow

```
[Task] NotifyDemo (prio 3)                        [ISR] DMTimer2 tick
  ├─ creates NotifyWait + NotifyTake (prio 2)
  ├─ xTaskNotify(NotifyWait, 0x01, eSetValueWithOverwrite)  ┐
  │      └─► NotifyWait TCB (value) ←───────────────────────┤ xTaskNotifyFromISR(
  │                                                            │  0x1000, eSetBits,
  │           NotifyWait: xTaskNotifyWait() logs value        │  ~1s interval)
  ├─ xTaskNotify(NotifyTake, eIncrement) ×2                   ┘
  │      └─► NotifyTake TCB (count)
  │           NotifyTake: ulTaskNotifyTake() logs count
  ├─ vTaskSuspend / vTaskResume / vTaskDelete (keduanya)
  └─ loop
```

Each task carries exactly **one** 32-bit notification value inside its own TCB — this is what makes it a *direct-to-task* notification. No queue, semaphore, or event group is allocated.

---

## Features

### ✅ Implemented & Verified (via JTAG)

| Feature | Status | Details |
|---------|--------|---------|
| **xTaskNotify()** | ✅ Active | Task-to-task notification, `eSetValueWithOverwrite` (32-bit value) & `eIncrement` (counting) |
| **xTaskNotifyWait()** | ✅ Active | NotifyWait task blocks with `portMAX_DELAY`, clears all bits on exit, logs value |
| **ulTaskNotifyTake()** | ✅ Active | NotifyTake task uses notification as counting semaphore, logs pending count |
| **Direct-to-task Notification** | ✅ Active | Notifications go straight into the TCB via `TaskHandle_t` |
| **ISR Notification** | ✅ Active | DMTimer2 tick ISR → `xTaskNotifyFromISR()` + `portYIELD_FROM_ISR()` every ~1s |
| **Task lifecycle demo** | ✅ Active | Create / suspend / resume / delete of both worker tasks, logged over UART |
| **Serial Logger** | ✅ Active | UART 115200 8N1, the only observation medium |
| **MMU & Cache** | ✅ Enabled | Data cache on via `InitMem()` in `configure_platform()` |
| **FreeRTOS Scheduler** | ✅ Running | 1 ms tick via DMTimer2, preemptive, 10 priority levels |

> **No GPIO / LED / button is used.** The demo is 100% software.

---

## Task Notification (demonstrated APIs)

### `src/application/TaskNotify.c`

```c
/* Task 1 — wait with bit mask, read full 32-bit value */
xTaskNotifyWait(0x00, 0xFFFFFFFFUL, &ulNotifiedValue, portMAX_DELAY);

/* Task 2 — take as counting semaphore (clear on exit) */
ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

/* Controller — send a 32-bit value (overwrite semantics) */
xTaskNotify(xNotifyWaitTaskHandle, 0x01, eSetValueWithOverwrite);

/* Controller — send as counting semaphore token */
xTaskNotify(xNotifyTakeTaskHandle, 0, eIncrement);
```

### ISR notification (`src/portable/AM335X/hal_bspInit.c` — `DMTimerIsr`)

```c
if (xISRNotifyTaskHandle != NULL) {
    ulTickCounter++;
    if (ulTickCounter >= ISR_NOTIFY_TICKS) {   /* 1000 ticks = ~1s */
        ulTickCounter = 0;
        xTaskNotifyFromISR(xISRNotifyTaskHandle, 0x1000, eSetBits, &xHigherPriorityTaskWoken);
    }
}
...
portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
```

> **ISR safety**: only `FromISR` APIs are used. `xISRNotifyTaskHandle` is reset to `NULL` before the target task is deleted (`TaskNotify.c`) so the ISR never notifies a dead task.

---

## Project Structure

```
FreeRTOS_AM335x_Task_Notification/
├── CMakeLists.txt                # Root build configuration
├── ProjectIncludes.cmake         # Include paths + library definitions
├── bbb.lds                       # Linker script (DDR at 0x80000000)
├── AM335xFreeRTOS_cmake_makefile_args.py  # Auto-generates CMake includes
├── gcc-a.toolchain.ccs12.cmake   # Toolchain file for CCS12 GCC ARM
│
├── src/
│   ├── application/              # Application layer
│   │   ├── main.c                # Entry point, task creation (NotifyDemo)
│   │   ├── TaskNotify.c          # 3 tasks: NotifyDemo, NotifyWait, NotifyTake
│   │   └── app_utils.c           # FreeRTOS hooks (malloc fail, stack overflow)
│   │
│   ├── inc/
│   │   ├── FreeRTOSConfig.h      # Kernel config (60 KB heap, 10 prios, notifications ON)
│   │   ├── TaskNotify.h          # Task prototypes + extern xISRNotifyTaskHandle
│   │   └── app_utils.h
│   │
│   └── portable/                 # Platform adaptation layer
│       ├── FreeRTOS/
│       │   └── portable/GCC/ARM_CA8_amm335x/
│       │       ├── port.c          # FreeRTOS C port (context switch, interrupts)
│       │       └── portASM_CA8_am335x.S  # ARM asm: SVC, IRQ, nested interrupts, FPU
│       │
│       ├── AM335X/
│       │   ├── hal_bspInit.c       # Board init: MMU, AINTC, UART, timer + ISR notify
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
│           ├── include/           # Headers (hw_*.h, interrupt.h)
│           ├── drivers/           # Peripheral drivers (dmtimer.c, uart.c, …)
│           └── …
│
├── docs/                          # Vision / architecture / requirements / build / flash notes
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
(gdb) break vNotifyWaitTask
(gdb) continue
```

### Inspect a Task's Notification State Live

The notification value lives in the task's TCB. Find the TCB address of `NotifyWait` and inspect `ulNotifiedValue`:

```powershell
(gdb) p/x *(unsigned int*)(xNotifyWaitTaskHandle + 0x... )   # field offset depends on kernel build
```

> Simplest live check: `p ulNotifiedValue` with a breakpoint inside `vNotifyWaitTask`, or set a breakpoint at `xTaskNotifyWait` / `xTaskNotifyFromISR` to watch sends.

---

## Live UART Log

The serial logger outputs UART messages at 115200 8N1. Connect a serial terminal (COM4) to view live output. Expected session (demo loops every ~7s):

```
TIMER2_CLKSEL=0x00000001
First tick!
Scheduler started
[Demo] FreeRTOS Direct-to-Task Notification demo started
[Demo] Task created: NotifyWait (xTaskNotifyWait receiver)
[Demo] Task created: NotifyTake (ulTaskNotifyTake receiver)
[NotifyTake][NotifyWait Task start] Task stared, blockinted, waitingg on ulTask on xTaskNoNotifyTake(tifyWait()..)...
.
[Demo] xTaskNotify() -> NotifyWait (value=0x01, eSetValueWithOverwrite)
[NotifyWait] Notification received, value=0x00000001
[ISR] xTaskNotifyFromISR() -> NotifyWait (bits 0x1000)
[NotifyWait] Notification received, value=0x00001000
[Demo] xTaskNotify() -> NotifyTake (eIncrement #1)
[Demo] xTaskNotify() -> NotifyTake (eIncrement #2)
[NotifyTake] ulTaskNotifyTake() returned, pending count=2
[Demo] vTaskSuspend() -> NotifyTake
[ISR] xTaskNotifyFromISR() -> NotifyWait (bits 0x1000)
[NotifyWait] Notification received, value=0x00001000
[Demo] vTaskResume() -> NotifyTake
[NotifyTake] ulTaskNotifyTake() returned, pending count=0
[Demo] vTaskSuspend() -> NotifyWait
[ISR] xTaskNotifyFromISR() -> NotifyWait (bits 0x1000)
[Demo] vTaskResume() -> NotifyWait
[NotifyWait] Notification received, value=0x00001000
[Demo] vTaskDelete() -> NotifyTake
[ISR] xTaskNotifyFromISR() -> NotifyWait (bits 0x1000)
[NotifyWait] Notification received, value=0x00001000
[Demo] vTaskDelete() -> NotifyWait
[Demo] Cycle complete, restarting demo
```

The `[ISR]` lines appear roughly every second, interleaved.

- `[NotifyWait] ... value=0x00000001` → value set by the controller (`eSetValueWithOverwrite`).
- `[NotifyWait] ... value=0x00001000` → ISR bit `0x1000` set by `xTaskNotifyFromISR()`.
- `[NotifyTake] ... pending count=2` → counting-semaphore behaviour from two consecutive `eIncrement` notifications.
- `[NotifyTake] ... pending count=0` → when resumed, the blocking call wakes and returns the count (0 if no notifications were received during suspend).
- `[NotifyWait] ... value=0x00001000` after resume → shows that during suspend, the ISR notification was written into the TCB and safely read upon task resume.
- `[Demo] ...` → lifecycle events of the orchestrator.

---

## Known Issues

| Issue | Impact | Workaround |
|-------|--------|------------|
| `monitor reset` kills DDR state | Issuing `reset` before `load` in JTAG mode can corrupt DDR contents, causing crash | Use `load` + `continue` only; never `monitor reset` |
| J-Link sessions are not reusable | Reusing a stale J-Link GDB Server can cause target state mismatch | Always start a **fresh** JLinkGDBServer for each flash |
| No SD Card boot verified | TI image conversion and SD card boot sequence not tested | Use JTAG for reliable flashing |
| Timer clock runs at ~3 MHz instead of 24 MHz | Tick fires every 1 ms as expected, but root clock source is ~3 MHz instead of 24 MHz | Adjusted `TIMER_INITIAL_COUNT` / `TIMER_RLD_COUNT` to `0xfffff44c` (~1 ms @ ~3 MHz) in `hal_bspInit.c` |
| Notify a deleted task = crash | ISR must never notify a task that was `vTaskDelete()`'d | Clear `xISRNotifyTaskHandle = NULL` before deleting NotifyWait (`TaskNotify.c`) |

---

## License

This project contains code from:
- **FreeRTOS** — MIT licensed
- **TI StarterWare** — Texas Instruments BSD-style license
- **Amazon FreeRTOS** — Apache License 2.0

See individual files for specific license text.
