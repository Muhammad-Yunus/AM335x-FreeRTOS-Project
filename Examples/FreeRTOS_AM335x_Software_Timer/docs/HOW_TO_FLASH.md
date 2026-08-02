# Flash Instructions — AM3352 FreeRTOS GPIO LED Blink

> Proven & working via JTAG + J-Link GDB Server.

## Prerequisites

- **J-Link GDB Server V8.44** — terinstall di `C:\Program Files\SEGGER\JLink_V844\JLinkGDBServer.exe`
- **GNU ARM GCC** 7.3.1 toolchain — untuk `arm-none-eabi-gdb.exe`
- **JTAG cable** terhubung ke AM3352 JTAG header
- **Power ON** — board harus hidup saat flashing

## Step-by-Step

### 1. Start J-Link GDB Server (Terminal 1)

GUI application (recommended):

```
"C:\Program Files\SEGGER\JLink_V844\JLinkGDBServer.exe"
```

Configure in the GUI:

| Setting | Value |
|---------|-------|
| Device | `AM3352` |
| Target interface | `JTAG` |
| Speed | `12000 kHz` |
| Port | `2331` |

Atau command line (optional):

```powershell
Start-Process -FilePath "C:\Program Files\SEGGER\JLink_V844\JLinkGDBServer.exe" `
  -ArgumentList "-if JTAG -device AM3352 -endian little -speed 12000 -port 2331"
```

Verifikasi GDB Server siap sebelum lanjut:

```powershell
Get-NetTCPConnection -LocalPort 2331 | Select-Object State
```

Harusnya muncul `State = Listen`. Tunggu pesan `"Connected to target"` di console GDB Server **sebelum** menjalankan flash command dari Terminal 2. Jika port 2331 sudah `Listen` tapi belum ada "Connected to target", cek kembali koneksi JTAG.

### 2. Flash via GDB (Terminal 2)

**Option A: One-shot flash** (GDB keluar setelah load):

```powershell
C:\ti\gcc-arm-none-eabi-7-2018-q2-update\bin\arm-none-eabi-gdb.exe build/freeRTOSBBB.elf `
  -batch `
  -ex "target remote localhost:2331" `
  -ex "load" `
  -ex "monitor go 0x80000100"
```

> **Note:** `-batch` wajib digunakan untuk mencegah GDB prompting "Quit anyway? (y or n)" saat session remote berakhir. Menggunakan `-ex "quit"` tanpa `-batch` akan hang di terminal.

**Option B: Interactive flash** (GDB tetap attached untuk debugging):

```powershell
C:\ti\gcc-arm-none-eabi-7-2018-q2-update\bin\arm-none-eabi-gdb.exe build/freeRTOSBBB.elf
(gdb) target remote localhost:2331
(gdb) load
(gdb) continue
```

Tekan **Ctrl+C** untuk break kembali ke GDB untuk debugging.

### 3. Verify Serial Output

Serial output muncul di UART **115200 8N1**. Sambungkan USB TTL ke board dan buka serial monitor di port COM sesuai setup hardware kamu.

Expected output:

```
TIMER2_CLKSEL=0x00000001
First tick!
```

## Important Warnings

| Warning | Reason |
|---------|--------|
| **Jangan** `monitor reset` sebelum `load` | `reset` sebelum `load` akan merusak DDR state — program akan crash |
| **Selalu mulai fresh** JLinkGDBServer untuk setiap flash session | Reusing instance lama bisa menyebabkan target state mismatch |
| Gunakan `localhost:2331` | JLink GDB Server berjalan di PC development, bukan di target network IP |

## Flash Output Artifacts

Setelah flash sukses, program berjalan langsung dari DDR. Tidak perlu SD Card atau external bootloader saat menggunakan JTAG.
