#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Following ignore list for PEP8 for Formatting "--ignore=E501,E126,E241,E221"

""" Run this where build is a subfolder """
import os
import sys

from lib.third_party.c_source_tools.src.listFiles import make_generate_cmake_project_includes as make_generate_cmake_project_includes


class cmake_list_file_args():
    args = None

    def __init__(self, subfolders=None, path=None):
        cmake_list_file_args.args = self.create_args(subfolders=None, path=None)

    def getargs(self):
        return cmake_list_file_args.args

    def create_args(self, subfolders=None, path=None):
        args = {}
        if path is not None:
            if(os.path.isdir(path)):
                root = path
            else:
                return args
        else:
            root = os.getcwd()
        # Folders that should only provide include_directories(), NOT subdirs()
        # These folders contain only header files or have CMake binary dir conflicts
        args['no_subdirs_paths'] = [
            'lib/third_party/ti/mmcsdlib/include',
            'lib/third_party/ti/nandlib/include',
            'lib/third_party/ti/system_config/armv7a/am335x/gcc',
            'lib/third_party/ti/system_config/armv7a/gcc',
            'lib/third_party/amazon/freertos_kernel/include',
            'src/portable/AM335X/inc',
            'src/portable/ported_aws_bufpool/inc',
        ]
        args['prefix'] = 'bbb_freeRTOS'
        args['application_libs'] = {
                                        'FreeRTOS'      :   [   # Important to Keep this on top of Amazon Includes"
                                                                'lib/third_party/amazon/freertos_kernel/include',
                                                                'src/portable/FreeRTOS/portable/GCC/ARM_CA8_amm335x',
                                                            ],

                                        'ti_starterware':   [
                                                                'lib/third_party/ti/include',
                                                                'lib/third_party/ti/include/armv7a',
                                                                'lib/third_party/ti/include/armv7a/am335x',
                                                                'lib/third_party/ti/include/hw',
                                                            ],
                                         'application':      [
                                                                 'src/inc',
                                                                 'src/portable/AM335X/inc',
                                                                 'src/inc/config_files',
                                                                 'src/portable/AM335X/inc',
                                                             ],
                                       
                                    }
        args['suf']     = ''
        args['pattern'] = ['*.c', '*.h', '*.S']
        args['headers'] = ['.h']
        args['sources'] = ['.c','.S']
        args['exclude'] =   [
                            #general exclusions
                            'unused',  # File or Folder
                            'tools/',  # This is not used in our stack
                            'Utils/',  # This is not used in our stack
                            'binary/',  # This is not used in our stack
                            'build/',  # This is not used in our stack
                            'target_config/',  # This is not used in our stack
                            'mdio_interrupt.c',  # This is not used in our stack   

                            #ti Starterware exclusions                           
                            'ewarm',  # This is not used in our stack                             
                            'cgt',  # This is not used in our stack       
                            'lib/third_party/ti/',  # We dont want the complete stack
                            'fatfs/port',  # This is not used in our stack
                            'raster.c',  # This is not used in our stack
                            'cmdline.c',  # This is not used in our stack
                            'startup.c',  # We have our own version in application
                            'init.S',     # We have our own version in application
                            'usbphyGS70.c',  # This is not used in our stack
                            'clk_config_dmtimer.c',  # This is not used in our stack
                            'serial.c',  # This is not used in our stack
                            'mmu_init.c',  # This is not used in our stack
                            'exceptionhandler.S',  # This is not used in our stack
                            'interrupt.c',  # We have our own version in application

                            #Amazon Exclusions
                            'lib/third_party/amazon/',  # This is not used in our stack only some subfolders!
                            
                            #FreeRTOS Exclusions
                            'heap_1.c',  # This is not used in our stack
                            'heap_2.c',  # This is not used in our stack
                            'heap_3.c',  # This is not used in our stack
                            # 'heap_4.c',  # We want to use Simple FreeRTOS heaps
                            'heap_5.c',  # This is not used in our stack
                             'lib/third_party/amazon/freertos_kernel/portable',  # This is not used in our stack

                              ]

        # Exclude the ported_aws_bufpool folder since it has no .c files (only headers in inc/)
        args['exclude'].append('src/portable/ported_aws_bufpool')

        # Manual target_sources for folders that can't use subdirs() due to CMake binary dir conflicts
        # e.g., lib/third_party/ti/system_config/armv7a/gcc/ — has cp15.S, cpu.c but conflicts with other gcc/ folders
        # ${PROJECT_NAME}.elf is the actual exec target — used by all subdirectories with CMakeLists.txt
        # The cmake binary dir conflict in gcc/ folder requires manual target_sources
        args['manual_target_sources'] = {
            '${PROJECT_NAME}.elf': [
                '${PROJECT_SOURCE_DIR}/lib/third_party/ti/system_config/armv7a/gcc/cp15.S',
                '${PROJECT_SOURCE_DIR}/lib/third_party/ti/system_config/armv7a/gcc/cpu.c',
            ],
        }

        args['root']           = root
        args['CmakeIncludes']  = "ProjectIncludes.cmake"
        if subfolders is None:
            args['subfolders'] = [
                                ['lib/third_party/ti/drivers', '-DBOOT=MMCSD -DCONSOLE=UARTCONSOLE'],  #Starterware from ti
                                ['lib/third_party/ti/utils', '-DBOOT=MMCSD -DCONSOLE=UARTCONSOLE'],  #Starterware from ti
                                ['lib/third_party/ti/mmcsdlib', '-DBOOT=MMCSD -DCONSOLE=UARTCONSOLE'],  #Starterware from ti
                                ['lib/third_party/ti/platform/beaglebone', '-DBOOT=MMCSD -DCONSOLE=UARTCONSOLE'],  #Starterware from ti
                                ['lib/third_party/ti/nandlib', '-DBOOT=MMCSD -DCONSOLE=UARTCONSOLE'],  #Starterware from ti
                                ['lib/third_party/ti/system_config/armv7a', '-DBOOT=MMCSD -DCONSOLE=UARTCONSOLE'],  #Starterware from ti
                               
                               

                                 # ['src', '-DBOOT=MMCSD -DCONSOLE=UARTCONSOLE'],  # Application

                                 # FreeRTOS inclusions
                                ['lib/third_party/amazon/freertos_kernel'],  # using a port of FreeRTOS for AM335x with critical Section nesting baed on CA9
                                ['src/portable/FreeRTOS/portable/GCC/ARM_CA8_amm335x'],
                                ['lib/third_party/amazon/freertos_kernel/portable/MemMang'],

                                 #platform - am335X
                                 ['src/portable/AM335X','-DBOOT=MMCSD -DCONSOLE=UARTCONSOLE'],  # Our own versions of files changed from Starterware & Hal initialisations built on Starterware functions
                                 # misc & Aws etc
                                 # ['src/portable/ported_aws_bufpool'],  # Commented: folder has no .c files, only headers in inc/
                                 #Application Glues
                                ['src/application'],  #lwip from Amazon stack as a third party lib
                            ]
        return args


