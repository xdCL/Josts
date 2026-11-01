$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$zipPath = Join-Path $root 'DM_Sans.zip'
$target = Join-Path $root 'res\DM_Sans'
New-Item -ItemType Directory -Force -Path $target | Out-Null
$zip = [IO.Compression.ZipFile]::OpenRead($zipPath)
try {
    foreach ($style in @('Regular','Medium','Bold','Italic')) {
        $name = "DMSans-$style.ttf"
        $entry = $zip.GetEntry("static/$name")
        if (-not $entry) { throw "Falta $name en DM_Sans.zip" }
        [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, (Join-Path $target $name), $true)
    }
    $license = $zip.GetEntry('OFL.txt')
    if (-not $license) { throw 'Falta OFL.txt en DM_Sans.zip' }
    [IO.Compression.ZipFileExtensions]::ExtractToFile($license, (Join-Path $root 'LICENSE_DM_Sans.txt'), $true)
} finally { $zip.Dispose() }
Write-Host 'DM Sans y su licencia están listos para compilar.'
