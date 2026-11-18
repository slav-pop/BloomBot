# BloomBot – Schnittstelle ESP32 ↔ Server


## 1. Wer macht was

| Gerät (ESP32) | Server (Cloudflare Worker) |
|---|---|
| Misst die Bodenfeuchte und den Wasserstand im Tank | Telegram-Bot: Benachrichtigungen und Einstellungen |
| Gießt die Pflanze **vollständig selbstständig** nach dem aktiven Profil | Pflanzenkatalog (im Code des Workers, Quelle `server/plants.json`) |
| Speichert das Profil im Flash-Speicher (NVS) und arbeitet ohne WLAN und Server | Macht aus „Pflanze + Topfgröße + Modus“ ein **Profil** (Abschnitt 5) |
| Fragt den Server ab, sendet Status und Ereignisse | Speichert Status und Verlauf in D1, setzt die Uhrzeit (der ESP32 hat keine Uhr) |
| Führt Befehle aus, immer mit eigenen Sicherheitsprüfungen | Berechnet die **Prognose des Wasservorrats** und verwaltet den **Urlaubsmodus** (Abschnitt 7) |
| | Antwortet nur Nutzern mit Zugriff auf das Gerät (Tabelle `device_access`) |

**Prinzip:** BloomBot ist ein autonomes Bewässerungsgerät. Der Benutzer gießt
nicht aus der Ferne. Der Bot ist für zwei Dinge da:
1. **Benachrichtigungen:** was passiert ist und was zu tun ist (Wasser nachfüllen, Schlauch prüfen).
2. **Einstellungen:** Auswahl der Pflanze, Topfgröße, Urlaubsmodus.

Der Server steuert die Pumpe niemals direkt.

## 2. Hosting und Kommunikation

### Hosting
- **Cloudflare Workers**, kostenloser Tarif ohne Kreditkarte:
  100 000 Anfragen pro Tag, D1-Datenbank mit 5 GB.
  Nichts muss eingeschaltet bleiben oder aktualisiert werden.
- Sprache: JavaScript (Hauptsprache von Workers). Python Workers gibt es auch,
  aber die üblichen Bot-Bibliotheken (z. B. `python-telegram-bot`) laufen dort
  **nicht**. Der Bot kommuniziert mit der Telegram Bot API über einfache HTTP-Anfragen.
- Telegram → Server: **Webhook**. Telegram ruft den Worker bei jeder Nachricht selbst auf.
- Speicher: **D1** (SQL). KV ist für den Status ungeeignet: Im kostenlosen
  Tarif sind nur 1 000 KV-Schreibvorgänge pro Tag erlaubt.
- Code im Repo unter `server/`. Cloudflare deployt ihn automatisch bei jedem Push in `main`
  (Workers Builds). Im Browser-Editor von Cloudflare wird **nicht** mehr geändert:
  der nächste Push würde es überschreiben.

### Warum das Gerät den Server selbst abfragt
Der ESP32 steht hinter einem Router und hat keine öffentliche Adresse, daher
kann der Server ihn nicht erreichen. Das Gerät fragt selbst regelmäßig: „Gibt es
etwas für mich?“ Geänderte Einstellungen warten auf dem Server bis zur nächsten Abfrage.

### Adressen

Basis-URL: `https://<worker>.workers.dev`

| Methode und Pfad | Aufrufer | Request-Body | Antwort |
|---|---|---|---|
| `POST /api/device/<id>/poll` | ESP32 | Status (Abschnitt 3) | Poll-Antwort (unten) |
| `POST /api/device/<id>/event` | ESP32 | ein Ereignis (Abschnitt 4) | `{"ok": true}` |
| `POST /telegram` | Telegram | Telegram-Update | `200` |

- `<id>` – Geräte-ID, z. B. `bloombot-01`. Das Gerät muss in der Tabelle `devices` stehen,
  sonst antwortet der Server `404 unknown_device`.
- Authentifizierung des Geräts: Header `Authorization: Bearer <DEVICE_KEY>`.
  Der Schlüssel liegt auf dem ESP32 in `src/secrets.h` (Datei steht in `.gitignore`),
  auf dem Server wird er als Worker-Secret gespeichert.
- Authentifizierung von Telegram: Der Webhook wird mit `secret_token` registriert,
  der Worker prüft den Header `X-Telegram-Bot-Api-Secret-Token`.
- Alle Request-Bodys sind JSON in UTF-8.

### Poll-Antwort

