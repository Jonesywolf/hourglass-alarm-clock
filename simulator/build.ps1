param(
    [string]$EmsdkPath = $env:EMSDK,
    [string]$CMakePath = "cmake",
    [string]$NinjaPath = "ninja",
    [string]$PythonPath = "$env:USERPROFILE\.platformio\penv\Scripts\python.exe"
)
$ErrorActionPreference = "Stop"
if (-not $EmsdkPath -or -not (Test-Path "$EmsdkPath\upstream\emscripten\emcmake.py")) {
    throw "Pass -EmsdkPath pointing to an activated Emscripten 4.0.10 SDK."
}
if (-not (Test-Path $PythonPath)) { throw "Pass -PythonPath pointing to a working Python." }
$cmake = (Get-Command $CMakePath -ErrorAction Stop).Source
$ninja = (Get-Command $NinjaPath -ErrorAction Stop).Source
$env:EMSDK_PYTHON = $PythonPath
$env:PATH = "$EmsdkPath\upstream\emscripten;$EmsdkPath\upstream\bin;$(Split-Path $PythonPath);$env:PATH"
$build = Join-Path $PSScriptRoot "build"
& $PythonPath "$EmsdkPath\upstream\emscripten\emcmake.py" $cmake `
    -S $PSScriptRoot -B $build -G Ninja "-DCMAKE_MAKE_PROGRAM=$ninja" -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { throw "Browser configure failed ($LASTEXITCODE)." }
& $cmake --build $build --parallel 4
if ($LASTEXITCODE -ne 0) { throw "Browser build failed ($LASTEXITCODE)." }
Write-Output "Browser build ready. Run: node `"$PSScriptRoot\serve.cjs`""
