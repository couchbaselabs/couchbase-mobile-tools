$env:DOTNET_ROOT="$env:LOCALAPPDATA\Microsoft\dotnet"

function Banner {
    param (
        [Parameter(Mandatory=$true)]
        [string]
        $Text
    )
    Write-Host
    Write-Host -ForegroundColor Green "===== $Text ====="
    Write-Host
}

function Install-DotNet {
    param(
        [Parameter()]
        [string]
        $Version = "8.0"
    )
    if(-Not (Test-Path .\dotnet-install.ps1)) {
        Invoke-WebRequest https://dot.net/v1/dotnet-install.ps1 -OutFile dotnet-install.ps1
    }

    Banner -Text "Installing .NET SDK $Version"
    .\dotnet-install.ps1 -c $Version
}

function Install-DotNetRuntime {
    param(
        [Parameter()]
        [string]
        $Version = "8.0"
    )
    if(-Not (Test-Path .\dotnet-install.ps1)) {
        Invoke-WebRequest https://dot.net/v1/dotnet-install.ps1 -OutFile dotnet-install.ps1
    }

    Banner -Text "Installing .NET Runtime $Version"
    .\dotnet-install.ps1 -c $Version -Runtime dotnet
}

function Install-XHarness {
    Banner -Text "Installing XHarness"
    & $env:DOTNET_ROOT\dotnet tool install --global --add-source https://pkgs.dev.azure.com/dnceng/public/_packaging/dotnet-eng/nuget/v3/index.json Microsoft.DotNet.XHarness.CLI --version "8.0.0-prerelease*"
}

function Install-Maui {
    Banner -Text "Installing MAUI workload"

    & $env:DOTNET_ROOT\dotnet workload install maui
}
