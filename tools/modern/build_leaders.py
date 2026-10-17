r"""MCM2:Reborn's lap leaderboards: build_leaders.py <game ui folder> <out folder> <preview dir>

LEADERS.DAT (read as Leaders.dtm), in place of the main menu's HIGH SCORES (dlgprocs.c,
leaders_handle; code/dev/leaders.c). Down the left the tracks with laps; across the top the event
and the bike class, each class its own board; on the right the board: rank, rider, best lap and
when it was set, and whether it is the server's or this computer's.

Each tab is two buttons, as it looks chosen and not (NAME, NAMEOn): the code shows one of them.
"""
import sys
from pathlib import Path

import reborn_kit as kit
from reborn_kit import EDGE, GREY, LEFT, ORANGE, PAPER, RIGHT, box, heading, sub
from build_mainmenu import rs2
from build_online import EMPTY_LAYOUT, PILL_Y, tab_states, text
from reborn_layout import colorref, font, xywh
from reborn_pages import PLACE, footer, full_info, full_screen, preview

KINDS = [("BtnKind1", "BAJA"), ("BtnKind2", "NATIONALS"), ("BtnKind3", "SUPERCROSS"), ("BtnKind5", "ENDURO")]
CLASSES = [("BtnClass125", "125 2-STROKE"), ("BtnClass250", "250 2-STROKE"), ("BtnClass400", "400 4-STROKE"),
           ("BtnClass500", "500 2-STROKE"), ("BtnClass600", "600 4-STROKE")]
ROWS = 12                                   # leaders_handle's BOARD_ROWS
TRACKS = (LEFT, PLACE[1] + 160, LEFT + 640, PLACE[3])
BOARD = (TRACKS[2] + 40, PLACE[1] + 160, RIGHT, PLACE[3])


def tabs(screen, board, items, y, size=26):
    x = LEFT
    for name, words in items:
        w = kit.heading_ink(words, size) + 34 + 40
        area = (x, y, x + w, y + 60)
        for suffix, on in (("", False), ("On", True)):
            screen.add(name + suffix, Type="BUTTON", SoundPush='"CLICK04"')
            screen.button(name + suffix, area, tab_states(board, area, words, on))
        x += w + 16


def build(data):
    screen, board = full_screen(data, "LEADERS", "RECORDS", "LEADERBOARDS")
    tabs(screen, board, KINDS, PLACE[1])
    tabs(screen, board, CLASSES, PLACE[1] + 76)

    # Where the board is from: the server, or this computer.
    pill = (RIGHT - 520, PILL_Y[0], RIGHT, PILL_Y[1])
    box(board, pill, (16, 17, 20, 230), 32, EDGE + (255,))
    text(screen, "TxtSource", (pill[0] + 36, pill[1], pill[2] - 24, pill[3]), 26, PAPER)

    kit.panel(board, TRACKS, 230)
    heading(board, "TRACKS", TRACKS[0] + 36, TRACKS[1] + 56, 28, ORANGE)
    listing = (TRACKS[0] + 24, TRACKS[1] + 80, TRACKS[2] - 24, TRACKS[3] - 24)
    box(board, listing, (12, 12, 14, 235), 8, EDGE + (255,))
    keys = {"Type": "LISTBOX", "Xywh": xywh((listing[0] + 8, listing[1] + 8, listing[2] - 8, listing[3] - 8)),
            "TextLMargin": "16", "TextTMargin": "0", "SelectColor": colorref(ORANGE), "SoundPush": '"CLICK04"'}
    keys.update(font(28, PAPER))
    screen.add("LstTracks", **keys)

    kit.panel(board, BOARD, 230)
    text(screen, "TxtBoard", (BOARD[0] + 36, BOARD[1] + 22, BOARD[2] - 36, BOARD[1] + 68), 34, PAPER)
    text(screen, "TxtBoardInfo", (BOARD[0] + 36, BOARD[1] + 70, BOARD[2] - 36, BOARD[1] + 102), 22, GREY, bold=False)
    head_y = BOARD[1] + 132
    for words, x in (("#", 36), ("RIDER", 120), ("BEST LAP", 640), ("SET", 900)):
        sub(board, words, BOARD[0] + x, head_y, 22, GREY, bold=True)
    pitch = (BOARD[3] - head_y - 40) / ROWS
    for k in range(ROWS):
        y = head_y + 16 + k * pitch
        if k % 2 == 0:
            box(board, (BOARD[0] + 20, y, BOARD[2] - 20, y + pitch - 6), (30, 31, 36, 200), 6)
        text(screen, "TxtRank%d" % k, (BOARD[0] + 36, y, BOARD[0] + 110, y + pitch - 6), 28, ORANGE)
        text(screen, "TxtName%d" % k, (BOARD[0] + 120, y, BOARD[0] + 620, y + pitch - 6), 28)
        text(screen, "TxtTime%d" % k, (BOARD[0] + 640, y, BOARD[0] + 880, y + pitch - 6), 28)
        text(screen, "TxtDate%d" % k, (BOARD[0] + 900, y, BOARD[2] - 36, y + pitch - 6), 22, GREY, bold=False)
    text(screen, "TxtEmpty", (BOARD[0] + 36, BOARD[1] + 300, BOARD[2] - 36, BOARD[1] + 380), 30, GREY, "CENTER")

    screen.add("ButBack", Type="BUTTON")
    footer(screen, board, [("ButBack", "BACK")], [])
    full_info(screen, board)
    return screen, board, None


def main(ui, out, previews):
    ui, out, previews = Path(ui), Path(out), Path(previews)
    kit.fonts_from(ui / "modern" / "fonts")
    out.mkdir(parents=True, exist_ok=True)
    previews.mkdir(parents=True, exist_ok=True)
    screen = build(rs2([("Leaders.dtm", EMPTY_LAYOUT.encode("latin-1"))]))
    data = screen[0].pack()
    (out / "LEADERS.DAT").write_bytes(data)
    print("LEADERS  %3d pictures, %d bytes" % (len(screen[0].pictures), len(data)))
    photo = sorted(previews.parent.glob("menu photo *.jpg"))[0]
    preview(photo, [screen]).save(previews / "reborn leaders.png")


if __name__ == "__main__":
    main(*sys.argv[1:4])
