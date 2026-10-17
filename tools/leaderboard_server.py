"""leaderboard_server.py [port] [data file]: MCM2:Reborn's lap leaderboards (code/dev/leaders.c).

  POST /lap      body "kind<TAB>class<TAB>track<TAB>ms<TAB>name": a rider's lap, kept when it is
                 their best on that board
  GET  /board?kind=&class=&track=      the board, best first: "name<TAB>ms<TAB>date" lines

A board is a track's, an event kind's (1 baja, 2 nationals, 3 supercross, 5 enduro) and a bike
class's. The game points at it with [leaderboard] url=http://host:port in MCM2Reborn.ini. Nothing
here proves a lap was ridden: run it for friends, not the open internet, until it does.
"""
import json
import os
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

KINDS = {1, 2, 3, 5}
CLASSES = {125, 250, 400, 500, 600}
BOARD_ROWS = 100
FASTEST_MS = 5000                   # nothing laps a track quicker
NAME_MOST = 15                      # leaders.c's leader.name, less its end

lock = threading.Lock()
boards = {}                         # "kind class track" -> {name: [ms, date]}
data_file = "leaderboards.json"


def key(kind, cc, track):
    return "%d %d %s" % (kind, cc, track.lower())


def save():
    with open(data_file + ".new", "w", encoding="utf-8") as f:
        json.dump(boards, f)
    os.replace(data_file + ".new", data_file)


class Handler(BaseHTTPRequestHandler):
    def answer(self, code, words=""):
        body = words.encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        url = urlparse(self.path)
        if url.path != "/board":
            return self.answer(404)
        query = {k: v[0] for k, v in parse_qs(url.query).items()}
        try:
            board = key(int(query["kind"]), int(query["class"]), query["track"])
        except (KeyError, ValueError):
            return self.answer(400)
        with lock:
            laps = sorted(boards.get(board, {}).items(), key=lambda lap: lap[1][0])[:BOARD_ROWS]
        self.answer(200, "".join("%s\t%d\t%s\n" % (name, ms, date) for name, (ms, date) in laps))

    def do_POST(self):
        if self.path != "/lap":
            return self.answer(404)
        length = min(int(self.headers.get("Content-Length") or 0), 512)
        fields = self.rfile.read(length).decode("utf-8", "replace").split("\t")
        try:
            kind, cc, track, ms, name = int(fields[0]), int(fields[1]), fields[2], int(fields[3]), fields[4]
        except (IndexError, ValueError):
            return self.answer(400)
        name = name.strip()[:NAME_MOST]
        if kind not in KINDS or cc not in CLASSES or not track or not name or ms < FASTEST_MS:
            return self.answer(400)
        with lock:
            board = boards.setdefault(key(kind, cc, track), {})
            if name not in board or ms < board[name][0]:
                board[name] = [ms, time.strftime("%Y-%m-%d")]
                save()
        self.answer(200, "ok")

    def log_message(self, form, *args):
        sys.stderr.write("%s %s\n" % (self.address_string(), form % args))


def main():
    global boards, data_file
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8770
    data_file = sys.argv[2] if len(sys.argv) > 2 else data_file
    if os.path.exists(data_file):
        with open(data_file, encoding="utf-8") as f:
            boards = json.load(f)
    print("leaderboards on port %d, kept in %s" % (port, data_file))
    ThreadingHTTPServer(("", port), Handler).serve_forever()


if __name__ == "__main__":
    main()
