$ErrorActionPreference = "Stop"

$cmake = (Get-Command cmake -ErrorAction SilentlyContinue | Select-Object -First 1).Source
if (-not $cmake) {
    throw "未找到 cmake。请安装 CMake，或把它加入 PATH。"
}

$source = Split-Path -Parent $MyInvocation.MyCommand.Path
$build = Join-Path $source "build"
$juce = $env:JUCE_PATH

if (-not $juce -and (Test-Path "D:\VSTDev\SDKs\JUCE\CMakeLists.txt")) {
    $juce = "D:\VSTDev\SDKs\JUCE"
}

$configure = @("-S", $source, "-B", $build, "-G", "Visual Studio 17 2022", "-A", "x64")
if ($juce) {
    $configure += @("-DJUCE_PATH=$juce")
}

& $cmake @configure
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $cmake --build $build --config Release
exit $LASTEXITCODE
