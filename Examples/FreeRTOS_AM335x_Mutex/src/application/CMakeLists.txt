
target_sources(src_application
                PUBLIC
                    "${CMAKE_CURRENT_SOURCE_DIR}/app_utils.c"
                    "${CMAKE_CURRENT_SOURCE_DIR}/CriticalSectionDemo.c"
                    "${CMAKE_CURRENT_SOURCE_DIR}/main.c"
                    "${CMAKE_CURRENT_SOURCE_DIR}/MutexDemo.c"
                    "${CMAKE_CURRENT_SOURCE_DIR}/PriorityInheritanceDemo.c"
                    "${CMAKE_CURRENT_SOURCE_DIR}/RecursiveMutexDemo.c"
                    "${CMAKE_CURRENT_SOURCE_DIR}/TaskLifecycleDemo.c"
                    "${CMAKE_CURRENT_SOURCE_DIR}/UARTProtectionDemo.c"

            )
