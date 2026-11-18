// BloomBot server - data model v2 (users, devices, sharing, my plants)
// See doc "BloomBot – Datenmodell".
// Bindings: D1 database "DB"
// Secrets:  DEVICE_KEY, TELEGRAM_TOKEN, TELEGRAM_SECRET

const POLL_S = 60;        // demo: 60 s, normal use: 600
const DEFAULT_DEVICE_NAME = "BloomBot";   // name after "Gerät zurücksetzen"
const MAX_NAME_LEN = 24;
const COMMAND_TTL_S = 30 * 60;   // commands older than 30 min are dropped, not sent (Integration.md section 2)

// Protection
const MAX_DEVICE_BODY = 16 * 1024;     // bytes; a status is ~0.5 KB
const MAX_TELEGRAM_BODY = 64 * 1024;   // bytes; Telegram updates can be a few KB
const TELEGRAM_TIMEOUT_MS = 8000;      // give up waiting for the Telegram API

// Pump: ~23 ml/s, PUMP_MAX_ON_MS = 10 s -> one portion must stay well below 230 ml
const DOSE_MAX_ML = 150;
const DEFAULT_POT_L = 1;
const POT_SIZES = [0.5, 1, 2, 3, 5, 10];
const PAUSE_FACTOR = 0.5; // min_pause_h = minimum_days * 24 * PAUSE_FACTOR (TBD)
const INVITE_TTL_S = 24 * 3600;

