# Publishes launcher.json (the notice that tells players a new launcher exists, and what the launcher needs to update itself).
# It reads the GitHub release (version, installer address, SHA256SUMS.txt), re-downloads the installer to double-check its
# SHA-256 and writes launcher/launcher.json into the R2 bucket over its S3 API (r2-config.json). Nobody is told about a launcher
# until this runs: publishing the GitHub release alone does not reach the players.
# Usage: .\tools\publish_launcher.ps1                 (latest GitHub release)
#        .\tools\publish_launcher.ps1 -Tag v1.0.3     (a specific one)
#        .\tools\publish_launcher.ps1 -DryRun         (shows what it would publish, writes nothing)
param(
    [string]$Tag = '',
    [switch]$DryRun,
    [switch]$AllowDowngrade
)
$ErrorActionPreference = 'Stop'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
$repo = 'Maxjose/plomo-launcher'
$cdn = 'https://cdn.playplomo.com'
$headers = @{ 'User-Agent' = 'PLOMO-publish-launcher' }

$api = if ($Tag) { "https://api.github.com/repos/$repo/releases/tags/$Tag" } else { "https://api.github.com/repos/$repo/releases/latest" }
$release = Invoke-RestMethod $api -Headers $headers
$version = $release.tag_name.TrimStart('v')
if ($version -notmatch '^\d+\.\d+\.\d+$') { throw "La etiqueta '$($release.tag_name)' no es una version X.Y.Z." }
$installerName = "PLOMO-Setup-$version.exe"
$installer = $release.assets | Where-Object { $_.name -eq $installerName } | Select-Object -First 1
$sums = $release.assets | Where-Object { $_.name -eq 'SHA256SUMS.txt' } | Select-Object -First 1
if (-not $installer) { throw "La release $($release.tag_name) no tiene el archivo $installerName." }
if (-not $sums) { throw "La release $($release.tag_name) no tiene SHA256SUMS.txt." }

# Hash according to the release's own SHA256SUMS.txt...
$sumsText = (Invoke-WebRequest $sums.browser_download_url -Headers $headers -UseBasicParsing).Content
if ($sumsText -is [byte[]]) { $sumsText = [Text.Encoding]::UTF8.GetString($sumsText) }
$line = ($sumsText -split "`r?`n") | Where-Object { $_ -match ('^[0-9a-fA-F]{64}\s+\*?' + [regex]::Escape($installerName) + '\s*$') } | Select-Object -First 1
if (-not $line) { throw "SHA256SUMS.txt no lista $installerName." }
$sha = $line.Substring(0, 64).ToLower()

# ...and the hash of what is actually downloadable: both must agree.
$tmp = Join-Path ([IO.Path]::GetTempPath()) $installerName
Invoke-WebRequest $installer.browser_download_url -Headers $headers -OutFile $tmp -UseBasicParsing
$real = (Get-FileHash $tmp -Algorithm SHA256).Hash.ToLower()
Remove-Item $tmp -ErrorAction SilentlyContinue
if ($real -ne $sha) { throw "El instalador descargado ($real) no coincide con SHA256SUMS.txt ($sha). No se publica nada." }

$json = [ordered]@{
    version      = $version
    download_url = 'https://playplomo.com/download'
    installer_url = $installer.browser_download_url
    sha256       = $sha
} | ConvertTo-Json

# Do not announce an older launcher than the one already announced.
$current = $null
try { $current = Invoke-RestMethod "$cdn/launcher/launcher.json?nocache=$([guid]::NewGuid())" } catch { }
"Release:          $($release.tag_name)"
"Instalador:       $($installer.browser_download_url)"
"SHA-256:          $sha (coincide con el archivo descargado)"
"launcher.json hoy: $(if ($current) { $current.version } else { '(no disponible)' })"
if ($current -and ([version]$current.version -gt [version]$version) -and -not $AllowDowngrade) {
    throw "El CDN ya anuncia la version $($current.version), mayor que $version. Usa -AllowDowngrade si es a proposito."
}
if ($DryRun) { "Prueba: no se escribio nada."; return }

. "$PSScriptRoot\r2_common.ps1"
$tmpJson = Join-Path ([IO.Path]::GetTempPath()) 'plomo-launcher.json'
[IO.File]::WriteAllText($tmpJson, $json, (New-Object Text.UTF8Encoding($false)))
try { Invoke-R2 put $tmpJson 'launcher/launcher.json' --content-type application/json } finally { Remove-Item $tmpJson -ErrorAction SilentlyContinue }

# Verification through the public URL; the CDN can take a moment to show it, so retry for a while.
$deadline = (Get-Date).AddMinutes(20)
$ok = $false
Write-Host "Esperando a que el CDN muestre launcher.json $version (hasta 20 min)..."
while ((Get-Date) -lt $deadline) {
    try {
        $live = Invoke-RestMethod "$cdn/launcher/launcher.json?nocache=$([guid]::NewGuid())"
        if ($live.version -eq $version -and $live.sha256 -eq $sha) { $ok = $true; break }
        Write-Host "  el CDN aun muestra la version $($live.version); reintentando..."
    } catch {
        Write-Host "  todavia no esta disponible ($($_.Exception.Message.Split([char]10)[0].Trim())); reintentando..."
    }
    Start-Sleep -Seconds 15
}
if (-not $ok) { throw "Pasaron 20 min y el CDN no muestra launcher.json en la version $version. El archivo ya esta en R2; vuelve a comprobar $cdn/launcher/launcher.json mas tarde." }
"OK: launcher $version anunciado en $cdn/launcher/launcher.json"
