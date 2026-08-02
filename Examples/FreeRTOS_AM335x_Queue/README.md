# FreeRTOS AM3352 Queue (Task Communication)

Port of FreeRTOS for Texas Instruments AM3352 (ARM Cortex-A8) platform demonstrating **Queue-based task communication**: a Producer task sends messages to a FIFO queue with `xQueueSend()`, a Consumer task receives them with `xQueueReceive()`. The demo also highlights **Queue Length** (`uxQueueMessagesWaiting` / `uxQueueSpacesAvailable`), **Queue Blocking Time** (blocking on empty and full queue), and the **Producer-Consumer pattern**. Originally based on the FreeRTOS Cortex-A9 port, adapted for AM3352's AINTC instead of GIC.

![Hardware](https://img.shields.io/badge/Hardware-AM3352%20Cortex--A8-blue) ![FreeRTOS](https://img.shields.io/badge/RTOS-FreeRTOS%20v10.2.0-green) ![Build](https://img.shields.io/badge/Build-CMake%203.11+-purple)

---

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
- [Features](#features)
- [Queue Architecture](#queue-architecture)
- [Project Structure](#project-structure)
- [Toolchain](#toolchain)
- [Build Instructions](#build-instructions)
- [Flash Instructions](#flash-instructions)
- [Live UART Log](#live-uart-log)
- [Known Issues](#known-issues)
- [License](#license)

---

## Overview

This project runs a bare-metal FreeRTOS kernel on a TI AM3352 SoC (BeagleBone Black / Antminer L3+ hardware). The system boots directly into DDR memory via JTAG, initializes MMU, AINTC, UART, and DMTimer2, then starts two FreeRTOS tasks communicating through a single queue:

- **Producer Task (prio 2)** — Sends `AppQueueMessage_DSType` messages (8 bytes) via `xQueueSend()` every 200 ms. Faster than the consumer, so the queue fills up and `xQueueSend()` starts blocking (Blocking Time). Also demonstrates suspend/resume and deletion of the Consumer.
- **Consumer Task (prio 1)** — Receives messages via `xQueueReceive(..., portMAX_DELAY)` every 500 ms. Blocks on the empty queue at startup (Blocking Time) and is slower than the producer so the Queue Length can be observed filling.
- **Queue** — FIFO with capacity 4 items, created with `xQueueCreate(4, sizeof(AppQueueMessage_DSType))`.

No LED, no GPIO, no interrupts — all debug output goes through UART0 @ 115200 8N1.

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
                            │     │     Heap 10 MB            │   │
                            │     │     Stack 10 MB           │   │
          ┌──────────────────┤     └───────────────────────────┘   │
          │                  └─────────────────────────────────────┘
          │       FreeRTOS Kernel                        │
          │  ┌────────────────────────────────┐          │
          │  │   xQueueCreate(4, 8 bytes)     │          │
          │  │   Producer → xQueueSend        │          │
          │  │   Consumer ← xQueueReceive     │          │
          │  │   DMTimer2 ISR → Tick Handler  │          │
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
                   └── halBspInit() — MMU, AINTC, UART, DMTimer2
                        ├── xQueueCreate(4, sizeof(msg))
                        ├── Tasks Created (Producer + Consumer)
                        └── vTaskStartScheduler()
```

---

## Features

### ✅ Implemented & Verified (via JTAG)

| Feature | Status | Details |
|---------|--------|---------|
| **Queue Creation** | ✅ | `xQueueCreate(4, sizeof(AppQueueMessage_DSType))` → `Queue created (length=4, item_size=8)` |
| **Producer → Send** | ✅ | `xQueueSend()` every 200 ms, prints `qlen` & `spaces` |
| **Consumer → Receive** | ✅ | `xQueueReceive(..., portMAX_DELAY)` every 500 ms, prints `qlen` |
| **Queue Length** | ✅ | `uxQueueMessagesWaiting()` / `uxQueueSpacesAvailable()` log queue filling to `qlen=4, spaces=0` |
| **Blocking Time (empty)** | ✅ | Consumer blocks `portMAX_DELAY` on the empty queue at startup |
| **Blocking Time (full)** | ✅ | Producer `xQueueSend(..., 1000 ms)` times out when the queue is full |
| **Producer-Consumer** | ✅ | Two independent tasks, one FIFO queue, no shared variables |
| **Task Lifecycle** | ✅ | Suspend/resume Consumer + cross-task delete + self-delete, state via `eTaskGetState()` |
| **Serial Logger** | ✅ Active | UART 115200 8N1: queue + lifecycle events |
| **FreeRTOS Scheduler** | ✅ Running | 1 ms tick via DMTimer2, preemptive, 10 priority levels |

---

## Queue Architecture

### API

| API | Called by | Behavior | Log |
|-----|-----------|----------|-----|
| `xQueueCreate(4, 8)` | `main` | Allocate FIFO queue (4 items × 8 bytes) | `Queue created (length=4, item_size=8)` |
| `xQueueSend(q, &m, 1000ms)` | Producer | Copy item to queue tail; block until timeout if full | `Producer sent msg=N (qlen=X, spaces=Y)` |
| `xQueueReceive(q, &m, portMAX_DELAY)` | Consumer | Copy item from queue head; block forever if empty | `Consumer recv msg=N (data=D, qlen=X)` |
| `uxQueueMessagesWaiting(q)` | both | Current number of items in the queue | printed as `qlen` |
| `uxQueueSpacesAvailable(q)` | Producer | Current free slots in the queue | printed as `spaces` |

### Blocking Time — both directions

```
Queue empty:                              Queue full (4/4):
  Consumer ─► xQueueReceive()              Producer ─► xQueueSend()
              │ block portMAX_DELAY                     │ block pdMS_TO_TICKS(1000)
              │◄─ Producer sends → unblock              │◄─ Consumer receives → unblock
              ▼                                          ▼
          recv msg                                    send msg
```

### Message struct

```c
typedef struct {
    uint32_t ulMsgId;   /* sequence number from the Producer */
    uint32_t ulData;    /* sample payload: ulMsgId * 10       */
} AppQueueMessage_DSType;   /* 8 bytes */
```

---

## Project Structure

```
FreeRTOS_AM335x_Queue/
├── CMakeLists.txt                # Root build configuration
├── ProjectIncludes.cmake         # Include paths + library definitions
├── bbb.lds                       # Linker script (DDR at 0x80000000)
├── AM335xFreeRTOS_cmake_makefile_args.py  # Auto-generates CMake includes
├── gcc-a.toolchain.ccs12.cmake   # Toolchain file for CCS12 GCC ARM
│
├── src/
│   ├── application/              # Application layer
│   │   ├── main.c                # Entry point, queue + task creation
│   │   ├── TaskQueue.c           # vProducerTask + vConsumerTask
│   │   └── app_utils.c           # FreeRTOS hooks (malloc fail, stack overflow)
│   │
│   ├── inc/
│   │   ├── FreeRTOSConfig.h      # Kernel config (10 MB heap, 10 prios)
│   │   ├── TaskQueue.h           # Message typedef + queue macros
│   │   └── app_utils.h
│   │
│   └── portable/                 # Platform adaptation layer
│       ├── FreeRTOS/
│       │   └── portable/GCC/ARM_CA8_amm335x/
│       │       ├── port.c          # FreeRTOS C port (context switch, interrupts)
│       │       └── portASM_CA8_am335x.S  # ARM asm: SVC, IRQ, nested interrupts, FPU
│       │
│       ├── AM335X/
│       │   ├── hal_bspInit.c       # Board init: MMU, AINTC, UART, DMTimer2
│       │   ├── bsp_platform.c      # vApplicationFPUSafeIRQHandler (AINTC dispatch)
│       │   ├── ported_am335x_startup.c  # Vector table, DDR/PLL declarations
│       │   ├── ported_am335x_interrupt.c # AINTC driver (register/unregister ISR)
│       │   ├── hal_mmu.c           # MMU setup
│       │   ├── ported_am335x_clock_data.c
│       │   └── ported_amm335x_init.S  # Reset handler, stack setup, BSS clear
│       └── syscalls_minimal.c      # newlib syscalls (_sbrk, _write, etc.)
│
├── lib/
│   └── third_party/               # Cloned dependencies (see Build Instructions)
│       ├── ti/                    # TI StarterWare (UART, timers, AINTC)
│       ├── amazon/                # Amazon FreeRTOS v10.2.0
│       └── c_source_tools/        # CMake utility
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
| **Build System** | CMake 3.11+ (verified with 4.3.3) | `cmake` |
| **Generator** | Ninja (verified 1.13.2) | `ninja` |
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

Verified build results (this project): ELF = 8,660,440 bytes, BIN = 244,816 bytes; `arm-none-eabi-size`: text=224,044 B, data=20,760 B, bss=31,840,268 B (heap+stack 10 MB each in DDR). Build completed 92/92 targets — the only failing step is the SDCARD `tiimage.exe` conversion (expected, see above).

Verified toolchain: GCC ARM 7.3.1, CMake 4.3.3, Ninja 1.13.2, Python 3.12.1.

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

Serial output appears on UART at **115200 8N1** on **COM4** (dedicated USB TTL) once the program is running. Do not use COM5 — that is the J-Link CDC virtual port, not the console.

---

## Live UART Log

Connect a serial terminal to **COM4** (USB Serial Port / dedicated USB TTL — *not* COM5, the J-Link CDC virtual port) at **115200 8N1** to view the demo. Verified capture from the board:

```
TIMER2_CLKSEL=0x00000001
Queue created (length=4, item_size=8)
Producer task created (prio=2)
Consumer task created (prio=1)
Scheduler started
Enabling timer interrupt!
Timer interrupt enabled OK
Starting first task...
First tick!
Producer starting, waiting 1000 ms before first send...
Consumer blocking on receive (queue empty, portMAX_DELAY)...
Producer sending msg=0 (qlen before=0)...
Producer sent msg=0 (qlen=1, spaces=3)
Consumer recv msg=0 (data=0, qlen=0)
Producer sending msg=1 (qlen before=0)...
Producer sent msg=1 (qlen=1, spaces=3)
Producer sending msg=2 (qlen before=1)...
Producer sent msg=2 (qlen=2, spaces=2)
Consumer recv msg=1 (data=10, qlen=1)
Producer sending msg=3 (qlen before=1)...
Producer sent msg=3 (qlen=2, spaces=2)
Producer sending msg=4 (qlen before=2)...
Producer sent msg=4 (qlen=3, spaces=1)
Consumer recv msg=2 (data=20, qlen=2)
Producer sending msg=5 (qlen before=2)...
Producer sent msg=5 (qlen=3, spaces=1)
Producer suspending Consumer (state=Blocked)      ← Suspend
Consumer state after suspend: Suspended
Producer sending msg=6 (qlen before=3)...
Producer sent msg=6 (qlen=4, spaces=0)            ← Queue FULL
Producer sending msg=7 (qlen before=4)...
Producer send TIMEOUT after 1000 ms (queue full) — blocking time   ← Blocking Time
Producer sending msg=8 (qlen before=4)...
Producer send TIMEOUT after 1000 ms (queue full) — blocking time   ← Blocking Time
Producer resuming Consumer...                     ← Resume
Consumer state after resume: Ready
Consumer recv msg=3 (data=30, qlen=3)
Producer sending msg=9 (qlen before=3)...
Producer sent msg=9 (qlen=4, spaces=0)
Producer sending msg=10 (qlen before=4)...
Producer sent msg=10 (qlen=4, spaces=0)
Consumer recv msg=4 (data=40, qlen=4)
Producer sending msg=11 (qlen before=4)...
Producer sent msg=11 (qlen=4, spaces=0)
Producer deleting Consumer (cross-task)           ← Delete (cross-task)
Consumer deleted
Producer deleting itself (self)                   ← Delete (self)
```

- `qlen` (from `uxQueueMessagesWaiting`) shows the queue filling to `4/4`
- `Consumer blocking on receive` shows blocking on an **empty** queue
- `Producer send TIMEOUT after 1000 ms` shows blocking on a **full** queue (Blocking Time)
- `Producer sent msg=N` ↔ `Consumer recv msg=N` shows the Producer-Consumer pattern
- Suspend at msg 5, resume at msg 8, cross-task delete + self-delete at msg 11 show the task lifecycle

> **Note:** the demo is **self-terminating** — the Producer deletes itself at iteration 11, so the full log appears only once (~15-20 s). To watch again, reload the program via GDB and keep the serial terminal open on COM4 before `load`.
>
> **Note:** `qlen`/`spaces` are read at print time, not at send/receive time. With preemptive scheduling the Consumer (prio 1) may be preempted by the Producer (prio 2) between `xQueueReceive` and its log line, so the printed `qlen` can already include a message the Producer re-enqueued into the freed slot. This is expected interleaving, not a bug.

---

## Known Issues

| Issue | Impact | Workaround |
|-------|--------|------------|
| `monitor reset` kills DDR state | Issuing `reset` before `load` in JTAG mode can corrupt DDR contents, causing crash | Use `load` + `continue` only; never `monitor reset` |
| J-Link sessions are not reusable | Reusing a stale J-Link GDB Server can cause target state mismatch | Always start a **fresh** JLinkGDBServer for each flash |
| `xQueueHandle` reserved by FreeRTOS | `xQueueHandle` is a legacy compat macro for `QueueHandle_t`, so a variable with that name fails to compile | Queue variable is named `xMsgQueue` |
| No SD Card boot verified | TI image conversion and SD card boot sequence not tested | Use JTAG for reliable flashing |
| Exact UART ordering varies | Blocking/queue timing depends on tick & preemption | Order of log lines may differ run-to-run; patterns remain consistent |

---

## License

This project contains code from:
- **FreeRTOS** — MIT licensed
- **TI StarterWare** — Texas Instruments BSD-style license
- **Amazon FreeRTOS** — Apache License 2.0

See individual files for specific license text.
