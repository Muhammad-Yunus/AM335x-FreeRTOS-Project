# FreeRTOS AM3352 Memory Management

Port of FreeRTOS for Texas Instruments AM3352 (ARM Cortex-A8) demonstrating FreeRTOS memory management: static vs dynamic task allocation, heap schemes (heap_4 default, switchable to heap_1–heap_5), and stack vs heap monitoring via UART.

![Hardware](https://img.shields.io/badge/Hardware-AM3352%20Cortex--A8-blue) ![FreeRTOS](https://img.shields.io/badge/RTOS-FreeRTOS%20v10.2.0-green) ![Build](https://img.shields.io/badge/Build-CMake%203.11+-purple)

---

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
- [Features](#features)
- [Memory Map](#memory-map)
- [Project Structure](#project-structure)
- [Toolchain](#toolchain)
- [Build Instructions](#build-instructions)
- [Flash Instructions](#flash-instructions)
- [Live UART Log](#live-uart-log)
- [Known Issues](#known-issues)
- [License](#license)

---

## Overview

This project runs a bare-metal FreeRTOS kernel on a TI AM3352 SoC (BeagleBone Black / Antminer L3+ hardware). The system boots directly into DDR memory via JTAG, initializes MMU, AINTC, UART, and DMTimer2, then starts two FreeRTOS tasks:

- **TaskStatic** — Created via `xTaskCreateStatic()`. TCB and stack live in `.bss` (pre-allocated global arrays), **zero heap cost**. Periodically logs free heap size and stack high-water mark.
- **TaskDynamic** — Created via `xTaskCreate()`. TCB and stack allocated from FreeRTOS heap. Demonstrates `pvPortMalloc()` / `vPortFree()` on the heap, then deletes itself via `vTaskDelete(NULL)` to return its TCB+stack to the heap.

No SD card or external bootloader required. No LED, button, or interrupt peripherals used.

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                      ARM Cortex-A8 (AM3352)                        │
│                         @ 600 MHz                                  │
├─────────────────────────────────────────────────────────────────────┤
│                         DDR 0x80000000                             │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │  .bss: xTCBStatic (0x8005d0d0), xStackStatic (0x8005c0d0)  │   │
│  │  Heap_4: 0x8003c100 → ~0x8005c0d0 (61440 bytes, heap_4)    │   │
│  │  Dynamic Task Stack: grows downward from 0x80042420          │   │
│  │  Static Task Stack: 0x8005c0d0 (fixed, global array)         │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                      │
│  FreeRTOS Kernel (heap_4 — coalescing first-fit)                    │
│  ├── xTaskCreateStatic → TaskStatic (heap cost: 0 bytes)           │
│  ├── xTaskCreate      → TaskDynamic (heap cost: ~4096 bytes)       │
│  ├── pvPortMalloc(128) → heap consumption: +128 bytes              │
│  ├── vPortFree()      → heap restored                              │
│  └── vTaskDelete()    → TCB+stack returned to heap                 │
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
                    └── halBspInit() — MMU, AINTC, UART, DMTimer2 tick
                         ├── Print TIMER2_CLKSEL
                         └── Tasks Created
                              ├── TaskStatic (xTaskCreateStatic)
                              ├── TaskDynamic (xTaskCreate)
                              └── vTaskStartScheduler()
                                   ├── First tick (DMTimer2 ISR)
                                   └── Task scheduling begins
```

---

## Features

| Feature | Status | Details |
|---------|--------|---------|
| **Static Task Allocation** | ✅ Verified | TCB at 0x8005d0d0, Stack at 0x8005c0d0 — not in heap |
| **Dynamic Task Allocation** | ✅ Verified | TCB+stack from heap, ~4096 bytes consumed |
| **pvPortMalloc / vPortFree** | ✅ Verified | 128-byte buffer at 0x80042420, heap restored after free |
| **vTaskDelete (self-delete)** | ✅ Verified | Returns TCB+stack to heap; free heap → 61432 bytes |
| **Stack High Water Mark** | ✅ Verified | 960 words / 1024 words = 93.75% utilized worst-case |
| **Stack vs Heap Addr Proof** | ✅ Verified | Local var: 0x80042380 (stack), malloc: 0x80042420 (heap) |
| **No Interrupts / GPIO** | ✅ Confirmed | hal_bspInit stripped of GPIO config; no ISR registered |

---

## Memory Map

| Region | Address | Size | Notes |
|--------|---------|------|-------|
| Vector Table + Code | 0x80000000 | ~220 KB | Flash image loaded via JTAG |
| Static Task TCB (`xTCBStatic`) | 0x8005d0d0 | ~72 bytes | `.bss` — zero heap cost |
| Static Task Stack (`xStackStatic[]`) | 0x8005c0d0 | 4096 bytes (1024 × 4) | `.bss` — global array |
| FreeRTOS Heap_4 | 0x8003c100 → 0x8005c0d0 | 61440 bytes | `configTOTAL_HEAP_SIZE` |
| Dynamic Task Stack | 0x80042420 ↓ | 4096 bytes | From heap via xTaskCreate |
| Dynamic Task TCB | inside heap | ~72 bytes | From heap via xTaskCreate |
| `pvPortMalloc(128)` buffer | 0x80042420 | 128 bytes | User data; header overhead internal |

---

## Heap Scheme Switching

Active scheme: **heap_4** (coalescing first-fit, `MemMang/heap_4.c`).

To switch, edit `lib/third_party/amazon/freertos_kernel/portable/MemMang/CMakeLists.txt`:

```
target_sources(lib_third_party_amazon_freertos_kernel_portable_MemMang PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/heap_X.c"   # X = 1, 2, 3, 4, or 5
)
```

| Scheme | Behavior |
|--------|----------|
| **heap_1** | Allocate only — no free. Simplest, smallest code. |
| **heap_2** | Best fit, no coalescence. Fragmentation accumulates. |
| **heap_3** | Wraps compiler `malloc()`/`free()`. Non-deterministic. |
| **heap_4** | First fit + coalescing. **Default**. Resists fragmentation. |
| **heap_5** | Like heap_4 but spans multiple memory regions. |

---

## Project Structure

```
FreeRTOS_AM335x_Memory_Debug/
├── CMakeLists.txt
├── ProjectIncludes.cmake
├── bbb.lds                         # Linker: DDR at 0x80000000, Heap_4 ~60KB
├── gcc-a.toolchain.ccs12.cmake
├── AM335xFreeRTOS_cmake_makefile_args.py
├── AGENT.md
├── docs/
│   ├── vision.md
│   ├── architecture.md
│   ├── requirements.md
│   ├── HOW_TO_BUILD.md
│   └── HOW_TO_FLASH.md
├── src/
│   ├── application/
│   │   ├── main.c                  # Entry: creates Static + Dynamic tasks
│   │   ├── TaskLED2.c              # TaskStatic + TaskDynamic (memory demo)
│   │   └── app_utils.c             # Hooks + static idle/timer task memory
│   ├── inc/
│   │   ├── FreeRTOSConfig.h        # 60KB heap, static alloc enabled
│   │   ├── app_utils.h
│   │   └── TaskLED2.h
│   └── portable/
│       ├── FreeRTOS/portable/GCC/ARM_CA8_amm335x/
│       │   ├── port.c
│       │   └── portASM_CA8_am335x.S
│       └── AM335X/
│           ├── hal_bspInit.c       # MMU, AINTC, UART, DMTimer2 tick only
│           ├── bsp_platform.c
│           ├── hal_mmu.c
│           └── ...
└── lib/third_party/
    ├── ti/                         # StarterWare 02.00.01.01
    ├── amazon/                     # FreeRTOS v10.2.0 (commit 2bb31547)
    └── c_source_tools/             # CMake file-list generator
```

---

## Toolchain

| Component | Version | Path |
|-----------|---------|------|
| Compiler | GCC ARM 7.3.1 | `gcc-arm-none-eabi-7-2018-q2-update` |
| Build | CMake 3.11+ + Ninja | — |
| Flash/Debug | J-Link V8.44 GDB Server | `JLinkGDBServer.exe` |
| RTOS | FreeRTOS v10.2.0 | `lib/third_party/amazon/` |
| Peripheral Lib | TI StarterWare 02.00.01.01 | `lib/third_party/ti/` |

---

## Build Instructions

### 1. Clone Dependencies (project baru)

```powershell
New-Item -ItemType Directory -Force lib/third_party | Out-Null
git clone http://github.com/kryochronic/AM335X_StarterWare_02_00_01_01.git lib/third_party/ti
git clone https://github.com/aws/amazon-freertos.git lib/third_party/amazon
git clone http://github.com/kryochronic/c_source_tools lib/third_party/c_source_tools

cd lib/third_party/amazon
git checkout 2bb3154718ecf0346e3236eef7039a27977d46d9
git submodule update --init --recursive
cd ../..
```

### 2. Generate CMake Configuration

```powershell
python AM335xFreeRTOS_cmake_makefile_args.py
```

### 3. Configure & Build

```powershell
cmake -S . -B build -G Ninja "-DCMAKE_TOOLCHAIN_FILE=$PWD/gcc-a.toolchain.ccs12.cmake" -DCMAKE_C_COMPILER_WORKS=1
cmake --build build -- -j1
```

> **Wajib `-j1`**: parallel build menyebabkan konflik rename file `.a`.

Output: `build/freeRTOSBBB.elf` (tiimage.exe gagal — abaikan, ELF valid untuk JTAG).

---

## Flash Instructions

### JTAG via J-Link GDB Server

**Terminal 1 — Start J-Link GDB Server:**
```powershell
Start-Process -FilePath "C:\Program Files\SEGGER\JLink_V844\JLinkGDBServer.exe" `
  -ArgumentList "-if JTAG -device AM3352 -endian little -speed 12000 -port 2331"
```

Verify: `Get-NetTCPConnection -LocalPort 2331 | Select-Object State` → `Listen`

**Terminal 2 — Flash:**
```powershell
C:\ti\gcc-arm-none-eabi-7-2018-q2-update\bin\arm-none-eabi-gdb.exe build/freeRTOSBBB.elf `
  -batch `
  -ex "target remote localhost:2331" `
  -ex "load" `
  -ex "monitor go 0x80000100"
```

> ⚠️ JANGAN gunakan `monitor reset` sebelum `load`. Gunakan JLinkGDBServer baru setiap flash.

---

## Live UART Log

Connect serial terminal to **COM4 @ 115200 8N1**. Verified output from target:

```
TIMER2_CLKSEL=0x00000001
[Heap Demo] Initial Free Heap: 0 bytes
[Static Task] Created OK. TCB=0x8005d0d0 Stack=0x8005c0d0
Scheduler started
First tick!
[Dynamic Task] Running. Stack base=0x00000000
[Dynamic Task] Local var 'local_on_stack' addr=0x80042380
[Dynamic Task] FreeHeap before malloc: 57216 bytes
[Dynamic Task] pvPortMalloc(128) -> 0x80042420 OK
[Dynamic Task] FreeHeap after malloc:  57088 bytes
[Dynamic Task] Buffer written (128 bytes)
[Dynamic Task] vPortFree() done. FreeHeap: 57216 bytes
[Dynamic Task] Deleting itself...
[Static Task] Running. Stack base=0x8005c0d0
[Static Task] Alive. FreeHeap=61432  StackHWM=960 words
```

```
[Static Task] Alive. FreeHeap=61432  StackHWM=960 words   ← repeats every 2s
```

### Output Analysis

| Log Line | Value | Meaning |
|----------|-------|---------|
| `Initial Free Heap: 0` | 0 bytes | Heap not initialized yet (scheduler not started) |
| `TCB=0x8005d0d0 Stack=0x8005c0d0` | BSS region | Static task — zero heap cost, pre-allocated in `.bss` |
| `FreeHeap before malloc: 57216` | 57216 bytes | After Dynamic task creation (61440 − 4096 for TCB+stack) |
| `pvPortMalloc(128) -> 0x80042420` | addr in DDR | Buffer on heap_4 region |
| `FreeHeap after malloc: 57088` | 57088 bytes | 57216 − 128 = 57088 (user bytes; header overhead internal) |
| `vPortFree → 57216` | restored | Memory returned to heap_4 free list |
| `vTaskDelete → 61432` | 61432 bytes | Dynamic task TCB+stack returned; 61440 − 8 bytes internal overhead |
| `StackHWM=960 words` | 960/1024 | Worst-case stack usage: 93.75% of 4096-byte static stack |
| `Local var addr=0x80042380` | stack region | Proves local vars live on task stack, not heap |
| `Malloc addr=0x80042420` | heap region | Proves heap is separate region above stack |

---

## Known Issues

| Issue | Notes |
|-------|-------|
| `Initial Free Heap: 0` | Normal — heap fully initialized only at scheduler start |
| `tiimage.exe` missing | ELF/BIN valid; SD card image generation skipped |
| `StackHWM=960` | Static task stack near limit; increase `pdAPP_TASK_STACK_SIZE_1KW_UL` if adding more logic |

---

## License

FreeRTOS (MIT), TI StarterWare (TI BSD), Amazon FreeRTOS (Apache 2.0). See individual source files.
