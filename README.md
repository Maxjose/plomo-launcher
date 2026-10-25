# PLOMO Launcher

Launcher for the **PLOMO** Minecraft modpack (Minecraft 1.20.1, Forge 47.4.16). It installs and updates the modpack from `https://cdn.playplomo.com`, prepares Minecraft and Java automatically, and starts the game with your Microsoft (or offline) account.

Launcher oficial del modpack **PLOMO** (Minecraft 1.20.1, Forge 47.4.16): instala y actualiza el pack desde `https://cdn.playplomo.com`, prepara Minecraft y Java, e inicia el juego con tu cuenta Microsoft (u offline).

- Download / Descarga: <https://playplomo.com/launcher/>
- Releases: <https://github.com/Maxjose/plomo-launcher/releases>
- Latest installer / Último instalador: <https://github.com/Maxjose/plomo-launcher/releases/latest/download/PLOMO-Setup.exe>
- Privacy policy / Política de privacidad: [PRIVACY.md](PRIVACY.md)

## Origin and license

PLOMO Launcher is free software licensed under the **GNU GPL v3** (see [LICENSE](LICENSE) and [COPYING.md](COPYING.md)). It is a fork of [ElyPrismLauncher/Launcher](https://github.com/ElyPrismLauncher/Launcher), which in turn is a fork of [Prism Launcher](https://github.com/PrismLauncher/PrismLauncher). Copyright and license headers of the original authors are kept. This project is **not** endorsed by Prism Launcher, ElyPrism, Mojang or Microsoft.

Changes in this fork: PLOMO user interface and pack updater, metadata served from PLOMO's own mirror, and removal of the CurseForge, Modrinth, ATLauncher, FTB, Technic and Ely.by integrations.

## Building

Windows x64, Visual Studio 2022 (C++), Qt 6.10.2 (`qtimageformats`, `qtnetworkauth`), JDK 17 and vcpkg. The exact steps are in [`.github/workflows/plomo-windows.yml`](.github/workflows/plomo-windows.yml), which is also what builds the official releases. The installer is an [Inno Setup](https://jrsoftware.org/isinfo.php) script in [`plomo/installer`](plomo/installer).

## Verifying downloads

The installers are built by GitHub Actions from this repository and are **not code-signed**, so Windows SmartScreen may warn about an unknown publisher the first time (*More info* → *Run anyway*). You can check that a download is the one built from this repository:

- **SHA-256:** compare `Get-FileHash PLOMO-Setup.exe` with the `SHA256SUMS.txt` attached to the [release](https://github.com/Maxjose/plomo-launcher/releases).
- **Build provenance:** `gh attestation verify PLOMO-Setup.exe --repo Maxjose/plomo-launcher` confirms that GitHub Actions built the file from this repository.

Privacy: see [PRIVACY.md](PRIVACY.md). The launcher has no telemetry.

## Contributing

Open an issue or pull request on this repository. Changes from non-members are reviewed before they are merged.