default_args = cmake_list_file_args(path=os.getcwd()).args

if __name__ == '__main__':
    make_generate_cmake_project_includes(default_args)

# Post-process ProjectIncludes.cmake to fix issues that the generator can't handle
import re

cmake_file = os.path.join(os.getcwd(), "ProjectIncludes.cmake")
if os.path.exists(cmake_file):
    with open(cmake_file, "r") as f:
        content = f.read()
    
    # 1. Remove subdirs() lines for folders that only contain headers (no .c files)
    # These are folders where list_files() found only .h files, but generator still added subdirs()
    no_subdir_patterns = [
        r'\tsubdirs\("\$\{PROJECT_SOURCE_DIR\}/lib/third_party/ti/mmcsdlib/include"\)\n',
        r'\tsubdirs\("\$\{PROJECT_SOURCE_DIR\}/lib/third_party/ti/nandlib/include"\)\n',
        r'\tsubdirs\("\$\{PROJECT_SOURCE_DIR\}/lib/third_party/ti/system_config/armv7a/am335x/gcc"\)\n',
        r'\tsubdirs\("\$\{PROJECT_SOURCE_DIR\}/lib/third_party/ti/system_config/armv7a/gcc"\)\n',
        r'\tsubdirs\("\$\{PROJECT_SOURCE_DIR\}/lib/third_party/amazon/freertos_kernel/include"\)\n',
        r'\tsubdirs\("\$\{PROJECT_SOURCE_DIR\}/src/portable/AM335X/inc"\)\n',
        r'\tsubdirs\("\$\{PROJECT_SOURCE_DIR\}/src/portable/ported_aws_bufpool/inc"\)\n',
    ]
    for pattern in no_subdir_patterns:
        content = re.sub(pattern, '', content)
    
    # 2. Add target_sources for cp15.S and cpu.c (manual because gcc/ folder has CMake binary dir conflict)
    cp15_target_sources = '''
# Manual target_sources for cp15.S and cpu.c (gcc/ folder has CMake binary dir conflicts)
target_sources(${PROJECT_NAME}.elf PRIVATE
	"${PROJECT_SOURCE_DIR}/lib/third_party/ti/system_config/armv7a/gcc/cp15.S"
	"${PROJECT_SOURCE_DIR}/lib/third_party/ti/system_config/armv7a/gcc/cpu.c"
)
'''
    # Only add if not already present
    if 'cp15.S' not in content:
        content = content.rstrip() + '\n' + cp15_target_sources
    
    with open(cmake_file, "w") as f:
        f.write(content)
    
    print("Post-processed ProjectIncludes.cmake: fixed subdirs() and added manual target_sources")