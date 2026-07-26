param(
    [string]$BuildDir,
    [string]$ElfName,
    [string]$BinName,
    [string]$SdcardName
)

# Step 1: Convert ELF to BIN
$OBJCOPY = "C:/ti/gcc-arm-none-eabi-7-2018-q2-update/bin/arm-none-eabi-objcopy.exe"
& $OBJCOPY -I elf32-littlearm -O binary -B arm -S "`"$BuildDir/$ElfName`"" "`"$BuildDir/$BinName`"" 2>&1

# Step 2: Create TI SDCARD image
$TIIMAGE = "C:/D/DOCUMENT_BCK/GitHub/AM335X-FreeRTOS-lwip/lib/third_party/ti/tools/ti_image/tiimage.exe"
& $TIIMAGE "0x80000000 NONE" "`"$BuildDir/$BinName`"" "`"$BuildDir/$SdcardName`"" 2>&1
