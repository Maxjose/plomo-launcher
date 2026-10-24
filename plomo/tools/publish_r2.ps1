# Publishes a PLOMO pack release to the R2 bucket mounted as a drive (RaiDrive).
# Usage: .\tools\publish_r2.ps1 -Zip PLOMO-CLIENTE-0.29.zip -Version 0.29 -Drive X:
param(
    [Parameter(Mandatory)][string]$Zip,
    [Parameter(Mandatory)][string]$Version,
    [Parameter(Mandatory)][ValidatePattern('^[A-Za-z]:$')][string]$Drive
)
$ErrorActionPreference = 'Stop'
$cdn = 'https://cdn.playplomo.com'
$root = Split-Path $PSScriptRoot -Parent
$py = Join-Path $root '.tools\vcpkg\downloads\tools\python\python-3.14.7-x64\python.exe'
$out = Join-Path $root "release-$Version"

if (-not (Test-Path "$Drive\")) { throw "La unidad $Drive no esta montada (RaiDrive)." }

& $py (Join-Path $PSScriptRoot 'build_pack_release.py') $Zip $out --version $Version --base-url "$cdn/files/" --materialize
if ($LASTEXITCODE) { throw 'build_pack_release.py fallo; no se subio nada.' }

# Files first, manifest last, so nobody is told about a version that is not fully uploaded.
# No /MIR on purpose: old files stay in the bucket, the launcher only reads what the manifest lists.
robocopy "$out\files" "$Drive\files" /E /R:3 /W:5 /MT:4 /NP /NFL /NDL
if ($LASTEXITCODE -ge 8) { throw "robocopy fallo (codigo $LASTEXITCODE); el manifiesto NO se subio." }
Copy-Item "$out\manifest.json" "$Drive\manifest.json" -Force

# Verification through the public URL, the same one the launcher uses.
$live = Invoke-RestMethod "$cdn/manifest.json?nocache=$([guid]::NewGuid())"
if ($live.version -ne $Version) { throw "El CDN sigue mostrando la version $($live.version) (cache o subida incompleta)." }
$sample = $live.files | Get-Random
$head = Invoke-WebRequest "$cdn/files/$($sample.path)" -Method Head -UseBasicParsing
if ([int64]$head.Headers['Content-Length'] -ne $sample.size) { throw "Tamano distinto en $($sample.path)." }
"OK: PLOMO $Version publicado en $cdn/manifest.json"
