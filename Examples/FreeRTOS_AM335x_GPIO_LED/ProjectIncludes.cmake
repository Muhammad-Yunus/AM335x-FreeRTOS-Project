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
	add_library(lib_third_party_ti_drivers "")
	set_target_properties(lib_third_party_ti_drivers PROPERTIES LINKER_LANGUAGE C)
	target_compile_definitions(lib_third_party_ti_drivers 
		PRIVATE -DBOOT=MMCSD -DCONSOLE=UARTCONSOLE
	)
	subdirs("${PROJECT_SOURCE_DIR}/lib/third_party/ti/drivers")
#adding entries for lib_third_party_ti_utils
	include_directories("${PROJECT_SOURCE_DIR}/lib/third_party/ti/utils")
	add_library(lib_third_party_ti_utils "")
	set_target_properties(lib_third_party_ti_utils PROPERTIES LINKER_LANGUAGE C)
	target_compile_definitions(lib_third_party_ti_utils 
		PRIVATE -DBOOT=MMCSD -DCONSOLE=UARTCONSOLE
	)
	subdirs("${PROJECT_SOURCE_DIR}/lib/third_party/ti/utils")
#adding entries for lib_third_party_ti_mmcsdlib
	include_directories("${PROJECT_SOURCE_DIR}/lib/third_party/ti/mmcsdlib")
	include_directories("${PROJECT_SOURCE_DIR}/lib/third_party/ti/mmcsdlib/include")
	add_library(lib_third_party_ti_mmcsdlib "")
	set_target_properties(lib_third_party_ti_mmcsdlib PROPERTIES LINKER_LANGUAGE C)
	target_compile_definitions(lib_third_party_ti_mmcsdlib 
		PRIVATE -DBOOT=MMCSD -DCONSOLE=UARTCONSOLE
	)
	subdirs("${PROJECT_SOURCE_DIR}/lib/third_party/ti/mmcsdlib")
#adding entries for lib_third_party_ti_platform_beaglebone
	include_directories("${PROJECT_SOURCE_DIR}/lib/third_party/ti/platform/beaglebone")
	add_library(lib_third_party_ti_platform_beaglebone "")
	set_target_properties(lib_third_party_ti_platform_beaglebone PROPERTIES LINKER_LANGUAGE C)
	target_compile_definitions(lib_third_party_ti_platform_beaglebone 
		PRIVATE -DBOOT=MMCSD -DCONSOLE=UARTCONSOLE
	)
	subdirs("${PROJECT_SOURCE_DIR}/lib/third_party/ti/platform/beaglebone")
#adding entries for lib_third_party_ti_nandlib
	include_directories("${PROJECT_SOURCE_DIR}/lib/third_party/ti/nandlib")
	include_directories("${PROJECT_SOURCE_DIR}/lib/third_party/ti/nandlib/include")
	add_library(lib_third_party_ti_nandlib "")
	set_target_properties(lib_third_party_ti_nandlib PROPERTIES LINKER_LANGUAGE C)
	target_compile_definitions(lib_third_party_ti_nandlib 
		PRIVATE -DBOOT=MMCSD -DCONSOLE=UARTCONSOLE
	)
	subdirs("${PROJECT_SOURCE_DIR}/lib/third_party/ti/nandlib")
#adding entries for lib_third_party_ti_system_config_armv7a
	include_directories("${PROJECT_SOURCE_DIR}/lib/third_party/ti/system_config/armv7a")
	include_directories("${PROJECT_SOURCE_DIR}/lib/third_party/ti/system_config/armv7a/am335x")
	include_directories("${PROJECT_SOURCE_DIR}/lib/third_party/ti/system_config/armv7a/am335x/gcc")
	include_directories("${PROJECT_SOURCE_DIR}/lib/third_party/ti/system_config/armv7a/gcc")
	add_library(lib_third_party_ti_system_config_armv7a "")
	set_target_properties(lib_third_party_ti_system_config_armv7a PROPERTIES LINKER_LANGUAGE C)
	target_compile_definitions(lib_third_party_ti_system_config_armv7a 
		PRIVATE -DBOOT=MMCSD -DCONSOLE=UARTCONSOLE
	)
	subdirs("${PROJECT_SOURCE_DIR}/lib/third_party/ti/system_config/armv7a")
	subdirs("${PROJECT_SOURCE_DIR}/lib/third_party/ti/system_config/armv7a/am335x")
