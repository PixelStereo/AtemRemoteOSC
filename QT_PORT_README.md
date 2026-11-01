# atemOSC v4.0 (Qt 6 Multi-Platform Rewrite)

Portage complet et moderne de **atemOSC** en **C++17 / Qt 6** avec support multi-plateforme natif (**macOS, Windows et Linux**), intégrant l'intégralité du **SDK Blackmagic ATEM Switchers 11.0**.

---

## 🚀 Architecture Multi-Plateforme

```
                             ┌─────────────────────────┐
                             │    Interface GUI Qt 6   │
                             │ (Widgets / OSC Browser) │
                             └────────────┬────────────┘
                                          │
                             ┌────────────▼────────────┐
                             │       Moteur OSC        │
                             │ (QUdpSocket / C++17)    │
                             └────────────┬────────────┘
                                          │
                             ┌────────────▼────────────┐
                             │  Interface IAtemBackend │
                             └──────┬───────────┬──────┘
                                    │           │
       (macOS & Windows)            │           │    (Linux)
    ┌───────────────────────────────▼┐         ┌▼─────────────────────────────┐
    │       BmdSwitcherBackend       │         │      NetworkAtemBackend      │
    │  (SDK 11.0 officiel Blackmagic)│         │  (Protocole direct UDP 9910) │
    └────────────────────────────────┘         └──────────────────────────────┘
```

1. **macOS & Windows** :
   - Pilote natif officiel utilisant les interfaces COM du **Blackmagic ATEM Switchers SDK 11.0** (`IBMDSwitcher`, `IBMDSwitcherMixEffectBlock`, `IBMDSwitcherFairlightAudioMixer`, `IBMDSwitcherRecordAV`, `IBMDSwitcherDisplayClock`, etc.).
