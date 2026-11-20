#!/usr/bin/env pwsh

# Run without arguments to update all components, or pass one or more component names.
# The staging build uses CMake and Ninja, a native C compiler for the PC port and
# arm-none-eabi-gcc on PATH for the embedded ports. CMAKE_BUILD_PARALLEL_LEVEL applies.
# The next run restores any interrupted publication before downloading new archives.
param([Parameter(Position=0, ValueFromRemainingArguments)][string[]]$Components)
$ErrorActionPreference = 'Stop'
$pins = Import-PowerShellDataFile "$PSScriptRoot/upstream.psd1"

function Get-Snapshot([string]$Repository) {
    $pin = $pins[$Repository]
    if ($pin.Revision -notmatch '^[0-9a-f]{40}$' -or $pin.SHA256 -notmatch '^[0-9a-f]{64}$') {
        throw "Missing revision or SHA256 for $Repository"
    }
    $name = $Repository.Split('/')[-1]
    $zip = "$downloads/$name.zip"
    Invoke-WebRequest -Uri "https://codeload.github.com/$Repository/zip/$($pin.Revision)" -OutFile $zip
    if ((Get-FileHash -Algorithm SHA256 -LiteralPath $zip).Hash -ne $pin.SHA256) {
        throw "SHA256 mismatch for $Repository at $($pin.Revision)"
    }
    [IO.Compression.ZipFile]::ExtractToDirectory($zip, $downloads)
    return "$downloads/$name-$($pin.Revision)"
}

function Copy-SourceDirectory([string]$Source, [string]$Destination, [string[]]$Patterns) {
    if (Test-Path -LiteralPath $Destination) {
        Remove-Item -LiteralPath $Destination -Recurse -Force
    }
    if (!$Patterns) {
        Copy-Item -LiteralPath $Source -Destination $Destination -Recurse
        return
    }
    $null = New-Item -ItemType Directory -Force $Destination
    foreach ($pattern in $Patterns) {
        $files = @(Get-ChildItem -File -LiteralPath $Source -Filter $pattern)
        if (!$files) { throw "Missing $pattern in $Source" }
        Copy-Item -LiteralPath $files.FullName -Destination $Destination
    }
}

function Copy-Component([string]$Component, [string]$Destination) {
    switch ($Component) {
        'CMSIS-Core' {
            $source = Get-Snapshot 'ARM-software/CMSIS_5'
            Copy-SourceDirectory "$source/CMSIS/Core/Include" "$Destination/include"
            Copy-Item "$source/LICENSE.txt" "$Destination/License.txt" -Force
        }
        { $_ -in 'DFP-STM32L4', 'DFP-STM32H7' } {
            $family = $Component.Substring(9).ToLowerInvariant()
            $device = Get-Snapshot "STMicroelectronics/cmsis-device-$family"
            $hal = Get-Snapshot "STMicroelectronics/stm32${family}xx-hal-driver"
            Copy-SourceDirectory "$device/Include" "$Destination/include_device"
            Copy-SourceDirectory "$device/Source/Templates/gcc" "$Destination/src_device"
            Copy-Item "$device/Source/Templates/system_stm32${family}xx.c" "$Destination/src_device"
            Copy-SourceDirectory "$hal/Inc" "$Destination/include_hal"
            Copy-SourceDirectory "$hal/Src" "$Destination/src_hal"
            Copy-Item "$hal/LICENSE.md" "$Destination/LICENSE.md" -Force
        }
        'USB-STM-Device' {
            $source = Get-Snapshot 'STMicroelectronics/stm32-mw-usb-device'
            Copy-SourceDirectory "$source/Core/Inc" "$Destination/include_core"
            Copy-SourceDirectory "$source/Core/Src" "$Destination/src_core"
            Copy-SourceDirectory "$source/Class/CDC/Inc" "$Destination/include_cdc"
            Copy-SourceDirectory "$source/Class/CDC/Src" "$Destination/src_cdc"
            Copy-Item "$source/LICENSE.md" "$Destination/LICENSE.md" -Force
        }
        'FreeRTOS-Kernel' {
            $source = Get-Snapshot 'FreeRTOS/FreeRTOS-Kernel'
            Copy-SourceDirectory $source "$Destination/src" '*.c'
            Copy-SourceDirectory "$source/include" "$Destination/include"
            Remove-Item "$Destination/include/CMakeLists.txt", "$Destination/include/stdint.readme"
            Copy-SourceDirectory "$source/portable/MemMang" "$Destination/src_memmng" '*.c'
            Copy-SourceDirectory "$source/portable/GCC/ARM_CM4F" "$Destination/src_port/arm_cm4f" @('*.c','*.h')
            Copy-SourceDirectory "$source/portable/GCC/ARM_CA9" "$Destination/src_port/arm_ca9" @('*.c','*.h','*.S')
            Copy-SourceDirectory "$source/portable/ThirdParty/GCC/Posix" "$Destination/src_port/pc_posix" @('*.c','*.h')
            Copy-SourceDirectory "$source/portable/ThirdParty/GCC/Posix/utils" "$Destination/src_port/pc_posix/utils" @('*.c','*.h')
            Copy-SourceDirectory "$source/portable/MSVC-MingW" "$Destination/src_port/pc_windows" @('*.c','*.h')
            Copy-Item "$source/LICENSE.md" "$Destination/LICENSE.md" -Force
        }
        'FreeRTOS-FAT' {
            $source = Get-Snapshot 'FreeRTOS/Lab-Project-FreeRTOS-FAT'
            Copy-SourceDirectory $source "$Destination/src" '*.c'
            Copy-SourceDirectory "$source/include" "$Destination/include" '*.h'
        }
    }
}

