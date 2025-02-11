param (
    [Parameter(Mandatory=$true)]
    [string]
    $Version,
    [Parameter(Mandatory=$true)]
    [Int32]
    $Build
)

& dotnet add package Couchbase.Lite.Enterprise --version $Version
& dotnet run -- $Version-b$($Build.ToString().PadLeft(4, '0'))
