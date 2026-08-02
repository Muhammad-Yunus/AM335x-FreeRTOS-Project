set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_SYSTEM_PROCESSOR arm)

# CCS 12 comes with gcc-arm-none-eabi-7-2018-q2-update
set(TOOLCHAIN_DIR "C:/ti/gcc-arm-none-eabi-7-2018-q2-update" CACHE FILEPATH "Toolchain Path")

message(STATUS "toolchain path_variable: ${TOOLCHAIN_DIR}")

set(CMAKE_C_COMPILER "${TOOLCHAIN_DIR}/bin/arm-none-eabi-gcc.exe" CACHE FILEPATH "ARM GCC compiler")
set(CMAKE_ASM_COMPILER "${TOOLCHAIN_DIR}/bin/arm-none-eabi-gcc.exe" CACHE FILEPATH "ARM ASM compiler")
find_program(OBJCOPY NAMES arm-none-eabi-objcopy
  PATHS ${TOOLCHAIN_DIR}/bin
  NO_DEFAULT_PATH
)

find_program(SIZE_TOOL NAMES arm-none-eabi-size
  PATHS ${TOOLCHAIN_DIR}/bin
  NO_DEFAULT_PATH
)

message(STATUS "CMAKE_C_COMPILER: ${CMAKE_C_COMPILER}")

execute_process(
  COMMAND ${CMAKE_C_COMPILER} -print-file-name=libc.a
  OUTPUT_VARIABLE _LIBC_PATH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
get_filename_component(_INSTALL_PREFIX "${_LIBC_PATH}" DIRECTORY)
get_filename_component(_INSTALL_PREFIX "${_INSTALL_PREFIX}/.." REALPATH)
set(CMAKE_FIND_ROOT_PATH "${_INSTALL_PREFIX}" CACHE INTERNAL "Find root path")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Set tools via CMAKE variables
set(CMAKE_OBJCOPY ${OBJCOPY} CACHE INTERNAL "objcopy tool")

# Disable platform file for bare-metal cross-compile
# The Windows-GNU platform file adds PE-specific linker flags that arm-none-eabi-ld doesn't understand
# Override with empty link flags
set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
set(CMAKE_INSTALL_PREFIX "${_INSTALL_PREFIX}" CACHE INTERNAL "")

# Disable Windows PE-specific linker flags that arm-none-eabi-ld doesn't support
set(CMAKE_GNULD_IMAGE_VERSION "" CACHE INTERNAL "")
set(CMAKE_SYSTEM_GENERATED_LIBRARIES "" CACHE STRING "")

# Override standard libraries (kernel32, user32 etc are injected by Windows-GNU.cmake)
# This must be set BEFORE project() so platform files pick it up
if(NOT DEFINED CMAKE_C_STANDARD_LIBRARIES_INIT)
    set(CMAKE_C_STANDARD_LIBRARIES_INIT "" CACHE STRING "")
endif()
if(NOT DEFINED CMAKE_CXX_STANDARD_LIBRARIES_INIT)
    set(CMAKE_CXX_STANDARD_LIBRARIES_INIT "" CACHE STRING "")
endif()

message(STATUS "Cross-compiling with CCS 12 gcc-arm-none-eabi toolchain")
