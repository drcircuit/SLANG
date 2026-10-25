[CmdletBinding()]
param(
    [string]$BuildDirectory = 'build/native-ninja',
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [switch]$Package,
    [switch]$VerifyParity
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildRoot = [IO.Path]::GetFullPath((Join-Path $projectRoot $BuildDirectory))

# Locate developer tools only. Finished packages do not run this script.
$visualStudio = $null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (Test-Path -LiteralPath $vswhere) {
    $visualStudio = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
}

$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
$cmake = if ($cmakeCommand) { $cmakeCommand.Source } elseif ($visualStudio) {
    Join-Path $visualStudio 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
} else { throw 'Install a C++20 compiler and CMake to build SLANG from source.' }
if (-not (Test-Path -LiteralPath $cmake)) { throw "CMake was not found at $cmake" }
$cmakeTools = Split-Path -Parent $cmake
$ctest = Join-Path $cmakeTools 'ctest.exe'
$cpack = Join-Path $cmakeTools 'cpack.exe'

# Ninja avoids MSBuild's dependency-tracking service and works in restricted
# build environments. Import only the compiler environment fields we need.
if ($visualStudio -and -not (Get-Command cl -ErrorAction SilentlyContinue)) {
    $vcvars = Join-Path $visualStudio 'VC/Auxiliary/Build/vcvars64.bat'
    $compilerEnvironment = & $env:ComSpec /d /s /c ('call "' + $vcvars + '" >nul && set')
    if ($LASTEXITCODE -ne 0) { throw 'Could not initialize the Visual C++ build environment.' }
    foreach ($entry in $compilerEnvironment) {
        if ($entry -match '^(PATH|INCLUDE|LIB|LIBPATH|VCToolsInstallDir|WindowsSdkDir|WindowsSDKVersion)=(.*)$') {
            [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process')
        }
    }
}
$ninjaCommand = Get-Command ninja -ErrorAction SilentlyContinue
$ninja = if ($ninjaCommand) { $ninjaCommand.Source } elseif ($visualStudio) {
    Join-Path $visualStudio 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
} else { throw 'Ninja is required by this Windows build helper.' }
if (-not (Test-Path -LiteralPath $ninja)) { throw "Ninja was not found at $ninja" }

& $cmake -S $projectRoot -B $buildRoot -G Ninja "-DCMAKE_BUILD_TYPE=$Configuration" "-DCMAKE_MAKE_PROGRAM=$ninja"
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
& $cmake --build $buildRoot --parallel
if ($LASTEXITCODE -ne 0) { throw 'Native build failed.' }
& $ctest --test-dir $buildRoot --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Native tests failed.' }

if ($VerifyParity) {
    $oldNativeExecutable = $env:SLANG_NATIVE_EXE
    try {
        $env:SLANG_NATIVE_EXE = Join-Path $buildRoot 'slang.exe'
        Push-Location $projectRoot
        try {
            & node --test test/language.test.js
            if ($LASTEXITCODE -ne 0) { throw 'JavaScript reference comparison failed.' }
            & node scripts/verify-arithmetic.mjs $env:SLANG_NATIVE_EXE
            if ($LASTEXITCODE -ne 0) { throw 'Independent arithmetic verification failed.' }
        } finally { Pop-Location }
    } finally { $env:SLANG_NATIVE_EXE = $oldNativeExecutable }
}
if ($Package) {
    & $cpack --config (Join-Path $buildRoot 'CPackConfig.cmake') -C $Configuration -B (Join-Path $projectRoot 'dist')
    if ($LASTEXITCODE -ne 0) { throw 'Packaging failed.' }
}
Write-Output "Native interpreter ready: $(Join-Path $buildRoot 'slang.exe')"
