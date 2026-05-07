param(
    [int]$Port = 8080
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$binDir = Join-Path $scriptDir "bin"
$exePath = Join-Path $binDir "competition_server.exe"

if (-not (Test-Path $binDir)) {
    New-Item -ItemType Directory -Path $binDir | Out-Null
}

Write-Host "Compiling backend server..."
& g++ -std=c++17 -Wall -Wextra -pedantic -O2 `
    (Join-Path $scriptDir "src\\main.cpp") `
    (Join-Path $scriptDir "src\\http_server.cpp") `
    (Join-Path $scriptDir "src\\competition_repository.cpp") `
    -o $exePath `
    -lws2_32

Write-Host "Starting backend server on http://127.0.0.1:$Port"
& $exePath $Port
