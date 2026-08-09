
target_sources(src_application
                PUBLIC
                    "${CMAKE_CURRENT_SOURCE_DIR}/app_utils.c"
                    "${CMAKE_CURRENT_SOURCE_DIR}/ili9341.c"
                    "${CMAKE_CURRENT_SOURCE_DIR}/lcd_display.c"
                    "${CMAKE_CURRENT_SOURCE_DIR}/lds_scan.c"
                    "${CMAKE_CURRENT_SOURCE_DIR}/lidar_lds08rr.c"
                    "${CMAKE_CURRENT_SOURCE_DIR}/main.c"
                    "${CMAKE_CURRENT_SOURCE_DIR}/spi_lcd.c"
                    "${CMAKE_CURRENT_SOURCE_DIR}/uart1_driver.c"
                    "${CMAKE_CURRENT_SOURCE_DIR}/uart1_reader.c"

            )
