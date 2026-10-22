param(
    [Parameter(Mandatory=$true)][string]$Exe,
    [string]$OutDir = '.'
)
$ErrorActionPreference = 'Stop'
$exePath = (Resolve-Path $Exe).Path
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) { throw 'vswhere.exe not found' }
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'MSVC toolset not found' }
$toolsets = Get-ChildItem (Join-Path $vs 'VC\Tools\MSVC') -Directory | Sort-Object Name -Descending
if (-not $toolsets) { throw 'No MSVC v143 toolchain' }
$toolset = $toolsets[0]
$dumpbin = Join-Path $toolset.FullName 'bin\Hostx64\x64\dumpbin.exe'
$cl = Join-Path $toolset.FullName 'bin\Hostx64\x64\cl.exe'
if (-not (Test-Path $dumpbin)) { throw 'dumpbin.exe not found' }
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$imports = & $dumpbin /dependents $exePath 2>&1 | Out-String
$headers = & $dumpbin /headers $exePath 2>&1 | Out-String
$sections = & $dumpbin /summary $exePath 2>&1 | Out-String
$loadconfig = & $dumpbin /loadconfig $exePath 2>&1 | Out-String
$compiler = (Get-Item $cl).VersionInfo.FileVersion
$metadata = Get-Item $exePath
$hash = (Get-FileHash $exePath -Algorithm SHA256).Hash
$report = @(
    'CopyFix.exe',
    "Size: $($metadata.Length) bytes",
    "SHA256: $hash",
    "MSVC: $($toolset.Name)",
    "Compiler file version: $compiler"
) -join [Environment]::NewLine
$report | Tee-Object -FilePath (Join-Path $OutDir 'pe-summary.txt')
$imports | Set-Content (Join-Path $OutDir 'pe-dependents.txt')
$headers | Set-Content (Join-Path $OutDir 'pe-headers.txt')
$sections | Set-Content (Join-Path $OutDir 'pe-sections.txt')
$loadconfig | Set-Content (Join-Path $OutDir 'pe-loadconfig.txt')
Write-Host 'Relevant PE header flags:'
$headers -split '[\r\n]+' | Where-Object { $_ -match '(?i)DLL characteristics|dynamic base|NX compatible|guard|control flow' } | ForEach-Object { Write-Host $_ }
Write-Host 'Relevant PE load configuration:'
$loadconfig -split '[\r\n]+' | Where-Object { $_ -match '(?i)guard|cf function|load config' } | Select-Object -First 24 | ForEach-Object { Write-Host $_ }
$bad = @('VCRUNTIME140.dll','VCRUNTIME140_1.dll','MSVCP140.dll','ucrtbase.dll',
         'hostfxr.dll','hostpolicy.dll','coreclr.dll')
foreach ($name in $bad) {
    if ($imports -match [regex]::Escape($name)) { throw "Unexpected dynamic runtime: $name" }
}
# The hex/flags line in DUMPBIN headers contains these exact named flags.
if ($headers -notmatch '(?i)Dynamic base') { throw 'PE ASLR flag missing' }
if ($headers -notmatch '(?i)NX compatible') { throw 'PE NX flag missing' }
if ($headers -notmatch '(?i)Guard CF') { throw 'PE Control Flow Guard flag missing' }
Write-Host 'PE runtime import and hardening checks: PASS'
