param([switch]$DependenciesOnly,[string]$ToolRoot,[string]$DependencyInstall)
$ErrorActionPreference = 'Stop'
$projectDir = Split-Path $PSScriptRoot -Parent
$sharedProject = Join-Path (Split-Path $projectDir -Parent) 'Pickpocketing-Loot'
if (-not $ToolRoot) { $ToolRoot = Join-Path $projectDir 'tools'; if (-not (Test-Path (Join-Path $ToolRoot 'msvc/VC/Tools/MSVC'))) { $ToolRoot = Join-Path $sharedProject 'tools' } }
$toolDir = $ToolRoot
if (-not $DependencyInstall) { $DependencyInstall = Join-Path $projectDir 'vcpkg_installed'; if (-not $DependenciesOnly -and -not (Test-Path $DependencyInstall)) { $DependencyInstall = Join-Path $sharedProject 'vcpkg_installed' } }
$compilerDir = (Get-ChildItem (Join-Path $toolDir 'msvc/VC/Tools/MSVC') -Directory | Sort-Object Name -Descending | Select-Object -First 1).FullName
$sdkDir = Join-Path $toolDir 'msvc/Windows Kits/10'
$sdkVersion = (Get-ChildItem (Join-Path $sdkDir 'Include') -Directory | Sort-Object Name -Descending | Select-Object -First 1).Name
$cmakeDir = Join-Path $toolDir 'python-packages/cmake/data/bin'
$ninjaDir = Join-Path $toolDir 'ninja'
$env:PATH = "$compilerDir/bin/Hostx64/x64;$sdkDir/bin/$sdkVersion/x64;$cmakeDir;$ninjaDir;$env:PATH"
$env:INCLUDE = "$compilerDir/include;$sdkDir/Include/$sdkVersion/ucrt;$sdkDir/Include/$sdkVersion/shared;$sdkDir/Include/$sdkVersion/um;$sdkDir/Include/$sdkVersion/winrt;$sdkDir/Include/$sdkVersion/cppwinrt"
$env:LIB = "$compilerDir/lib/x64;$sdkDir/Lib/$sdkVersion/ucrt/x64;$sdkDir/Lib/$sdkVersion/um/x64"
$env:VCPKG_FORCE_SYSTEM_BINARIES = '1'
$env:VCPKG_ROOT = Join-Path $toolDir 'vcpkg'
$env:VCPKG_DOWNLOADS = Join-Path $toolDir 'vcpkg-downloads'
$env:VCPKG_DEFAULT_BINARY_CACHE = Join-Path $toolDir 'vcpkg-cache'
New-Item -ItemType Directory -Force $env:VCPKG_DEFAULT_BINARY_CACHE | Out-Null
if ($DependenciesOnly) {
    & "$env:VCPKG_ROOT/vcpkg.exe" install "--x-manifest-root=$projectDir" "--x-install-root=$DependencyInstall" "--overlay-triplets=$projectDir/cmake/triplets" --triplet=x64-portable --host-triplet=x64-portable --disable-metrics --allow-unsupported
    if ($LASTEXITCODE -ne 0) { throw 'Dependency installation failed' }
    exit
}
& "$cmakeDir/cmake.exe" -S $projectDir -B "$projectDir/build" -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" "-DVCPKG_OVERLAY_TRIPLETS=$projectDir/cmake/triplets" "-DVCPKG_INSTALLED_DIR=$DependencyInstall" -DVCPKG_TARGET_TRIPLET=x64-portable -DVCPKG_HOST_TRIPLET=x64-portable -DVCPKG_MANIFEST_INSTALL=OFF
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
& "$cmakeDir/cmake.exe" --build "$projectDir/build" --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
& "$cmakeDir/ctest.exe" --test-dir "$projectDir/build" --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
