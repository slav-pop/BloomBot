# BloomBot – Review des Cloudflare Workers

**Für:** Luca · **Von:** Vladylen · **Stand:** 04.11.2026
**Geprüft:** Worker `bloombot` (Deploy vom 04.11.2026, 08:38), D1 `bloombot-db`, Bindings, Secrets, Cron
**Verglichen mit:** `Integration.md` (v0.3), `Kommunikation_wie_warum.md`, `data/plants.json` und `app/` aus PR #1
**Alles nur gelesen:** Es wurde nichts geändert und nichts deployt.

---

## TL;DR

Der Worker ist **sauber und sicher geschrieben**, aber er ist ein **„Phase 1: monitoring only“-Prototyp mit eigener API**.
Er setzt das Protokoll aus `Integration.md` **nicht** um. Mehrere Aussagen in `Kommunikation_wie_warum.md`
(sofortige Ereignisse, Poll alle 10 Minuten, `poll_s` auf 60 s für die Messe, Offline-Meldung nach 30 Minuten)
funktionieren mit dem aktuellen Code nicht.

Wir müssen uns entscheiden:

- **Variante A:** Worker auf v0.3 bringen. Das ist die Empfehlung, der Plan steht unten.
- **Variante B:** „Phase 1 Monitoring“ als eigene Stufe in `Integration.md` dokumentieren und die Erklär-Doku anpassen.

Unabhängig davon gilt: **Der Worker-Code gehört ins Git-Repo.** Im Moment existiert er nur im Cloudflare-Dashboard.

---

## 1. Was im Cloudflare-Account läuft

| Ressource | Stand |
|---|---|
| Worker | `bloombot`, nur `fetch`-Handler, `compatibility_date` 2026-11-04, Observability an |
| URL | `https://bloombot.cloudflare-condone897.workers.dev` |
| Endpunkte | `GET /health`, `POST /api/telemetry`, `POST /telegram` |
| Bindings | `DB` (D1 `bloombot-db`), `AUTHORIZED_USER_ID` (plain text) |
| Secrets | `BOT_TOKEN`, `DEVICE_TOKEN`, `WEBHOOK_SECRET` ✅ (als Secrets, nicht im Code) |
| Cron Triggers | **keine** |
| D1-Tabellen | `plants(id, name, dry_threshold)`, `readings(sensor_id, moisture_percent, raw, received_at)`, `device_state(id, tank_percent, pump_on, error, received_at)` |
| D1-Inhalt | `plants`: `soil-1`, `soil-2` (Schwelle 30). `readings` und `device_state` sind **leer** (der ESP32 hat noch nie gesendet) |

Live geprüft: `GET /health` → `{"service":"BloomBot","phase":"monitoring"}`, `POST /api/device/bloombot-01/poll` → **404**.

---

## 2. Abgleich mit Integration.md und Kommunikation_wie_warum.md

