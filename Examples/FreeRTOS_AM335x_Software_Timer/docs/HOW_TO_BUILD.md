# Build Instructions — AM3352 FreeRTOS GPIO LED Blink

> Versi proven & working. Ikuti langkah-langkah ini secara berurutan.

## Prerequisites

1. **Git** — dengan dukungan submodule
2. **CMake** 3.11+ (diuji hingga 4.x)
3. **Python** 2.x atau 3.x
4. **GNU ARM GCC** 7.3.1 (direkomendasikan: `gcc-arm-none-eabi-7-2018-q2-update`)
5. **Ninja** build system

## Langkah-langkah

### 1. Clone Third-Party Repositories

Build bergantung pada tiga repository yang harus di-clone ke `lib/third_party/`. Project ini **tidak** menggunakan Git submodules — clone setiap repo secara manual:

```powershell
New-Item -ItemType Directory -Force lib/third_party | Out-Null

git clone http://github.com/kryochronic/AM335X_StarterWare_02_00_01_01.git lib/third_party/ti

git clone https://github.com/aws/amazon-freertos.git lib/third_party/amazon

git clone http://github.com/kryochronic/c_source_tools lib/third_party/c_source_tools
```

Kemudian checkout Amazon FreeRTOS ke commit spesifik yang dipakai project ini, dan inisialisasi submodules internal:

```powershell
cd lib/third_party/amazon
git checkout 2bb3154718ecf0346e3236eef7039a27977d46d9
git submodule update --init --recursive
cd ../..
```

Direktori yang diharapkan:

| Directory | Contents |
|---|---|
| `lib/third_party/ti` | TI StarterWare — GPIO, UART, timers, AINTC drivers |
| `lib/third_party/amazon` | Amazon FreeRTOS v10.2.0 kernel |
| `lib/third_party/c_source_tools` | CMake utility library untuk C source/header handling |

### 2. Generate CMake Configuration

```powershell
python AM335xFreeRTOS_cmake_makefile_args.py
```

Script ini secara otomatis menghasilkan include paths dan referensi library untuk semua komponen TI dan FreeRTOS. Script juga melakukan post-processing untuk memperbaiki folder header-only dan menambahkan manual target sources untuk fungsi ARM CP15.

### 3. Configure Toolchain

> ⚠️ **Penting**: Project ini **wajib** memakai **GCC ARM 7.3.1** (`gcc-arm-none-eabi-7-2018-q2-update`) yang berlokasi di `C:/ti/gcc-arm-none-eabi-7-2018-q2-update`. Versi GCC ARM lain (contoh: GCC 13.x dari STM32CubeIDE di `C:/ST/...`) **tidak kompatibel** karena path dan prefix-nya beda — project memakai `arm-none-eabi-`, bukan `arm-eabi-`. Jika direktori `C:/ti/gcc-arm-none-eabi-7-2018-q2-update` belum ada, install dulu sebelum lanjut.

Dua file toolchain disediakan:

- **`gcc-a.toolchain.ccs12.cmake`** (recommended) — untuk TI CCS 12 bundled toolchain di `C:/ti/gcc-arm-none-eabi-7-2018-q2-update``
- **`gcc-a.toolchain.win10.sample.cmake`** — sample file menggunakan prefix `arm-eabi-` (sesuaikan path sesuai kebutuhan)

Jika menggunakan CCS12 toolchain, tidak perlu editing. Jika tidak, copy dan edit sample:

```powershell
Copy-Item gcc-a.toolchain.win10.sample.cmake gcc-a.toolchain.cmake
```

Edit `gcc-a.toolchain.cmake` agar mengarah ke path compiler GCC ARM kamu:

```cmake
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER "C:/ti/gcc-arm-none-eabi-7-2018-q2-update/bin/arm-none-eabi-gcc.exe")
```

### 4. Clean Build (Recommended)

Selalu hapus direktori build sebelum mengkonfigurasi ulang untuk menghindari state yang stale:

```powershell
Remove-Item -Recurse -Force build
```

### 5. Configure and Build with Ninja

> **Penting:** Gunakan `-j1` (single-threaded build) saat linking. Project ini memiliki konflik parallel build di mana multiple target mencoba rename file `.a` yang sama secara bersamaan. Sequential build menghindari masalah ini.

> **PowerShell note:** Jika menggunakan PowerShell, ganti `%cd%` dengan `$PWD` pada baris `CMAKE_TOOLCHAIN_FILE`. Contoh: `"-DCMAKE_TOOLCHAIN_FILE=$PWD/gcc-a.toolchain.ccs12.cmake"`. `%cd%` adalah syntax **cmd.exe**, tidak didukung di PowerShell.

```powershell
cmake -S . -B build -G Ninja ^
  "-DCMAKE_TOOLCHAIN_FILE=%cd%/gcc-a.toolchain.ccs12.cmake" ^
  -DCMAKE_C_COMPILER_WORKS=1

cmake --build build -- -j1
```

> **Note:** `-DCMAKE_C_COMPILER_WORKS=1` skips the compiler ABI detection test which fails for bare-metal cross-compilers. This is safe for embedded targets.
>
> **Note:** You may see a CMake warning `Manually-specified variables were not used by the project: CMAKE_TOOLCHAIN_FILE`. This is harmless and can be ignored — the toolchain file is applied correctly despite the warning.

> **Note:** The final build step attempts to generate both a BIN file and an SDCARD image via `tiimage.exe`. If `tiimage.exe` is missing (not included in this repo), the SDCARD image step will fail with exit code 1 but **the ELF and BIN files are already built and valid**. Check for `build/freeRTOSBBB.elf` — if it exists, the build succeeded for JTAG flashing. To regenerate the BIN manually if needed:
> ```powershell
> arm-none-eabi-objcopy -O binary -B arm build/freeRTOSBBB.elf build/app.freeRTOSBBB.bin
> ```

### 6. Verify Output

After a successful build, you should have:

```
build/freeRTOSBBB.elf       — Linked ELF executable (~8.1 MB with debug symbols)
build/app.freeRTOSBBB.bin   — Raw binary image (objcopy)
```

> **Note:** The SD card image (`freeRTOSBBB.sdcard`) is NOT generated automatically. The `tiimage.exe` tool required for TI SDCARD image conversion is not included in the repository (only source `.c` is present).

## Known Build Issues

| Problem | Cause | Solution |
|---------|-------|----------|
| `tiimage.exe` missing | Tool tidak termasuk dalam repo | Abaikan — ELF dan BIN tetap valid untuk JTAG flash |
| CMake warning `CMAKE_TOOLCHAIN_FILE` | Tidak digunakan oleh project | Aman, diabaikan |
| Parallel build gagal | Konflik rename `.a` file | Gunakan `-j1` |
| Compiler ABI test gagal | Cross-compiler bare-metal | Gunakan `-DCMAKE_C_COMPILER_WORKS=1` |
