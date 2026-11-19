# Run the H743 lifecycle against a model of the ST entry points and the common recovery code.
add_executable(uni_hal_test_i2c_h7_recovery recovery.c ../../src/i2c/uni_hal_i2c_stm32h7.c)
target_include_directories(uni_hal_test_i2c_h7_recovery PRIVATE fakes ../i2c)
target_link_libraries(uni_hal_test_i2c_h7_recovery PRIVATE uni.hal)
add_test(NAME i2c_h7_recovery COMMAND uni_hal_test_i2c_h7_recovery)