| # | Spezifikation (Integration.md v0.3) | Worker heute | Folge | Prio |
|---|---|---|---|---|
| 1 | `POST /api/device/<id>/poll` | fehlt, stattdessen `POST /api/telemetry` | ESP32 nach Spezifikation bekommt 404 | 🔴 |
| 2 | Poll-Antwort `{poll_s, commands, profile}` (§2) | Antwort `{ok, received_at}` | kein Pflanzenprofil zum Gerät, kein `reset`, `poll_s` nicht steuerbar (Messe 60 s geht nicht) | 🔴 |
| 3 | `POST /api/device/<id>/event` (§4) | fehlt | keine sofortigen Benachrichtigungen („Tank leer“, „Fehler“). Das ist der Kern von `Kommunikation_wie_warum.md` | 🔴 |
| 4 | Status-Body §3: `moisture_pct`, `tank_pct`, `tank_low`, `phase`, `total_pumped_ml`, `plant_id`, `profile_ver`, `attempts`, `last_watered_s_ago`, `rssi` … | `device_id`, `pump_on`, `tank_percent`, `error`, `plants[{sensor_id, moisture_percent, raw}]` | andere Feldnamen. Ohne `phase`, `total_pumped_ml` und `profile_ver` gibt es keine Prognose und keinen Profilabgleich | 🔴 |
| 5 | Pflanzen-DB `data/plants.json`, Profil nach §5 | D1 `plants(id, name, dry_threshold)` mit `soil-1`/`soil-2` | unsere Pflanzendatenbank wird nicht genutzt, es gibt kein Profil | 🔴 |
| 6 | Verlauf für die Wasserprognose (§7: 7 Tage `total_pumped_ml`) | `readings`/`device_state` werden überschrieben (Upsert) | Prognose „reicht für N Tage“ und Urlaubsmodus sind unmöglich | 🟠 |
| 7 | Offline nach 3 × `poll_s` = 30 min (§2) | `STALE_SECONDS = 180` (3 min) | bei Poll alle 10 min zeigt der Bot fast immer „Daten veraltet“ | 🟠 |
| 8 | Cron prüft offline → „BloomBot ist nicht erreichbar“ | kein `scheduled`-Handler, keine Cron Triggers | der Bot meldet einen Ausfall nie von selbst | 🟠 |
| 9 | Geräte-ID im Pfad, z. B. `bloombot-01` | fest im Body `device_id === 'esp32-1'` | Name weicht ab, nur ein Gerät möglich | 🟡 |
| 10 | Bot-Buttons: Status, Pflanze, Topf, Urlaub, Urlaub aus, Fehler zurücksetzen (§7) | `/status`, `/pflanzen`, `/help`, `/getmyid` | Pflanzenwahl, Topf, Urlaub und Reset fehlen | 🟠 |
| 11 | Befehlswarteschlange, Befehle älter als 30 min verwerfen (§2, §6) | nicht vorhanden | „Fehler zurücksetzen“ ist nicht möglich | 🟠 |
| 12 | Fehlergrund als Enum: `still_dry`, `pump_timeout`, `sensor_tank` (§4) | freier Text `error` (≤ 160 Zeichen) | Bot-Texte nicht pro Fehlerart möglich | 🟡 |
| 13 | Gerät-Key heißt `DEVICE_KEY` (`src/secrets.h`) | Secret heißt `DEVICE_TOKEN` | nur Benennung, in Doku oder Code angleichen | 🔵 |

Legende: 🔴 blockiert die Spezifikation · 🟠 Funktion fehlt · 🟡 Inkonsistenz · 🔵 Kleinigkeit

---

## 3. Code-Qualität: was gut ist

- Secrets liegen als Cloudflare Secrets vor und stehen nicht im Code.
- Secret-Vergleich in konstanter Zeit (SHA-256 + XOR), also kein Timing-Leak.
- Die Body-Größe ist auf 16 KB begrenzt (Stream wird gelesen und abgebrochen).
- `validateTelemetry` prüft streng: Typen, Bereiche 0–100, `raw` 0–4095, keine doppelten Sensoren.
- D1 nur mit Prepared Statements und `batch()`: keine SQL-Injection, atomar.
- Der Webhook prüft `X-Telegram-Bot-Api-Secret-Token`, wie spezifiziert.
- Zugriff nur für eine Telegram-ID und nur im privaten Chat.
- Telegram-Aufrufe haben einen Timeout von 8 s, Fehler-Logs enthalten keine Tokens.
- `received_at` kommt vom Server, weil der ESP32 keine Uhr hat. Das ist richtig.

---

## 4. Bugs und Risiken im aktuellen Code

### 4.1 🔴 Code nicht im Repository
Der Worker wurde über das Dashboard hochgeladen (`worker.js`). Es gibt kein `wrangler.jsonc`, keine D1-Migrationen und keine Tests im Repo.
Änderungen sind für das Team nicht sichtbar, und es gibt kein Review und kein Rollback.
**Fix:** Ordner `server/` im Repo anlegen (Vorschlag in Abschnitt 6.1), deployen nur noch mit `wrangler deploy`.
Achtung: Das Repo `slav-pop/BloomBot` ist **seit heute öffentlich**. Secrets nur mit `wrangler secret put` setzen, niemals committen.

