# mobileair_pico

Capteur mobile MobileAir basé sur le Raspberry Pi Pico 2W.

## Prérequis

- [Pico SDK](https://github.com/raspberrypi/pico-sdk) 2.1.0 (installé via l'extension VS Code ou manuellement)
- CMake
- Ninja
- Toolchain ARM GCC (`arm-none-eabi-gcc`)

### Installation rapide des outils (Windows)

```powershell
winget install Kitware.CMake
winget install Ninja-build.Ninja
```

## Compilation

```bash
mkdir build && cd build
cmake -G Ninja \
  -DPICO_BOARD=pico2_w \
  -DPICO_PLATFORM=rp2350-arm-s \
  -DPICO_SDK_PATH="$HOME/.pico-sdk/sdk/2.1.0" \
  -DPICO_TOOLCHAIN_PATH="$HOME/.pico-sdk/toolchain/13_3_Rel1" \
  -Dpicotool_DIR="$HOME/.pico-sdk/picotool/2.1.1/picotool" \
  ..
ninja
```

Le fichier `build/mobileair_pico.uf2` est généré à la fin du build.

## Chargement du firmware sur le Pico

1. Brancher le Pico 2W en USB sur le PC **en maintenant le bouton BOOTSEL enfoncé**. Un lecteur amovible `RP2350` apparaît (ex: `E:`).
2. Copier le fichier `.uf2` sur ce lecteur :
   ```powershell
   cp build/mobileair_pico.uf2 E:\
   ```
3. Le Pico redémarre automatiquement et exécute le firmware.

> **Note :** le bouton BOOTSEL n'est nécessaire que si le Pico n'est pas déjà en mode bootloader. Si le lecteur `RP2350` n'apparaît pas, débrancher le Pico, maintenir BOOTSEL, puis rebrancher.

## Câblage modem SARA-R500S

| Signal | Pin Pico | GPIO |
|--------|----------|------|
| TX (Pico → SARA) | Pin 6 | GP4 |
| RX (SARA → Pico) | Pin 7 | GP5 |

UART1, 115200 baud, 8N1, pas de flow control matériel.

## Consultation des logs série (PuTTY)

Une fois le firmware chargé, le Pico se présente comme un port série USB (ex: `COM7`).

### Trouver le port COM

Dans PowerShell :
```powershell
Get-PnpDevice -Class Ports -Status OK | Select-Object FriendlyName
```

### Ouvrir PuTTY en mode série

```powershell
& "C:\Program Files\PuTTY\putty.exe" -serial COM7 -sercfg 115200
```

Remplacer `COM7` par le port trouvé à l'étape précédente. Les logs s'affichent en temps réel dans la fenêtre PuTTY.
