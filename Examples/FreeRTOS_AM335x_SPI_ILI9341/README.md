# FreeRTOS AM3352 ILI9341 LCD Demo

Port of FreeRTOS for Texas Instruments AM3352 (ARM Cortex-A8) platform driving an **ILI9341 2.8" TFT LCD** via SPI0. Includes a full graphics demo with color bands, shapes, text, and pixel art.

![Hardware](https://img.shields.io/badge/Hardware-AM3352%20Cortex--A8-blue)
![Display](https://img.shields.io/badge/Display-ILI9341%202.8%22%20TFT-9B59B6)
![Build](https://img.shields.io/badge/Build-CMake%203.11+-purple)
![SPI](https://img.shields.io/badge/SPI-16MHz%20Mode0-orange)

---

## Table of Contents

- [Overview](#overview)
- [Hardware](#hardware)
- [Pin Mapping](#pin-mapping)
- [Demo Features](#demo-features)
- [Architecture](#architecture)
- [Project Structure](#project-structure)
- [Toolchain](#toolchain)
- [Build Instructions](#build-instructions)
- [Flash Instructions](#flash-instructions)
- [Performance](#performance)
- [Known Issues](#known-issues)
- [License](#license)

---

## Overview

This project runs a bare-metal FreeRTOS kernel on a TI AM3352 SoC (BeagleBone Black hardware). The system boots directly into DDR memory via JTAG, initializes the MMU, AINTC, UART, and SPI0 controller, then drives an ILI9341 2.8" TFT display at **16 MHz**.

### Demo Sequence (loops forever)

| # | Demo | Description | Duration |
|---|------|-------------|----------|
| 1 | **Color Bands** | 8 vertical stripes (Red → Orange → Yellow → Green → Cyan → Blue → Magenta → White) | 1.5s |
| 2 | **Shapes** | Hollow/rects, circles, diagonal lines | 1.5s |
| 3 | **Text** | ASCII alphabet (A-Z, a-z, 0-9) | 1.5s |
| 4 | **Pixel Grid** | Checkerboard pattern + solid panel | 2.0s |
| 5 | **Constant Data** | Full-screen color fills with timing printout | 0.3s each |

### SPI Optimization

- **Clock:** 16 MHz (vs original 10 MHz)
- **CS Control:** Single assert/deassert per buffer (not per-byte)
- **Delay:** 0x10 per byte for stable ILI9341 timing
- **Fill Time:** ~156ms full-screen (320×240 RGB565)

---

## Hardware

### Required Components

- **BeagleBone Black** or **AM3352 development board**
- **ILI9341 2.8" TFT LCD** (SPI interface, 320×240, RGB565)
- **J-Link debugger** (SEGGER J-Link)
- **USB cable** (for UART console)

### LCD Module Wiring

| LCD Signal | BeagleBone Pin | AM3352 Function |
|-----------|----------------|-----------------|
| **SCLK** | P9_22 | SPI0_SCLK |
| **MOSI** | P9_18 | SPI0_D1 (MOSI) |
| **CS** | P8_26 | GPIO1_29 (DC pin) |
| **DC** | P8_19 | GPIO0_22 (RST pin) |
| **RST** | — | GPIO0_22 |
| **GND** | P9_1 | GND |
| **VCC** | P9_3 | 3.3V |

> **Note:** The ILI9341 LCD module used in this project has CS, DC, and RST hardwired to the GPIO pins above. SPI data lines connect via header pins.

---

## Pin Mapping

### SPI Pins (BeagleBone Header)

| Signal | Pin Header | Pad Control Register | Offset | Function |
|--------|-----------|---------------------|--------|----------|
| **CLK** | **P9_22** | `CONTROL_CONF_SPI0_SCLK` | 0x950 | SPI0 clock (CPOL=0, CPHA=0) |
| **MOSI** | **P9_18** | `CONTROL_CONF_SPI0_D1` | 0x958 | SPI0 data line 1 → output enabled |
| **MISO** | **P9_21** | `CONTROL_CONF_SPI0_D0` | 0x954 | SPI0 data line 0 — input (unused for LCD) |
| **CS** | Hardware | — | — | Controlled by GPIO (DC pin) |

### GPIO DC/RST Pins

| Signal | Pin Header | Register | GPIO Bank | GPIO # | Function |
|--------|-----------|----------|-----------|--------|----------|
| **DC** | **P8_26** | `CONTROL_CONF_GPMC_CSN(0)` | GPIO1 | 29 | Data/Command select (active HIGH) |
| **RST** | **P8_19** | `CONTROL_CONF_GPMC_AD(8)` | GPIO0 | 22 | Reset (active HIGH) |

---

## Demo Features

### Graphics Primitives

- `ILI9341_FillScreen(color)` — Fill entire 320×240 screen
- `ILI9341_FillRect(x, y, w, h, color)` — Fill rectangle
- `ILI9341_DrawRect(x, y, w, h, color)` — Hollow rectangle outline
- `ILI9341_DrawCircle(cx, cy, r, color)` — Hollow circle
- `ILI9341_FillCircle(cx, cy, r, color)` — Filled circle
- `ILI9341_DrawLine(x0, y0, x1, y1, color)` — Bresenham line
- `ILI9341_DrawPixel(x, y, color)` — Single pixel
- `ILI9341_DrawString(x, y, text, fg, bg)` — 5×7 ASCII text

### SPI Low-Level Driver

- `Spi0TxByte(uint8_t b)` — Transmit single byte with CS control
- `Spi0TxBuffer(const uint8_t *buf, uint32_t len)` — Burst transmit with D-Cache flush
- `LcdDcLow()` / `LcdDcHigh()` — GPIO control for DC pin
- `LcdRstLow()` / `LcdRstHigh()` — GPIO control for RST pin

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                      ARM Cortex-A8 (AM3352)                        │
│                         @ 600 MHz                                  │
├─────────────────────────────────────────────────────────────────────┤
│                         DDR 0x80000000                             │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │  Vector Table @ 0x80000000                                  │   │
│  │  .text (code)                                               │   │
│  │  .data / .bss                                               │   │
│  │  Heap 10 MB                                                 │   │
│  │  Stack 10 MB                                                │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                      │
│  FreeRTOS Kernel                                                     │
│  ├── Task: LCD (ILI9341 demo loop, priority 1)                      │
│  │     ├── ILI9341_Init()                                          │
│  │     ├── Demo_ColorBands() — 8 color stripes                      │
│  │     ├── Demo_Shapes() — Rects, circles, lines                    │
│  │     ├── Demo_Text() — ASCII alphabet                             │
│  │     ├── Demo_PixelGrid() — Checkerboard pattern                  │
│  │     └── Demo_ConstantData() — Full-screen fills + timing         │
│  │                                                                   │
│  └── DMTimer2 ISR @ 1 ms → FreeRTOS_Tick_Handler                    │
│                                                                      │
│  Board Init (hal_bspInit):                                          │
│  ├── MMU init (DDR + peripheral)                                     │
│  ├── AINTC init                                                      │
│  ├── DMTimer2 → FreeRTOS tick                                        │
│  ├── UART0 console                                                   │
│  ├── SPI0: 16 MHz, Mode 0, 8-bit, manual CS                         │
│  └── GPIO: DC (GPIO1_29), RST (GPIO0_22)                            │
│                                                                      │
│  SPI Pin Mapping:                                                   │
│  ├── P9_22 (SCLK) - CONTROL_CONF_SPI0_SCLK                          │
│  ├── P9_18 (MOSI) - CONTROL_CONF_SPI0_D1                            │
│  ├── P9_21 (MISO) - CONTROL_CONF_SPI0_D0  (input, unused)           │
│  └── DC/RST via GPIO — ILI9341 control lines                        │
└─────────────────────────────────────────────────────────────────────┘
```

### Boot Flow

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
                           ├── UART0 console
                           ├── SPI0: pinmux, reset, clock config @ 16MHz
                           ├── GPIO1: DC pin (P8_26)
                           └── GPIO0: RST pin (P8_19)
                                └── Tasks Created
                                     └── vTaskStartScheduler()
                                          └── LCD Task runs
                                               └── ILI9341_Init()
                                               └── Demo loop (ColorBands → Shapes → Text → PixelGrid → ConstantData)
```

---

## Project Structure

```
FreeRTOS_AM335x_SPI_ILI9341/
├── CMakeLists.txt                # CMake build configuration
├── ProjectIncludes.cmake         # Auto-generated include paths
├── bbb.lds                       # Linker script
├── AM335xFreeRTOS_cmake_makefile_args.py  # Build args generator
├── gcc-a.toolchain.ccs12.cmake   # GCC ARM toolchain
├── README.md                     # ← THIS FILE
├── src/
│   ├── application/
│   │   ├── main.c                # SPI hooks + LCD demo task + main()
│   │   ├── ili9341.c             # ILI9341 driver implementation
│   │   ├── ili9341.h             # ILI9341 driver API
│   │   ├── fonts.h               # 5×7 monospace font
│   │   ├── app_utils.c           # Utility functions
│   │   ├── app_utils.h
│   │   ├── TaskSPI_TX.c          # SPI TX task (kept for reference)
│   │   └── TaskSPI_TX.h
│   ├── inc/
│   │   └── FreeRTOSConfig.h      # FreeRTOS configuration
│   └── portable/
│       ├── FreeRTOS/portable/GCC/ARM_CA8_amm335x/
│       │   ├── port.c            # FreeRTOS port implementation
│       │   ├── portmacro.h       # Port macros
│       │   └── portASM_CA8_am335x.S  # Assembly startup
│       └── AM335X/
│           ├── hal_bspInit.c     # Board init (SPI0, GPIO, MMU, UART)
│           ├── hal_bspInit.h
│           ├── hal_mmu.c         # MMU initialization
│           ├── hal_mmu.h
│           ├── bsp_platform.c
│           ├── ported_am335x_startup.c
│           ├── ported_am335x_interrupt.c
│           ├── ported_am335x_clock_data.c
│           ├── ported_amm335x_init.S
│           └── CMakeLists.txt
└── lib/third_party/              # Third-party libraries
    ├── ti/           — TI StarterWare 02.00.01.01
    ├── amazon/       — Amazon FreeRTOS v10.2.0
    └── c_source_tools/ — CMake utility
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
4. **GNU ARM GCC** 7.3.1 (`gcc-arm-none-eabi-7-2018-q2-update`)
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
git submodule update --init --recursive
cd ../..
```

#### 2. Generate CMake Configuration

```powershell
python AM335xFreeRTOS_cmake_makefile_args.py
```

#### 3. Configure Toolchain

> **Important**: This project **requires** **GCC_ARM 7.3.1** (`gcc-arm-none-eabi-7-2018-q2-update`) located at `C:/ti/gcc-arm-none-eabi-7-2018-q2-update`. Other GCC ARM versions are **not compatible** because the path and prefix differ — this project uses `arm-none-eabi-`, not `arm-eabi-`.

`gcc-a.toolchain.ccs12.cmake` is provided and ready to use without editing.

#### 4. Clean Build (Recommended)

```powershell
Remove-Item -Recurse -Force build
```

#### 5. Configure and Build with Ninja

> **Important**: Use `-j1` (single-threaded build) during linking. This project has a parallel build conflict where multiple targets try to rename the same `.a` file simultaneously. Sequential build avoids this issue.

```powershell
cmake -S . -B build -G Ninja ^
  "-DCMAKE_TOOLCHAIN_FILE=$PWD/gcc-a.toolchain.ccs12.cmake" ^
  -DCMAKE_C_COMPILER_WORKS=1

cmake --build build -- -j1
```

> **Note**: The final build step attempts to generate both a BIN file and an SDCARD image via `tiimage.exe`. If `tiimage.exe` is missing (not included in this repo), the SDCARD image step will fail with exit code 1 but **the ELF and BIN files are already built and valid**. Check for `build/freeRTOSBBB.elf` — if it exists, the build succeeded for JTAG flashing.

#### 6. Verify Output

```
build/freeRTOSBBB.elf       — Linked ELF executable (~8.9 MB with debug symbols)
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

> **Note**: `-batch` is required to prevent GDB from prompting "Quit anyway? (y or n)" when the remote session ends.

**Option B: Interactive flash** (GDB stays attached for debugging):

```powershell
C:\ti\gcc-arm-none-eabi-7-2018-q2-update\bin\arm-none-eabi-gdb.exe build/freeRTOSBBB.elf
(gdb) target remote localhost:2331
(gdb) load
(gdb) continue
```

> **Important**: Do **NOT** issue `monitor reset` or `monitor halt` before `load`. The AM335x DDR state must be preserved for the program to run. Always start a **fresh** JLinkGDBServer for each flash session.

Serial output appears on UART at **115200 8N1** (e.g. COM4) once the program is running.

---

## Performance

### SPI Performance

| Metric | Value |
|--------|-------|
| **SPI Clock** | 16 MHz |
| **SPI Mode** | Mode 0 (CPOL=0, CPHA=0) |
| **Fill Time** | ~156 ms (full 320×240 screen) |
| **Throughput** | ~2.05 Mpix/s |

### Memory Usage

| Section | Size |
|---------|------|
| .text | 232,188 bytes |
| .data | 20,760 bytes |
| .bss | 31,841,036 bytes |
| **Total** | **~30.6 MB** |

### Demo Timing

| Demo | Duration |
|------|----------|
| Color Bands | 1,500 ms |
| Shapes | 1,500 ms |
| Text | 1,500 ms |
| Pixel Grid | 2,000 ms |
| Constant Data | 300 ms (per color) |
| **Total Cycle** | **~7,200 ms** |

---

## Expected UART Log

```
[SPI] SPI0 configured @ 16MHz, Mode 0
[GPIO] DC pin (GPIO1_29) initialized
[GPIO] RST pin (GPIO0_22) initialized
[UART] UART0 console ready @ 115200
[TASK] Created: LCD Task
Scheduler started
[LCD] ILI9341 initialized, starting demo
[LCD] Fill 0: 158 ms
[LCD] Fill 1: 157 ms
[LCD] Fill 2: 156 ms
[LCD] Fill 3: 156 ms
[LCD] Fill 4: 155 ms
[LCD] Fill 5: 156 ms
[LCD] Fill 6: 157 ms
[LCD] Fill 7: 156 ms
```

---

## Verification

1. **Build succeeds** → `build/freeRTOSBBB.elf` and `build/app.freeRTOSBBB.bin` exist
2. **UART log** → appears on COM4 (115200 8N1) with `[LCD] ILI9341 initialized`
3. **LCD Display** → demo loop runs continuously:
   - Color bands (8 stripes)
   - Shapes (rectangles, circles, lines)
   - Text (alphabet, numbers)
   - Pixel grid (checkerboard pattern)
   - Color fills (with timing in ms)
4. **SPI Performance** → ~156ms full-screen fill time

---

## SPI Configuration Details

```c
/* SPI0 Configuration — 16 MHz, Mode 0 */
McSPIMasterModeConfig(SPI_BASE,
                      MCSPI_SINGLE_CH,              /* Single channel mode */
                      MCSPI_TX_RX_MODE,             /* TX/RX mode */
                      MCSPI_DATA_LINE_COMM_MODE_1,  /* D1=MOSI, D0=MISO */
                      SPI_CH);
McSPIClkConfig(SPI_BASE,
               MCSPI_IN_CLK,    /* Input clock: 48 MHz (parent of SPI) */
               MCSPI_OUT_FREQ,  /* Output: 16 MHz */
               SPI_CH,
               MCSPI_CLK_MODE_0);  /* CPOL=0, CPHA=0 */
McSPICSPolarityConfig(SPI_BASE, MCSPI_CS_POL_LOW, SPI_CH);  /* Active-low CS */
McSPITxFIFOConfig(SPI_BASE, MCSPI_TX_FIFO_ENABLE, SPI_CH);  /* TX FIFO on */
McSPIRxFIFOConfig(SPI_BASE, MCSPI_RX_FIFO_ENABLE, SPI_CH);  /* RX FIFO on */
```

---

## ILI9341 Driver API

### Initialization

```c
/* Initialize LCD: reset, config, set orientation (landscape) */
void ILI9341_Init(void);
```

### Graphics Functions

```c
/* Fill entire screen with one color */
void ILI9341_FillScreen(uint16_t color);

/* Fill rectangle */
void ILI9341_FillRect(uint16_t x, uint16_t y,
                      uint16_t w, uint16_t h, uint16_t color);

/* Draw hollow rectangle */
void ILI9341_DrawRect(uint16_t x, uint16_t y,
                      uint16_t w, uint16_t h, uint16_t color);

/* Draw circle outline */
void ILI9341_DrawCircle(int16_t cx, int16_t cy,
                        int16_t r, uint16_t color);

/* Draw filled circle */
void ILI9341_FillCircle(int16_t cx, int16_t cy,
                        int16_t r, uint16_t color);

/* Draw line (Bresenham) */
void ILI9341_DrawLine(int16_t x0, int16_t y0,
                      int16_t x1, int16_t y1, uint16_t color);

/* Draw single pixel */
void ILI9341_DrawPixel(uint16_t x, uint16_t y, uint16_t color);

/* Draw 5×7 ASCII string */
void ILI9341_DrawString(uint16_t x, uint16_t y,
                        const char *s, uint16_t fg, uint16_t bg);
```

### Color Constants

```c
ILI9341_COLOR_BLACK       0x0000
ILI9341_COLOR_WHITE       0xFFFF
ILI9341_COLOR_RED         0xF800
ILI9341_COLOR_GREEN       0x07E0
ILI9341_COLOR_BLUE        0x001F
ILI9341_COLOR_CYAN        0x07FF
ILI9341_COLOR_MAGENTA     0xF81F
ILI9341_COLOR_YELLOW      0xFFE0
ILI9341_COLOR_ORANGE      0xFD20
ILI9341_COLOR_GREY        0x8410
```

---

## Known Issues

| Issue | Impact | Workaround |
|-------|--------|------------|
| `tiimage.exe` missing | SDCARD image generation fails | Ignore — ELF and BIN are valid for JTAG flash |
| `CMAKE_TOOLCHAIN_FILE` warning | CMake warning during configure | Safe to ignore |
| Parallel build fails | `ren` conflict on `.a` files | Use `-j1` for single-threaded build |
| Cross-compiler ABI test fails | Bare-metal toolchain | Use `-DCMAKE_C_COMPILER_WORKS=1` |
| COM port locked | Cannot read UART via software | Check with your own serial terminal |
| Tearing effect at 16 MHz | Flickering during updates | Reduced delay to 0x10 — verified stable |

### SPI Timing Notes

- **Delay 0x10**: Proven stable for ILI9341 at 16 MHz
- **CS Control**: Single assert/deassert per buffer reduces overhead
- **D-Cache Flush**: Required before SPI burst to ensure cache coherence

---

## License

This project contains code from:
- **FreeRTOS** — MIT licensed
- **TI StarterWare** — Texas Instruments BSD-style license
- **Amazon FreeRTOS** — Apache License 2.0
- **ILI9341 Driver** — Custom implementation for AM3352

See individual files for specific license text.