### 4.2 🟠 Telegram wiederholt Updates bei Fehlern
Wenn `sendMessage` fehlschlägt, antwortet der Worker mit 503. Telegram schickt jedes Update, das nicht mit 2xx beantwortet wird, erneut.
Die Folge sind doppelte oder verspätete Bot-Antworten.
**Fix:** `/telegram` antwortet nach erfolgreicher Secret-Prüfung **immer mit 200**. Fehler nur loggen:

```js
if (path === '/telegram') {
  try { await webhook(body, env) }
  catch { console.error(JSON.stringify({ message: 'telegram webhook failed' })) }
  return json({ ok: true })
}
```

### 4.3 🟡 `/getmyid` kann nie helfen
Nur der bereits autorisierte Benutzer darf `/getmyid` ausführen. Wer seine ID noch nicht kennt, bekommt keine Antwort.
**Fix:** `/getmyid` vor die Berechtigungsprüfung ziehen. Die Antwort enthält nur die eigene ID, das ist unkritisch.

### 4.4 🟡 `STALE_SECONDS` passt nicht zum Poll-Intervall
180 s widerspricht den 10 Minuten aus beiden Dokumenten.
**Fix:** `stale = 3 × poll_s` (bei 600 s also 1800 s), am besten aus derselben Konstante wie `poll_s`.

### 4.5 🔵 Missverständlicher Text
`über Grenzwert` erscheint, wenn die Erde **feucht genug** ist. Besser: `ok`.

---

## 5. Was aus PR #1 (UI + Datenbank) direkt nutzbar ist

- **`data/plants.json`**: 55 Pflanzen, genau die Felder, die §5 braucht. Ein Test im Repo sichert ab, dass keine Felder fehlen oder dazukommen und alle Werte in den erlaubten Bereichen liegen. Die Datei ist ca. 34 KB groß und kann direkt in den Worker importiert werden (`import plants from '../data/plants.json'`; Wrangler bündelt JSON).
- **Profil-Berechnung**: Die Funktion unten ist die Server-Version von `app/src/lib/deviceProfile.js`, auf die DB-Felder (snake_case) umgeschrieben und um den `eco`-Modus aus §7 ergänzt.
  **Getestet** gegen alle 55 Pflanzen × 5 Topfgrößen × 2 Modi = 550 Profile, **alle gültig** nach den Bereichen aus §5.

```js
// server/src/profile.js – Integration.md §5 + eco aus §7
const DOSE = { min: 10, max: 500 } // zulässiger Bereich laut §5
const clamp = (v, lo, hi) => Math.min(hi, Math.max(lo, v))

export function buildProfile(plant, { potLiters, mode = 'normal', profileVer }) {
  const { soil_moisture: m, watering_cycle: c, pump_calculation: p } = plant.irrigation_profile
  const min = m.minimum_before_watering_percent
  const target = mode === 'eco'
    ? Math.max(min + 3, min + Math.round((m.recommended_target_percent - min) * 0.3)) // Koeffizient 0,3 TBD
    : m.recommended_target_percent
  const lo = clamp(p.minimum_water_ml, DOSE.min, DOSE.max)
  const hi = clamp(p.maximum_water_ml, lo, DOSE.max)
  return {
    profile_ver: profileVer,
    plant_id: plant.id,
    name: [...plant.common_name].slice(0, 31).join(''), // max. 31 Zeichen
    mode,
    moist_min: min,
    moist_target: target,
    moist_max: m.maximum_safe_percent,
    dose_ml: Math.round(clamp(p.base_ml_per_liter_of_soil * potLiters, lo, hi)),
    min_pause_h: clamp(Math.round(c.minimum_days * 24 * 0.5), 0, 720), // Koeffizient 0,5 TBD
  }
}
```

Beispiel: Pflanze 711, Topf 2 l → `dose_ml 320`, `moist 45/65/85`, `min_pause_h 36`. Im `eco`-Modus wird `moist_target` zu 51.
Das sind dieselben Werte, die die App anzeigt.

---

