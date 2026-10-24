# Explicit network setup only. Application and normal builds never call this script.
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
$taskLock = Get-Content -LiteralPath (Join-Path $taskRoot 'DEPENDENCIES.json') -Raw | ConvertFrom-Json
Add-Type -AssemblyName System.IO.Compression.FileSystem
function Get-PinnedFile([string]$Url,[string]$Path,[string]$Sha256) {
    if ((Test-Path -LiteralPath $Path) -and (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -eq $Sha256) { return }
    [IO.Directory]::CreateDirectory((Split-Path -Parent $Path)) | Out-Null
    Invoke-WebRequest -Uri $Url -OutFile $Path -UserAgent 'Wget' -UseBasicParsing
    if ((Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -ne $Sha256) { throw "Pinned SHA-256 mismatch: $Url. No parser will be executed. Check the dependency version; do not accept a new digest automatically." }
}
function Copy-ZipEntry([IO.Compression.ZipArchiveEntry]$Entry,[string]$Destination) {
    [IO.Directory]::CreateDirectory((Split-Path -Parent $Destination)) | Out-Null
    $taskInput = $Entry.Open()
    try {
        $taskOutput = [IO.File]::Create($Destination)
        try { $taskInput.CopyTo($taskOutput) } finally { $taskOutput.Dispose() }
    } finally { $taskInput.Dispose() }
}
$taskExifZip = Join-Path $taskRoot 'tools\exiftool.zip'
Get-PinnedFile $taskLock.exiftool.url $taskExifZip $taskLock.exiftool.archive_sha256
$taskArchive = [IO.Compression.ZipFile]::OpenRead($taskExifZip)
try {
    $taskPrefix = 'exiftool-13.59_64/'
    $taskExifRoot = [IO.Path]::GetFullPath((Join-Path $taskRoot 'tools\exiftool'))
    foreach ($taskEntry in $taskArchive.Entries) {
        if (!$taskEntry.FullName.StartsWith($taskPrefix) -or $taskEntry.FullName.EndsWith('/')) { continue }
        $taskRelative = $taskEntry.FullName.Substring($taskPrefix.Length)
        if ($taskRelative -eq 'exiftool(-k).exe') { $taskRelative = 'exiftool.exe' }
        elseif ($taskRelative -ne 'README.txt' -and !$taskRelative.StartsWith('exiftool_files/')) { continue }
        $taskDestination = [IO.Path]::GetFullPath((Join-Path $taskExifRoot $taskRelative))
        if (!$taskDestination.StartsWith($taskExifRoot + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Archive entry escapes the dependency directory.' }
        Copy-ZipEntry $taskEntry $taskDestination
    }
} finally { $taskArchive.Dispose() }
$taskC2paZip = Join-Path $taskRoot 'tools\c2patool.zip'
Get-PinnedFile $taskLock.c2patool.url $taskC2paZip $taskLock.c2patool.archive_sha256
$taskArchive = [IO.Compression.ZipFile]::OpenRead($taskC2paZip)
try {
    $taskEntry = $taskArchive.GetEntry('c2patool/c2patool.exe')
    if (!$taskEntry) { throw 'Expected c2patool executable missing from archive.' }
    Copy-ZipEntry $taskEntry (Join-Path $taskRoot 'tools\c2patool\c2patool.exe')
} finally { $taskArchive.Dispose() }
Get-PinnedFile $taskLock.c2pa_trust_list.url (Join-Path $taskRoot 'tools\c2patool\c2pa-trust-list.pem') $taskLock.c2pa_trust_list.sha256
Get-PinnedFile $taskLock.nlohmann_json.url (Join-Path $taskRoot 'vendor\json.hpp') $taskLock.nlohmann_json.sha256
Get-PinnedFile $taskLock.signed_test_fixture.url (Join-Path $taskRoot 'tests\fixtures\c2pa-signed.jpg') $taskLock.signed_test_fixture.sha256
Get-PinnedFile 'https://raw.githubusercontent.com/nlohmann/json/v3.12.0/LICENSE.MIT' (Join-Path $taskRoot 'vendor\JSON-LICENSE.txt') '46a65cffd1ea955132d95a8dd921640714a8d6b537d2e4e482d31145ae95b603'
Get-PinnedFile 'https://raw.githubusercontent.com/contentauth/c2patool/v0.28.2/LICENSE-MIT' (Join-Path $taskRoot 'tools\c2patool\LICENSE-MIT') '89375a50de90d2dcaa04406086da832ad452ebcaf6ab402ef3d51b8401a67c71'
Get-PinnedFile 'https://raw.githubusercontent.com/contentauth/c2patool/v0.28.2/LICENSE-APACHE' (Join-Path $taskRoot 'tools\c2patool\LICENSE-APACHE') '86bdd5dafab77451044b6fd6d2efab23e3410ce658eb097f04c04f4f54aed62f'
foreach ($taskFormatSample in (Get-Content -LiteralPath (Join-Path $taskRoot 'tests/format-fixtures.json') -Raw | ConvertFrom-Json)) {
    Get-PinnedFile $taskFormatSample.url (Join-Path $taskRoot ('tests/fixtures/'+$taskFormatSample.name)) $taskFormatSample.sha256
}
foreach ($taskDeviceSample in (Get-Content -LiteralPath (Join-Path $taskRoot 'tests/device-fixtures.json') -Raw | ConvertFrom-Json)) {
    Get-PinnedFile $taskDeviceSample.url (Join-Path $taskRoot ('tests/fixtures/'+$taskDeviceSample.name)) $taskDeviceSample.sha256
}
Write-Output 'Pinned dependencies, signed fixture and public metadata test fixtures are ready. Run scripts/build.ps1.'