// Plant catalog (generated from data/plants.json, do not edit by hand)
const PLANTS = [
  {"id": 2275, "de": "Alpenveilchen", "en": "cyclamen", "sci": "Cyclamen persicum", "min": 30, "target": 50, "max": 70, "days_min": 6, "days_max": 9, "ml_per_l": 120, "ml_min": 80, "ml_max": 600},
  {"id": 1820, "de": "Bergpalme", "en": "parlor palm", "sci": "Chamaedorea elegans", "min": 30, "target": 50, "max": 70, "days_min": 6, "days_max": 9, "ml_per_l": 120, "ml_min": 80, "ml_max": 600},
  {"id": 2954, "de": "Birkenfeige", "en": "weeping fig", "sci": "Ficus benjamina", "min": 30, "target": 50, "max": 70, "days_min": 6, "days_max": 9, "ml_per_l": 120, "ml_min": 80, "ml_max": 600},
  {"id": 9005, "de": "Bogenhanf", "en": "Snake plant", "sci": "Sansevieria trifasciata", "min": 15, "target": 35, "max": 55, "days_min": 14, "days_max": 21, "ml_per_l": 80, "ml_min": 50, "ml_max": 400},
  {"id": 2468, "de": "Dieffenbachie", "en": "dumb cane", "sci": "Dieffenbachia seguine", "min": 30, "target": 50, "max": 70, "days_min": 6, "days_max": 9, "ml_per_l": 120, "ml_min": 80, "ml_max": 600},
  {"id": 2531, "de": "Drachenbaum", "en": "dragontree", "sci": "Dracaena marginata", "min": 30, "target": 50, "max": 70, "days_min": 6, "days_max": 9, "ml_per_l": 120, "ml_min": 80, "ml_max": 600},
  {"id": 2528, "de": "Duftender Drachenbaum", "en": "fragrant dracaena", "sci": "Dracaena fragrans", "min": 30, "target": 50, "max": 70, "days_min": 6, "days_max": 9, "ml_per_l": 120, "ml_min": 80, "ml_max": 600},
  {"id": 9013, "de": "Echeverie", "en": "Mexican snowball", "sci": "Echeveria elegans", "min": 15, "target": 35, "max": 55, "days_min": 12, "days_max": 18, "ml_per_l": 80, "ml_min": 50, "ml_max": 400},
  {"id": 728, "de": "Echte Aloe", "en": "aloe", "sci": "Aloe vera", "min": 15, "target": 35, "max": 55, "days_min": 10, "days_max": 16, "ml_per_l": 80, "ml_min": 50, "ml_max": 400},
  {"id": 2773, "de": "Efeutute", "en": "golden pothos", "sci": "Epipremnum aureum", "min": 30, "target": 50, "max": 70, "days_min": 6, "days_max": 9, "ml_per_l": 120, "ml_min": 80, "ml_max": 600},
  {"id": 9003, "de": "Einblatt", "en": "Peace lily", "sci": "Spathiphyllum wallisii", "min": 45, "target": 65, "max": 85, "days_min": 4, "days_max": 6, "ml_per_l": 160, "ml_min": 100, "ml_max": 800},
  {"id": 1192, "de": "Elefantenfuß", "en": "ponytail palm", "sci": "Beaucarnea recurvata", "min": 30, "target": 50, "max": 70, "days_min": 6, "days_max": 9, "ml_per_l": 120, "ml_min": 80, "ml_max": 600},
  {"id": 9001, "de": "Fensterblatt", "en": "Swiss cheese plant", "sci": "Monstera deliciosa", "min": 30, "target": 50, "max": 70, "days_min": 7, "days_max": 9, "ml_per_l": 120, "ml_min": 80, "ml_max": 600},
  {"id": 855, "de": "Flamingoblume", "en": "anthurium", "sci": "Anthurium andraeanum", "min": 30, "target": 50, "max": 70, "days_min": 6, "days_max": 9, "ml_per_l": 120, "ml_min": 80, "ml_max": 600},
  {"id": 543, "de": "Frauenhaarfarn", "en": "maidenhair fern", "sci": "Adiantum capillus-veneris", "min": 30, "target": 50, "max": 70, "days_min": 2, "days_max": 3, "ml_per_l": 120, "ml_min": 80, "ml_max": 600},
  {"id": 2963, "de": "Geigenfeige", "en": "fiddle-leaf fig", "sci": "Ficus lyrata", "min": 30, "target": 50, "max": 70, "days_min": 6, "days_max": 9, "ml_per_l": 120, "ml_min": 80, "ml_max": 600},
  {"id": 9004, "de": "Glücksfeder", "en": "ZZ plant", "sci": "Zamioculcas zamiifolia", "min": 15, "target": 35, "max": 55, "days_min": 14, "days_max": 21, "ml_per_l": 80, "ml_min": 50, "ml_max": 400},
  {"id": 9010, "de": "Glückskastanie", "en": "Money tree", "sci": "Pachira aquatica", "min": 30, "target": 50, "max": 70, "days_min": 7, "days_max": 9, "ml_per_l": 120, "ml_min": 80, "ml_max": 600},
  {"id": 2568, "de": "Goldfruchtpalme", "en": "bamboo palm", "sci": "Dypsis lutescens", "min": 30, "target": 50, "max": 70, "days_min": 6, "days_max": 9, "ml_per_l": 120, "ml_min": 80, "ml_max": 600},
  {"id": 9012, "de": "Goldkugelkaktus", "en": "Golden barrel cactus", "sci": "Echinocactus grusonii", "min": 15, "target": 35, "max": 55, "days_min": 18, "days_max": 25, "ml_per_l": 80, "ml_min": 50, "ml_max": 400},
  {"id": 1846, "de": "Grünlilie", "en": "spider plant", "sci": "Chlorophytum 'Bonnie'", "min": 30, "target": 50, "max": 70, "days_min": 6, "days_max": 9, "ml_per_l": 120, "ml_min": 80, "ml_max": 600},
  {"id": 2961, "de": "Gummibaum", "en": "Rubber plant", "sci": "Ficus elastica", "min": 30, "target": 50, "max": 70, "days_min": 6, "days_max": 9, "ml_per_l": 120, "ml_min": 80, "ml_max": 600},
  {"id": 9015, "de": "Guzmanie", "en": "Scarlet star bromeliad", "sci": "Guzmania lingulata", "min": 15, "target": 35, "max": 55, "days_min": 10, "days_max": 14, "ml_per_l": 80, "ml_min": 50, "ml_max": 400},
  {"id": 2193, "de": "Jadebaum", "en": "jade plant", "sci": "Crassula ovata", "min": 15, "target": 35, "max": 55, "days_min": 10, "days_max": 16, "ml_per_l": 80, "ml_min": 50, "ml_max": 400},
  {"id": 2045, "de": "Keulenlilie", "en": "Ti plant", "sci": "Cordyline fruticosa", "min": 30, "target": 50, "max": 70, "days_min": 6, "days_max": 9, "ml_per_l": 120, "ml_min": 80, "ml_max": 600},
  {"id": 9002, "de": "Kletterphilodendron", "en": "Heartleaf philodendron", "sci": "Philodendron hederaceum", "min": 30, "target": 50, "max": 70, "days_min": 7, "days_max": 9, "ml_per_l": 120, "ml_min": 80, "ml_max": 600},
  {"id": 626, "de": "Kolbenfaden", "en": "Chinese evergreen", "sci": "Aglaonema 'Cutlass'", "min": 30, "target": 50, "max": 70, "days_min": 6, "days_max": 9, "ml_per_l": 120, "ml_min": 80, "ml_max": 600},
  {"id": 1470, "de": "Korbmarante", "en": "Calathea", "sci": "Calathea makoyana", "min": 30, "target": 50, "max": 70, "days_min": 6, "days_max": 9, "ml_per_l": 120, "ml_min": 80, "ml_max": 600},
  {"id": 1214, "de": "Königsbegonie", "en": "rex begonia", "sci": "Begonia rex-cultorum", "min": 30, "target": 50, "max": 70, "days_min": 6, "days_max": 9, "ml_per_l": 120, "ml_min": 80, "ml_max": 600},
  {"id": 9014, "de": "Marante", "en": "Prayer plant", "sci": "Maranta leuconeura", "min": 30, "target": 50, "max": 70, "days_min": 5, "days_max": 7, "ml_per_l": 120, "ml_min": 80, "ml_max": 600},
  {"id": 2976, "de": "Mosaikpflanze", "en": "Nerve Plant", "sci": "Fittonia albivenis (Argyroneura Group)", "min": 30, "target": 50, "max": 70, "days_min": 1, "days_max": 2, "ml_per_l": 120, "ml_min": 80, "ml_max": 600},
  {"id": 1038, "de": "Nestfarn", "en": "bird's nest fern", "sci": "Asplenium nidus 'Antiquum'", "min": 30, "target": 50, "max": 70, "days_min": 2, "days_max": 3, "ml_per_l": 120, "ml_min": 80, "ml_max": 600},
  {"id": 710, "de": "Pfeilblatt", "en": "elephant's ear", "sci": "Alocasia amazonica", "min": 30, "target": 50, "max": 70, "days_min": 6, "days_max": 9, "ml_per_l": 120, "ml_min": 80, "ml_max": 600},
  {"id": 1036, "de": "Schusterpalme", "en": "cast-iron plant", "sci": "Aspidistra elatior", "min": 30, "target": 50, "max": 70, "days_min": 6, "days_max": 9, "ml_per_l": 120, "ml_min": 80, "ml_max": 600},
  {"id": 9011, "de": "Strahlenaralie", "en": "Dwarf umbrella tree", "sci": "Schefflera arboricola", "min": 30, "target": 50, "max": 70, "days_min": 7, "days_max": 10, "ml_per_l": 120, "ml_min": 80, "ml_max": 600},
  {"id": 9006, "de": "Ufopflanze", "en": "Chinese money plant", "sci": "Pilea peperomioides", "min": 30, "target": 50, "max": 70, "days_min": 6, "days_max": 8, "ml_per_l": 120, "ml_min": 80, "ml_max": 600},
  {"id": 9009, "de": "Wachsblume", "en": "Wax plant", "sci": "Hoya carnosa", "min": 15, "target": 35, "max": 55, "days_min": 10, "days_max": 14, "ml_per_l": 80, "ml_min": 50, "ml_max": 400},
  {"id": 1999, "de": "Wunderstrauch", "en": "croton", "sci": "Codiaeum variegatum", "min": 30, "target": 50, "max": 70, "days_min": 6, "days_max": 9, "ml_per_l": 120, "ml_min": 80, "ml_max": 600},
  {"id": 9008, "de": "Zebrakraut", "en": "Inch plant", "sci": "Tradescantia zebrina", "min": 45, "target": 65, "max": 85, "days_min": 4, "days_max": 6, "ml_per_l": 160, "ml_min": 100, "ml_max": 800},
  {"id": 9007, "de": "Zwergpfeffer", "en": "Baby rubber plant", "sci": "Peperomia obtusifolia", "min": 30, "target": 50, "max": 70, "days_min": 9, "days_max": 12, "ml_per_l": 120, "ml_min": 80, "ml_max": 600},
];

const KEYBOARD = {
  keyboard: [[{ text: "Status" }], [{ text: "Pflanze" }, { text: "Topf" }], [{ text: "Teilen" }]],
  resize_keyboard: true,
  is_persistent: true,
};

const SEARCH_PROMPT =
  "Schreib den Namen der Pflanze (deutsch, englisch oder lateinisch), z. B. „Monstera“ oder „Bogenhanf“.";

const ERROR_REASONS = {
  still_dry: "Boden ist nach allen Portionen noch trocken (Schlauch? Sensor? Pumpe?)",
  pump_timeout: "Pumpe lief zu lange",
  sensor_tank: "Keine Daten vom Tanksensor",
  sensor_moisture: "Keine Daten von beiden Feuchtesensoren, es wird nicht gegossen",
};

// Only these errors need the button; sensor errors go away by themselves
const RESETTABLE = ["still_dry", "pump_timeout"];