## 6. Vorschlag für den Umbau auf v0.3 (Variante A)

### 6.1 Struktur im Repo

```
server/
  wrangler.jsonc
  migrations/0001_init.sql
  src/worker.js        # Routing, Auth (vorhandener Code bleibt größtenteils)
  src/profile.js       # buildProfile (oben)
  src/telegram.js
  test/                # Vitest (@cloudflare/vitest-pool-workers)
```

```jsonc
// server/wrangler.jsonc
{
  "name": "bloombot",
  "main": "src/worker.js",
  "compatibility_date": "2026-11-04",
  "d1_databases": [
    { "binding": "DB", "database_name": "bloombot-db", "database_id": "772d29fb-fa40-430f-beac-3f00e6e5827e", "migrations_dir": "migrations" }
  ],
  "triggers": { "crons": ["*/10 * * * *"] },
  "vars": { "AUTHORIZED_USER_ID": "<Telegram-ID>" },
  "observability": { "enabled": true }
}
// Secrets: npx wrangler secret put BOT_TOKEN / DEVICE_TOKEN / WEBHOOK_SECRET
```

### 6.2 D1-Schema (Vorschlag)

```sql
-- server/migrations/0001_init.sql
CREATE TABLE devices (
  id            TEXT PRIMARY KEY,              -- z. B. 'bloombot-01'
  poll_s        INTEGER NOT NULL DEFAULT 600 CHECK (poll_s BETWEEN 60 AND 3600),
  last_seen     INTEGER,                       -- Unix-Sekunden (Serverzeit)
  last_status   TEXT,                          -- letzter Status-Body (JSON, §3)
  offline_sent  INTEGER NOT NULL DEFAULT 0     -- „nicht erreichbar“ schon gemeldet?
);

CREATE TABLE settings (
  device_id      TEXT PRIMARY KEY REFERENCES devices(id),
  plant_id       INTEGER,                      -- id aus data/plants.json
  pot_liters     REAL CHECK (pot_liters > 0 AND pot_liters <= 50),
  mode           TEXT NOT NULL DEFAULT 'normal' CHECK (mode IN ('normal', 'eco')),
  vacation_until TEXT,                         -- ISO-Datum
  profile_ver    INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE status_history (                  -- für die Prognose (§7)
  device_id        TEXT NOT NULL REFERENCES devices(id),
  received_at      INTEGER NOT NULL,
  total_pumped_ml  INTEGER,
  tank_pct         INTEGER,
  moisture_pct     INTEGER
);
CREATE INDEX idx_history_device_time ON status_history(device_id, received_at);

CREATE TABLE events (
  id          INTEGER PRIMARY KEY AUTOINCREMENT,
  device_id   TEXT NOT NULL REFERENCES devices(id),
  type        TEXT NOT NULL,
  payload     TEXT NOT NULL,                   -- Ereignis-JSON (§4)
  received_at INTEGER NOT NULL
);

CREATE TABLE commands (                        -- §6, max. 30 min gültig
  id          INTEGER PRIMARY KEY AUTOINCREMENT,
  device_id   TEXT NOT NULL REFERENCES devices(id),
  cmd         TEXT NOT NULL CHECK (cmd IN ('reset')),
  created_at  INTEGER NOT NULL
);
```

`status_history` wächst. Per Cron Einträge löschen, die älter als 14 Tage sind.

### 6.3 Routen

| Route | Logik |
|---|---|
| `POST /api/device/:id/poll` | Auth → Status validieren (§3) → `devices.last_seen`, `last_status` und `status_history` schreiben → Befehle jünger als 30 min holen und löschen → `profile` mitsenden, wenn `status.profile_ver < settings.profile_ver` → Antwort `{ poll_s, commands, profile }` |
| `POST /api/device/:id/event` | Auth → Ereignis validieren (§4) → in `events` speichern → Telegram-Nachricht für `tank_low`, `tank_empty`, `tank_refilled`, `error`, `error_cleared`, `profile_ok` (`watered` und `boot` nur ins Log) |
| `POST /telegram` | wie heute, aber immer 200 (4.2). Buttons aus §7: Pflanze (Suche in `plants.json`), Topf, Urlaub, Urlaub aus, Fehler zurücksetzen (nur in `ERROR`) |
| `scheduled` (Cron) | Offline-Check: `now − last_seen > 3 × poll_s` und noch nicht gemeldet → „BloomBot ist nicht erreichbar“. Am Rückkehrtag `eco` → `normal`. Verlauf aufräumen |

