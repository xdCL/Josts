# Josts — desarrollado por xdCL.
# Construye y verifica la entrega de un solo archivo para Windows 10 x86/x64.
param(
    [string]$ToolchainRoot = (Join-Path $PSScriptRoot 'tools\llvm-mingw-20260908-ucrt-x86_64'),
    [string]$CMakePath,
    [string]$NinjaPath
)
$ErrorActionPreference = 'Stop'

function Find-BuildTool([string]$Requested, [string]$Name, [string]$Fallback) {
    if ($Requested) { return (Resolve-Path -LiteralPath $Requested).Path }
    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    if (Test-Path -LiteralPath $Fallback -PathType Leaf) { return $Fallback }
    throw "No se encontro $Name. Indique su ruta al ejecutar este script."
}

$CMakePath = Find-BuildTool $CMakePath 'cmake.exe' 'C:\msys64\ucrt64\bin\cmake.exe'
$NinjaPath = Find-BuildTool $NinjaPath 'ninja.exe' 'C:\msys64\ucrt64\bin\ninja.exe'
$compilerBin = Join-Path (Resolve-Path -LiteralPath $ToolchainRoot).Path 'bin'
$compiler = Join-Path $compilerBin 'i686-w64-mingw32-clang++.exe'
$resourceCompiler = Join-Path $compilerBin 'i686-w64-mingw32-windres.exe'
$inspector = Join-Path $compilerBin 'llvm-readobj.exe'
$ctest = Join-Path (Split-Path -Parent $CMakePath) 'ctest.exe'
foreach ($required in @($compiler, $resourceCompiler, $inspector, $ctest)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Falta $required" }
}

$buildDir = Join-Path $PSScriptRoot 'build'
$outputDir = Join-Path $PSScriptRoot 'dist'
$savedPath = $env:PATH
try {
    $env:PATH = $compilerBin + ';' + (Split-Path -Parent $CMakePath) + ';' + $savedPath
    # --fresh descarta configuraciones x64 anteriores sin borrar los datos del usuario.
    & $CMakePath --fresh -S $PSScriptRoot -B $buildDir -G Ninja `
        '-DCMAKE_BUILD_TYPE=Release' "-DCMAKE_MAKE_PROGRAM=$($NinjaPath.Replace('\','/'))" `
        "-DCMAKE_CXX_COMPILER=$($compiler.Replace('\','/'))" "-DCMAKE_RC_COMPILER=$($resourceCompiler.Replace('\','/'))" `
        '-DJOSTS_REQUIRE_X86=ON' '-DJOSTS_BUILD_TESTS=ON'
    if ($LASTEXITCODE -ne 0) { throw 'No se pudo configurar la compilacion x86 (requiere CMake 3.24 o posterior).' }
    & $CMakePath --build $buildDir --parallel 4
    if ($LASTEXITCODE -ne 0) { throw 'La compilacion fallo.' }
    & $ctest --test-dir $buildDir --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Las pruebas fallaron; no se actualizo la entrega.' }

    $exe = Join-Path $buildDir 'Josts.exe'
    $inspection = (& $inspector --file-headers --coff-imports $exe | Out-String)
    if ($LASTEXITCODE -ne 0) { throw 'No se pudo inspeccionar el ejecutable.' }
    if ($inspection -notmatch 'Machine: IMAGE_FILE_MACHINE_I386' -or
        $inspection -notmatch 'AddressSize: 32bit' -or
        $inspection -notmatch 'CLRRuntimeHeaderSize: 0x0' -or
        $inspection -notmatch 'DelayImportDescriptorSize: 0x0') {
        throw 'El ejecutable no cumple el formato nativo x86 esperado.'
    }
    $imports = @([regex]::Matches($inspection, '(?m)^\s+Name: (\S+\.dll)\s*$', 'IgnoreCase') |
        ForEach-Object { $_.Groups[1].Value.ToLowerInvariant() } | Sort-Object -Unique)
    if ($imports.Count -eq 0) { throw 'No se pudieron leer las dependencias.' }
    $allowedSystemDll = '^(bcrypt|ole32|advapi32|comctl32|comdlg32|gdi32|kernel32|shell32|user32|ws2_32|api-ms-win-crt-[a-z0-9-]+)\.dll$'
    $unexpected = @($imports | Where-Object { $_ -notmatch $allowedSystemDll })
    if ($unexpected.Count) { throw ('Hay dependencias externas sin verificar: ' + ($unexpected -join ', ')) }
    if ((Get-Item -LiteralPath $exe).Length -gt 4000000) { throw 'El ejecutable supera los 4 MB.' }

    # Sin hosts.txt, fuentes ni DLL junto a la copia; PATH contiene solo Windows.
    $checkDir = Join-Path $buildDir ('portable-check-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $checkDir | Out-Null
    Copy-Item -LiteralPath $exe -Destination $checkDir
    $testExe = Join-Path $checkDir 'josts_core_tests.exe'
    Copy-Item -LiteralPath (Join-Path $buildDir 'josts_core_tests.exe') -Destination $testExe
    $env:PATH = (Join-Path $env:SystemRoot 'System32') + ';' + $env:SystemRoot
    & $testExe
    if ($LASTEXITCODE -ne 0) { throw 'Fallo la prueba de recursos incorporados y runtime sin archivos auxiliares.' }

    New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
    $deliveredExe = Join-Path $outputDir 'Josts.exe'
    Copy-Item -LiteralPath $exe -Destination $deliveredExe -Force
    $report = [ordered]@{
        verifiedAt = (Get-Date).ToString('o')
        architecture = 'x86'
        target = 'Windows 10 x86 / x64'
        bytes = (Get-Item -LiteralPath $deliveredExe).Length
        sha256 = (Get-FileHash -LiteralPath $deliveredExe -Algorithm SHA256).Hash.ToLowerInvariant()
        imports = $imports
        coreTestsPassed = $true
        uiTestsPassed = $true
        edgePolicyTestsPassed = $true
        privilegedBrokerTestsPassed = $true
        executionLevel = 'asInvoker'
        version = '1.1.1'
        embeddedResourcesWithoutSidecarsPassed = $true
        hostIs64Bit = [Environment]::Is64BitOperatingSystem
        limitation = 'La prueba local no certifica todas las compilaciones de Windows 10 ni sustituye una prueba en Windows 10 x86 real.'
    }
    $report | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $buildDir 'portabilidad.json') -Encoding UTF8
    Write-Output "Entrega lista: $deliveredExe ($($report.bytes) bytes)."
} finally {
    $env:PATH = $savedPath
}
