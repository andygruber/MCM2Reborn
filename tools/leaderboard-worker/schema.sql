-- MCM2:Reborn's leaderboards (worker.js). Run once: npx wrangler d1 execute mcm2reborn-leaderboards --remote --file schema.sql

CREATE TABLE IF NOT EXISTS riders (
    id       TEXT PRIMARY KEY,      -- the game's random rider id
    key_hash TEXT NOT NULL,         -- SHA-256 of its key
    name     TEXT NOT NULL,
    seen_at  INTEGER NOT NULL       -- ms since 1970
);

-- A rider's best lap on a board: an event kind, bike class and track.
CREATE TABLE IF NOT EXISTS laps (
    board   TEXT NOT NULL,          -- "kind|class|track", the track lower case
    kind    INTEGER NOT NULL,
    class   INTEGER NOT NULL,
    track   TEXT NOT NULL,
    rider   TEXT NOT NULL REFERENCES riders (id),
    name    TEXT NOT NULL,
    ms      INTEGER NOT NULL,
    set_at  INTEGER NOT NULL,
    version TEXT NOT NULL DEFAULT '',
    PRIMARY KEY (board, rider)
);

CREATE INDEX IF NOT EXISTS laps_by_time ON laps (board, ms);
CREATE INDEX IF NOT EXISTS laps_by_event ON laps (kind, class);
