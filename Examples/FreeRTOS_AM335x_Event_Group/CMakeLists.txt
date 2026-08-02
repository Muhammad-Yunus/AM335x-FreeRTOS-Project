cmake_minimum_required (VERSION 3.11)

# CMP0013: Ignore implicit dependencies (install() does not re-run cmake)
# CMP0014: Ignore wildcard in FILE(GLOB/WILDCARD)
cmake_policy(SET CMP0013 NEW)
cmake_policy(SET CMP0014 NEW)

enable_language(C ASM)
project(freeRTOSBBB C)
set(BUILD_EXT "elf")

# # GLOB_RECURSE(${SRC} src/app/*.c src/bsp/*.c)
# set(SRC "SOURCE/imageLoad.S")
# set(SRC "SOURCE/main.c")
#GLOB_RECURSE(${HEADER} )

add_executable(${PROJECT_NAME}.elf ${SRC} src/portable/syscalls_minimal.c)
set (CMAKE_VERBOSE_MAKEFILE ON)
# The version number.
set (freeRTOSBBB_VERSION_MAJOR 1)
set (freeRTOSBBB_VERSION_MINOR 0)
#Workaround for Removing '[rollbar] Unable to automatically determine compiler'
set(CMAKE_CXX_COMPILER ${CMAKE_CXX_COMPILER} CACHE FILEPATH "Compiler")
set(CMAKE_C_COMPILER ${CMAKE_C_COMPILER} CACHE FILEPATH "Compiler")

set(gcc 1)
set(TARGET "armv7a")
set(DEVICE "am335x")
set(EVM "beaglebone")
set(COMPILER "gcc")
# Below are Handled in Arguments now XXX_makefile_args.py
# set(BOOT "MMCSD")
# set(CONSOLE "UARTCONSOLE")

include(ProjectIncludes.cmake)
#Continue
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mcpu=cortex-a8  -march=armv7-a -marm -mfloat-abi=hard -mfpu=neon")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mlong-calls -fdata-sections -funsigned-char -ffunction-sections -Wall")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Dgcc -D${DEVICE} -D${EVM} -DSUPPORT_UNALIGNED -DMMCSD -DUARTCONSOLE")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g3 -gdwarf-3 -gstrict-dwarf -Wall")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-value")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --specs=\"nosys.specs\"")

set(CMAKE_ASM_FLAGS ${CMAKE_C_FLAGS})


# Prevent Windows PE linker flags and CRT libs on cross-compile
set(CMAKE_STATIC_LIBRARY_LINK_C_FLAGS "")
set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
set(CMAKE_IMPORT_LIBRARY_SUFFIX "")
set(CMAKE_C_IMPLICIT_LINK_LIBRARIES "")
set(CMAKE_C_IMPLICIT_LINK_DIRECTORIES "")
set(CMAKE_CXX_IMPLICIT_LINK_LIBRARIES "")
set(CMAKE_SYSTEM_GENERATED_LIBRARIES "")

# Override the C executable linker command template to strip Windows PE flags
# This replaces the Ninja/Windows default link command that adds --out-implib and --major-image-version
set(CMAKE_C_LINK_EXECUTABLE "<CMAKE_C_COMPILER> <FLAGS> <CMAKE_C_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")

set(CMAKE_EXE_LINKER_FLAGS "")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -e Entry -Wl,-T,${PROJECT_SOURCE_DIR}/bbb.lds -Wl,-u,__aeabi_uidiv -Wl,-u,__aeabi_idiv")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Map,\"${PROJECT_NAME}.map\"")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--print-output-format")
# No nostdlib — let newlib supply libg which works with our syscall hooks
# Just link libgcc for __aeabi_uidiv etc intrinsics
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lgcc")

message(STATUS "By Abhinav Tripathi with alot of help from Saket Gupta")

# For POST BUILD SETUIP
if ( APPLE )
    message(STATUS "Apple")
    set( ENV_LIB_PATHS ENV DYLD_LIBRARY_PATH )
elseif ( UNIX )
    message(STATUS "JUNIX")
    set(TOOLS_TI "${CMAKE_CURRENT_BINARY_DIR}/../tools/ti/tiimage")
elseif ( WIN32 )    
    message(STATUS "WINDOZE 32")
    set(TOOLS_TI "${CMAKE_CURRENT_SOURCE_DIR}/lib/third_party/ti/tools/ti_image/tiimage.exe")
else ()    
    message(STATUS "OTHERS")
    set( ENV_LIB_PATHS "" )
    set(TOOLS_TI "${CMAKE_CURRENT_SOURCE_DIR}/lib/third_party/ti/tools/ti_image/tiimage.exe")
endif()
#   <executable file name> <load address> <boot mode> <input image path/name>   <output image path/name>
set (ELF_INPUT ${PROJECT_NAME}.${BUILD_EXT})
set(ELF_TO_APP_LOAD_ADDRESS "0x80000000")
set(ELF_TO_APP_BOOT_MODE    "NONE")
# set(MAKE_APP_COMMAND_ARGS "  0x80000000      NONE       ./${PROJECT_NAME}.elf     ./app.${PROJECT_NAME}.elf.rename")
set(ELF_TO_APP_INPUT  "\"${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.elf\"")

set(ELF_TO_APP_OUTPUT "\"${CMAKE_CURRENT_BINARY_DIR}/app.${PROJECT_NAME}.bin\"")

set(ELF_TO_BIN_ARGS "-v -I elf32-littlearm -O binary -B arm -S ${ELF_TO_APP_INPUT} ${ELF_TO_APP_OUTPUT}")

set(SD_CARD_FILE_NAME  "\"${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.sdcard\"")

set(MAKE_APP_COMMAND_ARGS "${ELF_TO_APP_LOAD_ADDRESS} ${ELF_TO_APP_BOOT_MODE} ${ELF_TO_APP_OUTPUT} ${SD_CARD_FILE_NAME}" )

# Custom target for SD card image generation
add_custom_target(GenerateSDCard ALL DEPENDS ${PROJECT_NAME}.elf COMMENT "Generating SD Card image...")

set(BUILD_PATH "${PROJECT_SOURCE_DIR}/build")

add_custom_command(TARGET GenerateSDCard PRE_BUILD
    COMMAND arm-none-eabi-objcopy "-Ielf32-littlearm" "-Obinary" "-Barm" "-S"
            "${BUILD_PATH}/${PROJECT_NAME}.elf" "${BUILD_PATH}/app.${PROJECT_NAME}.bin"
    COMMENT "Converting ELF to BIN...")

add_custom_command(TARGET GenerateSDCard POST_BUILD
    COMMAND ${TOOLS_TI} 0x80000000 NONE "${BUILD_PATH}/app.${PROJECT_NAME}.bin" "${BUILD_PATH}/${PROJECT_NAME}.sdcard"
    COMMENT "Creating TI SDCARD image...")



# set(TARGET_ELF_COPY_DIR "D:\\CCS8_WorkSpace\\trt\\Debug")
# set(ELF_COPY_DBG_NAME  "${TARGET_ELF_COPY_DIR}\\trt.out")
# add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
#                    COMMAND cp ${ELF_TO_APP_INPUT} ${ELF_COPY_DBG_NAME}
#                    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
#                    COMMENT "\n
#                         Copy ELF for Debug in CCS .... invoking:
#                             cp ${ELF_TO_APP_INPUT} ${ELF_COPY_DBG_NAME}
#                             Input Filename     --> ${ELF_COPY_DBG_NAME}
#                             By Abhinav Tripathi ELF --> CCSDBG")  