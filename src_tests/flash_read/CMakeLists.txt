add_executable(uni_hal_test_flash_read boundaries.c ../../src/flash/uni_hal_flash_read_stm32h7.c)
target_include_directories(uni_hal_test_flash_read PRIVATE fakes)
target_link_libraries(uni_hal_test_flash_read PRIVATE uni.hal)
add_test(NAME flash_h743_read_bounds COMMAND uni_hal_test_flash_read)
