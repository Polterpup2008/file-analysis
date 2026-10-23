param([string]$BuildDirectory = 'build-v04', [switch]$Package)
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
Push-Location -LiteralPath $taskRoot
try {
    $taskCmake = Get-Command cmake -ErrorAction SilentlyContinue
    if ($taskCmake) { $taskCmakePath = $taskCmake.Source }
    else {
        $taskVswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
        $taskVs = & $taskVswhere -latest -products '*' -property installationPath
        $taskCmakePath = Join-Path $taskVs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
        if (!(Test-Path -LiteralPath $taskCmakePath)) { throw 'Install CMake and the Visual Studio C++ build tools.' }
    }
    $taskCtestPath = Join-Path (Split-Path -Parent $taskCmakePath) 'ctest.exe'
    foreach ($taskDependency in @('tools\exiftool\exiftool.exe','tools\c2patool\c2patool.exe','tests\fixtures\c2pa-signed.jpg','vendor\json.hpp')) {
        if (!(Test-Path -LiteralPath $taskDependency)) { throw "Missing $taskDependency. Run scripts/bootstrap-dependencies.ps1 once with network access." }
    }
    & python tests/make_fixtures.py
    if ($LASTEXITCODE) { throw 'Fixture generation failed.' }
    & $taskCmakePath -S . -B $BuildDirectory -G 'Visual Studio 17 2022' -A x64
    if ($LASTEXITCODE) { throw 'CMake configuration failed.' }
    & $taskCmakePath --build $BuildDirectory --config Release -j 4
    if ($LASTEXITCODE) { throw 'Build failed.' }
    & $taskCtestPath --test-dir $BuildDirectory -C Release --output-on-failure
    if ($LASTEXITCODE) { throw 'Tests failed.' }
    if ($Package) {
        $taskVersionMatch = [regex]::Match((Get-Content -LiteralPath 'CMakeLists.txt' -Raw), 'project\(FileAnalysis VERSION ([0-9]+\.[0-9]+\.[0-9]+) ')
        if (!$taskVersionMatch.Success) { throw 'Cannot find the application version in CMakeLists.txt.' }
        $taskVersion = $taskVersionMatch.Groups[1].Value
        & $taskCmakePath --install $BuildDirectory --config Release --prefix "dist/FileAnalysis-$taskVersion"
        if ($LASTEXITCODE) { throw 'Portable installation failed.' }
    }
} finally { Pop-Location }