function Add-LocalPatches([string]$Component, [string]$Tree) {
    $patchDir = "$PSScriptRoot/patches/$Component"
    if (Test-Path -LiteralPath $patchDir) {
        # Patch paths start with 3rdparty/. Isolate them from the containing checkout.
        if (!(Test-Path "$Tree/.git")) {
            git -C $Tree init --quiet
            if ($LASTEXITCODE -ne 0) { throw 'Cannot initialize the patch staging tree' }
        }
        foreach ($patch in Get-ChildItem -File -LiteralPath $patchDir -Filter '*.patch' | Sort-Object Name) {
            git -C $Tree apply -- $patch.FullName
            if ($LASTEXITCODE -ne 0) { throw "Cannot apply $($patch.Name) to $Component" }
        }
    }
}

function Build-StagedComponents {
    $targets = [ordered]@{
        PC = @('FreeRTOS-Kernel')
        ZYNQ7000 = @('FreeRTOS-Kernel')
        STM32H743 = @('CMSIS-Core', 'DFP-STM32H7', 'FreeRTOS-Kernel', 'USB-STM-Device')
        STM32L496 = @('CMSIS-Core', 'DFP-STM32L4', 'FreeRTOS-Kernel', 'FreeRTOS-FAT')
    }
    foreach ($target in $targets.Keys) {
        if (!($Components | Where-Object { $_ -in $targets[$target] })) { continue }
        $build = "$active/build-$target"
        $options = @("-DSTAGED_VENDOR_DIR=$staging/3rdparty", "-DUNI_HAL_TARGET_MCU=$target",
                     '-DCMAKE_BUILD_TYPE=Release')
        if ($target -ne 'PC') {
            $options += "-DCMAKE_TOOLCHAIN_FILE=$PSScriptRoot/../cmake/toolchain.cmake"
        }
        cmake -S "$PSScriptRoot/verify" -B $build -G Ninja @options
        if ($LASTEXITCODE -ne 0) { throw "Staged vendor configure failed for $target" }
        cmake --build $build --config Release
        if ($LASTEXITCODE -ne 0) { throw "Staged vendor build failed for $target" }
    }
}

$directories = [ordered]@{
    'CMSIS-Core' = 'arm_cmsis_core'
    'DFP-STM32L4' = 'st_stm32l4'
    'DFP-STM32H7' = 'st_stm32h7'
    'USB-STM-Device' = 'st_usb_device'
    'FreeRTOS-Kernel' = 'freertos_kernel'
    'FreeRTOS-FAT' = 'freertos_fat'
}
if (!$Components) { $Components = $directories.Keys }
$Components = @($Components | Select-Object -Unique)
foreach ($component in $Components) {
    if (!$directories.Contains($component)) { throw "Unknown component '$component'" }
}

# All renames stay on the same filesystem. While "active" exists, every directory in "old"
# must be restored. Renaming "active" to "finished" commits the complete set in one step.
function Restore-InterruptedUpdate {
    if (!(Test-Path -LiteralPath $active)) { return }
    if (Test-Path "$active/old") {
        $null = [IO.Directory]::CreateDirectory("$active/discard")
        foreach ($backup in Get-ChildItem -LiteralPath "$active/old" -Directory) {
            $live = "$PSScriptRoot/$($backup.Name)"
            if ([IO.Directory]::Exists($live)) {
                [IO.Directory]::Move($live, "$active/discard/$($backup.Name)")
            }
            [IO.Directory]::Move($backup.FullName, $live)
        }
    }
    Remove-Item -LiteralPath $active -Recurse -Force
}

$transactionRoot = "$PSScriptRoot/~temp/vendor-update"
$active = "$transactionRoot/active"
$finished = "$transactionRoot/finished"
$downloads = "$active/downloads"
$staging = "$active/new"
$null = [IO.Directory]::CreateDirectory($transactionRoot)
$lock = [IO.File]::Open("$transactionRoot/update.lock", 'OpenOrCreate', 'ReadWrite', 'None')
try {
    Restore-InterruptedUpdate
    if (Test-Path -LiteralPath $finished) { Remove-Item -LiteralPath $finished -Recurse -Force }
    $null = [IO.Directory]::CreateDirectory($downloads)
    $null = [IO.Directory]::CreateDirectory("$active/old")
    $null = [IO.Directory]::CreateDirectory("$staging/3rdparty")

    foreach ($component in $Components) {
        $name = $directories[$component]
        $destination = "$staging/3rdparty/$name"
        # Preserve component-local build recipes, hooks, linker scripts and other project files.
        Copy-Item -LiteralPath "$PSScriptRoot/$name" -Destination $destination -Recurse -Force
        Copy-Component $component $destination
        Add-LocalPatches $component $staging
    }

    Build-StagedComponents

    foreach ($component in $Components) {
        $name = $directories[$component]
        [IO.Directory]::Move("$PSScriptRoot/$name", "$active/old/$name")
        [IO.Directory]::Move("$staging/3rdparty/$name", "$PSScriptRoot/$name")
    }
    [IO.Directory]::Move($active, $finished)
    Remove-Item -LiteralPath $finished -Recurse -Force
} finally {
    try { Restore-InterruptedUpdate } finally { $lock.Dispose() }
}
