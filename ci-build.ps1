<#
.Description
Builds the cbl-log tool for Windows only on Couchbase build servers (It will technically work for Mac and Linux if the directory
separator characters are flipped from backslash to slash)

.Parameter Branch
The branch of LiteCore to use when building.  If not specified, use the submodule commit (default)
#>
param(
    [string]$Branch
)

Push-Location $PSScriptRoot

if($Branch) {
    .\build.ps1 -Branch $Branch -Config RelWithDebInfo -Product cbl-log
    .\build.ps1 -Branch $Branch -Config RelWithDebInfo -Product cbl-logtest
} else {
    .\build.ps1 -Config RelWithDebInfo -Product cbl-log
    .\build.ps1 -Config RelWithDebInfo -Product cbl-logtest
}

Pop-Location