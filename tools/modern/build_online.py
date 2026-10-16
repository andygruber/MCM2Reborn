r"""MCM2:Reborn's online and LAN screens: build_online.py <game ui folder> <out folder> <preview dir>

The game has no such screens, so their layouts start empty here: ONLINE.DAT (read as Online.dtm)
for Internet play through Epic Online Services, and LAN.DAT (Lan.dtm) for the local network. Both
are the game's sessions screen in MCM2:Reborn's look: the games being hosted in a list, Host to
make one, Join to join the one picked. dlgprocs.c, online_handle, runs them on the game's own
sessions code, so the controls keep the original's names (LstSessions, ButHost, ButJoin,
ButCancel), and writes their status.
"""
import sys
from pathlib import Path

import reborn_kit as kit
from build_mainmenu import rs2
from reborn_kit import EDGE, GREY, LEFT, ORANGE, PAPER, RAISED, RIGHT, TOP, box, dot, heading, sub
from reborn_layout import arrow_states, colorref, font, xywh
from reborn_pages import PLACE, footer, full_info, full_screen, preview, scroll_bar

EMPTY_LAYOUT = "\r\n".join(["[Set_Control]", "", "[Set_Information]", "Type = <Unknown>", "ScreenWidth = 640",
                            "ScreenHeight = 480", "NoScale = 0", "PopUp = 0", "", "[Set_Anim]", ""])

SESSIONS = (LEFT, PLACE[1], 1580, PLACE[3])
ABOUT = (1620, PLACE[1], RIGHT, PLACE[3])
WELL = (12, 12, 14, 235)
GROUP = "130"                   # the list's id, which its arrows and slider are attached to

# Each screen: its layout, title, the service in its corner, the list's heading and the help.
MODES = {
    "ONLINE": ("Online.dtm", "ONLINE", "EPIC ONLINE SERVICES", "LOBBIES",
               (("JOIN", "Pick a lobby and press Join, or double-click it."),
                ("HOST", "Host makes a lobby that everyone playing online can see. No addresses, no ports."))),
    "LAN": ("Lan.dtm", "LAN", "LOCAL NETWORK", "GAMES ON YOUR NETWORK",
            (("JOIN", "Pick a game and press Join, or double-click it."),
             ("HOST", "Host makes a game the computers on your network can see."))),
}


def build(stem):
    layout, title, service, listed, help_text = MODES[stem]
    data = rs2([(layout, EMPTY_LAYOUT.encode("latin-1"))])
    screen, board = full_screen(data, stem, "MULTIPLAYER", title)

    pill = (RIGHT - 640, TOP + 64, RIGHT, TOP + 124)
    box(board, pill, (16, 17, 20, 230), 30, EDGE + (255,))
    dot(board, (pill[0] + 36, (pill[1] + pill[3]) / 2), 9, ORANGE)
    sub(board, service, pill[0] + 62, (pill[1] + pill[3]) / 2, 24, PAPER, anchor="lm", bold=True)

    # The sessions, as the game lists them; its arrows and slider page through a long list.
    kit.panel(board, SESSIONS, 230)
    heading(board, listed, SESSIONS[0] + 36, SESSIONS[1] + 56, 28, ORANGE)
    listing = (SESSIONS[0] + 24, SESSIONS[1] + 88, SESSIONS[2] - 24, SESSIONS[3] - 24)
    box(board, listing, WELL, 8, EDGE + (255,))
    keys = {"Type": "LISTBOX", "AttachId": GROUP, "TextAlignV": "MIDDLE", "TextAlign": "LEFT",
            "Xywh": xywh((listing[0] + 8, listing[1] + 8, listing[2] - 56, listing[3] - 8)),
            "TextLMargin": "16", "TextTMargin": "0", "SelectColor": colorref(ORANGE)}
    keys.update(font(30, PAPER))
    screen.add("LstSessions", **keys)
    up = (listing[2] - 52, listing[1] + 8, listing[2] - 12, listing[1] + 52)
    down = (listing[2] - 52, listing[3] - 52, listing[2] - 12, listing[3] - 8)
    for name, kind, where, way in (("SessionUp", "SCROLLUP", up, "up"), ("SessionDown", "SCROLLDOWN", down, "down")):
        screen.add(name, Type=kind, GroupId=GROUP, AttachId=GROUP)
        screen.button(name, where, arrow_states(board, where, way))
    bar = (listing[2] - 52, up[3] + 8, listing[2] - 12, down[1] - 8)
    screen.add("ScrollSessions", Type="VSCROLLBAR", GroupId=GROUP, AttachId=GROUP)
    screen.set("ScrollSessions", **scroll_bar(screen, board, "ScrollSessions", bar))

    kit.panel(board, ABOUT, 230)
    heading(board, "HOW IT WORKS", ABOUT[0] + 36, ABOUT[1] + 56, 28, ORANGE)
    y = ABOUT[1] + 112
    for name, words in help_text:
        heading(board, name, ABOUT[0] + 36, y, 24, PAPER)
        y += 34
        for line in kit.wrap(words, 26, ABOUT[2] - ABOUT[0] - 72):
            sub(board, line, ABOUT[0] + 36, y, 26, GREY)
            y += 36
        y += 30

    # What the screen is doing, written by dlgprocs.c.
    status = (ABOUT[0] + 24, ABOUT[3] - 120, ABOUT[2] - 24, ABOUT[3] - 24)
    box(board, status, RAISED + (255,), 8, EDGE + (255,))
    keys = {"Type": "STATICTEXT", "Xywh": xywh((status[0] + 22, status[1], status[2] - 22, status[3])),
            "TextLMargin": "0", "TextTMargin": "0", "Text": '""'}
    keys.update(font(26, PAPER))
    screen.add("TxtStatus", **keys)

    for name in ("ButCancel", "ButHost", "ButJoin"):
        screen.add(name, Type="BUTTON")
    footer(screen, board, [("ButCancel", "BACK")], [("ButJoin", "JOIN", 540, True), ("ButHost", "HOST", 400, False)])
    full_info(screen, board)
    return screen, board, None


def main(ui, out, previews):
    ui, out, previews = Path(ui), Path(out), Path(previews)
    kit.fonts_from(ui / "modern" / "fonts")
    out.mkdir(parents=True, exist_ok=True)
    previews.mkdir(parents=True, exist_ok=True)
    photo = sorted(previews.parent.glob("menu photo *.jpg"))[0]
    for stem in MODES:
        screen = build(stem)
        data = screen[0].pack()
        (out / (stem + ".DAT")).write_bytes(data)
        print("%-8s %3d pictures, %d bytes" % (stem, len(screen[0].pictures), len(data)))
        preview(photo, [screen]).save(previews / ("reborn %s.png" % stem.lower()))


if __name__ == "__main__":
    main(*sys.argv[1:4])
