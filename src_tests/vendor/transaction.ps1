$ErrorActionPreference = 'Stop'
$vendor = (Resolve-Path "$PSScriptRoot/../../3rdparty").Path
$root = Join-Path ([IO.Path]::GetTempPath()) ("uni-hal-update-" + [guid]::NewGuid())
$fixtureVendor = "$root/3rdparty"
$coreDir = "$fixtureVendor/arm_cmsis_core"
$usbDir = "$fixtureVendor/st_usb_device"

function Assert-OldTree {
    if (!(Test-Path "$coreDir/include/old.h") -or (Test-Path "$coreDir/include/new.h") -or
        !(Test-Path "$coreDir/CMakeLists.txt") -or !(Test-Path "$usbDir/untouched")) {
        throw 'Failed update changed the installed components'
    }
}

try {
    $null = New-Item -ItemType Directory "$coreDir/include", $usbDir
    Set-Content "$coreDir/include/old.h" 'old'
    Set-Content "$coreDir/CMakeLists.txt" 'project build recipe'
    Set-Content "$usbDir/untouched" 'old USB tree'
    Copy-Item "$vendor/update.ps1" $fixtureVendor

    $corePin = (Import-PowerShellDataFile "$vendor/upstream.psd1")['ARM-software/CMSIS_5']
    $snapshot = "$root/CMSIS_5-$($corePin.Revision)"
    $null = New-Item -ItemType Directory "$snapshot/CMSIS/Core/Include"
    Set-Content "$snapshot/CMSIS/Core/Include/new.h" 'new'
    Set-Content "$snapshot/LICENSE.txt" 'license'
    $fixtureArchive = "$root/cmsis.zip"
    Compress-Archive $snapshot $fixtureArchive
    (Get-Content -Raw "$vendor/upstream.psd1").Replace($corePin.SHA256, (Get-FileHash $fixtureArchive).Hash) |
        Set-Content "$fixtureVendor/upstream.psd1"

    function Invoke-WebRequest($Uri, $OutFile) {
        if ($Uri -like '*/CMSIS_5/zip/*') { Copy-Item $fixtureArchive $OutFile }
        else { throw 'Simulated USB download failure' }
    }

    try {
        & "$fixtureVendor/update.ps1" CMSIS-Core USB-STM-Device
        throw 'Expected USB download failure'
    } catch {
        if ($_.Exception.Message -ne 'Simulated USB download failure') { throw }
    }
    Assert-OldTree

    $patchDir = "$fixtureVendor/patches/CMSIS-Core"
    $null = New-Item -ItemType Directory $patchDir
    Set-Content "$patchDir/broken.patch" 'not a patch'
    try {
        & "$fixtureVendor/update.ps1" CMSIS-Core
        throw 'Expected patch failure'
    } catch {
        if ($_.Exception.Message -notlike 'Cannot apply broken.patch*') { throw }
    }
    Assert-OldTree
    Remove-Item $patchDir -Recurse

    # Simulate termination between renames. One component was installed, another only backed up.
    $old = "$fixtureVendor/~temp/vendor-update/active/old"
    $null = New-Item -ItemType Directory $old -Force
    Move-Item $coreDir $old
    Move-Item $usbDir $old
    $null = New-Item -ItemType Directory "$coreDir/include"
    Set-Content "$coreDir/include/new.h" 'incomplete publication'
    # A concurrent invoker must not roll back a transaction owned by a live writer.
    $writer = [IO.File]::Open("$fixtureVendor/~temp/vendor-update/update.lock", 'OpenOrCreate', 'ReadWrite', 'None')
    try {
        try {
            & "$fixtureVendor/update.ps1" USB-STM-Device
            throw 'A second writer acquired the update lock'
        } catch [IO.IOException] {
            if (!(Test-Path "$old/arm_cmsis_core/include/old.h") -or (Test-Path $usbDir)) {
                throw 'A second updater touched a transaction still in use'
            }
        }
    } finally {
        $writer.Dispose()
    }
    try {
        & "$fixtureVendor/update.ps1" USB-STM-Device
        throw 'Expected USB download failure'
    } catch {
        if ($_.Exception.Message -ne 'Simulated USB download failure') { throw }
    }
    Assert-OldTree

    & "$fixtureVendor/update.ps1" CMSIS-Core
    if (!(Test-Path "$coreDir/include/new.h") -or (Test-Path "$coreDir/include/old.h") -or
        (Get-Content "$coreDir/CMakeLists.txt") -ne 'project build recipe') {
        throw 'Retry did not replace vendor sources while preserving local files'
    }
} finally {
    Remove-Item -LiteralPath $root -Recurse -Force
}
