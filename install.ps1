param(
    [string]$Destination = ""
)

$ErrorActionPreference = "Stop"

if (-not $Destination) {
    $Destination = Join-Path $env:ProgramFiles "Common Files\VST3\Iridescent Flower.vst3"
}

$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)

if (-not $isAdmin -and $Destination.StartsWith($env:ProgramFiles, [StringComparison]::OrdinalIgnoreCase)) {
    throw "安装到 Program Files 需要管理员权限。请以管理员身份运行 PowerShell，或使用 -Destination 指定其他 VST3 目录。"
}

$project = Split-Path -Parent $MyInvocation.MyCommand.Path
$candidates = @(
    (Join-Path $project "dist\Iridescent Flower.vst3"),
    (Join-Path $project "build\IridescentFlower_artefacts\Release\VST3\Iridescent Flower.vst3")
)

$source = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1

if (-not $source)
{
    throw "找不到 Iridescent Flower.vst3，请先运行 .\build.ps1"
}

$destinationParent = Split-Path -Parent $Destination
if (-not (Test-Path -LiteralPath $destinationParent))
{
    throw "目标目录不存在：$destinationParent"
}

$oldLockedCopy = Join-Path $destinationParent "Iridescent Flower (old).vst3"
if (Test-Path -LiteralPath $oldLockedCopy)
{
    Remove-Item -LiteralPath $oldLockedCopy -Recurse -Force -ErrorAction SilentlyContinue
}

if (Test-Path -LiteralPath $Destination)
{
    Remove-Item -LiteralPath $Destination -Recurse -Force
}

Copy-Item -LiteralPath $source -Destination $Destination -Recurse -Force

$binary = Join-Path $Destination "Contents\x86_64-win\Iridescent Flower.vst3"
if (-not (Test-Path -LiteralPath $binary))
{
    throw "安装后校验失败：$binary 不存在"
}

$customSource = Join-Path $project "dist\Custom Modules"
$customDestination = Join-Path ([Environment]::GetFolderPath("MyDocuments")) "Iridescent Flower\Custom Modules"

if (Test-Path -LiteralPath $customSource)
{
    New-Item -ItemType Directory -Force -Path $customDestination | Out-Null
    Copy-Item -LiteralPath (Join-Path $customSource "*") -Destination $customDestination -Recurse -Force
    Write-Host "已安装示例自定义模块：$customDestination"
}

Write-Host "已安装：$Destination"
Write-Host "请在宿主中重新扫描 VST3 插件目录。"
