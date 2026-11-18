-- BloomBot D1 schema v2 (see doc "BloomBot – Datenmodell")
-- Only for a NEW, empty database: run in the D1 console.
-- The live database already has these tables - do not run it there.

CREATE TABLE users (
  telegram_id INTEGER PRIMARY KEY,
  name        TEXT,
  created_at  INTEGER NOT NULL
);

CREATE TABLE my_plants (
  id          INTEGER PRIMARY KEY AUTOINCREMENT,
  owner_id    INTEGER NOT NULL REFERENCES users(telegram_id),
  species_id  INTEGER NOT NULL,
  nickname    TEXT,
  pot_liters  REAL,
  created_at  INTEGER NOT NULL
);

CREATE TABLE devices (
  id             TEXT PRIMARY KEY,
  key_hash       TEXT,
  pair_code      TEXT UNIQUE,
  name           TEXT,
  plant_id       INTEGER UNIQUE REFERENCES my_plants(id),
  mode           TEXT NOT NULL DEFAULT 'normal',
  vacation_until TEXT,
  profile_ver    INTEGER NOT NULL DEFAULT 0,
  profile_json   TEXT,
  forecast_since INTEGER,
  water_warned   INTEGER NOT NULL DEFAULT 0,   -- 1 = "water lasts < 3 days" was sent, 0 again after refilling
  created_at     INTEGER NOT NULL
);

CREATE TABLE device_access (
  device_id  TEXT NOT NULL REFERENCES devices(id),
  user_id    INTEGER NOT NULL REFERENCES users(telegram_id),
  role       TEXT NOT NULL,
  created_at INTEGER NOT NULL,
  PRIMARY KEY (device_id, user_id)
);

CREATE TABLE invites (
  code       TEXT PRIMARY KEY,
  device_id  TEXT NOT NULL REFERENCES devices(id),
  created_by INTEGER NOT NULL REFERENCES users(telegram_id),
  expires_at INTEGER NOT NULL,
  used_by    INTEGER REFERENCES users(telegram_id)
);

CREATE TABLE status (
  device_id            TEXT PRIMARY KEY REFERENCES devices(id),
  data                 TEXT NOT NULL,
  updated_at           INTEGER NOT NULL,
  last_total_pumped_ml INTEGER
);

CREATE TABLE events (
  id         INTEGER PRIMARY KEY AUTOINCREMENT,
  device_id  TEXT NOT NULL,
  plant_id   INTEGER,
  type       TEXT NOT NULL,
  data       TEXT NOT NULL,
  created_at INTEGER NOT NULL
);

CREATE TABLE consumption (
  device_id TEXT NOT NULL,
  day       TEXT NOT NULL,
  ml        INTEGER NOT NULL DEFAULT 0,
  PRIMARY KEY (device_id, day)
);

CREATE TABLE commands (
  id         INTEGER PRIMARY KEY AUTOINCREMENT,
  device_id  TEXT NOT NULL,
  cmd_json   TEXT NOT NULL,
  created_at INTEGER NOT NULL
);