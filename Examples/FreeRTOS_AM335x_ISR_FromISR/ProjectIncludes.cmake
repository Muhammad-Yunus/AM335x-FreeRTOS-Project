#adding entries for FreeRTOS
	include_directories("${PROJECT_SOURCE_DIR}/lib/third_party/amazon/freertos_kernel/include")
	include_directories("${PROJECT_SOURCE_DIR}/src/portable/FreeRTOS/portable/GCC/ARM_CA8_amm335x")
#adding entries for ti_starterware
	include_directories("${PROJECT_SOURCE_DIR}/lib/third_party/ti/include")
	include_directories("${PROJECT_SOURCE_DIR}/lib/third_party/ti/include/armv7a")
	include_directories("${PROJECT_SOURCE_DIR}/lib/third_party/ti/include/armv7a/am335x")
	include_directories("${PROJECT_SOURCE_DIR}/lib/third_party/ti/include/hw")
#adding entries for application
	include_directories("${PROJECT_SOURCE_DIR}/src/inc")
	include_directories("${PROJECT_SOURCE_DIR}/src/portable/AM335X/inc")
	include_directories("${PROJECT_SOURCE_DIR}/src/inc/config_files")
	include_directories("${PROJECT_SOURCE_DIR}/src/portable/AM335X/inc")
#adding entries for lib_third_party_ti_drivers
	include_directories("${PROJECT_SOURCE_DIR}/lib/third_party/ti/drivers")
	add_subdirectory("${PROJECT_SOURCE_DIR}/lib/third_party/ti/drivers" "lib_third_party_ti_drivers_lib_third_party_ti_drivers")
#adding entries for lib_third_party_ti_utils
	include_directories("${PROJECT_SOURCE_DIR}/lib/third_party/ti/utils")
	add_subdirectory("${PROJECT_SOURCE_DIR}/lib/third_party/ti/utils" "lib_third_party_ti_utils_lib_third_party_ti_utils")
#adding entries for lib_third_party_ti_mmcsdlib
	include_directories("${PROJECT_SOURCE_DIR}/lib/third_party/ti/mmcsdlib")
	include_directories("${PROJECT_SOURCE_DIR}/lib/third_party/ti/mmcsdlib/include")
	add_subdirectory("${PROJECT_SOURCE_DIR}/lib/third_party/ti/mmcsdlib" "lib_third_party_ti_mmcsdlib_lib_third_party_ti_mmcsdlib")
#adding entries for lib_third_party_ti_platform_beaglebone
	include_directories("${PROJECT_SOURCE_DIR}/lib/third_party/ti/platform/beaglebone")
	add_subdirectory("${PROJECT_SOURCE_DIR}/lib/third_party/ti/platform/beaglebone" "lib_third_party_ti_platform_beaglebone_lib_third_party_ti_platform_beaglebone")
#adding entries for lib_third_party_ti_nandlib
	include_directories("${PROJECT_SOURCE_DIR}/lib/third_party/ti/nandlib")
	include_directories("${PROJECT_SOURCE_DIR}/lib/third_party/ti/nandlib/include")
	add_subdirectory("${PROJECT_SOURCE_DIR}/lib/third_party/ti/nandlib" "lib_third_party_ti_nandlib_lib_third_party_ti_nandlib")
#adding entries for lib_third_party_ti_system_config_armv7a
	include_directories("${PROJECT_SOURCE_DIR}/lib/third_party/ti/system_config/armv7a")
	include_directories("${PROJECT_SOURCE_DIR}/lib/third_party/ti/system_config/armv7a/am335x")
	include_directories("${PROJECT_SOURCE_DIR}/lib/third_party/ti/system_config/armv7a/am335x/gcc")
	include_directories("${PROJECT_SOURCE_DIR}/lib/third_party/ti/system_config/armv7a/gcc")
	add_subdirectory("${PROJECT_SOURCE_DIR}/lib/third_party/ti/system_config/armv7a" "lib_third_party_ti_system_config_armv7a_lib_third_party_ti_system_config_armv7a")
	add_subdirectory("${PROJECT_SOURCE_DIR}/lib/third_party/ti/system_config/armv7a/am335x" "lib_third_party_ti_system_config_armv7a_lib_third_party_ti_system_config_armv7a_am335x")
	add_subdirectory("${PROJECT_SOURCE_DIR}/lib/third_party/ti/system_config/armv7a/gcc" "lib_third_party_ti_system_config_armv7a_lib_third_party_ti_system_config_armv7a_gcc")
#adding entries for lib_third_party_amazon_freertos_kernel
	include_directories("${PROJECT_SOURCE_DIR}/lib/third_party/amazon/freertos_kernel")
	include_directories("${PROJECT_SOURCE_DIR}/lib/third_party/amazon/freertos_kernel/include")
	add_subdirectory("${PROJECT_SOURCE_DIR}/lib/third_party/amazon/freertos_kernel" "lib_third_party_amazon_freertos_kernel_lib_third_party_amazon_freertos_kernel")
#adding entries for src_portable_FreeRTOS_portable_GCC_ARM_CA8_amm335x
	include_directories("${PROJECT_SOURCE_DIR}/src/portable/FreeRTOS/portable/GCC/ARM_CA8_amm335x")
	add_subdirectory("${PROJECT_SOURCE_DIR}/src/portable/FreeRTOS/portable/GCC/ARM_CA8_amm335x" "src_portable_FreeRTOS_portable_GCC_ARM_CA8_amm335x_src_portable_FreeRTOS_portable_GCC_ARM_CA8_amm335x")
#adding entries for lib_third_party_amazon_freertos_kernel_portable_MemMang
	include_directories("${PROJECT_SOURCE_DIR}/lib/third_party/amazon/freertos_kernel/portable/MemMang")
	add_subdirectory("${PROJECT_SOURCE_DIR}/lib/third_party/amazon/freertos_kernel/portable/MemMang" "lib_third_party_amazon_freertos_kernel_portable_MemMang_lib_third_party_amazon_freertos_kernel_portable_MemMang")
#adding entries for src_portable_AM335X
	include_directories("${PROJECT_SOURCE_DIR}/src/portable/AM335X")
	include_directories("${PROJECT_SOURCE_DIR}/src/portable/AM335X/inc")
	add_subdirectory("${PROJECT_SOURCE_DIR}/src/portable/AM335X" "src_portable_AM335X_src_portable_AM335X")
#adding entries for src_application
	include_directories("${PROJECT_SOURCE_DIR}/src/application")
	add_subdirectory("${PROJECT_SOURCE_DIR}/src/application" "src_application_src_application")

# Sources added directly to ${{PROJECT_NAME}}.elf via target_sources()

# Manual target_sources for cp15.S and cpu.c (gcc/ folder has CMake binary dir conflicts)
target_sources(${PROJECT_NAME}.elf PRIVATE
	"${PROJECT_SOURCE_DIR}/lib/third_party/ti/system_config/armv7a/gcc/cp15.S"
	"${PROJECT_SOURCE_DIR}/lib/third_party/ti/system_config/armv7a/gcc/cpu.c"
)
