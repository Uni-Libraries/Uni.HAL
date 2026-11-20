# Exercise the production staging project with two small CMSIS consumers and a host compiler.
function New-StagedBuildFixture([string]$Root) {
    $vendor = (Resolve-Path "$PSScriptRoot/../../3rdparty").Path
    $fixtureVendor = "$Root/3rdparty"
    $null = New-Item -ItemType Directory "$Root/cmake"
    Set-Content "$Root/cmake/toolchain.cmake" ''
    Copy-Item "$vendor/verify" $fixtureVendor -Recurse
    Copy-Item "$vendor/arm_cmsis_core/CMakeLists.txt" "$fixtureVendor/arm_cmsis_core/"
    foreach ($device in 'st_stm32h7', 'st_stm32l4') {
        $null = New-Item -ItemType Directory "$fixtureVendor/$device"
        Set-Content "$fixtureVendor/$device/CMakeLists.txt" @'
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../arm_cmsis_core/include/old.h")
    message(FATAL_ERROR "The installed CMSIS tree was replaced before the candidate build")
endif()
add_library(candidate STATIC ../../check.c)
target_link_libraries(candidate PRIVATE arm_cmsis_core)
'@
    }
}
