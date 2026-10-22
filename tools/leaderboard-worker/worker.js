/*
    MCM2:Reborn's lap leaderboards: a Cloudflare Worker over a D1 database (schema.sql), as Open
    Unleashed keeps its own. The game's side is code/dev/leaders.c; every answer is plain text, a
    line a row, its fields tab separated.

      POST /v1/lap     rider key name kind class track ms version
                       -> "ok<TAB>best<TAB>rank<TAB>riders", the rider's best kept, never a slower lap
      GET  /v1/board   ?kind=&class=&track=   -> name<TAB>ms<TAB>date, best first (100)
      GET  /v1/tracks  ?kind=&class=          -> track<TAB>riders, every track with laps

    A rider is a random id the game makes once, and a key only it knows: the first lap from an id
    keeps a hash of its key, and laps with another key are turned away.

    A lap that beats the rider's best is told to Discord when the DISCORD_WEBHOOK secret is set
    (npx wrangler secret put DISCORD_WEBHOOK), unless its version is "old": a copy sending the laps
    it kept before it had the server.
*/

const KINDS = new Set([1, 2, 3, 5]);            // Baja, national, supercross, enduro
const CLASSES = new Set([125, 250, 400, 500, 600]);
const FASTEST_MS = 5000, SLOWEST_MS = 3600000;
const BOARD_ROWS = 100;
const HEX32 = /^[0-9a-f]{32}$/;

const text = (body, status = 200) =>
    new Response(body, { status, headers: { "content-type": "text/plain; charset=utf-8" } });

// Printable, no tabs or line ends, trimmed, at most `most` characters.
function clean(value, most) {
    const kept = String(value ?? "").replace(/[\x00-\x1f\x7f]/g, "").trim();
    return kept.length && kept.length <= most ? kept : null;
}

const boardOf = (kind, cls, track) => `${kind}|${cls}|${track.toLowerCase()}`;

const KIND_NAMES = { 1: "Baja", 2: "Nationals", 3: "Supercross", 5: "Enduro" };
const CLASS_NAMES = { 125: "125 2-stroke", 250: "250 2-stroke", 400: "400 4-stroke", 500: "500 2-stroke", 600: "600 4-stroke" };

