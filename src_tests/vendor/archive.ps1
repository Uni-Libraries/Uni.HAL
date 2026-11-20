$ErrorActionPreference = 'Stop'
$originalLocation = Get-Location
$vendor = (Resolve-Path "$PSScriptRoot/../../3rdparty").Path
$root = Join-Path ([IO.Path]::GetTempPath()) ("uni-hal-archive-" + [guid]::NewGuid())
$dir = "$root/3rdparty"
$repo = 'ARM-software/CMSIS_5'
try {
    $null = New-Item -ItemType Directory "$dir/arm_cmsis_core/include"
    Set-Content "$dir/arm_cmsis_core/include/old.h" 'old'
    Copy-Item "$vendor/update.ps1" $dir
    $pins = Import-PowerShellDataFile "$vendor/upstream.psd1"
    foreach ($entry in $pins.Values) {
        if ($entry.Revision -notmatch '^[0-9a-f]{40}$' -or $entry.SHA256 -notmatch '^[0-9a-f]{64}$') {
            throw 'Expected a full commit and an archive digest for every source'
        }
    }

    $pin = $pins[$repo]
    $snapshot = "$root/CMSIS_5-$($pin.Revision)"
    $null = New-Item -ItemType Directory "$snapshot/CMSIS/Core/Include"
    Set-Content "$snapshot/CMSIS/Core/Include/example.h" 'fixture'
    Set-Content "$snapshot/LICENSE.txt" 'license'
    $fixtureArchive = "$root/upstream.zip"
    Compress-Archive $snapshot $fixtureArchive
    $digest = (Get-FileHash $fixtureArchive).Hash
    (Get-Content -Raw "$vendor/upstream.psd1").Replace($pin.SHA256, $digest) |
        Set-Content "$dir/upstream.psd1"

    $expectedURL = "https://codeload.github.com/$repo/zip/$($pin.Revision)"
    function Invoke-WebRequest($Uri, $OutFile) {
        if ($Uri -ne $expectedURL) {
            throw "Expected the pinned URL, got $Uri"
        }
        Copy-Item $fixtureArchive $OutFile
    }
    & "$dir/update.ps1" CMSIS-Core
    if (!(Test-Path "$dir/arm_cmsis_core/include/example.h") -or (Test-Path "$dir/arm_cmsis_core/include/old.h")) {
        throw 'Pinned source was not installed as the replacement tree'
    }

    # The same URI now serves different bytes. Keep the installed sources.
    [IO.File]::AppendAllText($fixtureArchive, 'changed')
    try {
        & "$dir/update.ps1" CMSIS-Core
        throw 'Modified archive was accepted'
    } catch {
        if ($_.Exception.Message -notmatch 'SHA256 mismatch') { throw }
    }
    if (!(Test-Path "$dir/arm_cmsis_core/include/example.h")) { throw 'Lost the installed include tree' }
} finally {
    Set-Location $originalLocation
    Remove-Item -Recurse -Force $root
}
