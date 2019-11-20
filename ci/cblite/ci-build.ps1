<#
.Description
Builds the cbl-log tool for Windows only on Couchbase build servers (It will technically work for Mac and Linux if the directory
separator characters are flipped from backslash to slash)
#>
$CMakePath = "C:\Program Files\CMake\bin\cmake.exe"

New-Item -ItemType Directory -Path "$PSScriptRoot\build" -ErrorAction Ignore
Push-Location "$PSScriptRoot\build"

& "$CMakePath" "$PSScriptRoot\..\..\cblite"
& "$CMakePath" --build . --target cblite --config RelWithDebInfo
& "$CMakePath" --build . --target cblitetest --config RelWithDebInfo

Pop-Location
