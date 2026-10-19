[CmdletBinding()]
param([string]$AliasParent = $env:TEMP)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
foreach ($tool in @('cmake', 'ninja', 'g++', 'windres')) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        throw "Required tool is missing from PATH: $tool. See README.md."
    }
}
if ((& g++ -dumpmachine) -ne 'x86_64-w64-mingw32') {
    throw 'Use an x86_64-w64-mingw32 compiler for the Windows x64 build.'
}
if ($AliasParent -match '[^\x00-\x7F]') {
    throw 'AliasParent must be an existing ASCII-only path. Example: -AliasParent C:\Temp'
}
if (-not (Test-Path -LiteralPath $AliasParent -PathType Container)) {
    throw 'AliasParent must be an existing directory.'
}
$hasher = [System.Security.Cryptography.SHA256]::Create()
try {
    $digest = $hasher.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($projectRoot.ToLowerInvariant()))
    $suffix = ([BitConverter]::ToString($digest) -replace '-', '').Substring(0, 12).ToLowerInvariant()
} finally {
    $hasher.Dispose()
}
$sourceAlias = Join-Path $AliasParent "omok-build-$suffix"
if (Test-Path -LiteralPath $sourceAlias) {
    $existing = Get-Item -LiteralPath $sourceAlias -Force
    if ($existing.LinkType -ne 'Junction' -or @($existing.Target).Count -ne 1 -or
        ([string]$existing.Target).TrimEnd('\') -ne $projectRoot.TrimEnd('\')) {
        throw "Alias exists with an unexpected target; nothing was removed: $sourceAlias"
    }
} else {
    New-Item -ItemType Junction -Path $sourceAlias -Target $projectRoot | Out-Null
}

# Files remain in the project. Only the path presented to legacy build tools changes.
$buildDir = Join-Path $sourceAlias 'build/native-release'
# Old failed Unicode-path builds can leave absolute paths in Ninja's metadata.
# Delete only these two generated metadata files, never sources or directories.
foreach ($metadataName in @('.ninja_deps', '.ninja_log')) {
    $metadataPath = Join-Path $projectRoot "build/native-release/$metadataName"
    if (Test-Path -LiteralPath $metadataPath -PathType Leaf) {
        Remove-Item -LiteralPath $metadataPath
    }
}
& cmake --fresh -S $sourceAlias -B $buildDir -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed: $LASTEXITCODE" }
& cmake --build $buildDir
if ($LASTEXITCODE -ne 0) { throw "Build failed: $LASTEXITCODE" }
& cmake --install $buildDir --prefix (Join-Path $projectRoot 'dist/omok')
if ($LASTEXITCODE -ne 0) { throw "Install staging failed: $LASTEXITCODE" }
Write-Output "Executable: $(Join-Path $projectRoot 'dist/omok/omok.exe')"
Write-Output "Source alias: $sourceAlias (junction only; no source copy)"
