# BloomBot server (Cloudflare Worker)

| File | What it is |
|---|---|
| `worker.js` | the whole server: device API, Telegram bot, plant catalog |
| `schema.sql` | D1 tables (run by hand in the D1 console for a new database) |
| `wrangler.jsonc` | Worker settings used by the automatic deploy |
| `plants.json` | source of the plant catalog (the `PLANTS` list in `worker.js` was generated from it) |

Deploy: push to `main` -> Cloudflare deploys this folder automatically (Workers Builds).
Do not edit the code in the Cloudflare browser editor - the next push overwrites it.
