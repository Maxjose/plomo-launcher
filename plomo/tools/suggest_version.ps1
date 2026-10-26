# Prints the version number to propose for the next pack publication (one line; empty when it cannot tell).
# release: the published release + 1 (0.34 -> 0.35).   alpha: the next alpha of the line that follows the release
# (0.35-alpha2 -> 0.35-alpha3; 0.35-alpha1 when the alpha line is behind the release or does not exist yet).
param([Parameter(Mandatory)][ValidateSet('release', 'alpha')][string]$Channel)
$ErrorActionPreference = 'SilentlyContinue'
$cdn = 'https://cdn.playplomo.com'

function Get-Live($name) { try { (Invoke-RestMethod "$cdn/${name}?nocache=$([guid]::NewGuid())").version } catch { $null } }

$release = Get-Live 'manifest.json'
$nextRelease = $null
if ($release -match '^(\d+)\.(\d+)$') { $nextRelease = "$($Matches[1]).$([int]$Matches[2] + 1)" }

if ($Channel -eq 'release') { if ($nextRelease) { $nextRelease }; return }

$alpha = Get-Live 'manifest-alpha.json'
if ($alpha -match '^(\d+)\.(\d+)-alpha(\d+)$') {
    $alphaBase = [version]"$($Matches[1]).$($Matches[2])"
    if (-not $nextRelease -or $alphaBase -ge [version]$nextRelease) {
        "$($Matches[1]).$($Matches[2])-alpha$([int]$Matches[3] + 1)"
        return
    }
}
if ($nextRelease) { "$nextRelease-alpha1" }
