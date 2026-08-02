# Requirements — FreeRTOS AM3352 Software Timer

## Hardware Requirements

| Item | Specification |
|------|---------------|
| SoC | AM3352 Cortex-A8 @ 600 MHz (ARMv7-A, little-endian) |
| Board | Antminer L3+ control board (BBB formfactor, tanpa PRU) |
| UART Console | UART0 @ 115200 8N1, via USB TTL dedicated → **COM4** |
| Debugger | J-Link GDB Server (port 2331) |

### Hardware Setup (sudah terpasang)

- J-Link terhubung ke target board (programmer/debugger)
- USB TTL dedicated terhubung ke AM3352 UART → **COM4**
- Semua power menyala

## Functional Requirements

### FR-1: Timer Service Task Initialization
- Konfigurasi `configUSE_TIMERS` harus diset ke `1` di `FreeRTOSConfig.h`.
- Timer service task (Daemon task) harus diinisialisasi otomatis saat scheduler dimulai.

### FR-2: One-shot Timer Demonstration
- Membuat One-shot timer menggunakan API `xTimerCreate()`.
- Durasi timer: 5000 ms (5 detik).
- Menjalankan timer menggunakan `xTimerStart()`.
- Callback One-shot timer harus mencetak log ke UART ketika dipicu, dan tidak berjalan berulang.

### FR-3: Auto-reload Timer Demonstration
- Membuat Auto-reload timer menggunakan API `xTimerCreate()`.
- Periode timer: 2000 ms (2 detik).
- Menjalankan timer menggunakan `xTimerStart()`.
- Callback Auto-reload timer harus mencetak log ke UART secara periodik setiap kali dipicu.

### FR-4: Task Execution & UART Debug Logging
- Membuat task inisialisasi / monitoring untuk mengelola siklus hidup timer.
- UART0 harus menampilkan log debug untuk setiap fase penting:
  - Saat task monitoring dibuat.
  - Saat One-shot & Auto-reload timer berhasil dibuat (`xTimerCreate`).
  - Saat timer mulai dijalankan (`xTimerStart`).
  - Saat callback One-shot timer dieksekusi.
  - Saat callback Auto-reload timer dieksekusi (disertai counter hitungan pemanggilan).
- Tidak boleh ada blinking LED fisik ataupun interupsi hardware eksternal.

### FR-5: Build & Flash
- Build sistem: CMake + Ninja
- Toolchain: GCC ARM 7.3.1 (`gcc-arm-none-eabi-7-2018-q2-update`)
- Flash via JTAG GDB Server
- Output: `build/freeRTOSBBB.elf` dan `build/app.freeRTOSBBB.bin`

## Non-Functional Requirements

| ID | Requirement |
|----|-------------|
| NFR-1 | Mengikuti struktur dan gaya dokumentasi README dari project reference |
| NFR-2 | Infrastructure (CMake, linker, MMU, AINTC, UART, timer tick) **tidak diubah** — hanya logic task dan timer |
| NFR-3 | Callback function timer tidak boleh menggunakan fungsi blocking (misal `vTaskDelay` atau logic tunggu yang lama) |
| NFR-4 | Seluruh log output UART harus dikirim secara real-time via `ConsoleUtilsPrintf` atau fungsi sejenis |

## Files to Create / Modify

| File | Action | Keterangan |
|------|--------|------------|
| `src/application/main.c` | Modify | Inisialisasi UART, pembuatan monitoring task, memulai scheduler |
| `src/application/TaskTimerDemo.c` | Create/Modify | File implementasi setup timer, callback function, dan monitoring task |
| `src/inc/FreeRTOSConfig.h` | Modify | Memastikan `configUSE_TIMERS` bernilai 1 beserta parameter timer daemon lainnya |
| `docs/vision.md` | Create | Project vision |
| `docs/requirements.md` | Create | Dokumen ini |
| `docs/architecture.md` | Create | Architectural design |

## Dependencies

```
lib/third_party/ti/          — TI StarterWare 02.00.01.01
lib/third_party/amazon/      — Amazon FreeRTOS v10.2.0
lib/third_party/c_source_tools/ — CMake utility
```
