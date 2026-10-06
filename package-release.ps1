param(
    [string]$Configuration = "Release",
    [string]$OutputDir = "",
    [string]$PackageVersion = "",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

$repoRoot = $PSScriptRoot
$packageName = "SH3-NoSlurper"
$buildRoot = Join-Path $repoRoot "build\package"
$cmakeBuildDir = Join-Path $buildRoot "cmake"
$stageRoot = Join-Path $buildRoot "stage"
$scriptsDir = Join-Path $stageRoot "scripts"

if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $repoRoot "dist"
}

function New-CleanDirectory([string]$Path) {
    if (Test-Path -LiteralPath $Path) {
        $resolved = (Resolve-Path -LiteralPath $Path).Path
        $root = (Resolve-Path -LiteralPath $repoRoot).Path
        if (-not $resolved.StartsWith($root, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to clean outside repository: $resolved"
        }
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
    New-Item -ItemType Directory -Path $Path -Force | Out-Null
}

function Copy-RequiredFile([string]$Source, [string]$Destination) {
    if (-not (Test-Path -LiteralPath $Source)) {
        throw "Missing required file: $Source"
    }
    New-Item -ItemType Directory -Path (Split-Path $Destination -Parent) -Force | Out-Null
    Copy-Item -LiteralPath $Source -Destination $Destination -Force
}

if (-not $SkipBuild) {
    New-CleanDirectory $cmakeBuildDir

    & cmake -S $repoRoot -B $cmakeBuildDir -G "Visual Studio 17 2022" -A Win32
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configure failed."
    }

    & cmake --build $cmakeBuildDir --config $Configuration
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed."
    }

    & ctest --test-dir $cmakeBuildDir -C $Configuration --output-on-failure
    if ($LASTEXITCODE -ne 0) {
        throw "Tests failed."
    }
}

$asiPath = Join-Path $cmakeBuildDir "$Configuration\NoSlurper.asi"
if ($SkipBuild -and -not (Test-Path -LiteralPath $asiPath)) {
    $fallback = Join-Path $repoRoot "build\$Configuration\NoSlurper.asi"
    if (Test-Path -LiteralPath $fallback) {
        $asiPath = $fallback
    }
}

New-CleanDirectory $stageRoot
New-Item -ItemType Directory -Path $scriptsDir -Force | Out-Null
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

Copy-RequiredFile $asiPath (Join-Path $scriptsDir "NoSlurper.asi")
Copy-RequiredFile (Join-Path $repoRoot "packaging\README.txt") (Join-Path $stageRoot "README.txt")

$stamp = if ([string]::IsNullOrWhiteSpace($PackageVersion)) {
    Get-Date -Format "yyyyMMdd-HHmmss"
} else {
    $PackageVersion
}

$zipPath = Join-Path $OutputDir "$packageName-$stamp.zip"
if (Test-Path -LiteralPath $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}

Compress-Archive -Path (Join-Path $stageRoot "*") -DestinationPath $zipPath -Force

if ($env:GITHUB_OUTPUT) {
    "zip_path=$zipPath" | Add-Content -Path $env:GITHUB_OUTPUT -Encoding UTF8
}

Write-Host "PACKAGE_OK $zipPath"
