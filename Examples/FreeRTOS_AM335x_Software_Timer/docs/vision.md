# Vision — FreeRTOS AM3352 Software Timer

## Problem

Project base FreeRTOS AM3352 memerlukan demonstrasi penggunaan **Software Timer** bawaan FreeRTOS untuk memicu aksi terencana (one-shot dan auto-reload) secara asynchronous melalui timer service task, tanpa menggunakan hardware interrupt eksternal ataupun blinking LED fisik.

## Goal

Membuat project FreeRTOS AM3352 yang menampilkan:
- Inisialisasi FreeRTOS Timer Service Task (`xTimerCreate`)
- Demonstrasi **One-shot Timer**: Berjalan sekali setelah delay tertentu, memicu callback, lalu berhenti.
- Demonstrasi **Auto-reload Timer**: Berjalan berulang secara periodik setelah delay tertentu, memicu callback berulang kali.
- Penggunaan API `xTimerStart` untuk memulai timer.
- Output serial UART 115200 menampilkan siklus hidup timer dan trigger callback (print saat timer dibuat, dijalankan, callback dipicu, dll).
- Tidak ada blinking LED fisik ataupun interupsi GPIO eksternal.

## Verified UART Output

Output yang berhasil di-verify via hardware (COM4, 115200 8N1):

```
TIMER2_CLKSEL=0x00000001
First tick!
Scheduler started
Timer Monitor Task created!
One-shot Timer created!
Auto-reload Timer created!
Starting both timers...
Auto-reload Timer Callback triggered! (Count: 1)
Auto-reload Timer Callback triggered! (Count: 2)
One-shot Timer Callback triggered!
Auto-reload Timer Callback triggered! (Count: 3)
...
```

## Target Hardware

- **SoC**: AM3352 (Cortex-A8 @ 600 MHz, little-endian, ARMv7-A)
- **Board**: Antminer L3+ (PCB & form-factor = BeagleBone, tanpa PRU)
- **Debugger**: J-Link + JLinkGDBServer
- **Boot**: Langsung ke DDR via JTAG (no bootloader, no SD card)
- **Console**: UART0 @ 115200 8N1

## Reference Projects

| Project | Path | Peran |
|---------|------|-------|
| `FreeRTOS_AM335x_GPIO_INTERRUPT` | `../FreeRTOS_AM335x_GPIO_INTERRUPT/` | Base project — infrastructure 100% working (CMake, toolchain, linker, MMU, AINTC, UART, FreeRTOS port) |

## Key Insight

FreeRTOS Software Timer dijalankan di bawah context **Timer Service Task** (daemon task). Konfigurasi timer dikendalikan oleh:
- `configUSE_TIMERS` harus bernilai `1` di `FreeRTOSConfig.h`.
- `configTIMER_TASK_PRIORITY` menentukan prioritas daemon task (di project ini: 3).
- `configTIMER_QUEUE_LENGTH` menentukan kapasitas command queue untuk berkomunikasi dengan daemon task (di project ini: 10).
- Callback dijalankan di bawah context daemon task, sehingga callback function **tidak boleh** memanggil API yang memblokir (blocking).
