<#
.Description
Builds the cbl-log tool for Windows only on Couchbase build servers (It will technically work for Mac and Linux if the directory
separator characters are flipped from backslash to slash)
#>
$CMakePath = "C:\Program Files\CMake\bin\cmake.exe"

New-Item -ItemType Directory -Path "$PSScriptRoot\build" -ErrorAction Ignore
Push-Location "$PSScriptRoot\build"

& "$CMakePath" -G "Visual Studio 14 2015" "$PSScriptRoot\..\..\cbl-log"
& "$CMakePath" --build . --target cbl-log --config RelWithDebInfo
& "$CMakePath" --build . --target cbl-logtest --config RelWithDebInfo

Pop-Location
