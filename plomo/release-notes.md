Installer for Windows 10/11 (x64), built by GitHub Actions from this repository. `PLOMO-Setup.exe` is always the latest version; the file with a version number is the same installer.

This build is **not code-signed**, so Windows SmartScreen may warn about an unknown publisher: choose *More info* then *Run anyway*. To make sure the file is the one built from this repository, check it:
- SHA-256: compare `Get-FileHash PLOMO-Setup.exe` with `SHA256SUMS.txt`.
- Provenance: `gh attestation verify PLOMO-Setup.exe --repo Maxjose/plomo-launcher`.

Instalador para Windows 10/11 (x64), compilado por GitHub Actions desde este repositorio. `PLOMO-Setup.exe` es siempre la última versión; el archivo con número de versión es el mismo instalador.

Esta versión **no está firmada**, por lo que SmartScreen puede avisar de editor desconocido: pulsa *Más información* y luego *Ejecutar de todas formas*. Para comprobar que el archivo es el compilado desde este repositorio, verifica el SHA-256 con `SHA256SUMS.txt` o la procedencia con `gh attestation verify`.
