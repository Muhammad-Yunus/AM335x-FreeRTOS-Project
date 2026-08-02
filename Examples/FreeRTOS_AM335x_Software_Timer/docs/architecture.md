# Architecture — FreeRTOS AM3352 Software Timer

## Approach

Project ini dibangun dengan strategi **copy base project, lalu modifikasi minimal**:

1. **Copy seluruh isi** `FreeRTOS_AM335x_GPIO_INTERRUPT/` ke `FreeRTOS_AM335x_Software_Timer/`
2. **Ikuti README.md** untuk clone dependencies, generate CMake, configure, dan build
3. **Modifikasi/hapus kode GPIO interrupt & LED** dan ganti dengan logic **FreeRTOS Software Timer**.

Semua layer di bawahnya (boot assembly, vector table, MMU, AINTC init, DMTimer2 tick, UART console, syscalls, CMake, linker script) **tidak disentuh**.

## System Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                      ARM Cortex-A8 (AM3352)                        │
│                         @ 600 MHz                                  │
│ ├───────────────────────────────────────────────────────────────────┤
│ │                         DDR 0x80000000                             │
│ │  ┌─────────────────────────────────────────────────────────────┐   │
│ │  │  Vector Table @ 0x80000000                                  │   │
│ │  │  .text (code)                                               │   │
│ │  │  .data / .bss                                               │   │
│ │  │  Heap 10 MB                                                 │   │
│ │  │  Stack 10 MB                                                │   │
│ │  └─────────────────────────────────────────────────────────────┘   │
│ │                                                                    │
│ │  FreeRTOS Kernel                                                   │
│ │  ├── Timer Service Task (Daemon Task)                              │
│ │  │     ├── Manages One-shot and Auto-reload timers                 │
│ │  │     └── Executes timer callback functions                       │
│ │  ├── Task: Timer_Monitor (priority 1)                             │
│ │  │     ├── Creates One-shot & Auto-reload timers                   │
│ │  │     ├── Starts timers using xTimerStart                         │
│ │  │     └── Prints lifecycle logs to UART                           │
│ │  └── DMTimer2 ISR @ 1 ms → FreeRTOS_Tick_Handler                  │
│ │                                                                    │
│ │  Application:                                                      │
│ │  ├── One-shot Timer Callback                                       │
│ │  ├── Auto-reload Timer Callback                                    │
│ │  └── UART console output                                           │
│ └────────────────────────────────────────────────────────────────────┘
```

## Boot Flow

```
Reset → .S (ported_amm335x_init.S)
          ├── Setup stacks for all 6 ARM modes
          ├── Enable Neon/VFP FPU
          ├── Clear BSS
          └── Call start_boot()
               ├── Copy vector table to 0x80000000
               ├── Set up IRQ/FIQ exception handlers
               └── Call main()
                    └── halBspInit()
                         ├── MMU init (DDR + peripheral)
                         ├── AINTC init
                         ├── DMTimer2 → FreeRTOS tick
                         ├── UART0 console (no GPIO initialization)
                         └── Tasks & Timers Created
                              └── vTaskStartScheduler()
```

## FreeRTOS Software Timer Details

### Configured parameters (`FreeRTOSConfig.h`)
- `configUSE_TIMERS = 1`
- `configTIMER_TASK_PRIORITY = 3`
- `configTIMER_QUEUE_LENGTH = 10`
- `configTIMER_TASK_STACK_DEPTH = configMINIMAL_STACK_SIZE`

### Timers to Create
1. **One-shot Timer**:
   - Periode: 5000 ms (5000 ticks pada 1kHz tick rate)
   - Auto-reload: `pdFALSE`
   - Callback: `prvOneShotTimerCallback`
2. **Auto-reload Timer**:
   - Periode: 2000 ms (2000 ticks pada 1kHz tick rate)
   - Auto-reload: `pdTRUE`
   - Callback: `prvAutoReloadTimerCallback`

## Memory Map (sama dengan base project)

| Address | Region | Size |
|---------|--------|------|
| `0x80000000` | Vector Table + Code (.text) | ~480 KB |
| `0x80076000` | .data, .bss | ~25 KB |
| `0x8007A000` | Heap | 10 MB |
| `0x80A7A000` | Stack | 10 MB |

## Project Structure

Setelah modifikasi, struktur menjadi:

```
FreeRTOS_AM335x_Software_Timer/
├── CMakeLists.txt
├── ProjectIncludes.cmake
├── bbb.lds
├── AM335xFreeRTOS_cmake_makefile_args.py
├── gcc-a.toolchain.ccs12.cmake
├── flash.jlink
├── README.md
├── docs/
│   ├── vision.md
│   ├── requirements.md
│   └── architecture.md
├── src/
│   ├── application/
│   │   ├── main.c                    # ← MODIFIED: Timer monitoring task creation
│   │   ├── TaskTimerDemo.c           # ← NEW/REPLACED: software timer demo implementation
│   │   └── app_utils.c
│   ├── portable/
│   │   ├── FreeRTOS/portable/GCC/ARM_CA8_amm335x/
│   │   │   ├── port.c
│   │   │   └── portmacro.h
│   │   ├── AM335X/
│   │   │   ├── hal_bspInit.c         # ← MODIFIED: clean of GPIO interrupts
│   │   │   ├── hal_bspInit.h
│   │   │   ├── ported_am335x_startup.c
│   │   │   ├── ported_am335x_interrupt.c
│   │   │   ├── ported_am335x_clock_data.c
│   │   │   ├── hal_mmu.c
│   │   │   └── syscalls_minimal.c
│   │   └── AM335XInit/
│   │       └── ported_amm335x_init.S
│   └── inc/
│       └── FreeRTOSConfig.h          # ← MODIFIED: enable software timers
└── lib/third_party/
```

## Serial Monitor

Baca output via **COM4** pada 115200 8N1.

## Build Steps

```powershell
python AM335xFreeRTOS_cmake_makefile_args.py
cmake -S . -B build -G Ninja "-DCMAKE_TOOLCHAIN_FILE=%cd%/gcc-a.toolchain.ccs12.cmake" -DCMAKE_C_COMPILER_WORKS=1
cmake --build build -- -j1
arm-none-eabi-gdb.exe build/freeRTOSBBB.elf -batch -ex "target remote localhost:2331" -ex "load" -ex "monitor go 0x80000100"
```

## Verification

1. **Build sukses** → ELF & BIN terbentuk
2. **UART log** menunjukkan sequence:
   - `Scheduler started`
   - `Timer Monitor Task created!`
   - `One-shot Timer created!`
   - `Auto-reload Timer created!`
   - `Starting both timers...`
   - `Auto-reload Timer Callback triggered! (Count: 1)` pada T = 2s
   - `Auto-reload Timer Callback triggered! (Count: 2)` pada T = 4s
   - `One-shot Timer Callback triggered!` pada T = 5s (hanya sekali)
   - `Auto-reload Timer Callback triggered! (Count: 3)` pada T = 6s
   - ... dan seterusnya.