```json
{
  "poll_s": 600,
  "commands": [],
  "profile": null
}
```

| Feld | Bedeutung |
|---|---|
| `poll_s` | Sekunden bis zur nächsten Abfrage, normalerweise 600 (10 min) |
| `commands` | Befehle aus der Warteschlange (Abschnitt 6), normalerweise eine leere Liste. Ein Befehl wird aus der Warteschlange entfernt, sobald er ausgegeben wurde |
| `profile` | neues Pflanzenprofil (Abschnitt 5) oder `null`, wenn das Gerät bereits das aktuelle hat (der Server vergleicht mit `profile_ver` aus dem Status) |

### Regeln für das Gerät
- Abfrageintervall: `poll_s` nur im Bereich 60–3600 s übernehmen.
  Ohne gültige Antwort 600 verwenden.
- **Solange die Pumpe läuft, keine Netzwerkanfragen.** Eine HTTPS-Anfrage blockiert
  den ESP32 (siehe unten), die Pumpe muss aber rechtzeitig abschalten.
  Die Abfrage wird verschoben, bis die Pumpe aus ist.
- Ereignisse werden **sofort** gesendet, ohne auf die nächste Abfrage zu warten.
  Dadurch kommen Benachrichtigungen ohne Verzögerung an, obwohl selten abgefragt wird.
  Schlägt das Senden fehl, speichert das Gerät bis zu 5 Ereignisse und sendet sie
  vor der nächsten Abfrage.
- Bei Netzwerkfehlern: im normalen Intervall erneut versuchen. Das Gießen wird nie blockiert.
- **Keep-alive:** Eine neue HTTPS-Verbindung dauert auf dem ESP32 ~4 s (der TLS-Handshake prüft
  die Zertifikatskette mit ECDSA P-384, das rechnet der ESP32 in Software). Deshalb bleibt die
  Verbindung nach einer Anfrage offen; weitere Anfragen dauern ~0,2–0,4 s. Hat der Server die
  Verbindung inzwischen geschlossen, wird die Anfrage einmal sofort mit neuer Verbindung wiederholt.
- **Der Lichtring friert während einer Anfrage kurz ein.** Jede HTTPS-Anfrage hält `loop()`
  an (meist ~0,2 s, nach Neustart oder WLAN-Verlust einmal ~4 s), in dieser Zeit steht die
  LED-Animation still: auf der Messe (`poll_s` = 60) einmal pro Minute, im Alltag alle
  10 Minuten, dazu bei jedem Ereignis. Das ist bewusst so
  entschieden: Der Code bleibt einfach, und es gibt keinen gleichzeitigen Zugriff von zwei
  Prozessorkernen auf `state` (eine Fehlerquelle, die schwer zu finden ist). Die Pumpe ist nicht
  betroffen, weil während des Pumpens keine Anfragen laufen.
  Spätere Alternative: das Netzwerk als eigene FreeRTOS-Task auf dem zweiten Kern des ESP32.
- **Zertifikatsprüfung:** Das Gerät prüft das HTTPS-Zertifikat des Servers anhand fest
  eingebauter Root-Zertifikate. So kann niemand im selben Netz den Server vortäuschen und den
  Geräteschlüssel abfangen. Wechselt Cloudflare die Zertifizierungsstelle oder läuft ein
  Root-Zertifikat ab (die eingebauten gelten bis mindestens 2035), muss die Firmware
  aktualisiert werden.
- JSON wird mit der Bibliothek **ArduinoJson** erzeugt und gelesen.

### Regeln für den Server
- Befehle, die älter als **30 Minuten** sind, werden verworfen statt ausgegeben.
- Das Gerät gilt als **offline**, wenn länger als 3 × `poll_s` keine Abfrage kam.
  „Status“ zeigt dann „Nicht erreichbar (letzter Kontakt vor …)“. Eine aktive
  Meldung per Cron Trigger ist für später geplant.
- **Schutz:** Request-Body höchstens 16 KB (Gerät) bzw. 64 KB (Telegram); vom Gerät werden nur
  bekannte Felder mit gültigem Typ und Bereich gespeichert, unbekannte Ereignistypen werden
  abgelehnt (`400`); Schlüssel und Webhook-Secret werden in konstanter Zeit verglichen;
  Anfragen an Telegram haben ein Timeout von 8 s, der Bot-Token landet nie im Log.
- Zustellung „höchstens einmal“: Geht die Poll-Antwort verloren, geht auch der
  Befehl verloren. Das Ergebnis kommt immer als Ereignis.