#adding entries for lib_third_party_amazon_freertos_kernel
	include_directories("${PROJECT_SOURCE_DIR}/lib/third_party/amazon/freertos_kernel")
	include_directories("${PROJECT_SOURCE_DIR}/lib/third_party/amazon/freertos_kernel/include")
	add_library(lib_third_party_amazon_freertos_kernel "")
	set_target_properties(lib_third_party_amazon_freertos_kernel PROPERTIES LINKER_LANGUAGE C)
	subdirs("${PROJECT_SOURCE_DIR}/lib/third_party/amazon/freertos_kernel")
#adding entries for src_portable_FreeRTOS_portable_GCC_ARM_CA8_amm335x
	include_directories("${PROJECT_SOURCE_DIR}/src/portable/FreeRTOS/portable/GCC/ARM_CA8_amm335x")
	add_library(src_portable_FreeRTOS_portable_GCC_ARM_CA8_amm335x "")
	set_target_properties(src_portable_FreeRTOS_portable_GCC_ARM_CA8_amm335x PROPERTIES LINKER_LANGUAGE C)
	subdirs("${PROJECT_SOURCE_DIR}/src/portable/FreeRTOS/portable/GCC/ARM_CA8_amm335x")
#adding entries for lib_third_party_amazon_freertos_kernel_portable_MemMang
	include_directories("${PROJECT_SOURCE_DIR}/lib/third_party/amazon/freertos_kernel/portable/MemMang")
	add_library(lib_third_party_amazon_freertos_kernel_portable_MemMang "")
	set_target_properties(lib_third_party_amazon_freertos_kernel_portable_MemMang PROPERTIES LINKER_LANGUAGE C)
	subdirs("${PROJECT_SOURCE_DIR}/lib/third_party/amazon/freertos_kernel/portable/MemMang")
#adding entries for src_portable_AM335X
	include_directories("${PROJECT_SOURCE_DIR}/src/portable/AM335X")
	include_directories("${PROJECT_SOURCE_DIR}/src/portable/AM335X/inc")
	add_library(src_portable_AM335X "")
	set_target_properties(src_portable_AM335X PROPERTIES LINKER_LANGUAGE C)
	target_compile_definitions(src_portable_AM335X 
		PRIVATE -DBOOT=MMCSD -DCONSOLE=UARTCONSOLE
	)
	subdirs("${PROJECT_SOURCE_DIR}/src/portable/AM335X")
#adding entries for src_application
	include_directories("${PROJECT_SOURCE_DIR}/src/application")
	add_library(src_application "")
	set_target_properties(src_application PROPERTIES LINKER_LANGUAGE C)
	subdirs("${PROJECT_SOURCE_DIR}/src/application")

target_link_libraries (${PROJECT_NAME}.${BUILD_EXT}
	 
	lib_third_party_ti_drivers
	lib_third_party_ti_utils
	lib_third_party_ti_mmcsdlib
	lib_third_party_ti_platform_beaglebone
	lib_third_party_ti_nandlib
	lib_third_party_ti_system_config_armv7a
	lib_third_party_amazon_freertos_kernel
	src_portable_FreeRTOS_portable_GCC_ARM_CA8_amm335x
	lib_third_party_amazon_freertos_kernel_portable_MemMang
	src_portable_AM335X
	src_application
)

# Manual target_sources for cp15.S and cpu.c (gcc/ folder has CMake binary dir conflicts)
target_sources(${PROJECT_NAME}.elf PRIVATE
	"${PROJECT_SOURCE_DIR}/lib/third_party/ti/system_config/armv7a/gcc/cp15.S"
	"${PROJECT_SOURCE_DIR}/lib/third_party/ti/system_config/armv7a/gcc/cpu.c"
)
