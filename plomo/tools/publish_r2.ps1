# Publishes a PLOMO pack release to the R2 bucket mounted as a drive (rclone mount, K:).
# Usage: .\tools\publish_r2.ps1 -Zip PLOMO-CLIENTE-0.29.zip -Version 0.29 -Drive K:
#        .\tools\publish_r2.ps1 -Zip PLOMO-CLIENTE-0.30a1.zip -Version 0.30-alpha1 -Drive K: -Channel alpha
# The alpha channel has its own manifest (manifest-alpha.json) and its own file folder (files-alpha/), so a test build can
# never overwrite a file that the stable release lists. Give each alpha a version string different from the release one.
param(
    [Parameter(Mandatory)][string]$Zip,
    [Parameter(Mandatory)][string]$Version,
    [Parameter(Mandatory)][ValidatePattern('^[A-Za-z]:$')][string]$Drive,
    [ValidateSet('release', 'alpha')][string]$Channel = 'release'
)
$ErrorActionPreference = 'Stop'
$cdn = 'https://cdn.playplomo.com'
$root = Split-Path $PSScriptRoot -Parent
$py = Join-Path $root '.tools\vcpkg\downloads\tools\python\python-3.14.7-x64\python.exe'
$out = Join-Path $root "$Channel-$Version"
$filesDir = if ($Channel -eq 'alpha') { 'files-alpha' } else { 'files' }
$manifestName = if ($Channel -eq 'alpha') { 'manifest-alpha.json' } else { 'manifest.json' }

if (-not (Test-Path "$Drive\")) { throw "La unidad $Drive no esta montada (rclone)." }

& $py (Join-Path $PSScriptRoot 'build_pack_release.py') $Zip $out --version $Version --base-url "$cdn/$filesDir/" --channel $Channel --materialize
if ($LASTEXITCODE) { throw 'build_pack_release.py fallo; no se subio nada.' }

# Files first, manifest last, so nobody is told about a version that is not fully uploaded.
# No /MIR on purpose: old files stay in the bucket, the launcher only reads what the manifest lists.
robocopy "$out\files" "$Drive\$filesDir" /E /R:3 /W:5 /MT:4 /NP /NFL /NDL
if ($LASTEXITCODE -ge 8) { throw "robocopy fallo (codigo $LASTEXITCODE); el manifiesto NO se subio." }
Copy-Item "$out\$manifestName" "$Drive\$manifestName" -Force

# Verification through the public URL, the same one the launcher uses.
# rclone uploads in the background, so right after the copy the CDN can still answer 404 for a while: retry instead of failing.
$deadline = (Get-Date).AddMinutes(20)
$sample = $null
$ok = $false
Write-Host "Esperando a que rclone termine de subir y el CDN muestre $manifestName (hasta 20 min)..."
while ((Get-Date) -lt $deadline) {
    try {
        $live = Invoke-RestMethod "$cdn/${manifestName}?nocache=$([guid]::NewGuid())"
        if ($live.version -eq $Version) {
            if (-not $sample) { $sample = $live.files | Get-Random }
            $head = Invoke-WebRequest "$cdn/$filesDir/$($sample.path)?v=$($sample.sha256.Substring(0, 16))" -Method Head -UseBasicParsing
            if ([int64]$head.Headers['Content-Length'] -ne $sample.size) { throw "Tamano distinto en $($sample.path)." }
            $ok = $true
            break
        }
        Write-Host "  el CDN aun muestra la version $($live.version); reintentando..."
    } catch {
        if ($_.Exception.Message -like 'Tamano distinto*') { throw }
        Write-Host "  todavia no esta disponible ($($_.Exception.Message.Split([char]10)[0].Trim())); reintentando..."
    }
    Start-Sleep -Seconds 15
}
if (-not $ok) { throw "Pasaron 20 min y el CDN no muestra $manifestName en la version $Version. Puede que rclone siga subiendo: vuelve a comprobar $cdn/$manifestName mas tarde." }
"OK: PLOMO $Version ($Channel) publicado en $cdn/$manifestName"
