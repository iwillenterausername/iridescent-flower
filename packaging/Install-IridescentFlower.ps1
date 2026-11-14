param(
    [string]$VST3Folder = ""
)

$ErrorActionPreference = "Stop"

if (-not $VST3Folder) {
    $VST3Folder = Join-Path $env:ProgramFiles "Common Files\VST3"
}

$root = $PSScriptRoot
$source = Join-Path $root "Iridescent Flower.vst3"

if (-not (Test-Path -LiteralPath $source)) {
    throw "找不到 Iridescent Flower.vst3。请确认压缩包已完整解压，并且脚本与 .vst3 文件夹位于同一目录。"
}

$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)

if (-not $isAdmin -and $VST3Folder.StartsWith($env:ProgramFiles, [StringComparison]::OrdinalIgnoreCase)) {
    throw "安装到 Program Files 需要管理员权限。请右键 PowerShell → 以管理员身份运行，然后执行本脚本；或使用 -VST3Folder 指定其他 VST3 目录。"
}

New-Item -ItemType Directory -Force -Path $VST3Folder | Out-Null
$destination = Join-Path $VST3Folder "Iridescent Flower.vst3"

if (Test-Path -LiteralPath $destination) {
    Remove-Item -LiteralPath $destination -Recurse -Force
}

Copy-Item -LiteralPath $source -Destination $destination -Recurse -Force

$customSource = Join-Path $root "Custom Modules"
$customDestination = Join-Path ([Environment]::GetFolderPath("MyDocuments")) "Iridescent Flower\Custom Modules"

if (Test-Path -LiteralPath $customSource) {
    New-Item -ItemType Directory -Force -Path $customDestination | Out-Null
    Copy-Item -LiteralPath (Join-Path $customSource "*") -Destination $customDestination -Recurse -Force
}

Write-Host ""
Write-Host "Iridescent Flower 已安装到："
Write-Host "  $destination"
Write-Host ""
Write-Host "示例自定义模块已复制到："
Write-Host "  $customDestination"
Write-Host ""
Write-Host "请在宿主中重新扫描 VST3 插件目录，然后加载 Guillaume Audio → Iridescent Flower。"
