# Patchwerk Modular (VST3)

Modularer Synthesizer als VST3-Plugin (dazu Standalone und auf dem Mac auch AU).
Module werden mit Patchkabeln verbunden, genau wie in der Browser-Version.

## Module

| Modul | Funktion | Eingänge | Ausgänge |
|---|---|---|---|
| Tastatur | MIDI aus der DAW oder Bildschirmtastatur, Oktave, Glide, Pitchbend ±2 | – | Pitch, Gate |
| Sequenzer | 8 Schritte, Tempo intern oder vom Host („Host“), Länge, Gate-Dauer | – | Pitch, Gate |
| VCO | Sinus, Dreieck, Sägezahn, Rechteck (bandbegrenzt), Oktave, Fein, FM | 1V/Okt, FM | Out |
| Rauschen | Weißes Rauschen mit Farbfilter | – | Out |
| LFO | 4 Formen, 0,05–30 Hz, Tiefe | Rate | Out |
| Hüllkurve | ADSR mit Kurvenanzeige | Gate | Out |
| Filter | Tiefpass, Hochpass, Bandpass, Notch (State-Variable), Resonanz | In, CV 1, CV 2 | Out |
| VCA | Grundpegel und CV-Menge | In, CV | Out |
| Mixer | 4 Kanäle mit Summe | 1–4 | Out |
| Delay | Zeit bis 2 s, Feedback, Ton, Mix | In | Out |
| Hall | Größe, Mix | In | Out |
| Ausgang | Lautstärke, Oszilloskop, L/Mono und R | L/Mono, R | – |

Tonhöhe: 1 Einheit = 1 Oktave, 0 = C4. Ein Peak-Limiter am Ende schützt vor Übersteuerung.
Jeder Ausgang darf in jeden Eingang; Buchsen mit goldenem Ring sind Gate-Buchsen.

## Bedienung

- **Verbinden:** von einer Buchse zur anderen ziehen
- **Umstecken:** an einem belegten Eingang ziehen; im Leeren loslassen = Kabel abziehen
- **Kabel löschen:** Kabel anklicken
- **Module:** oben „Modul“ zum Hinzufügen, am Titel verschieben, × entfernt
- **Regler:** ziehen oder Mausrad, Doppelklick = Standardwert
- Der komplette Patch wird mit dem DAW-Projekt gespeichert.

## Bauen

Voraussetzungen: CMake 3.22+, ein C++17-Compiler (Windows: Visual Studio 2022 mit „Desktop-Entwicklung mit C++“; macOS: Xcode).
JUCE 8.0.4 wird beim Konfigurieren automatisch heruntergeladen.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
```

```bash
cmake --build build --config Release
```

Das Plugin liegt danach in `build/PatchwerkModular_artefacts/Release/VST3/Patchwerk Modular.vst3`.

### Installieren

- **Windows:** `Patchwerk Modular.vst3` nach `C:\Program Files\Common Files\VST3\` kopieren
- **macOS:** nach `~/Library/Audio/Plug-Ins/VST3/` kopieren. Da das Plugin nicht signiert ist, einmal ausführen:

```bash
xattr -cr ~/Library/Audio/Plug-Ins/VST3/"Patchwerk Modular.vst3"
```

- **Linux:** nach `~/.vst3/` kopieren

### Automatisch über GitHub

Liegt das Projekt in einem GitHub-Repository, baut `.github/workflows/build.yml` bei jedem Push
Windows-, macOS- (Universal) und Linux-Versionen. Die fertigen Plugins stehen unter **Actions → Lauf → Artifacts** zum Download.

## FL Studio (Windows)

1. `Patchwerk Modular.vst3` nach `C:\Program Files\Common Files\VST3\` kopieren.
2. In FL Studio: **Options → Manage plugins → Find installed plugins** (Scan).
3. Patchwerk Modular in der Liste mit einem Stern markieren, dann über **Add → Patchwerk Modular** in den Channel Rack laden.
4. Im Mixer einem Insert zuweisen und mit dem Piano Roll oder MIDI-Keyboard spielen.

Der Synth ist monophon. Für Akkorde mehrere Instanzen laden.

## Lizenz-Hinweis

JUCE 8 ist unter AGPLv3 frei nutzbar; für die Weitergabe als Closed-Source-Produkt braucht es eine JUCE-Lizenz.