const lapTime = ms => `${Math.floor(ms / 60000)}:${String(Math.floor(ms / 1000) % 60).padStart(2, "0")}.${String(ms % 1000).padStart(3, "0")}`;
const plain = words => words.replace(/[\\*_~`|>#[\]()@<]/g, "\\$&");    // no markdown, links or mentions

function tellDiscord(webhook, lap) {
    const record = lap.rank === 1;
    const embed = {
        title: record ? "New track record" : "New personal best",
        description: `**${plain(lap.name)}**  ${lapTime(lap.ms)}  on **${plain(lap.track)}**`,
        color: record ? 0xFF8C1A : 0x4A90D9,
        fields: [
            { name: "Event", value: `${KIND_NAMES[lap.kind]} · ${CLASS_NAMES[lap.cls]}`, inline: true },
            { name: "Place", value: `${lap.rank} of ${lap.riders}`, inline: true },
        ],
        footer: { text: lap.before ? `was ${lapTime(lap.before)}` : "first lap on this board" },
    };
    return fetch(webhook, {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify({ username: "MCM2:Reborn Leaderboards", embeds: [embed], allowed_mentions: { parse: [] } }),
    }).catch(error => console.error("discord:", error));
}

async function sha256(value) {
    const digest = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(value));
    return [...new Uint8Array(digest)].map(b => b.toString(16).padStart(2, "0")).join("");
}

async function postLap(request, env, ctx) {
    const db = env.DB;
    const fields = (await request.text()).split("\t");
    if (fields.length !== 8)
        return text("bad lap", 400);
    const [rider, key] = fields;
    const name = clean(fields[2], 15), track = clean(fields[5], 63), version = clean(fields[7], 16) ?? "";
    const kind = Number(fields[3]), cls = Number(fields[4]), ms = Number(fields[6]);
    if (!HEX32.test(rider) || !HEX32.test(key) || !name || !track || !KINDS.has(kind) || !CLASSES.has(cls)
        || !Number.isInteger(ms) || ms < FASTEST_MS || ms > SLOWEST_MS)
        return text("bad lap", 400);

    const keyHash = await sha256(key);
    const known = await db.prepare("SELECT key_hash FROM riders WHERE id = ?").bind(rider).first();
    if (known && known.key_hash !== keyHash)
        return text("not your rider", 403);
    const now = Date.now(), board = boardOf(kind, cls, track);
    const before = await db.prepare("SELECT ms FROM laps WHERE board = ? AND rider = ?").bind(board, rider).first();
    await db.batch([
        known ? db.prepare("UPDATE riders SET name = ?, seen_at = ? WHERE id = ?").bind(name, now, rider)
              : db.prepare("INSERT INTO riders (id, key_hash, name, seen_at) VALUES (?, ?, ?, ?)")
                  .bind(rider, keyHash, name, now),
        db.prepare(`INSERT INTO laps (board, kind, class, track, rider, name, ms, set_at, version)
                    VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
                    ON CONFLICT (board, rider) DO UPDATE SET
                        ms = excluded.ms, set_at = excluded.set_at, name = excluded.name,
                        track = excluded.track, version = excluded.version
                    WHERE excluded.ms < laps.ms`)
          .bind(board, kind, cls, track, rider, name, ms, now, version),
    ]);
    const best = await db.prepare("SELECT ms FROM laps WHERE board = ? AND rider = ?").bind(board, rider).first();
    const place = await db.prepare(`SELECT COUNT(*) + 1 AS rank, (SELECT COUNT(*) FROM laps WHERE board = ?1) AS riders
                                    FROM laps WHERE board = ?1 AND ms < ?2`).bind(board, best.ms).first();
    if (env.DISCORD_WEBHOOK && version !== "old" && (!before || ms < before.ms))
        ctx.waitUntil(tellDiscord(env.DISCORD_WEBHOOK, { name, track, kind, cls, ms, before: before?.ms,
                                                         rank: place.rank, riders: place.riders }));
    return text(`ok\t${best.ms}\t${place.rank}\t${place.riders}`);
}

function wanted(url) {
    const kind = Number(url.searchParams.get("kind")), cls = Number(url.searchParams.get("class"));
    return KINDS.has(kind) && CLASSES.has(cls) ? { kind, cls } : null;
}

async function getBoard(url, db) {
    const which = wanted(url), track = clean(url.searchParams.get("track"), 63);
    if (!which || !track)
        return text("bad board", 400);
    const { results } = await db.prepare("SELECT name, ms, set_at FROM laps WHERE board = ? ORDER BY ms, set_at LIMIT ?")
        .bind(boardOf(which.kind, which.cls, track), BOARD_ROWS).all();
    return text(results.map(r => `${r.name}\t${r.ms}\t${new Date(r.set_at).toISOString().slice(0, 10)}\n`).join(""));
}

async function getTracks(url, db) {
    const which = wanted(url);
    if (!which)
        return text("bad event", 400);
    const { results } = await db.prepare(`SELECT MIN(track) AS track, COUNT(*) AS riders FROM laps
                                          WHERE kind = ? AND class = ? GROUP BY board ORDER BY track COLLATE NOCASE`)
        .bind(which.kind, which.cls).all();
    return text(results.map(r => `${r.track}\t${r.riders}\n`).join(""));
}

export default {
    async fetch(request, env, ctx) {
        const url = new URL(request.url);
        try {
            if (request.method === "POST" && url.pathname === "/v1/lap")
                return await postLap(request, env, ctx);
            if (request.method === "GET" && url.pathname === "/v1/board")
                return await getBoard(url, env.DB);
            if (request.method === "GET" && url.pathname === "/v1/tracks")
                return await getTracks(url, env.DB);
            return text("not found", 404);
        } catch (error) {
            console.error(error);
            return text("server error", 500);
        }
    },
};