Bei Änderungen von Pflanze, Topf oder Modus gilt: `settings.profile_ver += 1`. Der nächste Poll liefert dann das neue Profil.

### 6.4 Gerät-ID und Pfad

Die ID wird aus dem Pfad gelesen und gegen eine Allowlist geprüft, statt `'esp32-1'` fest einzutragen:

```js
const m = path.match(/^\/api\/device\/([a-z0-9-]{1,32})\/(poll|event)$/)
```

Den Namen (`bloombot-01` oder `esp32-1`) einmal festlegen und überall gleich verwenden: Doku, `secrets.h`, Worker.

---

## 7. Offene Fragen ans Team

| Thema | Konflikt | Wer |
|---|---|---|
| Portionen pro Zyklus | Integration.md §8: **3**, `src/config.h` `MAX_WATER_ATTEMPTS`: **2** | Firmware + Doku |
| Maximale Dosis | §5 erlaubt `dose_ml` bis **500 ml**, aber `PUMP_MAX_ON_MS` 10 s × 23 ml/s ≈ **230 ml** pro Lauf. Größere Dosen enden in `pump_timeout` | Firmware (Limit erhöhen oder Dosis teilen) bzw. §5 auf 230 senken |
| Ein oder zwei Feuchtesensoren | Status §3 kennt nur ein `moisture_pct`, der Worker erwartet `plants[]` mit `soil-1`/`soil-2`, die Firmware liest einen Sensor (GPIO 34) | alle |
| Profil auf dem Gerät | Die Firmware nutzt noch Konstanten (`THRESHOLD_DRY 30`, `WATER_DOSE_ML 50`) statt eines Profils aus NVS | Firmware |
| Variante A oder B | v0.3 umsetzen oder „Phase 1 Monitoring“ dokumentieren | Luca + Team |

---

## 8. Sicherheit

- **Zugangsdaten wurden im Chat geteilt** (E-Mail, Passwort, 2FA-Secret). Bitte **Passwort ändern und 2FA neu einrichten**. Teammitglieder besser über *Manage Account → Members* einladen oder ihnen API-Tokens mit minimalen Rechten geben, statt den Login zu teilen.
- Für dieses Review wurde ein Wrangler-OAuth-Login auf Vladylens Mac angelegt. Er wird mit `wrangler logout` wieder entfernt.
- Das Repo ist **öffentlich**. Kein `BOT_TOKEN`, `DEVICE_TOKEN` oder `WEBHOOK_SECRET` in Commits, `.dev.vars` in `.gitignore` aufnehmen.

---

## 9. Checkliste für Luca

- [ ] Worker-Code nach `server/` ins Repo, mit `wrangler.jsonc` und Migration (6.1, 6.2)
- [ ] Entscheidung Variante A oder B mit dem Team
- [ ] `/telegram` immer 200 (4.2)
- [ ] `/getmyid` vor die Berechtigungsprüfung (4.3)
- [ ] `/api/device/:id/poll` mit `{poll_s, commands, profile}` (6.3)
- [ ] `/api/device/:id/event` mit Telegram-Benachrichtigungen (6.3)
- [ ] `data/plants.json` + `buildProfile` einbinden (5)
- [ ] `status_history` für die Prognose, Urlaubsmodus (§7)
- [ ] Cron: Offline-Meldung, `eco` zurücksetzen, Verlauf aufräumen (6.3)
- [ ] Offline-Grenze `3 × poll_s` statt 180 s (4.4)
- [ ] Gerät-ID und Secret-Namen mit Doku und Firmware angleichen (6.4, Tabelle #13)
- [ ] Passwort und 2FA erneuern (8)
