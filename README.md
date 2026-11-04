# BloomBot

## Components
* ESP32-microcontroller
* WS2812B LED strip (60 LEDs total, ~20 in use)
* HC-SR04 sensor
* 2x HD38 soil moisture sensor
* JQC-3FF-S-Z relay
* 5V pump

## Telegram-Bot buttons and dialogs
- Status: displays the latest status, data age, and how many days the water supply will last
- Plant: search by name and selection from a list
- Pot: input field for volume in liters (with numeric validation)
- Vacation: date entry, three response options (see below), and confirmation for "eco" mode
- Vacation off
- Reset error: button visible only during the ERROR phase

### Vacation Mode
The user simply enters their return date. The system handles the rest:
* days_left ≥ days until return + 1-day buffer → "Everything is fine; there will be enough water until ~Nov 25."
* Not enough for the full duration, but sufficient in "eco" mode → the bot suggests: "There is only enough water until Nov 16. In eco mode, it will last until ~Nov 22. Shall I enable it?"
* Not enough even in "eco" mode → the bot states clearly: "There won't be enough water, even in eco mode. Please ask someone to top it up, or install a larger tank."

### Gruppenteilnehmer: Tobias Bahlinger, ...

# Schritt 1. Installation der Programme:
1. Öffnen Sie VS Code und wechseln Sie links in den Reiter „Extensions“ (Erweiterungen) (Strg+Umschalt+X).
2. Suchen Sie das Plugin „PlatformIO IDE“ und installieren Sie es. (Nach der Installation muss VS Code möglicherweise neu gestartet werden.)

# Schritt 2. Projekt von GitHub herunterladen
1. Rufen Sie unser Repository auf: [https://github.com/slav-pop/BloomBot](https://github.com/slav-pop/BloomBot).
2. Klicken Sie auf die grüne Schaltfläche „Code“ $\rightarrow$ „Download ZIP“.
3. Entpacken Sie das Archiv in einen beliebigen Ordner.

# Schritt 3. Projekt öffnen und kompilieren
1. Klicken Sie in VS Code auf "File" $\rightarrow$ "Open Folder..." und wählen Sie den entpackten Projektordner aus.
2. Warten Sie ein paar Minuten, bis PlatformIO automatisch die erforderlichen Bibliotheken und Einstellungen aus der Datei „platformio.ini“ heruntergeladen hat (unten erscheinen ein Häkchen und das PlatformIO-Symbol mit dem Ameisenhaufen bzw. dem Totenkopf).
3. Schließen Sie das ESP32-Board über ein USB-Kabel an den PC an.
4. Klicken Sie auf das Häkchen-Symbol ($\checkmark$) in der unteren Leiste von VS Code (oder drücken Sie Strg+Alt+B), um das Projekt zu kompilieren.
5. Klicken Sie auf das Pfeil-Symbol ($\rightarrow$) in der unteren Leiste, um den Code auf das ESP32 zu flashen!

# Web: Homepage und App

| Ordner | Inhalt |
|---|---|
| `site/` | Homepage (statisches HTML/CSS/JS) mit interaktiver Simulation der Gießlogik |
| `app/` | Vue-App: Pflanze wählen, Topfgröße einstellen, Profil für das Gerät ansehen |
| `data/plants.json` | Pflanzendatenbank (55 Pflanzen), Felder wie in `Integration.md` §5 |
| `scripts/build-pages.sh` | baut Homepage + App zusammen nach `_site/` |

Online (GitHub Pages): Homepage unter `https://slav-pop.github.io/BloomBot/`, App unter `…/BloomBot/app/`.
Jeder Push auf `main`, der diese Ordner ändert, testet, baut und veröffentlicht automatisch
(`.github/workflows/web.yml`).

### Lokal starten

Voraussetzung: Node.js 20.19 oder neuer.

```bash
cd app
npm ci
npm run dev            # App mit Live-Reload
npm test               # Tests (App, Datenbank, Simulation der Homepage)
npm run test:coverage  # dazu Testabdeckung, Minimum 80 %

cd ..
bash scripts/build-pages.sh               # Homepage + App nach _site/
python3 -m http.server -d _site 8080      # ansehen unter http://localhost:8080
```

### Pflanzendatenbank

`data/plants.json` enthält nur Felder, die wirklich genutzt werden: Namen, Licht, Wasserbedarf
sowie Bodenfeuchte-Grenzen, Gießzyklus und Pumpenberechnung für das Geräteprofil.
Fotos liegen unter `app/public/plants/<id>.webp`. Ein Test (`app/src/lib/plantsData.test.js`)
prüft bei jedem Lauf, dass keine zusätzlichen Felder auftauchen, alle Werte in den erlaubten
Bereichen liegen und jede Pflanze ein Foto hat.