## 3. Status (Body von `/poll`)

Wird mit jeder Abfrage gesendet.

```json
{
  "fw": "0.4.0",
  "uptime_s": 12345,
  "moisture_index": 42,
  "moisture_sensors": [45, 39],
  "moisture_raw": [2930, 3080],
  "tank_pct": 63,
  "tank_low": false,
  "tank_capacity_ml": 2000,
  "total_pumped_ml": 840,
  "phase": "IDLE",
  "pump_on": false,
  "need_water": false,
  "error": false,
  "error_reason": null,
  "attempts": 0,
  "plant_id": 9012,
  "profile_ver": 3,
  "last_dose_ml": 120,
  "last_watered_s_ago": 3600,
  "rssi": -61
}
```

| Feld | Bedeutung |
|---|---|
| `moisture_index` | Feuchte-Index 0–100 (keine physikalischen Prozent), `-1` = keine Daten. Skala siehe Abschnitt 8. Daraus entscheidet das Gerät über das Gießen |
| `moisture_sensors` | Index der beiden Feuchtesensoren einzeln, `-1` = Sensor ausgefallen. `moisture_index` ist ihr Mittelwert (oder der eine funktionierende) |
| `moisture_raw` | Rohwerte des ADC 0–4095 der beiden Sensoren, `-1` = ausgefallen. Für Kalibrierung und Fehlersuche |
| `tank_pct` | 0–100, `-1` = keine Daten oder Sensorfehler |
| `tank_low` | true unter 20 %, wieder false über 25 % (Hysterese) |
| `tank_capacity_ml` | Tankvolumen aus `config.h` (`TANK_CAPACITY_ML`) |
| `total_pumped_ml` | wie viele ml seit dem Start gepumpt wurden. Beginnt nach einem Neustart bei 0 (der Server erkennt das, Abschnitt 7) |
| `phase` | `IDLE`, `PUMPING`, `SOAKING`, `TANK_EMPTY`, `ERROR` |
| `error` | true, wenn ein Fehler besteht (auch ein Sensorfehler) |
| `error_reason` | `null` oder der Grund: `still_dry`, `pump_timeout`, `sensor_tank`, `sensor_moisture` (Abschnitt 4) |
| `attempts` | wie viele Portionen im aktuellen Gießzyklus gegeben wurden |
| `plant_id` | ID des aktiven Profils, `-1` = kein Profil (das Gerät arbeitet mit sicheren Standardwerten) |
| `profile_ver` | Version des aktiven Profils, `0` = kein Profil |
| `last_watered_s_ago` | Sekunden seit dem letzten Gießen, `-1` = seit dem Start nicht gegossen |
| `rssi` | WLAN-Signalstärke, dBm |

## 4. Ereignisse (Body von `/event`)

Der Bot macht daraus Telegram-Nachrichten. Das Gerät sendet jedes Ereignis einmal.

```json
{ "type": "watered", "ml": 120, "moisture_index": 38 }
```

| `type` | Zusatzfelder | Beispielnachricht des Bots |
|---|---|---|
| `boot` | `fw` | (nur ins Log) |
| `watered` | `ml`, `moisture_index` | (nur Log / Verlauf; keine Nachricht, um nicht zu spammen) |
| `tank_low` | `tank_pct` | „Wenig Wasser im Tank (~2 Tage). Bitte nachfüllen“ |
| `tank_empty` | – | „Die Pflanze braucht Wasser, aber der Tank ist leer!“ |
| `tank_refilled` | `tank_pct` | „Tank aufgefüllt, danke. Das Wasser reicht für etwa 9–12 Tage“ |
| `error` | `reason` | „Fehler: … Nach der Prüfung bitte „Fehler zurücksetzen“ drücken“ |
| `error_cleared` | `reason` (welcher Fehler weg ist) | „Fehler zurückgesetzt“ |
| `profile_ok` | `plant_id`, `profile_ver` | „Ich kümmere mich jetzt um: Golden barrel cactus“ |
| `profile_rejected` | `reason` | (Fehler auf dem Server, ins Log schreiben) |
| `cmd_rejected` | `cmd`, `reason` | (ins Log schreiben) |

Werte von `error.reason`:
- `still_dry` – Boden ist nach allen Portionen noch trocken (Schlauch? Sensor? Pumpe?);
- `pump_timeout` – Pumpe lief zu lange;
- `sensor_tank` – keine Daten vom Tanksensor (geht von selbst weg, sobald der Sensor wieder antwortet);
- `sensor_moisture` – keine Daten von beiden Feuchtesensoren, kein Gießen (geht ebenfalls von selbst weg).