function resetButton(deviceId) {
  return { inline_keyboard: [[{ text: "Fehler zurücksetzen", callback_data: `reset:${deviceId}` }]] };
}

// ---- What the device may send (Integration.md sections 3 and 4) ----
// Every field has a check. Only fields that pass are stored; anything else
// is dropped. So a broken or faked request cannot put strange data into
// the database or break the "Status" text.
const isInt = (min, max) => (v) => Number.isInteger(v) && v >= min && v <= max;
const isStr = (max) => (v) => typeof v === "string" && v.length <= max;
const isBool = (v) => typeof v === "boolean";
const isIntArray = (len, min, max) => (v) => Array.isArray(v) && v.length <= len && v.every(isInt(min, max));
const PHASES = ["IDLE", "PUMPING", "SOAKING", "TANK_EMPTY", "ERROR"];

const STATUS_FIELDS = {
  fw: isStr(16),
  uptime_s: isInt(0, 2 ** 31),
  moisture_index: isInt(-1, 100),
  moisture_sensors: isIntArray(2, -1, 100),
  moisture_raw: isIntArray(2, -1, 4095),
  tank_pct: isInt(-1, 100),
  tank_low: isBool,
  tank_capacity_ml: isInt(0, 100000),
  total_pumped_ml: isInt(0, 2 ** 31),
  phase: (v) => PHASES.includes(v),
  pump_on: isBool,
  need_water: isBool,
  error: isBool,
  error_reason: (v) => v === null || isStr(32)(v),
  attempts: isInt(0, 20),
  plant_id: isInt(-1, 2 ** 31),
  profile_ver: isInt(0, 65535),
  last_dose_ml: isInt(0, 10000),
  last_watered_s_ago: isInt(-1, 2 ** 31),
  rssi: isInt(-127, 0),
};

// allowed event types and their extra fields
const EVENT_FIELDS = {
  boot: { fw: isStr(16) },
  watered: { ml: isInt(0, 10000), moisture_index: isInt(-1, 100) },
  tank_low: { tank_pct: isInt(-1, 100) },
  tank_empty: {},
  tank_refilled: { tank_pct: isInt(-1, 100) },
  error: { reason: isStr(32) },
  error_cleared: { reason: isStr(32) },
  profile_ok: { plant_id: isInt(-1, 2 ** 31), profile_ver: isInt(0, 65535) },
  profile_rejected: { reason: isStr(64) },
  cmd_rejected: { cmd: isStr(16), reason: isStr(64) },
};

// copies only the allowed, valid fields
function pickFields(src, spec) {
  const out = {};
  for (const [key, check] of Object.entries(spec)) {
    if (Object.hasOwn(src, key) && check(src[key])) out[key] = src[key];
  }
  return out;
}

// Event -> Telegram text. null = log only, no message
function eventText(e, plantName) {
  const who = plantName ? `${plantName}: ` : "";
  switch (e.type) {
    case "tank_low":
      return `${who}Wenig Wasser im Tank (${pct(e.tank_pct)}). Bitte nachfüllen.`;
    case "tank_empty":
      return `${who}Die Pflanze braucht Wasser, aber der Tank ist leer!`;
    case "tank_refilled":
      return `${who}Tank aufgefüllt (${pct(e.tank_pct)}), danke.`;
    case "error":
      return RESETTABLE.includes(e.reason)
        ? `${who}Fehler: ${ERROR_REASONS[e.reason]}. Bitte prüfen und danach „Fehler zurücksetzen“ drücken.`
        : `${who}Fehler: ${ERROR_REASONS[e.reason] || e.reason || "unbekannt"}. Bitte prüfen.`;
    case "error_cleared":
      return RESETTABLE.includes(e.reason) || !e.reason
        ? `${who}Fehler zurückgesetzt.`
        : `${who}Behoben: ${ERROR_REASONS[e.reason] || e.reason}.`;
    case "profile_ok":
      return `${who}Profil übernommen, BloomBot kümmert sich jetzt darum.`;
    default:
      return null; // boot, watered, profile_rejected, cmd_rejected
  }
}

export default {
  async fetch(request, env, ctx) {
    const url = new URL(request.url);

    if (request.method === "GET" && url.pathname === "/") {
      return new Response("BloomBot server OK");
    }

    if (request.method === "POST" && url.pathname === "/telegram") {
      return handleTelegram(request, env);
    }

    // One-time setup: registers the Telegram webhook (protected by DEVICE_KEY)
    if (request.method === "POST" && url.pathname === "/admin/setup-webhook") {
      if (!(await deviceAuthOk(request, env))) return json({ error: "unauthorized" }, 401);
      return setupWebhook(url, env);
    }

    const m = url.pathname.match(/^\/api\/device\/([\w-]{1,32})\/(poll|event)$/);
    if (!m || request.method !== "POST") return json({ error: "not_found" }, 404);
    if (!(await deviceAuthOk(request, env))) return json({ error: "unauthorized" }, 401);

    const read = await readJsonBody(request, MAX_DEVICE_BODY);
    if (read.error === "too_large") return json({ error: "too_large" }, 413);
    if (read.error) return json({ error: "bad_json" }, 400);
    const raw = read.data;

    const [, deviceId, action] = m;
    const dev = await q1(env, "SELECT * FROM devices WHERE id = ?1", deviceId);
    if (!dev) return json({ error: "unknown_device" }, 404);

    if (action === "poll") {
      const body = pickFields(raw, STATUS_FIELDS);   // only valid fields are stored
      await run(env,
        `INSERT INTO status (device_id, data, updated_at) VALUES (?1, ?2, ?3)
         ON CONFLICT(device_id) DO UPDATE SET data = excluded.data, updated_at = excluded.updated_at`,
        dev.id, JSON.stringify(body), now());

      // Send the profile until the device reports the current version
      const profile = dev.profile_json && body.profile_ver !== dev.profile_ver
        ? JSON.parse(dev.profile_json)
        : null;

      // Waiting commands: send the fresh ones, delete all ("at most once")
      const rows = await qa(env,
        "SELECT cmd_json, created_at FROM commands WHERE device_id = ?1 ORDER BY id", dev.id);
      await run(env, "DELETE FROM commands WHERE device_id = ?1", dev.id);
      const commands = rows
        .filter((r) => now() - r.created_at <= COMMAND_TTL_S)
        .map((r) => JSON.parse(r.cmd_json));

      // Device was reset on the server (no profile any more), but still runs
      // with an old profile from its flash -> tell it to forget it. Comes with
      // every poll until the device reports profile_ver 0, so it also works
      // if the device was offline during the reset.
      if (!dev.profile_json && body.profile_ver > 0) commands.push({ cmd: "factory_reset" });

      return json({ poll_s: POLL_S, commands, profile });
    }

    // action === "event"
    const spec = typeof raw.type === "string" && Object.hasOwn(EVENT_FIELDS, raw.type) ? EVENT_FIELDS[raw.type] : null;
    if (!spec) return json({ error: "unknown_type" }, 400);
    const body = { type: raw.type, ...pickFields(raw, spec) };
    await run(env,
      "INSERT INTO events (device_id, plant_id, type, data, created_at) VALUES (?1, ?2, ?3, ?4, ?5)",
      dev.id, dev.plant_id, body.type, JSON.stringify(body), now());

    const plant = dev.plant_id != null
      ? await q1(env, "SELECT * FROM my_plants WHERE id = ?1", dev.plant_id)
      : null;
    const text = eventText(body, plant ? plantName(plant) : null);
    // an error that needs a reset gets the button right in the message
    const markup = body.type === "error" && RESETTABLE.includes(body.reason) ? resetButton(dev.id) : undefined;
    if (text) ctx.waitUntil(notifyDevice(env, dev.id, text, markup)); // answer the device right away
    return json({ ok: true });
  },
};

