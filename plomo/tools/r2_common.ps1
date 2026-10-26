# Shared by the publishing scripts: finds (or creates, the first time) the Python environment that talks to Cloudflare R2.
# Usage: . "$PSScriptRoot\r2_common.ps1" ; Invoke-R2 sync "<folder>" "<prefix>"
function Get-R2Python {
    $root = Split-Path $PSScriptRoot -Parent
    $venv = Join-Path $root '.tools\r2venv'
    $py = Join-Path $venv 'Scripts\python.exe'
    if (-not (Test-Path $py)) {
        Write-Host "Preparando Python para R2 (solo la primera vez, tarda un minuto)..."
        $system = (Get-Command python -ErrorAction SilentlyContinue).Source
        if (-not $system) { $system = (Get-Command py -ErrorAction SilentlyContinue).Source }
        if (-not $system) { throw "No encuentro Python en el PATH. Instala Python 3 (python.org) y vuelve a intentarlo." }
        & $system -m venv $venv
        if ($LASTEXITCODE) { throw "No se pudo crear el entorno de Python en $venv" }
        & $py -m pip install --quiet --disable-pip-version-check boto3
        if ($LASTEXITCODE) { throw "No se pudo instalar boto3 (hace falta conexion a internet la primera vez)." }
    }
    return $py
}

function Invoke-R2 {
    # Runs r2.py with the given arguments; throws with its message when it fails.
    $py = Get-R2Python
    & $py (Join-Path $PSScriptRoot 'r2.py') @args
    if ($LASTEXITCODE) { throw "r2.py fallo (codigo $LASTEXITCODE); mira el mensaje de arriba." }
}