`still_dry` und `pump_timeout` bleiben bestehen, bis „Fehler zurücksetzen“ gedrückt wird
(Befehl `reset`) oder am Gerät der Serial-Befehl `r` kommt. Für jeden Fehler, der wegfällt,
sendet das Gerät `error_cleared` mit demselben `reason`.

## 5. Pflanzenprofil (Feld `profile` in der Poll-Antwort)

Das Gerät kennt die Pflanzendatenbank **nicht**. Der Server berechnet einige Zahlen und sendet sie:

```json
{
  "profile_ver": 3,
  "plant_id": 9012,
  "name": "Golden barrel cactus",
  "mode": "normal",
  "moist_min": 15,
  "moist_target": 35,
  "moist_max": 55,
  "dose_ml": 120,
  "min_pause_h": 216
}
```

| Feld | Was das Gerät tut | Quelle in der DB | Zulässiger Bereich |
|---|---|---|---|
| `profile_ver` | speichert ihn und gibt ihn im Status zurück | Zähler auf dem Server, +1 bei jeder Änderung | ≥ 1 |
| `mode` | nur für Anzeige und Logs | `normal` oder `eco` (Abschnitt 7) | – |
| `moist_min` | unterhalb dieses Werts beginnt das Gießen | `soil_moisture.minimum_before_watering_percent` | 0–100 |
| `moist_target` | gibt Portionen, bis dieser Wert erreicht ist | `soil_moisture.recommended_target_percent` (im Modus `eco` niedriger, Abschnitt 7) | > `moist_min` |
| `moist_max` | ab diesem Wert schaltet die Pumpe niemals ein | `soil_moisture.maximum_safe_percent` | > `moist_target`, ≤ 100 |
| `dose_ml` | Volumen einer Portion | `pump_calculation`: `base_ml_per_liter_of_soil × pot_liters`, begrenzt auf `minimum_water_ml … maximum_water_ml` und auf 150 | 10–150 (die Pumpe schafft in ihrem Zeitlimit von 10 s nicht mehr, `DOSE_MAX_ML`) |
| `min_pause_h` | Mindestzeit zwischen zwei Gießzyklen (Schutz vor Sensordrift) | `watering_cycle.minimum_days × 24 × 0,5` | 0–720 |
| `name` | nur für Anzeige und Logs, max. 31 Zeichen | `common_name` | – |

- `pot_liters` legt der Benutzer im Bot fest, gespeichert auf dem Server.

- Liegt ein Wert außerhalb des Bereichs, behält das Gerät das alte Profil und
  sendet `profile_rejected`. Andernfalls speichert es das Profil im NVS und sendet `profile_ok`.
- Der Server sendet das Profil bei jeder Abfrage, bis der Status die neue `profile_ver` zeigt.
- **Der Modus `eco` ist für das Gerät einfach ein anderes Profil.** Das Gerät braucht
  keine eigene Logik: Der Server schickt einen niedrigeren `moist_target` und eine kleinere
  `dose_ml` (Abschnitt 7).

## 6. Befehle (Feld `commands` in der Poll-Antwort)

Der Benutzer **kann nicht** aus der Ferne gießen (Abschnitt 1). Es bleiben nur Servicebefehle:

```json
{ "cmd": "reset" }
```

| `cmd` | Aktion | Abgelehnt, wenn |
|---|---|---|
| `reset` | `ERROR` verlassen und sofort den Boden prüfen | Phase ist nicht `ERROR` |
| `factory_reset` | Profil aus dem Flash löschen, zurück zu den Standardwerten | nie |

`factory_reset` kommt, wenn das Gerät im Bot zurückgesetzt wurde („Gerät zurücksetzen“
oder Admin-Befehl `/admin_reset`) und auf dem Server kein Profil mehr hat. Der Server sendet
den Befehl bei jeder Abfrage, solange das Gerät noch `profile_ver` > 0 meldet. So kommt er auch an,
wenn das Gerät während des Zurücksetzens offline war.

Gießen für Test und Vorführung: Serial-Befehl `w`, nicht über den Bot.

Nach `reset` antwortet der Bot sofort: „Angenommen, BloomBot führt es beim nächsten Kontakt aus“
(auf der Messe bis ~1 min, sonst bis ~10 min).
Das eigentliche Ergebnis kommt als Ereignis `error_cleared`.