// ================= Catalog and profile =================

function speciesById(id) {
  return PLANTS.find((p) => p.id === id) || null;
}

function speciesLabel(s) {
  return `${s.de} (${s.sci})`;
}

function plantName(plant) {
  const s = speciesById(plant.species_id);
  return plant.nickname || (s ? s.de : `Pflanze ${plant.id}`);
}

function plantTitle(plant) {
  return `${plantName(plant)}, Topf ${plant.pot_liters ?? DEFAULT_POT_L} l`;
}

function norm(s) {
  return s.toLowerCase().replace(/ß/g, "ss").normalize("NFD")
    .replace(/[̀-ͯ]/g, "").replace(/['’()]/g, "").trim();
}

// German, English or Latin name; best matches first
function searchSpecies(query) {
  const q = norm(query);
  if (q.length < 2) return [];
  const hits = [];
  for (const s of PLANTS) {
    const names = [s.de, s.en, s.sci].map(norm);
    let score = 3;
    if (names.some((n) => n.startsWith(q))) score = 0;
    else if (names.some((n) => n.split(/[\s-]+/).some((w) => w.startsWith(q)))) score = 1;
    else if (names.some((n) => n.includes(q))) score = 2;
    if (score < 3) hits.push({ s, score });
  }
  hits.sort((a, b) => a.score - b.score || a.s.de.localeCompare(b.s.de));
  return hits.slice(0, 10).map((h) => h.s);
}

// species + pot -> numbers for the device
function buildProfile(s, potL, ver, mode, name) {
  // total water for one watering cycle, from the plant database
  let need = Math.round(s.ml_per_l * potL);
  need = Math.min(Math.max(need, s.ml_min), s.ml_max);
  // split it into equal portions, each at most DOSE_MAX_ML (pump limit)
  const portions = Math.ceil(need / DOSE_MAX_ML);
  const dose = Math.round(need / portions);
  return {
    profile_ver: ver,
    plant_id: s.id,
    name: name.slice(0, 31),
    mode,
    moist_min: s.min,
    moist_target: s.target,
    moist_max: s.max,
    dose_ml: Math.max(dose, 10),
    min_pause_h: Math.round(s.days_min * 24 * PAUSE_FACTOR),
  };
}

// Puts a plant on a device and builds a new profile version. Returns the reply text
async function assignPlant(env, dev, plantId) {
  const plant = await q1(env, "SELECT * FROM my_plants WHERE id = ?1", plantId);
  const s = speciesById(plant.species_id);
  const ver = dev.profile_ver + 1;
  const profile = buildProfile(s, plant.pot_liters ?? DEFAULT_POT_L, ver, dev.mode, s.en);

  await env.DB.batch([
    // a plant stands on at most one device
    env.DB.prepare("UPDATE devices SET plant_id = NULL WHERE plant_id = ?1 AND id <> ?2").bind(plantId, dev.id),
    // new plant -> water forecast starts again; same plant (pot changed) -> keep it
    env.DB.prepare(
      `UPDATE devices SET plant_id = ?1, profile_ver = ?2, profile_json = ?3,
         forecast_since = CASE WHEN plant_id IS ?1 THEN forecast_since ELSE ?4 END
       WHERE id = ?5`
    ).bind(plantId, ver, JSON.stringify(profile), now(), dev.id),
  ]);

  return [
    `${devLabel(dev.name)} kümmert sich jetzt um: ${plantName(plant)} (${s.sci})`,
    `Topf: ${plant.pot_liters ?? DEFAULT_POT_L} l`,
    `Gießen bei Bodenfeuchte unter ${profile.moist_min}, bis ${profile.moist_target}, nie über ${profile.moist_max} (von 100)`,
    `Portion: ${profile.dose_ml} ml (bis zu 3 pro Gießen), frühestens alle ${profile.min_pause_h} h`,
    "",
    `Wird beim nächsten Kontakt an das Gerät übertragen (bis ~${Math.ceil(POLL_S / 60)} min).`,
  ].join("\n");
}

// ================= Access =================

async function touchUser(env, from) {
  const name = [from.first_name, from.last_name].filter(Boolean).join(" ") || from.username || null;
  await run(env,
    `INSERT INTO users (telegram_id, name, created_at) VALUES (?1, ?2, ?3)
     ON CONFLICT(telegram_id) DO UPDATE SET name = excluded.name`,
    from.id, name, now());
}

// For the fair: one device per owner. With several, the first one is used
async function ownedDevice(env, uid) {
  return q1(env,
    `SELECT d.* FROM devices d JOIN device_access a ON a.device_id = d.id
     WHERE a.user_id = ?1 AND a.role = 'owner' ORDER BY d.id LIMIT 1`, uid);
}

// Owner's device, or null + an explanation to the user
async function requireOwner(env, uid) {
  const dev = await ownedDevice(env, uid);
  if (dev) return dev;
  const any = await q1(env, "SELECT 1 AS x FROM device_access WHERE user_id = ?1", uid);
  await send(env, uid, any
    ? "Das kann nur der Besitzer des Geräts ändern."
    : "Du hast noch kein Gerät. Koppeln mit /link und dem Code vom Aufkleber.");
  return null;
}

async function notifyDevice(env, deviceId, text, replyMarkup) {
  const users = await qa(env, "SELECT user_id FROM device_access WHERE device_id = ?1", deviceId);
  for (const u of users) await send(env, u.user_id, text, replyMarkup);
}

// ================= Telegram =================

async function handleTelegram(request, env) {
  // Only Telegram knows the secret (set in setWebhook)
  if (!(await sameSecret(request.headers.get("X-Telegram-Bot-Api-Secret-Token"), env.TELEGRAM_SECRET))) {
    return new Response("forbidden", { status: 403 });
  }

  // Always answer 200, otherwise Telegram repeats the update again and again
  try {
    const read = await readJsonBody(request, MAX_TELEGRAM_BODY);
    if (read.error) {
      console.log("telegram update ignored:", read.error);
      return new Response("ok");
    }
    const update = read.data;
    if (update.callback_query) {
      await onCallback(env, update.callback_query);
    } else if (update.message && typeof update.message.text === "string" &&
               update.message.chat?.type === "private") {   // no group chats
      await onText(env, update.message);
    }
  } catch (err) {
    console.log("telegram handler error", String(err));
  }
  return new Response("ok");
}

async function onText(env, msg) {
  const uid = msg.from.id;
  await touchUser(env, msg.from);

  const text = msg.text.trim();
  const [first, ...rest] = text.split(/\s+/);
  const cmd = first.toLowerCase().split("@")[0];
  const arg = rest.join(" ");

  switch (cmd) {
    case "/start":
      // QR code on the device: t.me/<bot>?start=pair_<code> -> pair right away
      if (arg.toLowerCase().startsWith("pair_")) return linkDevice(env, uid, arg.slice(5));
      if (arg) return acceptInvite(env, msg.from, arg);
      return send(env, uid,
        "Hallo! Ich bin BloomBot. Ich melde mich, wenn etwas zu tun ist.\n\n" +
        "Neues Gerät: QR-Code auf dem Gerät scannen oder /link und der Code vom Aufkleber.\n" +
        "„Pflanze“ – Pflanze wählen, „Topf“ – Topfgröße, „Teilen“ – jemandem Zugriff geben, " +
        "„Status“ – aktueller Zustand.\n" +
        "/name – dein Gerät umbenennen, z. B. /name Küche\n" +
        "/konto_loeschen – dein Konto und alle deine Daten löschen.", KEYBOARD);
    case "/link":
      return linkDevice(env, uid, arg);
    case "/name":
      return renameDevice(env, uid, arg);
    case "/konto_loeschen":
      return send(env, uid,
        "Dein Konto wirklich löschen? Deine Pflanzen und Zugriffe werden gelöscht, " +
        "Geräte, die dir gehören, werden zurückgesetzt.",
        { inline_keyboard: [[{ text: "Ja, Konto löschen", callback_data: "delacc" },
                             { text: "Abbrechen", callback_data: "cancel" }]] });
    case "/admin_reset":
      // only for ADMIN_IDS; for everybody else the command does not exist
      if (!isAdmin(env, uid)) return send(env, uid, "Unbekannter Befehl. Nutze die Tasten unten.", KEYBOARD);
      return adminReset(env, uid, arg);
    case "status":
    case "/status": {
      const st = await statusText(env, uid);
      return send(env, uid, st.text, st.markup || KEYBOARD);
    }
    case "pflanze":
    case "/pflanze":
      return choosePlant(env, uid);
    case "topf":
    case "/topf":
      return chooseTopf(env, uid);
    case "teilen":
    case "/teilen":
      return share(env, uid);
    default:
      if (text.startsWith("/")) return send(env, uid, "Unbekannter Befehl. Nutze die Tasten unten.", KEYBOARD);
      return searchReply(env, uid, text);   // any other text = plant search
  }
}

async function onCallback(env, q) {
  const uid = q.from.id;
  await telegram(env, "answerCallbackQuery", { callback_query_id: q.id });
  if (q.message?.chat?.type !== "private") return;
  await touchUser(env, q.from);

  const [kind, a, b] = String(q.data || "").split(":");

  if (kind === "new") return send(env, uid, SEARCH_PROMPT);

  if (kind === "list") {
    const rows = [];
    for (let i = 0; i < PLANTS.length; i += 2) {
      rows.push(PLANTS.slice(i, i + 2).map((s) => ({ text: s.de, callback_data: `sp:${s.id}` })));
    }
    return send(env, uid, "Alle Pflanzen:", { inline_keyboard: rows });
  }

  // species chosen -> ask for the pot
  if (kind === "sp") {
    const s = speciesById(Number(a));
    if (!s) return;
    return send(env, uid, `${speciesLabel(s)}\nWie groß ist der Topf?`, {
      inline_keyboard: [POT_SIZES.map((l) => ({ text: `${l} l`, callback_data: `np:${s.id}:${l}` }))],
    });
  }

  // species + pot chosen -> new entry in "my plants", put on the device
  if (kind === "np") {
    const s = speciesById(Number(a));
    const l = Number(b);
    if (!s || !POT_SIZES.includes(l)) return;
    const dev = await requireOwner(env, uid);
    if (!dev) return;
    const row = await q1(env,
      `INSERT INTO my_plants (owner_id, species_id, nickname, pot_liters, created_at)
       VALUES (?1, ?2, ?3, ?4, ?5) RETURNING id`,
      uid, s.id, s.de, l, now());
    return send(env, uid, await assignPlant(env, dev, row.id), KEYBOARD);
  }

  // saved plant chosen ("use"), or confirmed although it stands on another device ("usef")
  if (kind === "use" || kind === "usef") {
    const dev = await requireOwner(env, uid);
    if (!dev) return;
    const plant = await q1(env, "SELECT * FROM my_plants WHERE id = ?1 AND owner_id = ?2", Number(a), uid);
    if (!plant) return;
    const other = await q1(env, "SELECT * FROM devices WHERE plant_id = ?1 AND id <> ?2", plant.id, dev.id);
    if (other && kind === "use") {
      return send(env, uid, `${plantName(plant)} steht gerade an ${devLabel(other.name)}. Umstellen?`, {
        inline_keyboard: [[{ text: "Ja, umstellen", callback_data: `usef:${plant.id}` }]],
      });
    }
    return send(env, uid, await assignPlant(env, dev, plant.id), KEYBOARD);
  }

  // pot size of the plant that is on the device now
  if (kind === "pot") {
    const l = Number(a);
    if (!POT_SIZES.includes(l)) return;
    const dev = await requireOwner(env, uid);
    if (!dev) return;
    if (dev.plant_id == null) return send(env, uid, "Wähle zuerst eine Pflanze mit „Pflanze“.");
    await run(env, "UPDATE my_plants SET pot_liters = ?1 WHERE id = ?2", l, dev.plant_id);
    return send(env, uid, await assignPlant(env, dev, dev.plant_id), KEYBOARD);
  }

  if (kind === "cancel") return send(env, uid, "Abgebrochen.", KEYBOARD);

  // the name is typed as a command, so no "waiting for text" state is needed
  if (kind === "rename") {
    return send(env, uid, `Schreib /name und den neuen Namen (max. ${MAX_NAME_LEN} Zeichen), z. B. /name Küche`);
  }

  // "Gerät zurücksetzen": step 1 asks, step 2 ("devreset2") does it. Owner only
  if (kind === "devreset" || kind === "devreset2") {
    const deviceId = a;
    const own = await q1(env,
      "SELECT d.name FROM devices d JOIN device_access a ON a.device_id = d.id " +
      "WHERE d.id = ?1 AND a.user_id = ?2 AND a.role = 'owner'", deviceId, uid);
    if (!own) return send(env, uid, "Das kann nur der Besitzer des Geräts.");
    if (kind === "devreset") {
      return send(env, uid,
        `${devLabel(own.name)} wirklich zurücksetzen? Alle verlieren den Zugriff, ` +
        "die Pflanze wird abgemeldet, das Gerät ist danach wie neu und kann wieder gekoppelt werden.",
        { inline_keyboard: [[{ text: "Ja, zurücksetzen", callback_data: `devreset2:${deviceId}` },
                             { text: "Abbrechen", callback_data: "cancel" }]] });
    }
    await resetDevice(env, deviceId, uid);
    return send(env, uid, `${devLabel(own.name)} ist zurückgesetzt.`, KEYBOARD);
  }

  if (kind === "delacc") {
    await deleteAccount(env, uid);
    return send(env, uid, "Dein Konto ist gelöscht. Mit /start kannst du jederzeit neu beginnen.");
  }

  // "Fehler zurücksetzen": owner AND member may press it - the person who
  // looks after the plant (e.g. during a holiday) must be able to reset it
  if (kind === "reset") {
    const deviceId = a;
    const access = await q1(env,
      "SELECT 1 AS x FROM device_access WHERE device_id = ?1 AND user_id = ?2", deviceId, uid);
    if (!access) return send(env, uid, "Kein Zugriff auf dieses Gerät.");
    // one waiting reset is enough
    await run(env, "DELETE FROM commands WHERE device_id = ?1", deviceId);
    await run(env,
      "INSERT INTO commands (device_id, cmd_json, created_at) VALUES (?1, ?2, ?3)",
      deviceId, JSON.stringify({ cmd: "reset" }), now());
    return send(env, uid,
      `Angenommen, BloomBot führt es beim nächsten Kontakt aus (bis ~${Math.ceil(POLL_S / 60)} min).`);
  }

  // owner removes a member
  if (kind === "rm") {
    const dev = await requireOwner(env, uid);
    if (!dev) return;
    const res = await run(env,
      "DELETE FROM device_access WHERE device_id = ?1 AND user_id = ?2 AND role = 'member'",
      dev.id, Number(a));
    return send(env, uid, res.meta.changes ? "Zugriff entfernt." : "Dieser Nutzer hat keinen Zugriff mehr.");
  }
}

async function linkDevice(env, uid, code) {
  if (!code) return send(env, uid, "Schreib /link und den Code vom Aufkleber, z. B. /link BB-7K3Q");
  const dev = await q1(env, "SELECT * FROM devices WHERE upper(pair_code) = upper(?1)", code.trim());
  if (!dev) return send(env, uid, "Diesen Code kenne ich nicht.");

  const owner = await q1(env, "SELECT user_id FROM device_access WHERE device_id = ?1 AND role = 'owner'", dev.id);
  if (owner) {
    return send(env, uid, owner.user_id === uid
      ? "Dieses Gerät gehört dir schon."
      : "Dieses Gerät hat schon einen Besitzer.");
  }
  await run(env,
    `INSERT INTO device_access (device_id, user_id, role, created_at) VALUES (?1, ?2, 'owner', ?3)
     ON CONFLICT(device_id, user_id) DO UPDATE SET role = 'owner'`,
    dev.id, uid, now());
  return send(env, uid,
    `Gekoppelt: ${devLabel(dev.name)}. Wähle jetzt die Pflanze mit „Pflanze“.\n` +
    "Tipp: Mit /name gibst du dem Gerät einen eigenen Namen, z. B. /name Küche", KEYBOARD);
}

// ---- device "as after purchase": nobody has access, no plant, no profile ----
// The device row and its pair code stay (the sticker keeps working).
// Plants stay with their owners ("my plants" belong to the user).
// The device forgets its profile via the "factory_reset" command (see poll).
async function resetDevice(env, deviceId, exceptUid = null) {   // exceptUid: who pressed it, gets no extra message
  const users = await qa(env, "SELECT user_id FROM device_access WHERE device_id = ?1", deviceId);
  const dev = await q1(env, "SELECT name FROM devices WHERE id = ?1", deviceId);
  await env.DB.batch([
    env.DB.prepare("DELETE FROM device_access WHERE device_id = ?1").bind(deviceId),
    env.DB.prepare("DELETE FROM invites WHERE device_id = ?1").bind(deviceId),
    env.DB.prepare("DELETE FROM commands WHERE device_id = ?1").bind(deviceId),
    env.DB.prepare(
      `UPDATE devices SET plant_id = NULL, profile_json = NULL, mode = 'normal',
         vacation_until = NULL, forecast_since = NULL, name = ?2 WHERE id = ?1`
    ).bind(deviceId, DEFAULT_DEVICE_NAME),
  ]);
  for (const u of users) {
    if (u.user_id === exceptUid) continue;
    await send(env, u.user_id, `${devLabel(dev ? dev.name : deviceId)} wurde zurückgesetzt. Du hast keinen Zugriff mehr.`);
  }
}

// ---- delete a user and everything that belongs to them ----
// Order matters: the database refuses to delete rows that others still point to.
async function deleteAccount(env, uid) {
  const owned = await qa(env,
    "SELECT device_id FROM device_access WHERE user_id = ?1 AND role = 'owner'", uid);
  for (const o of owned) await resetDevice(env, o.device_id, uid);   // also removes the plants from the devices
  await env.DB.batch([
    env.DB.prepare("DELETE FROM device_access WHERE user_id = ?1").bind(uid),
    env.DB.prepare("DELETE FROM invites WHERE created_by = ?1 OR used_by = ?1").bind(uid),
    env.DB.prepare("DELETE FROM my_plants WHERE owner_id = ?1").bind(uid),
    env.DB.prepare("DELETE FROM users WHERE telegram_id = ?1").bind(uid),
  ]);
}

// ---- admin ----
// ADMIN_IDS = Telegram IDs (comma-separated), stored as a Worker secret.
function isAdmin(env, uid) {
  return (env.ADMIN_IDS || "").split(",").map((x) => x.trim()).includes(String(uid));
}

// /admin_reset [device id] - resets a device no matter who owns it.
// Without an id: the only device, if there is exactly one.
async function adminReset(env, uid, deviceId) {
  if (!deviceId) {
    const all = await qa(env, "SELECT id FROM devices");
    if (all.length !== 1) {
      return send(env, uid, `Bitte Geräte-ID angeben: /admin_reset <id>. Geräte: ${all.map((d) => d.id).join(", ")}`);
    }
    deviceId = all[0].id;
  }
  const dev = await q1(env, "SELECT id, name, pair_code FROM devices WHERE id = ?1", deviceId);
  if (!dev) return send(env, uid, `Gerät „${deviceId}“ gibt es nicht.`);
  await resetDevice(env, dev.id);
  return send(env, uid, `Admin: ${devLabel(dev.name)} (${dev.id}) ist zurückgesetzt. Koppel-Code: ${dev.pair_code}`);
}

// "BloomBot „Küche“", or just "BloomBot" while the device has the default name
function devLabel(name) {
  return !name || name === DEFAULT_DEVICE_NAME ? DEFAULT_DEVICE_NAME : `BloomBot „${name}“`;
}

// ---- /name <new name>: the owner renames their device ----
async function renameDevice(env, uid, name) {
  const dev = await requireOwner(env, uid);
  if (!dev) return;
  name = (name || "").replace(/\s+/g, " ").trim();
  if (!name) return send(env, uid, "Schreib /name und den neuen Namen, z. B. /name Küche");
  if (name.length > MAX_NAME_LEN) return send(env, uid, `Der Name ist zu lang (max. ${MAX_NAME_LEN} Zeichen).`);
  await run(env, "UPDATE devices SET name = ?1 WHERE id = ?2", name, dev.id);
  return send(env, uid, `Dein Gerät heißt jetzt „${name}“.`, KEYBOARD);
}

async function choosePlant(env, uid) {
  const dev = await requireOwner(env, uid);
  if (!dev) return;
  const plants = await qa(env, "SELECT * FROM my_plants WHERE owner_id = ?1 ORDER BY id", uid);
  if (plants.length === 0) {
    return send(env, uid, SEARCH_PROMPT,
      { inline_keyboard: [[{ text: "Alle Pflanzen anzeigen", callback_data: "list" }]] });
  }
  const rows = plants.map((p) => [{
    text: plantTitle(p) + (p.id === dev.plant_id ? " (aktuell)" : ""),
    callback_data: `use:${p.id}`,
  }]);
  rows.push([{ text: "Neue Pflanze", callback_data: "new" }]);
  return send(env, uid, "Welche Pflanze steht jetzt am BloomBot?", { inline_keyboard: rows });
}

async function chooseTopf(env, uid) {
  const dev = await requireOwner(env, uid);
  if (!dev) return;
  if (dev.plant_id == null) return send(env, uid, "Wähle zuerst eine Pflanze mit „Pflanze“.");
  return send(env, uid, "Wie groß ist der Topf?", {
    inline_keyboard: [POT_SIZES.map((l) => ({ text: `${l} l`, callback_data: `pot:${l}` }))],
  });
}

async function searchReply(env, uid, text) {
  const found = searchSpecies(text);
  if (found.length === 0) {
    return send(env, uid, `Keine Pflanze gefunden für „${text.slice(0, 40)}“.`,
      { inline_keyboard: [[{ text: "Alle Pflanzen anzeigen", callback_data: "list" }]] });
  }
  return send(env, uid, "Welche Pflanze meinst du?", {
    inline_keyboard: found.map((s) => [{ text: speciesLabel(s), callback_data: `sp:${s.id}` }]),
  });
}

async function share(env, uid) {
  const dev = await requireOwner(env, uid);
  if (!dev) return;

  const code = randomCode(10);
  await run(env,
    "INSERT INTO invites (code, device_id, created_by, expires_at) VALUES (?1, ?2, ?3, ?4)",
    code, dev.id, uid, now() + INVITE_TTL_S);
  const bot = await botUsername(env);

  const members = await qa(env,
    `SELECT u.telegram_id, u.name FROM device_access a JOIN users u ON u.telegram_id = a.user_id
     WHERE a.device_id = ?1 AND a.role = 'member' ORDER BY a.created_at`, dev.id);

  const lines = [
    `Schick diesen Link an die Person, die mitgießen soll (24 h gültig, nur einmal nutzbar):`,
    `https://t.me/${bot}?start=${code}`,
    "",
    members.length ? "Hat schon Zugriff:" : "Bisher hat niemand sonst Zugriff.",
  ];
  const buttons = members.map((m) => [{
    text: `${m.name || m.telegram_id} entfernen`,
    callback_data: `rm:${m.telegram_id}`,
  }]);
  for (const m of members) lines.push(`– ${m.name || m.telegram_id}`);
  return send(env, uid, lines.join("\n"), buttons.length ? { inline_keyboard: buttons } : undefined);
}

async function acceptInvite(env, from, code) {
  const uid = from.id;
  const inv = await q1(env, "SELECT * FROM invites WHERE code = ?1", code);
  if (!inv || inv.used_by != null || inv.expires_at < now()) {
    return send(env, uid, "Diese Einladung ist ungültig oder abgelaufen. Bitte um einen neuen Link.");
  }
  const dev = await q1(env, "SELECT * FROM devices WHERE id = ?1", inv.device_id);
  const has = await q1(env, "SELECT role FROM device_access WHERE device_id = ?1 AND user_id = ?2", dev.id, uid);
  if (has) return send(env, uid, `Du hast schon Zugriff auf ${devLabel(dev.name)}.`, KEYBOARD);

  await env.DB.batch([
    env.DB.prepare(
      "INSERT INTO device_access (device_id, user_id, role, created_at) VALUES (?1, ?2, 'member', ?3)"
    ).bind(dev.id, uid, now()),
    env.DB.prepare("UPDATE invites SET used_by = ?1 WHERE code = ?2").bind(uid, code),
  ]);

  const owner = await q1(env, "SELECT user_id FROM device_access WHERE device_id = ?1 AND role = 'owner'", dev.id);
  const name = from.first_name || from.username || String(uid);
  if (owner) await send(env, owner.user_id, `${name} hat jetzt Zugriff auf ${devLabel(dev.name)}.`);
  return send(env, uid,
    `Du hast jetzt Zugriff auf ${devLabel(dev.name)}. Du bekommst seine Benachrichtigungen, ` +
    "„Status“ zeigt den aktuellen Zustand.", KEYBOARD);
}

// Returns { text, markup }: markup = "Fehler zurücksetzen" buttons for devices in ERROR
async function statusText(env, uid) {
  const devices = await qa(env,
    `SELECT d.*, a.role FROM devices d JOIN device_access a ON a.device_id = d.id
     WHERE a.user_id = ?1 ORDER BY a.role DESC, d.id`, uid);   // owner first
  if (devices.length === 0) {
    return { text: "Du hast noch kein Gerät. Koppeln mit /link und dem Code vom Aufkleber oder über einen Einladungslink." };
  }

  const blocks = [];
  const buttons = [];
  for (const d of devices) {
    const lines = [`${devLabel(d.name)}${d.role === "member" ? " (geteilt mit dir)" : ""}`];
    const plant = d.plant_id != null ? await q1(env, "SELECT * FROM my_plants WHERE id = ?1", d.plant_id) : null;
    lines.push(plant ? `Pflanze: ${plantTitle(plant)}` : "Pflanze: nicht gewählt");

    const st = await q1(env, "SELECT data, updated_at FROM status WHERE device_id = ?1", d.id);
    if (!st) {
      lines.push("Noch keine Daten vom Gerät.");
    } else {
      const s = JSON.parse(st.data);
      const age = now() - st.updated_at;
      if (plant) lines.push(s.profile_ver === d.profile_ver ? "Profil: aktiv" : "Profil: wird übertragen …");
      if (age > 3 * POLL_S) lines.push(`Nicht erreichbar (letzter Kontakt vor ${ago(age)})`);
      lines.push(
        `Bodenfeuchte: ${idx(s.moisture_index)}`,
        `Tank: ${pct(s.tank_pct)}${s.tank_low ? " (wenig Wasser)" : ""}`,
        `Phase: ${s.phase || "?"} (vor ${ago(age)})`,
      );
      if (s.error) lines.push(`Fehler: ${ERROR_REASONS[s.error_reason] || s.error_reason || "unbekannt"}`);
      if (s.phase === "ERROR" && RESETTABLE.includes(s.error_reason)) {
        buttons.push([{ text: `Fehler zurücksetzen („${d.name}“)`, callback_data: `reset:${d.id}` }]);
      }
    }
    if (d.role === "owner") {
      buttons.push([
        { text: "Umbenennen", callback_data: `rename:${d.id}` },
        { text: "Gerät zurücksetzen", callback_data: `devreset:${d.id}` },
      ]);
    }
    blocks.push(lines.join("\n"));
  }
  return { text: blocks.join("\n\n"), markup: buttons.length ? { inline_keyboard: buttons } : undefined };
}

// ================= Telegram API =================

async function send(env, chatId, text, replyMarkup) {
  const body = { chat_id: chatId, text };
  if (replyMarkup) body.reply_markup = replyMarkup;
  await telegram(env, "sendMessage", body);
}

async function telegram(env, method, payload) {
  try {
    const r = await fetch(`https://api.telegram.org/bot${env.TELEGRAM_TOKEN}/${method}`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
      signal: AbortSignal.timeout(TELEGRAM_TIMEOUT_MS),   // never hang if Telegram does not answer
    });
    if (!r.ok) console.log(`telegram ${method} failed`, r.status, await r.text());
    return r.ok ? r.json() : null;
  } catch (err) {
    // timeout or network error; the token is in the URL, so the URL is never logged
    console.log(`telegram ${method} failed:`, err.name);
    return null;
  }
}

let cachedBotUsername = null;
async function botUsername(env) {
  if (!cachedBotUsername) {
    const me = await telegram(env, "getMe", {});
    cachedBotUsername = me?.result?.username || "BOT";
  }
  return cachedBotUsername;
}

async function setupWebhook(url, env) {
  if (!env.TELEGRAM_TOKEN || !env.TELEGRAM_SECRET) {
    return json({ error: "TELEGRAM_TOKEN or TELEGRAM_SECRET missing" }, 500);
  }
  const result = await telegram(env, "setWebhook", {
    url: `${url.origin}/telegram`,
    secret_token: env.TELEGRAM_SECRET,
    allowed_updates: ["message", "callback_query"],
  });
  return json(result || { ok: false, description: "Telegram did not answer" });
}

// ================= helpers =================

function stmt(env, sql, args) {
  const s = env.DB.prepare(sql);
  return args.length ? s.bind(...args) : s;
}
async function q1(env, sql, ...args) { return stmt(env, sql, args).first(); }
async function qa(env, sql, ...args) { return (await stmt(env, sql, args).all()).results; }
async function run(env, sql, ...args) { return stmt(env, sql, args).run(); }

function now() {
  return Math.floor(Date.now() / 1000);
}

function randomCode(len) {
  const abc = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghjkmnpqrstuvwxyz23456789";
  const bytes = crypto.getRandomValues(new Uint8Array(len));
  return Array.from(bytes, (b) => abc.charAt(b % abc.length)).join("");
}

async function deviceAuthOk(request, env) {
  return sameSecret(request.headers.get("Authorization"), env.DEVICE_KEY ? `Bearer ${env.DEVICE_KEY}` : null);
}

// ---- compare two secrets without leaking information through the time ----
// A normal "===" stops at the first different character, so a wrong key
// with the right beginning takes a tiny bit longer to reject. With many
// tries an attacker could guess the key character by character from these
// times. Here both values are hashed (always 32 bytes) and ALL bytes are
// compared, so the time is always the same.
async function sameSecret(actual, expected) {
  if (!actual || !expected) return false;
  const enc = new TextEncoder();
  const [a, b] = await Promise.all([actual, expected].map((v) => crypto.subtle.digest("SHA-256", enc.encode(v))));
  const x = new Uint8Array(a), y = new Uint8Array(b);
  let diff = 0;
  for (let i = 0; i < x.length; i++) diff |= x[i] ^ y[i];
  return diff === 0;
}

// ---- read a JSON body, but at most maxBytes ----
// request.json() would read any size into memory. Here reading stops as
// soon as the limit is passed. Returns { data } or { error: "too_large" | "bad_json" }.
async function readJsonBody(request, maxBytes) {
  if (Number(request.headers.get("Content-Length") || 0) > maxBytes) return { error: "too_large" };
  if (!request.body) return { error: "bad_json" };
  const reader = request.body.getReader();
  const decoder = new TextDecoder();
  let size = 0, text = "";
  while (true) {
    const { value, done } = await reader.read();
    if (done) break;
    size += value.byteLength;
    if (size > maxBytes) {
      await reader.cancel();
      return { error: "too_large" };
    }
    text += decoder.decode(value, { stream: true });
  }
  text += decoder.decode();
  try {
    const data = JSON.parse(text);
    // must be a JSON object { ... }, not a number, text or list
    if (typeof data !== "object" || data === null || Array.isArray(data)) return { error: "bad_json" };
    return { data };
  } catch {
    return { error: "bad_json" };
  }
}

function pct(v) {
  return typeof v === "number" && v >= 0 ? `${v} %` : "keine Daten";
}

// Soil moisture is an index 0..100 (0 = dry soil, 100 = freshly watered), not physical %
function idx(v) {
  return typeof v === "number" && v >= 0 ? `${v} von 100` : "keine Daten";
}

function ago(sec) {
  if (sec < 60) return `${sec} s`;
  if (sec < 3600) return `${Math.floor(sec / 60)} min`;
  if (sec < 86400) return `${Math.floor(sec / 3600)} h`;
  return `${Math.floor(sec / 86400)} Tagen`;
}

function json(obj, status = 200) {
  return new Response(JSON.stringify(obj), {
    status,
    headers: { "Content-Type": "application/json" },
  });
}
