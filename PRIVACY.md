# Privacy policy / Política de privacidad

## English

PLOMO Launcher has **no telemetry, no analytics and no advertising**. It does not collect or send usage statistics, and the PLOMO project does not receive your Microsoft credentials: you sign in on Microsoft's own page (device code or browser) and the launcher only keeps the tokens Microsoft returns.

**What stays on your computer.** Accounts and tokens (`accounts.json`), settings (`PLOMO.cfg`), the installed modpack, logs and downloaded game files are stored in the folder where PLOMO is installed. Nothing in that folder is uploaded. Uninstalling offers to delete it.

**Servers the launcher contacts.** Only to do what you ask:

| Server | Why | Data sent |
| --- | --- | --- |
| `cdn.playplomo.com` | Modpack manifest and files, launcher update information, Minecraft/Forge metadata mirror | Normal HTTPS request data (your IP address, launcher version) |
| `login.live.com`, `*.xboxlive.com`, `api.minecraftservices.com`, `session.minecraft.net`, `textures.minecraft.net` | Microsoft/Minecraft sign-in, ownership check, profile and skin | The sign-in tokens issued to you by Microsoft |
| `libraries.minecraft.net`, `resources.download.minecraft.net`, Mojang/Forge/LWJGL download hosts listed in PLOMO's metadata | Minecraft, libraries, assets and Java runtime | Normal HTTPS request data |

**Only if you choose to use them:** uploading a game log to a paste service (`api.mclo.gs`, `paste.gg`, `0x0.st`, `hst.sh`) or a screenshot to Imgur sends that file to the service you picked, under its own privacy policy.

The Minecraft game and the mods in the PLOMO modpack are separate software and have their own behavior; see their documentation.

Questions: open an issue at <https://github.com/Maxjose/plomo-launcher/issues>.

## Español

PLOMO Launcher **no tiene telemetría, analítica ni publicidad**. No recoge ni envía estadísticas de uso, y el proyecto PLOMO no recibe tus credenciales de Microsoft: inicias sesión en la página de Microsoft (código de dispositivo o navegador) y el launcher solo guarda los tokens que Microsoft devuelve.

**Lo que se queda en tu equipo.** Cuentas y tokens (`accounts.json`), ajustes (`PLOMO.cfg`), el modpack instalado, registros y archivos del juego se guardan en la carpeta donde está instalado PLOMO. Nada de esa carpeta se sube. Al desinstalar, puedes pedir que se borre.

**Servidores con los que se conecta.** Solo para lo que tú pides:

| Servidor | Para qué | Datos enviados |
| --- | --- | --- |
| `cdn.playplomo.com` | Manifiesto y archivos del modpack, información de actualización del launcher, espejo de metadatos de Minecraft/Forge | Datos normales de una petición HTTPS (tu IP, versión del launcher) |
| `login.live.com`, `*.xboxlive.com`, `api.minecraftservices.com`, `session.minecraft.net`, `textures.minecraft.net` | Inicio de sesión Microsoft/Minecraft, comprobación de licencia, perfil y skin | Los tokens que Microsoft te emite |
| `libraries.minecraft.net`, `resources.download.minecraft.net`, y los servidores de Mojang/Forge/LWJGL y de Java que indican los metadatos de PLOMO | Minecraft, librerías, recursos y Java | Datos normales de una petición HTTPS |

**Solo si tú lo eliges:** subir un registro del juego a un servicio de pegado (`api.mclo.gs`, `paste.gg`, `0x0.st`, `hst.sh`) o una captura a Imgur envía ese archivo al servicio elegido, con su propia política de privacidad.

El juego Minecraft y los mods del modpack de PLOMO son software aparte con su propio comportamiento.

Dudas: abre una incidencia en <https://github.com/Maxjose/plomo-launcher/issues>.
