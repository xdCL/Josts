# Josts - xdCL. Genera rutas absolutas para Windows Sandbox desde cualquier carpeta.
param([switch]$PrepareOnly)
$ErrorActionPreference = 'Stop'

try {
    $app = Join-Path $PSScriptRoot 'Josts.exe'
    if (-not (Test-Path -LiteralPath $app -PathType Leaf)) {
        $app = Join-Path (Split-Path -Parent $PSScriptRoot) 'dist\Josts.exe'
    }
    if (-not (Test-Path -LiteralPath $app -PathType Leaf)) {
        throw 'Falta Josts.exe. Extraiga todo el ZIP antes de ejecutar Probar_Josts.cmd.'
    }
    $appFolder = Split-Path -Parent (Resolve-Path -LiteralPath $app).Path
    $escapedApp = [System.Security.SecurityElement]::Escape($appFolder)
    $escapedScripts = [System.Security.SecurityElement]::Escape($PSScriptRoot)
    $configuration = @"
<Configuration>
  <Networking>Enable</Networking>
  <MappedFolders>
    <MappedFolder>
      <HostFolder>$escapedApp</HostFolder>
      <SandboxFolder>C:\JostsApp</SandboxFolder>
      <ReadOnly>true</ReadOnly>
    </MappedFolder>
    <MappedFolder>
      <HostFolder>$escapedScripts</HostFolder>
      <SandboxFolder>C:\JostsSandbox</SandboxFolder>
      <ReadOnly>true</ReadOnly>
    </MappedFolder>
  </MappedFolders>
  <LogonCommand>
    <Command>powershell.exe -NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -File C:\JostsSandbox\iniciar.ps1</Command>
  </LogonCommand>
</Configuration>
"@
    $configurationPath = Join-Path $PSScriptRoot 'Josts_Sandbox.wsb'
    [IO.File]::WriteAllText($configurationPath, $configuration, [Text.Encoding]::UTF8)
    if ($PrepareOnly) { Write-Output $configurationPath; return }
    $sandboxApp = Join-Path $env:WINDIR 'System32\WindowsSandbox.exe'
    if (-not (Test-Path -LiteralPath $sandboxApp -PathType Leaf)) {
        throw 'Windows Sandbox no esta disponible. Active Windows Sandbox en las caracteristicas de Windows y reinicie si se solicita.'
    }
    # Esta es la ventana interactiva solicitada para probar Josts.
    Start-Process -FilePath $sandboxApp -ArgumentList ('"' + $configurationPath + '"')
} catch {
    if ($PrepareOnly) { throw }
    Add-Type -AssemblyName System.Windows.Forms
    [System.Windows.Forms.MessageBox]::Show($_.Exception.Message, 'Prueba de Josts') | Out-Null
    exit 1
}
