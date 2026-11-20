$ErrorActionPreference = 'Stop'
$patches = (Resolve-Path "$PSScriptRoot/../../3rdparty/patches").Path
$root = Join-Path ([IO.Path]::GetTempPath()) ("uni-hal-patches-" + [guid]::NewGuid())
try {
    $null = New-Item -ItemType Directory $root
    git -C $root init --quiet
    if ($LASTEXITCODE -ne 0) { throw 'Cannot create test repository' }
    Copy-Item -LiteralPath $patches -Destination "$root/patches" -Recurse -Force
    git -C $root -c core.autocrlf=input add -- patches
    if ($LASTEXITCODE -ne 0) { throw 'Cannot stage test patches' }
    Remove-Item "$root/patches" -Recurse -Force
    git -C $root -c core.autocrlf=true checkout-index --all
    if ($LASTEXITCODE -ne 0) { throw 'Cannot check out patches' }

    foreach ($patch in Get-ChildItem "$root/patches" -Filter '*.patch' -Recurse) {
        git -C $root apply --stat -- $patch.FullName
        if ($LASTEXITCODE -ne 0) { throw "Patch from the CRLF checkout is invalid: $($patch.Name)" }
    }
} finally {
    Remove-Item -LiteralPath $root -Recurse -Force
}
