<#
.Description
Builds the cblite tool for Windows only (It will technically work for Mac and Linux if the directory
separator characters are flipped from backslash to slash)

.Parameter Branch
The branch of LiteCore to use when building.  If not specified, use the submodule commit (default)

.Parameter Config
The configuration to use when building (Debug (default), Release, MinSizeRel, RelWithDebInfo)

.Parameter Product
The product to build (cblite (default) or cbl-log)

.Parameter NoSubmodule
If enabled, checking out LiteCore will be skipped (useful is using another repo management tool)

.Parameter Output
The output directory to write the build products to (default ci/<product>/build

.Parameter GitPath
The path to the git executable (default: C:\Program Files\Git\bin\git.exe)

.Parameter CMakePath
The path to the cmake executable (default: C:\Program Files\CMake\bin\cmake.exe
#>
param(
    [string]$Branch,
    [string]$Config = "Debug",
    [string]$Product = "cblite",
    [switch]$NoSubmodule,
    [string]$Output = "",
    [string]$GitPath = "C:\Program Files\Git\bin\git.exe",
    [string]$CMakePath = "C:\Program Files\CMake\bin\cmake.exe"
)

Push-Location $PSScriptRoot\..

if(-Not $Output) {
    $Output = "ci/$Product/build"
}

if($NoSubmodule) {
    Write-Host "Skipping submodule checkout..."
} else {
    & "$GitPath" submodule update --init --recursive
}

if($Branch) {
    Push-Location vendor\couchbase-lite-core
    & "$GitPath" reset --hard
    & "$GitPath" checkout $Branch
    & "$GitPath" fetch origin
    & "$GitPath" pull origin $Branch
    & "$GitPath" submodule update --init --recursive
    Pop-Location
}

if(-Not (Test-Path $Output)) {
    New-Item -ItemType Directory -Name $Output
}

if($Product -eq "cblite") {
    $CMakeFlags = "-DBUILD_CBLITE=ON"
} elseif($Product -eq "cbl-log") {
    $CMakeFlags = "-DBUILD_CBL_LOG=ON"
}

Push-Location $Output
& "$CMakePath" $CMakeFlags $PSScriptRoot\..
& "$CMakePath" --build . --target $Product --config $Config
& "$CMakePath" --build . --target ${Product}test --config $Config
Pop-Location
Pop-Location