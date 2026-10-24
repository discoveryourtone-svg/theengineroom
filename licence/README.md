# Licence keys for pre-sale buyers

Every buyer gets a key like `DYT-ER-MXxGfDF8S3ls...` that carries their name, email and **founder number**, signed by you. The plugin checks it offline, so there's no server to run and nothing to break on launch day. A key can't be edited or forged without your private signing key.

## One-time setup (10 minutes)

1. Open **Licence-Desk.html** in Chrome, Safari or Firefox (double-click it; keep it next to `licence-core.js`).
2. Open *First time? Create your signing key* and click **Create signing key**. Two files download:
   - `engine-room-private.jwk` — **your master key. Keep it secret. Back it up in two places** (e.g. a password manager and a USB stick). Never email it, never upload it, never put it in GitHub.
   - `LicencePublicKey.h` — copy it over `Source/LicencePublicKey.h`, commit, and let GitHub rebuild the plugin.
3. Issue yourself a test key (*One buyer* tab) and activate it in the plugin to check the chain works end to end.

The project ships with a *development* public key. Keys you make won't work until you've done step 2, and GitHub refuses to publish a tagged release while the development key is still in place.

## Issuing keys after a pre-sale drop

1. Export your orders as CSV from your store or HighLevel. It needs an email column; names and order numbers are found automatically.
2. Open Licence Desk → load your signing key → load your **latest** `issued_ledger.csv` (skip only for the very first batch).
3. Drop in the buyers CSV, choose *Founder*, click **Issue keys**.
4. Click **Download everything**. You get:
   - `licence_files/F001-Sam-Lee.dytlicence` — one per buyer, to attach to emails
   - `emails/F001-Sam-Lee.txt` — ready-to-send email in the DYT voice
   - `crm_import.csv` — import into HighLevel, map `engine_room_licence_key` and `engine_room_number` to custom fields, then send one email with merge fields
   - `issued_ledger.csv` — **replace your old ledger with this one** and back it up

Running the same buyers again is safe: anyone who already has a key gets the same one back, never a duplicate. Founder numbers follow the order of your CSV, so sort it by order date first if "first in = #001" matters to you.

## Founder vs standard

| | Founder | Standard |
|---|---|---|
| Who | Pre-sale / drop buyers | Everyone after launch |
| Shows on the pedal | FOUNDER #007 · SAM LEE | LICENCE #142 · SAM LEE |
| Numbering | Its own count from #001 | Its own count from #1 |

## Trial and refunds

- No key: everything works for 14 days, then the pedal drops to clean for 4 seconds every minute.
- Refund or leaked key: add its number to `kRevokedFounder` / `kRevokedStandard` in `Source/Licence.cpp`. It stops working in the next update.

## Command-line version

`dyt_licence.py` does the same as Licence Desk (same keys, same ledger format) for automation later, e.g. issuing a key automatically when an order webhook fires. Needs `pip install cryptography`.

```bash
python3 dyt_licence.py batch presale.csv --edition founder
python3 dyt_licence.py issue --name "Sam Lee" --email sam@example.com
python3 dyt_licence.py verify "DYT-ER-..."
```
