$ErrorActionPreference = 'Stop'

$source = 'C:\JostsApp'
$destination = Join-Path ([Environment]::GetFolderPath('Desktop')) 'Josts-Prueba'

try {
    New-Item -ItemType Directory -Path $destination -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $source 'Josts.exe') -Destination $destination -Force
    # Solo el ejecutable: la prueba utiliza la precarga incorporada.
    Start-Process -FilePath (Join-Path $destination 'Josts.exe') -WorkingDirectory $destination
} catch {
    Add-Type -AssemblyName System.Windows.Forms
    [System.Windows.Forms.MessageBox]::Show(
        "No se pudo preparar Josts en Windows Sandbox: $($_.Exception.Message)",
        'Prueba de Josts'
    ) | Out-Null
}
