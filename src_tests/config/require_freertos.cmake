execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${CMAKE_CURRENT_LIST_DIR}/../.."
        -B "${CMAKE_CURRENT_BINARY_DIR}/without-freertos"
        -DUNI_HAL_TARGET_MCU=PC -DUNI_HAL_USE_FREERTOS=OFF
    RESULT_VARIABLE result
    ERROR_VARIABLE diagnostic)
if(result EQUAL 0 OR NOT diagnostic MATCHES "Uni[.]HAL requires FreeRTOS")
    message(FATAL_ERROR "Expected CMake to reject UNI_HAL_USE_FREERTOS=OFF, got ${result}.\n${diagnostic}")
endif()