2. **Linux** :
   - Pilote réseau direct implémentant le protocole bas niveau ATEM en UDP sur le port standard **9910** (handshake SYN/ACK, retransmission, paquet d'états et dispatch de commandes).
   - Ne nécessite aucune installation de pilote Blackmagic tiers sous Linux.
3. **Moteur OSC autonome** :
   - Remplacement complet de l'ancienne dépendance Cocoa `VVOSC` par un moteur OSC moderne et sans allocation superflue (`QUdpSocket`), compatible avec tous les contrôleurs OSC du marché (TouchOSC, Open Stage Control, QLab, Max/MSP, Companion, TouchDesigner, etc.).

---

## 🌟 Nouvelles fonctionnalités SDK 11.0 intégrées

atemOSC intègre désormais l'ensemble des fonctionnalités du SDK 11.0 :

### 1. Contrôle Caméra & PTZ
- `/atem/camera/<id>/iris <float>` : Réglage d'ouverture d'iris (0.0 à 1.0)
- `/atem/camera/<id>/focus <float>` : Mise au point manuelle (0.0 à 1.0)
- `/atem/camera/<id>/autofocus` : Déclenchement de l'autofocus ponctuel
- `/atem/camera/<id>/gain <float>` : Gain capteur en dB
- `/atem/camera/<id>/shutter <float>` : Vitesse d'obturation (dénominateur : 60 = 1/60 s)
- `/atem/camera/<id>/white-balance <float>` : Température de couleur en Kelvin (ex: 5600)
- `/atem/camera/<id>/ptz/pan <float>` : Vitesse Pan PTZ (-1.0 à 1.0, 0 = arrêt)
- `/atem/camera/<id>/ptz/tilt <float>` : Vitesse Tilt PTZ (-1.0 à 1.0, 0 = arrêt)
- `/atem/camera/<id>/ptz/zoom <float>` : Vitesse de zoom (-1.0 à 1.0, 0 = arrêt)
- `/atem/camera/<id>/ptz/recall <int>` : Rappel de préréglage PTZ (preset)

### 2. Fairlight Audio complet (Égaliseur, Dynamique, Solo, XLR)
- `/atem/audio/input/<number>/solo <bool>` : Écoute Solo au casque par canal
- `/atem/audio/input/<number>/preamp <float>` : Gain de préamplificateur analogique / XLR en dB
- `/atem/audio/input/<number>/phantom <bool>` : Alimentation fantôme +48V sur les entrées micro XLR
- `/atem/audio/input/<number>/eq/<band>/gain <float>` : Gain EQ par bande (-20 dB à +20 dB)
- `/atem/audio/input/<number>/eq/<band>/freq <float>` : Fréquence centrale EQ en Hz
- `/atem/audio/input/<number>/eq/<band>/q <float>` : Facteur Q de la bande EQ
- `/atem/audio/input/<number>/eq/<band>/enabled <bool>` : Activer / contourner une bande EQ
- `/atem/audio/input/<number>/eq/reset` : Réinitialiser l'égaliseur du canal
- `/atem/audio/input/<number>/compressor/threshold <float>` : Seuil compresseur en dB
- `/atem/audio/input/<number>/compressor/ratio <float>` : Ratio compresseur
- `/atem/audio/input/<number>/limiter/threshold <float>` : Seuil limiteur en dB

### 3. MultiView
- `/atem/multiview/<mv>/layout <string>` : Sélection du layout (default, quadrant, 1+7, 1+9, 2+8)
- `/atem/multiview/<mv>/window/<win>/source <int>` : Assignation personnalisée de source sur chaque fenêtre (1 à 16)
- `/atem/multiview/<mv>/safe-area <bool>` : Affichage des repères de cadrage (Safe Area)
- `/atem/multiview/<mv>/meters <bool>` : Affichage des VU-mètres audio

### 4. Horloge & Chronomètre d'incrustation (Display Clock)
- `/atem/clock/enabled <bool>` : Activer / masquer l'horloge à l'écran
- `/atem/clock/mode <string>` : Mode de l'horloge (`timeofday`, `countdown`, `countup`)
- `/atem/clock/start` : Lancer le décompte / chronomètre
- `/atem/clock/stop` : Mettre en pause le décompte / chronomètre
- `/atem/clock/reset` : Réinitialiser le chronomètre
- `/atem/clock/time <string>` : Définir l'heure/durée cible au format `HH:MM:SS`
- `/atem/clock/size <float>` : Taille d'affichage (0.5 à 2.0)
- `/atem/clock/opacity <float>` : Opacité d'affichage (0.0 à 1.0)

### 5. Enregistrement ISO & Streaming Cloud
- `/atem/recording/iso <bool>` : Enregistrement ISO de toutes les entrées individuelles simultanées
- `/atem/recording/switch-disk` : Basculer vers le support externe suivant
- `/atem/recording/filename <string>` : Définir le nom de fichier de base
- `/atem/stream/service <string>` : Sélectionner le profil / service de diffusion

### 6. SuperSource & Incrustateurs Avancés
- `/atem/supersource/border/enabled <bool>` : Bordure globale SuperSource
- `/atem/supersource/border/width-in <float>` & `width-out <float>` : Épaisseurs interne/externe
- `/atem/me/<me>/usk/<key>/chroma/sample-x <float>` & `sample-y <float>` : Position d'échantillonnage de la pipette pour Advanced Chroma Keyer
- `/atem/mplayer/capture <int>` : Capture directe d'une image fixe dans le Media Pool depuis une entrée vidéo

### 7. Sorties vidéo converties (Nouveauté SDK 11.0)
- `/atem/aux/<number>/converted-videomode <int>` : Configuration individuelle de la résolution et du framerate converti sur les sorties auxiliaires.

---

## ⚠️ État de l'implémentation

- **Compatibilité atemOSC 3.x** : le schéma d'adresses historique est conservé (`/atem/usk/1/tie/toggle`, `/atem/transition/bar`, `/atem/program/5` avec valeur dans l'adresse, `/atem/dsk/...`, `tie/set-next`, `led/green`...). Les adresses `/atem/me/<me>/...` restent disponibles.
- **Backend SDK (macOS / Windows)** : couvre l'ensemble des fonctions listées ci-dessus. Non testé sur un switcher réel dans cette session ; seule la compilation sur macOS a été vérifiée.
- **Backend réseau direct (Linux)** : implémente le handshake UDP 9910, la session, les ACK, la détection de perte de lien, et les commandes suivantes : program/preview, cut/auto/FTB, T-bar, type et durée de transition, tie/on-air/type/fill/key des USK, DSK (on-air, tie, auto, rate, sources), aux, macros (run/stop/loop), enregistrement et streaming (start/stop), gain/balance/mix audio classique, fenêtres MultiView, sélection Media Player. Les autres fonctions (Fairlight EQ/dynamique, caméra/PTZ, horloge, SuperSource, HyperDeck, paramètres luma/chroma/fly/mask…) écrivent un message « non disponible avec le backend réseau » dans le journal ; utiliser le backend SDK pour celles-ci. Les codes de commandes du protocole ont été écrits de mémoire et **doivent être validés sur un vrai switcher**.
- Les paramètres OSC partiels (ex. `eq/<band>/freq` seul) sont fusionnés avec les dernières valeurs connues avant l'envoi ; avant tout retour d'état, les valeurs par défaut sont utilisées pour les paramètres jamais envoyés.

## 🛠️ Instructions de Compilation

### Prérequis
- **C++17** compatible (Apple Clang, GCC 9+, ou MSVC 2019+)
- **CMake 3.16+**
- **Qt 6.2+** (modules `Core`, `Gui`, `Widgets`, `Network`, `Test`)

### Sur macOS
```bash
brew install qt cmake
mkdir build && cd build
cmake ..
cmake --build . -j
```
L'application autonome `atemOSC.app` est générée directement dans `build/`.

### Sur Linux (Ubuntu / Debian / Fedora / Arch)
```bash
# Ubuntu / Debian :
sudo apt install build-essential cmake qt6-base-dev
mkdir build && cd build
cmake ..
cmake --build . -j
```

### Sur Windows (Visual Studio / MSVC)
```cmd
mkdir build && cd build
cmake -DCMAKE_PREFIX_PATH="C:\Qt\6.x.x\msvc2019_64" ..
cmake --build . --config Release
```

---

## 🧪 Tests Automatisés
La suite de tests automatisée vérifie l'encodage/décodage OSC, la sérialisation des paquets et le routage des messages vers les switchers :
```bash
ctest --test-dir build --output-on-failure
```
