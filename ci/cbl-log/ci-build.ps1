<#
.Description
Builds the cbl-log tool for Windows only on Couchbase build servers (It will technically work for Mac and Linux if the directory
separator characters are flipped from backslash to slash)
#>
Push-Location $PSScriptRoot

.\build.ps1 -Config RelWithDebInfo -Product cbl-log -NoSubmodule
.\build.ps1 -Config RelWithDebInfo -Product cbl-logtest -NoSubmodule -Output ci\cbl-log\build

Pop-Location