## 7. Prognose des Wasservorrats und Urlaubsmodus (nur Server)

> **Stand:** umgesetzt im Worker (Branch `integration`). Vor dem Deploy einmal
> `server/migrations/0001_water_warned.sql` in der D1-Konsole ausführen.

### Prognose

```
remaining_ml = (tank_pct − 5) / 100 × tank_capacity_ml     # 5 % = Reserve, die die Pumpe nicht ansaugt
daily_ml     = im Messfenster gepumpt / Länge des Messfensters in Tagen
days_left    = remaining_ml / daily_ml
```

- **Messfenster:** die letzten max(7, 2 × `days_max`) Tage, höchstens 60, aber erst ab
  `forecast_since` (neue Pflanze oder anderer Modus → neu anfangen). Tage ohne Gießen zählen mit.

- Den Verbrauch in ml berechnet der Server aus der Differenz von `total_pumped_ml`
  zwischen den Abfragen. Ist der Wert gesunken, wurde das Gerät neu gestartet:
  Der neue Wert zählt vollständig als Verbrauch.
- Solange das Fenster kürzer als max(2, `days_max`) Tage ist (mindestens ein ganzer Gießzyklus)
  oder darin noch nichts gepumpt wurde, wird eine Schätzung aus der DB verwendet:
  `daily_ml ≈ dose_ml / ((watering_cycle.minimum_days + maximum_days) / 2)`.
  Sonst würde z. B. ein Kaktus, der in den ersten 5 Tagen nicht gegossen wurde, „0 ml pro Tag“ zeigen.
- Der Server rechnet intern immer mit dem Verbrauch im Modus `normal`; `eco` = × 0,7.
  Wurde im `eco`-Modus gemessen, wird durch 0,7 geteilt.
- Der Verbrauch wird pro Tag (deutsche Zeit) in der Tabelle `consumption` summiert.
- Als Bereich anzeigen (±20 %), z. B. „etwa 8–13 Tage“: Der Verbrauch hängt vom
  Wetter, der Heizung und dem Wachstum der Pflanze ab. Über 60 Tage: „mehr als 60 Tage“.
  „Status“ zeigt dazu, ob es eine Schätzung ist oder nach bisherigem Verbrauch berechnet.

Benachrichtigungen:
- bei `tank_refilled` und `tank_low`: die Nachricht bekommt „Das Wasser reicht für etwa N Tage“;
- wenn `days_left` < 3: „Das Wasser reicht nur noch für etwa N Tage“, **einmal pro Tankfüllung**
  (`devices.water_warned`, wird bei `tank_refilled` und beim Zurücksetzen wieder 0);
- geprüft wird bei jeder Abfrage des Geräts, ein Cron Trigger ist nicht nötig;
- einmal pro Woche eine kurze Zusammenfassung (optional, später).

### Urlaubsmodus

Der Benutzer gibt nur das **Rückkehrdatum** an: Button „Urlaub“ → 3 Tage / 1 Woche /
2 Wochen / 3 Wochen, oder `/urlaub 24.12.`. Alles andere entscheidet das System selbst:

1. `days_left` ≥ Tage bis zur Rückkehr + 1 Tag Reserve → „Alles in Ordnung, das Wasser reicht bis ~25.11.“
2. Reicht nicht, aber im `eco`-Modus schon → der Bot schlägt vor: „Das Wasser reicht nur bis 16.11.
   Im Sparmodus bis ~22.11. Einschalten?“
3. Reicht nicht einmal im `eco`-Modus → sagt es direkt: „Selbst im Sparmodus reicht das Wasser nicht.
   Bitten Sie jemanden, nachzufüllen, oder stellen Sie einen größeren Tank auf.“

In den Fällen 2 und 3 wählt der Benutzer per Button: Sparmodus an oder normal.
Am Rückkehrtag (bei der ersten Abfrage an diesem Tag) endet der Urlaubsmodus automatisch,
`eco` wird ausgeschaltet (wieder `normal`), alle mit Zugriff bekommen „Willkommen zurück!“.
„Urlaub beenden“ beendet ihn sofort.

Modus `eco`:
- `moist_target = moist_min + round((moist_target − moist_min) × 0.3)`, aber mindestens
  `moist_min + 3` (Koeffizient **TBD**). Der Boden wird an der unteren sicheren
  Grenze für diese Pflanze gehalten.
