# Publishes a PLOMO pack release to the Cloudflare R2 bucket over its S3 API (credentials in r2-config.json, see r2.py).
# Usage: .\tools\publish_r2.ps1 -Zip PLOMO-CLIENTE-0.29.zip -Version 0.29
#        .\tools\publish_r2.ps1 -Zip PLOMO-CLIENTE-0.30a1.zip -Version 0.30-alpha1 -Channel alpha
# The alpha channel has its own manifest (manifest-alpha.json) and its own file folder (files-alpha/), so a test build can
# never overwrite a file that the stable release lists. Give each alpha a version string different from the release one.
param(
    [Parameter(Mandatory)][string]$Zip,
    [Parameter(Mandatory)][string]$Version,
    [ValidateSet('release', 'alpha')][string]$Channel = 'release',
    [string[]]$OnlyDir = @(),      # publish only these top-level pack folders, e.g. config,mods,resourcepacks
    [string[]]$ExcludeDir = @(),   # top-level pack folders to leave out, e.g. tacz
    [switch]$Yes                   # skip the confirmation after the comparison with what is published
)
$ErrorActionPreference = 'Stop'
$cdn = 'https://cdn.playplomo.com'
$root = Split-Path $PSScriptRoot -Parent
$py = Join-Path $root '.tools\vcpkg\downloads\tools\python\python-3.14.7-x64\python.exe'
$out = Join-Path $root "$Channel-$Version"
$filesDir = if ($Channel -eq 'alpha') { 'files-alpha' } else { 'files' }
$manifestName = if ($Channel -eq 'alpha') { 'manifest-alpha.json' } else { 'manifest.json' }

. "$PSScriptRoot\r2_common.ps1"
Write-Host "Comprobando la conexion con R2..."
Invoke-R2 check

# Everything from here on runs inside try/finally so the generated package is always removed at the end.
try {
# The output folder is derived data: start from scratch so files of an earlier run never end up in the upload.
if ((Test-Path $out) -and ((Split-Path $out -Leaf) -match '^(alpha|release)-') -and ($out.StartsWith($root))) { Remove-Item -Recurse -Force $out }

$excludeArgs = @(); foreach ($d in ($OnlyDir | ForEach-Object { $_ -split ',' } | Where-Object { $_ })) { $excludeArgs += @('--only-dir', $d.Trim()) }; foreach ($d in ($ExcludeDir | ForEach-Object { $_ -split ',' } | Where-Object { $_ })) { $excludeArgs += @('--exclude-dir', $d.Trim()) }
& $py (Join-Path $PSScriptRoot 'build_pack_release.py') $Zip $out --version $Version --base-url "$cdn/$filesDir/" --channel $Channel @excludeArgs --materialize
if ($LASTEXITCODE) { throw 'build_pack_release.py fallo; no se subio nada.' }

# Compare with what players have now (alpha: the published alpha, or the release when there is no alpha yet) so a pack that
# drops files or whole folders is noticed BEFORE it is uploaded: the launcher deletes what the new manifest no longer lists.
$new = Get-Content "$out\$manifestName" -Raw -Encoding UTF8 | ConvertFrom-Json
$liveManifest = $null; $liveName = $manifestName
try { $liveManifest = Invoke-RestMethod "$cdn/${manifestName}?nocache=$([guid]::NewGuid())" } catch { }
if (-not $liveManifest -and $Channel -eq 'alpha') {
    $liveName = 'manifest.json'
    try { $liveManifest = Invoke-RestMethod "$cdn/${liveName}?nocache=$([guid]::NewGuid())" } catch { }
}
if ($liveManifest) {
    $old = @{}; foreach ($f in $liveManifest.files) { $old[$f.path] = $f }
    $cur = @{}; foreach ($f in $new.files) { $cur[$f.path] = $f }
    $added = @($cur.Keys | Where-Object { -not $old.ContainsKey($_) })
    $removed = @($old.Keys | Where-Object { -not $cur.ContainsKey($_) })
    $changed = @($cur.Keys | Where-Object { $old.ContainsKey($_) -and $old[$_].sha256 -ne $cur[$_].sha256 })
    $mb = [math]::Round((($added + $changed | ForEach-Object { $cur[$_].size } | Measure-Object -Sum).Sum) / 1MB, 1)
    Write-Host ""
    Write-Host "=== Comparacion con lo publicado ($liveName, version $($liveManifest.version)) ==="
    Write-Host "  Version nueva:  $Version"
    Write-Host "  Archivos nuevos:     $($added.Count)"
    Write-Host "  Archivos cambiados:  $($changed.Count)"
    Write-Host "  Archivos ELIMINADOS: $($removed.Count)   (el launcher los borrara a los jugadores)"
    Write-Host "  A descargar por cada jugador: $mb MB"
    $oldDirs = $old.Keys | ForEach-Object { $_.Split('/')[0] } | Sort-Object -Unique
    $newDirs = $cur.Keys | ForEach-Object { $_.Split('/')[0] } | Sort-Object -Unique
    foreach ($d in ($oldDirs | Where-Object { $newDirs -notcontains $_ })) { Write-Host "  ATENCION: desaparece la carpeta '$d' entera." -ForegroundColor Yellow }
    foreach ($d in ($newDirs | Where-Object { $oldDirs -notcontains $_ })) { Write-Host "  Carpeta nueva: '$d'" }
    if ($removed.Count) {
        Write-Host "  Eliminados (primeros 20):"
        $removed | Sort-Object | Select-Object -First 20 | ForEach-Object { Write-Host "    - $_" }
    }
} else {
    Write-Host "No se pudo leer el manifest publicado: no hay comparacion."
}
if (-not $Yes) {
    $answer = Read-Host "Subir esta version al CDN? (S/N)"
    if ($answer -notmatch '^[sS]$') { Write-Host "Cancelado: no se subio nada."; exit 2 }
}

# Files first, manifest last, so nobody is told about a version that is not fully uploaded.
# No /MIR on purpose: old files stay in the bucket, the launcher only reads what the manifest lists.
Invoke-R2 sync "$out\files" $filesDir
Invoke-R2 put "$out\$manifestName" $manifestName --content-type application/json

# Verification through the public URL, the same one the launcher uses.
# The CDN can take a moment to show the new manifest: retry instead of failing.
$deadline = (Get-Date).AddMinutes(20)
$sample = $null
$ok = $false
Write-Host "Esperando a que el CDN muestre $manifestName (hasta 20 min)..."
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
if (-not $ok) { throw "Pasaron 20 min y el CDN no muestra $manifestName en la version $Version. Los archivos ya estan en R2; vuelve a comprobar $cdn/$manifestName mas tarde." }
"OK: PLOMO $Version ($Channel) publicado en $cdn/$manifestName"
} finally {
    # The package is derived data (hundreds of MB): remove it whether the run succeeded, failed or was cancelled,
    # so alpha-<version>/release-<version> folders never pile up.
    if ((Test-Path $out) -and ((Split-Path $out -Leaf) -match '^(alpha|release)-') -and ($out.StartsWith($root))) {
        Remove-Item -Recurse -Force $out -ErrorAction SilentlyContinue
        if (-not (Test-Path $out)) { Write-Host "Carpeta temporal borrada: $out" }
    }
}
