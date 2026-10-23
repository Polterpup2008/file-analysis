param([string]$BuildDirectory = 'build', [string]$OutputDirectory = 'dist')
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
Push-Location -LiteralPath $taskRoot
try {
    if (git status --porcelain --untracked-files=normal) { throw 'Commit source changes before packaging so the source ZIP matches the build.' }
    $taskCommit = (git rev-parse HEAD).Trim()
    if ($LASTEXITCODE) { throw 'Packaging requires a Git source checkout.' }
    $taskVersion = [regex]::Match((Get-Content CMakeLists.txt -Raw),'project\(FileAnalysis VERSION ([0-9]+\.[0-9]+\.[0-9]+) ').Groups[1].Value
    if (!$taskVersion) { throw 'Application version missing.' }
    if ($env:GITHUB_REF_TYPE -eq 'tag' -and $env:GITHUB_REF_NAME -ne "v$taskVersion") { throw 'Release tag does not match the source version.' }
    $taskOutput = [IO.Path]::GetFullPath((Join-Path $taskRoot $OutputDirectory))
    [IO.Directory]::CreateDirectory($taskOutput) | Out-Null
    $taskBinary = Join-Path $taskOutput "FileAnalysis-$taskVersion-Windows-x64.zip"
    $taskSource = Join-Path $taskOutput "FileAnalysis-$taskVersion-Source.zip"
    $taskChecksums = Join-Path $taskOutput "SHA256-$taskVersion.txt"
    foreach ($taskDestination in @($taskBinary,$taskSource,$taskChecksums)) {
        if (Test-Path -LiteralPath $taskDestination) { throw "Release destination already exists: $taskDestination. Choose a fresh output directory." }
    }
    & (Join-Path $PSScriptRoot 'build.ps1') -BuildDirectory $BuildDirectory -Package
    if ($LASTEXITCODE) { throw 'Build or tests failed.' }
    # Fresh staging prevents obsolete files from an earlier install entering a release.
    $taskStage = Join-Path $taskRoot ('dist/package-' + [guid]::NewGuid().ToString('N'))
    [IO.Directory]::CreateDirectory($taskStage) | Out-Null
    $taskCmake = Get-Command cmake -ErrorAction SilentlyContinue
    if ($taskCmake) { $taskCmakePath = $taskCmake.Source }
    else {
        $taskVswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
        $taskVs = & $taskVswhere -latest -products '*' -property installationPath
        $taskCmakePath = Join-Path $taskVs 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
    }
    & $taskCmakePath --install $BuildDirectory --config Release --prefix $taskStage
    if ($LASTEXITCODE) { throw 'Fresh staging failed.' }
    $taskBuildInfo = @("File Analysis $taskVersion", "Source commit: $taskCommit", 'Target: Windows x64; MSVC C++20; unsigned', 'See README.md and THIRD_PARTY.md for use and dependencies.') -join "`r`n"
    [IO.File]::WriteAllText((Join-Path $taskStage 'BUILD.txt'),$taskBuildInfo,[Text.UTF8Encoding]::new($false))
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [IO.Compression.ZipFile]::CreateFromDirectory($taskStage,$taskBinary,[IO.Compression.CompressionLevel]::Optimal,$false)
    git archive --format=zip "--prefix=FileAnalysis-$taskVersion-Source/" "--output=$taskSource" HEAD
    if ($LASTEXITCODE) { throw 'Source archive failed.' }
    $taskLines = foreach ($taskFile in @($taskBinary,$taskSource)) { (Get-FileHash -LiteralPath $taskFile -Algorithm SHA256).Hash.ToLowerInvariant()+'  '+[IO.Path]::GetFileName($taskFile) }
    [IO.File]::WriteAllText($taskChecksums,($taskLines -join "`n")+"`n",[Text.UTF8Encoding]::new($false))
    Write-Output "Release assets are in $taskOutput (source $taskCommit)."
    Write-Output 'Rebuilding with another MSVC/SDK/runtime can produce different binaries; this is a repeatable build pipeline, not a bit-for-bit reproducibility claim.'
} finally { Pop-Location }