- `dose_ml × 0.7` (mindestens 10). Ohne kleinere Portion würde eine große Portion
  über das niedrigere Ziel hinausschießen, und es würde nichts gespart.
- Auch ohne Urlaub ein- und ausschaltbar: Button „Sparmodus an/aus“ in „Status“.
- Die Einsparung wird anfangs als `daily_ml × 0.7` geschätzt (**TBD**). Nach einigen Tagen
  wird die Prognose anhand des tatsächlichen Verbrauchs neu berechnet.
- Der Benutzer legt Gießmenge oder -häufigkeit **nicht** manuell fest: Die Werte bleiben
  innerhalb der Grenzen, die laut DB für die Pflanze zulässig sind.

### Befehle in Telegram

| Button / Befehl | Wer | Aktion |
|---|---|---|
| Status | alle | letzter Status + Alter („vor 4 min“), Fehler, Wasservorrat in Tagen, Urlaub/Sparmodus |
| Pflanze | Besitzer | „Meine Pflanzen“ oder Suche im Katalog, Topf wählen, Profil speichern |
| Topf | Besitzer | Topfvolumen in Litern |
| Teilen | Besitzer | Einladungslink (24 h, einmal nutzbar), Mitglieder entfernen |
| Fehler zurücksetzen | Besitzer und Mitglieder | Befehl `reset`; in der Fehlermeldung und in „Status“, nur bei `still_dry` / `pump_timeout` |
| Umbenennen, `/name <Name>` | Besitzer | Gerätename (max. 24 Zeichen) |
| Gerät zurücksetzen | Besitzer | alle verlieren den Zugriff, Pflanze und Profil weg, Name wieder „BloomBot“; das Gerät bekommt `factory_reset` |
| QR-Code / `/link <Code>` | alle | Gerät koppeln (`/start pair_<Code>` aus dem QR-Code), wer koppelt, wird Besitzer |
| `/konto_loeschen` | alle | eigenes Konto und alle Daten löschen; eigene Geräte werden zurückgesetzt |
| `/admin_reset [id]` | nur `ADMIN_IDS` | Gerät zurücksetzen, egal wem es gehört (für Vorführungen) |
| Urlaub, `/urlaub TT.MM.` | Besitzer | Rückkehrdatum → Prüfung → bei Bedarf `eco`; während des Urlaubs: „Urlaub beenden“ |
| Sparmodus an / aus | Besitzer | Button in „Status“, `eco` ohne Urlaub |

## 8. Gießlogik auf dem Gerät (zum Verständnis)

```
jede Stunde:
  wenn Feuchte < moist_min und seit dem letzten Zyklus ≥ min_pause_h vergangen:
      wiederholen (max. 3 Portionen):
          dose_ml gießen  →  10 min warten (Einsickern)  →  messen
          Stopp, wenn Feuchte ≥ moist_target
      nach allen Portionen immer noch < moist_min → ERROR (still_dry)
die Pumpe schaltet niemals ein, wenn: Tank leer (≤ 5 %), Feuchte ≥ moist_max
oder das Laufzeitlimit der Pumpe überschritten würde
```

**Feuchteskala:** Die Bodenfeuchte ist ein **Feuchte-Index von 0 bis 100**, keine physikalischen
Prozent. Ein resistiver Sensor misst die Leitfähigkeit, nicht den Wassergehalt. Er wird in echter
Erde kalibriert: 0 = trockene Erde, 100 = frisch gegossen und abgetropft. Die Werte aus der DB
(`moist_min`, `moist_target`, `moist_max`) werden auf diesen Index angewendet. Die ersten Tests
zeigen, ob die DB-Werte korrigiert werden müssen.

## 9. Offene Fragen

- **TBD** Koeffizient für `min_pause_h`
- **TBD** Koeffizienten des `eco`-Modus (Feuchteniveau 0,3, Portion 0,7, Einsparung 0,7) – in der Praxis prüfen
- **TBD** tatsächliches Tankvolumen → `TANK_CAPACITY_ML` in `config.h`
- **TBD** Schema der Geräte-ID (fester Name oder nach MAC) und ein eigener Schlüssel pro Gerät
  (`devices.key_hash`; zurzeit haben alle Geräte den gemeinsamen Schlüssel `DEVICE_KEY`)
- **Geplant:** Auswahl des Geräts im Bot, wenn ein Nutzer mehrere Geräte besitzt
- Tanksensor HC-SR04 ist nicht wasserdicht: vor Spritzwasser und Kondenswasser schützen