# FreeRTOS AM3352 — LDS08RR LiDAR Decoder + ILI9341 TFT Radar

Port FreeRTOS untuk **TI AM3352 (ARM Cortex-A8)** yang berfungsi sebagai **decoder LiDAR LDS08RR** (protokol **Delta-2A / 3irobotics**). Stream data LiDAR masuk lewat **UART1 @ 115200 8N1 (polling)**, di-decode paket-per-paket, dirakit menjadi satu *scan* 360°, lalu **digambar sebagai radar point-cloud live di layar ILI9341 TFT 2.8"** via **SPI0**.

```
LDS08RR LiDAR ──► UART1 RXD (D16) ──► decode Delta-2A ──► scan 360° ──► queue ──► radar view ILI9341 (SPI0)
```

![Hardware](https://img.shields.io/badge/Hardware-AM3352%20Cortex--A8-blue) ![RTOS](https://img.shields.io/badge/RTOS-FreeRTOS%20v10.2.0-green) ![Build](https://img.shields.io/badge/Build-CMake%20%2B%20Ninja-purple) ![Protocol](https://img.shields.io/badge/Protocol-Delta--2A%20%2F%20LDS08RR-orange) ![Display](https://img.shields.io/badge/Display-ILI9341%20SPI0-teal)

---

## Table of Contents

- [Overview](#overview)
- [Arsitektur & Data Flow](#arsitektur--data-flow)
- [Fitur](#fitur)
- [Protokol Delta-2A / LDS08RR](#protokol-delta-2a--lds08rr)
- [Radar View Layout](#radar-view-layout)
- [Project Structure](#project-structure)
- [Toolchain](#toolchain)
- [Build Instructions](#build-instructions)
- [Flash Instructions](#flash-instructions)
- [Contoh Output Serial](#contoh-output-serial)
- [Known Issues](#known-issues)
- [License](#license)

---

## Overview

Project ini menjalankan FreeRTOS bare-metal di AM3352 (BeagleBone Black / Antminer L3+). Board boot langsung ke DDR via JTAG, lalu:

1. **Mengaktifkan UART1** (`UART1Init()`) — pinmux D15/D16 → MODE0, clock `CM_PER_UART1_CLKCTRL`, reset, 115200 8N1 (StarterWare `uart_irda_cir`).
2. **Menginisialisasi SPI0 + GPIO** (`halBspInit()` → `SPI0Configure()`) — 16 MHz, Mode 0, manual CS; pinmux P9_22/P9_18/P9_21/P9_17, DC P8_26, RST P8_19.
3. **Membuat 2 task aplikasi** via `xTaskCreate()`:
   - `vUART1RxTask` (prio **2**, 1024 words) — polling UART1 tiap 1 ms, decode byte demi byte, merakit scan.
   - `vLCDDisplayTask` (prio **1**, 1536 words) — menunggu scan dari queue, menggambar radar view di ILI9341.
4. **Decode protokol Delta-2A** — sinkronisasi `0xAA`, validasi header + checksum 16-bit, ekstraksi sampel `[quality, distance, angle]`.
5. **Merender radar view** — ring 0.5/1/1.5/2 m silver, titik point-cloud cyan (magenta bila < 30 cm), HUD sidebar (RPM / MIN / ANG / PTS / RANGE), incremental redraw.

Hardware LiDAR LDS08RR terhubung ke pin D16 (UART1_RXD). Radar terlihat live di layar ILI9341; UART0 (COM4) hanya banner boot + log status.

---

## Arsitektur & Data Flow

```
                             ┌──────────────────────────────────────────┐
                             │           ARM Cortex-A8 (AM3352)         │
                             ├──────────────────────────────────────────┤
                             │   FreeRTOS Kernel (tick 1 ms DMTimer2)   │
                             │                                          │
                             │   ┌──────────────────────────────┐       │
   LDS08RR LiDAR             │   │  vUART1RxTask   (prio 2)     │       │
   (Delta-2A @115200)        │   │  polling UART1 RX tiap 1 ms  │       │
        │                    │   │  → lidar_lds08rr.c (decoder) │       │
        ▼                    │   │  → lds_scan.c (akumulasi)    │       │
   UART1 RXD (D16) ────────► │   └──────────────┬───────────────┘       │
   (uart1_driver.c)          │                  │ xQueueSend (scan)     │
                             │                  ▼                       │
                             │   ┌───────────────────────────────┐      │
                             │   │  vLCDDisplayTask (prio 1)     │      │
                             │   │  xQueueReceive → lcdUpdateScan│      │
                             │   └──────────────┬────────────────┘      │
                             │                  │ ILI9341 → SPI0 (16MHz)│
                             └──────────────────┼───────────────────────┘
                                                ▼
                                   radar point-cloud (layar TFT)
```

### Alur Data

```
LDS08RR ──► D16 (UART1_RXD, MODE0)
              │
              ▼
   uart1_driver.c : UART1BytesAvailable() / UART1ReadByte()
              │  byte demi byte, tiap 1 ms
              ▼
   lidar_lds08rr.c : lds_process_byte() — state machine Delta-2A
              │  bila 1 paket valid (header + checksum)
              ▼
   lds_scan.c : LDS_ScanAddPacket() — kumpulkan sampel satu rotasi
              │  bila paket start_angle == 0 (scan selesai)
              ▼
   queue (2 slot) ──► vLCDDisplayTask ──► lcdUpdateScan() ──► ILI9341/SPI0
```

### Alur Boot

```
Reset ─► .S (ported_amm335x_init.S) → start_boot() → main()
            └── halBspInit()            (MMU, AINTC, UART0 console, DMTimer2 tick,
                │                       SPI0 + GPIO DC/RST via SPI0Configure())
                └── UART1Init()        (UART1 115200 8N1 — uart1_driver.c)
                     └── LDS_ScanQueueCreate()   (queue 2 slot)
                          └── xTaskCreate(vUART1RxTask)    (prio 2)
                          └── xTaskCreate(vLCDDisplayTask) (prio 1)
                               └── vTaskStartScheduler()
```

---

## Fitur

| Fitur | Status | Detail |
|-------|--------|--------|
| **UART1 Pinmux + Clock + 115200 8N1** | ✅ | `uart1_driver.c` — D15/D16 MODE0, `CM_PER_UART1_CLKCTRL`, init proven `uart_irda_cir` |
| **RX Polling 1 ms** | ✅ | `vUART1RxTask` drain FIFO 64-byte tiap 1 ms (tidak pernah overflow @115200) |
| **Decoder Delta-2A / LDS08RR** | ✅ | `lidar_lds08rr.c` — sinkronisasi, validasi header, checksum 16-bit BE |
| **Scan accumulator** | ✅ | `lds_scan.c` — ~16 paket/rotasi, hingga 448 titik, snapshot stats decoder |
| **Scan queue** | ✅ | 2 slot, `xQueueSend` non-blocking (scan dibuang bila display lambat) |
| **SPI0 + ILI9341 driver** | ✅ | `hal_bspInit.c` `SPI0Configure()` + `spi_lcd.c` hooks + `ili9341.c` primitives (16 MHz, Mode 0) |
| **TFT radar renderer** | ✅ | `lcd_display.c` — ring 0.5/1/1.5/2 m, point-cloud, HUD sidebar, incremental redraw |
| **Dua task terpisah** | ✅ | Reader (prio 2) tidak pernah print panjang; Display (prio 1) satu-satunya consumer SPI0 (tanpa mutex) |
| **Konsol UART0** | ✅ | 115200 8N1: banner boot + log status (`[LCD] ILI9341 radar view ready`) |
| **FreeRTOS** | ✅ | 1 ms tick via DMTimer2, preemptive, `configCHECK_FOR_STACK_OVERFLOW` aktif |

> **Pelajaran penting (dari debugging):** task reader **tidak boleh mencetak baris panjang**. `ConsoleUtilsPrintf` bisa block saat TX FIFO UART0 penuh, dan task reader yang block membuat RX FIFO UART1 overflow → stream LiDAR korup. Print panjang hanya boleh dari task display.

---

## Protokol Delta-2A / LDS08RR

Stream dari LDS08RR (UART, semua field multi-byte **big-endian**):

| Byte | Field | Nilai |
|------|-------|-------|
| `[0]` | start_byte | `0xAA` |
| `[1-2]` | packet_length | total byte − 2 (tanpa checksum), biasanya `0x0055` (85) |
| `[3]` | protocol_version | `0x01` |
| `[4]` | packet_type | `0x61` |
| `[5]` | data_type | `0xAD` (RPM + measurement) / `0xAE` (RPM only) |
| `[6-7]` | data_length | n_samples = `(data_length − 5) / 3`, biasanya 77 |
| `[8]` | scan_freq_x20 | frekuensi = nilai × 0.05 Hz |
| `[9-10]` | offset_angle_x100 | bertanda, 0.01° |
| `[11-12]` | start_angle_x100 | 0.01°; **0 = awal scan baru** |
| `[13+]` | samples | 3 byte per sampel: `[quality, dist_hi, dist_lo]` |
| `[last-2]` | checksum | jumlah 16-bit semua byte sebelum field ini, big-endian |

- Jarak sampel: `distance_mm = (dist_hi << 8 | dist_lo) × 0.25`
- Sudut sampel diinterpolasi: `start_angle + i × (36000 / (16 × n))`
- **Satu scan = ~16 paket**; paket dengan `start_angle == 0` menandai paket pertama rotasi berikutnya sekaligus menutup scan saat ini.
- Checksum diverifikasi sebagai `sum(bytes sebelum checksum)` — akumulator internal sudah menyertakan 2 byte checksum, jadi dikurangi kembali sebelum dibandingkan.

---

## Radar View Layout

Layar ILI9341 320×240 landscape dibagi dua area (keduanya full-height, flush):

```
┌──────────────┬───────────────────────────────┐
│  sidebar     │        radar canvas           │
│  (x 0..99)   │      (x 100..319)             │
│              │   center (210,120)            │
│  SCANNER     │       · ring 0.5m (r=30)      │
│  LDS08RR     │      ·  ring 1.0m (r=60)      │
│ ──────────── │     ·   ring 1.5m (r=90)      │
│  MOTOR       │    ·    ring 2.0m (r=120)     │
│  x RPM       │   label "0.5m".."2m" diagonal │
│ ──────────── │   crosshair axes              │
│  TELEMETRY   │   titik 2×2 (cyan/magenta)    │
│  MIN/ANG/PTS │   point >2m → overflow,       │
│ ──────────── │   dipotong kotak radar        │
│  RANGE: x.xm │                               │
│  SCAN:360    │                               │
│  SCALE:      │                               │
│  [2m] [4m]   │                               │
└──────────────┴───────────────────────────────┘
```

- **Skala penuh 2 m** (`MAP_MAX_QMM = 8000` quarter-mm), ring luar `MAP_R_PX = 120` px mengisi tinggi layar.
- Ring digambar dengan **midpoint + box-clip** (`lcdDrawRing`) sehingga arc berhenti di tepi canvas — tidak menonjol keluar.
- **Titik > 2 m tidak di-clamp** — overflow ring luar dan hanya dipotong kotak radar `(y>=0 && y<240 && x>=100 && x<320)`.
- **Warna**: chrome silver (`COLOR_GRID`), titik cyan (`COLOR_POINT`), titik < 30 cm magenta (`COLOR_ALERT`), nilai live putih (`COLOR_TEXT`), `[2m]` aktif hijau (`COLOR_OK`).
- **Incremental redraw**: posisi dot lama disimpan untuk dihapus; pixel ring/axis/label yang tertimpa dot diperbaiki lokal (`lcdRestoreChromeAt`) — grid tidak pernah terkelupas, traffic SPI minimal, tidak flicker.
- **Chrome statis** (frame, ring, axis, label, headline SCANNER/LDS08RR font 2×) digambar sekali di `lcdDrawBackground()`.
- HUD sidebar per scan: RPM motor, `MIN:x.xxm`, `ANG:x.xd` (bearing nearest return), `PTS:n`, `RANGE:x.xm`.

---

## Project Structure

```
FreeRTOS_AM335x_SPI_ILI9341_LDS08RR/
├── CMakeLists.txt                        # Root build configuration
├── ProjectIncludes.cmake                 # Include paths + library definitions
├── bbb.lds                               # Linker script (DDR @ 0x80000000)
├── AM335xFreeRTOS_cmake_makefile_args.py # Auto-generate CMake includes
├── gcc-a.toolchain.ccs12.cmake           # Toolchain GCC ARM 7.3.1
├── AGENT.md                              # Panduan agent untuk project ini
├── README.md                             # Dokumen ini
│
├── src/application/                      # Lapisan aplikasi
│   ├── main.c                            # Entry: init + buat 2 task + scheduler
│   ├── uart1_driver.c / .h               # Driver UART1: pinmux, clk, 115200 8N1, baca byte
│   ├── lidar_lds08rr.c / .h              # Decoder protokol Delta-2A / LDS08RR
│   ├── lds_scan.c / .h                   # Akumulasi scan + queue
│   ├── uart1_reader.c / .h               # Task RX (polling UART1 → decoder → scan)
│   ├── spi_lcd.c / .h                    # SPI0 TX hooks + GPIO DC/RST hooks
│   ├── ili9341.c / .h                    # Driver ILI9341 + primitives + DrawStringScaled
│   ├── fonts.h                           # Font 5×7 ASCII (Font5x7)
│   ├── lcd_display.c / .h                # Radar view TFT + task display (vLCDDisplayTask)
│   ├── app_utils.c                       # FreeRTOS hooks (idle silent, malloc fail, stack overflow)
│   └── CMakeLists.txt                    # Daftar source src_application
│
├── src/portable/                         # Port AM3352 + hal_bspInit.c (SPI0/GPIO init)
├── src/inc/                              # FreeRTOSConfig.h, app_utils.h
│
├── lib/third_party/
│   ├── ti/                               # TI StarterWare (uart_irda_cir, mcspi, gpio_v2, timers, AINTC)
│   ├── amazon/                           # Amazon FreeRTOS v10.2.0
│   └── c_source_tools/                   # CMake utility
│
├── docs/                                 # vision / requirements / architecture / build / flash
└── build/                                # Output CMake
    ├── freeRTOSBBB.elf                   # ELF executable
    └── app.freeRTOSBBB.bin               # Raw binary (objcopy)
```

---

## Toolchain

| Komponen | Versi | Catatan |
|----------|-------|---------|
| Compiler | GCC ARM 7.3.1 | `gcc-arm-none-eabi-7-2018-q2-update` di `C:/ti/...` |
| Build | CMake 3.11+ + Ninja | wajib `-j1` saat build |
| Flash/Debug | J-Link GDB Server V8.44 | port 2331, device AM3352, JTAG 12000 kHz |
| RTOS | FreeRTOS v10.2.0 (Amazon fork) | tick 1 ms, preemptive |
| Peripheral Lib | TI AM335x StarterWare | UART = `uart_irda_cir.h`, SPI = `mcspi.h`, GPIO = `gpio_v2.h` |
| Target | ARM Cortex-A8 | `-mcpu=cortex-a8 -march=armv7-a -mfloat-abi=hard -mfpu=neon` |

---

## Build Instructions

> Langkah lengkap: [`docs/HOW_TO_BUILD.md`](docs/HOW_TO_BUILD.md)

```powershell
# 1. Clone dependencies (lihat HOW_TO_BUILD.md, hanya sekali)
# 2. Generate CMake args
python AM335xFreeRTOS_cmake_makefile_args.py
# 3. Clean build
Remove-Item -Recurse -Force build
# 4. Configure + build (WAJIB -j1, PowerShell pakai $PWD)
cmake -S . -B build -G Ninja "-DCMAKE_TOOLCHAIN_FILE=$PWD/gcc-a.toolchain.ccs12.cmake" -DCMAKE_C_COMPILER_WORKS=1
cmake --build build -- -j1
```

Output: `build/freeRTOSBBB.elf` + `build/app.freeRTOSBBB.bin`. Langkah `tiimage.exe` (SD card image) akan gagal — **aman diabaikan**, ELF/BIN tetap valid untuk JTAG.

---

## Flash Instructions

> Langkah lengkap: [`docs/HOW_TO_FLASH.md`](docs/HOW_TO_FLASH.md)

```powershell
# Terminal 1 — J-Link GDB Server (HARUS baru setiap sesi)
Start-Process -FilePath "C:\Program Files\SEGGER\JLink_V844\JLinkGDBServer.exe" `
  -ArgumentList "-if JTAG -device AM3352 -endian little -speed 12000 -port 2331"

# Terminal 2 — Flash one-shot (dari ROOT project, bukan dari build/)
C:\ti\gcc-arm-none-eabi-7-2018-q2-update\bin\arm-none-eabi-gdb.exe build/freeRTOSBBB.elf `
  -batch -ex "target remote localhost:2331" -ex "load" -ex "monitor go 0x80000100"
```

> ⚠️ Jangan `monitor reset` sebelum `load` (merusak state DDR). Selalu mulai JLinkGDBServer baru.

---

## Contoh Output Serial

Console (UART0 @ 115200 8N1, COM4):

```
TIMER2_CLKSEL=0x00000001
First tick!
UART1 RX ready @115200 8N1
[UART1Rx] LDS08RR decoder ready
UART1Rx task created (prio=2)
LCD display task created (prio=1)
Scheduler started
[LCD] ILI9341 radar view ready
```

Output utama (radar) tampil di **layar ILI9341**: frame sidebar + radar canvas, ring 0.5/1/1.5/2 m silver, titik point-cloud cyan (magenta bila < 30 cm), HUD sidebar ter-update ~5 Hz. UART0 tidak lagi mencetak peta ASCII.

---

## Known Issues

| Issue | Dampak | Solusi |
|-------|--------|--------|
| `monitor reset` sebelum `load` | Merusak state DDR → crash | Hanya `load` + `go`; jangan pernah `reset` |
| Sesi J-Link tidak bisa dipakai ulang | State mismatch target | Selalu mulai JLinkGDBServer baru per flash |
| `tiimage.exe` tidak ada | SD card image gagal dibuat | Abaikan — ELF & BIN valid untuk JTAG |
| `%lu`/`%ld`/`%f` tidak didukung `ConsoleUtilsPrintf` | Format salah / argumen rusak | Pakai `%u` + cast, atau integer math + `SIN_TAB` |
| Build paralel | Konflik rename file `.a` | Selalu `-j1` |
| Layar TFT gelap / tidak ada radar | Wiring SPI0/DC/RST atau power | Cek P9_22/P9_18/P9_17, DC P8_26, RST P8_19, ground bersama |
| `bad_chk` naik | Reader pernah print panjang / FIFO overflow / kabel | Cek kabel TX→D16, ground, 115200 8N1; reader jangan print |

---

## License

Berisi kode dari:
- **FreeRTOS** — MIT
- **TI StarterWare** — BSD-style
- **Amazon FreeRTOS** — Apache 2.0
- Decoder Delta-2A di-port dari [kaiaai/LDS](https://github.com/kaiaai/LDS)
