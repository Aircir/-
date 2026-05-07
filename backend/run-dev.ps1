param(
    [int]$Port = 8080
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$binDir = Join-Path $scriptDir "bin"
$exePath = Join-Path $binDir "competition_server.exe"
$envFilePath = Join-Path $scriptDir ".env.local"
$mysqlRoot = if ($env:MYSQL_ROOT) { $env:MYSQL_ROOT } else { "D:\MySQL\MySQL\MySQL Server 8.0" }
$mysqlIncludeDir = Join-Path $mysqlRoot "include"
$mysqlLibDir = Join-Path $mysqlRoot "lib"

if (-not (Test-Path $binDir)) {
    New-Item -ItemType Directory -Path $binDir | Out-Null
}

if (Test-Path $envFilePath) {
    Get-Content $envFilePath | ForEach-Object {
        $line = $_.Trim()
        if (-not $line -or $line.StartsWith("#")) {
            return
        }

        $separatorIndex = $line.IndexOf("=")
        if ($separatorIndex -lt 1) {
            return
        }

        $name = $line.Substring(0, $separatorIndex).Trim()
        $value = $line.Substring($separatorIndex + 1).Trim()

        if ($name) {
            [Environment]::SetEnvironmentVariable($name, $value, "Process")
        }
    }
}

if (-not (Test-Path $mysqlIncludeDir) -or -not (Test-Path $mysqlLibDir)) {
    throw "MySQL development libraries were not found. Please check MYSQL_ROOT or the default path [$mysqlRoot]."
}

Write-Host "Compiling backend server..."
$env:PATH = "$mysqlLibDir;$env:PATH"
& g++ -std=c++17 -Wall -Wextra -pedantic -O2 `
    (Join-Path $scriptDir "src\\auth_service.cpp") `
    (Join-Path $scriptDir "src\\activity_service.cpp") `
    (Join-Path $scriptDir "src\\main.cpp") `
    (Join-Path $scriptDir "src\\http_server.cpp") `
    (Join-Path $scriptDir "src\\competition_repository.cpp") `
    (Join-Path $scriptDir "src\\mysql_client.cpp") `
    (Join-Path $scriptDir "src\\consultation_service.cpp") `
    -I $mysqlIncludeDir `
    -L $mysqlLibDir `
    -o $exePath `
    -lmysql `
    -lws2_32 `
    -lwinhttp

if (-not $env:OPENAI_API_KEY) {
    Write-Warning "OPENAI_API_KEY not found. AI consultation will not work."
} else {
    $modelName = if ($env:OPENAI_MODEL) { $env:OPENAI_MODEL } else { "gpt-4.1" }
    Write-Host "OpenAI model: $modelName"
}

Write-Host "Starting backend server on http://127.0.0.1:$Port"
& $exePath $Port
