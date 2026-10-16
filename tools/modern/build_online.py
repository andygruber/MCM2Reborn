r"""MCM2:Reborn's online screen: build_online.py <game ui folder> <out folder> <preview dir>

The game has no such screen, so its layout starts empty here (ONLINE.DAT, read as Online.dtm).
It is where online play through Epic Online Services will be: its sessions, friends and join
codes. Until then a friend is joined by their address (TCP/IP, as the original's Internet
was), and Host makes a game as LAN does. dlgprocs.c, online_handle, answers its buttons and
writes its status.
"""
import sys
from pathlib import Path

import reborn_kit as kit
from build_mainmenu import rs2
from reborn_kit import EDGE, GREY, LEFT, ORANGE, PAPER, RAISED, RIGHT, TOP, box, dot, heading, sub
from reborn_layout import font, under, xywh
from reborn_pages import PLACE, footer, full_info, full_screen, preview

EMPTY_LAYOUT = "\r\n".join(["[Set_Control]", "", "[Set_Information]", "Type = <Unknown>", "ScreenWidth = 640",
                            "ScreenHeight = 480", "NoScale = 0", "PopUp = 0", "", "[Set_Anim]", ""])

SESSIONS = (LEFT, PLACE[1], 1580, 1150)
JOIN = (LEFT, 1180, 1580, PLACE[3])
ABOUT = (1620, PLACE[1], RIGHT, PLACE[3])
FADED = (120, 122, 128)


def build_online():
    data = rs2([("Online.dtm", EMPTY_LAYOUT.encode("latin-1"))])
    screen, board = full_screen(data, "ONLINE", "MULTIPLAYER", "ONLINE")

    # Where the service will be: not there yet.
    pill = (RIGHT - 640, TOP + 64, RIGHT, TOP + 124)
    box(board, pill, (16, 17, 20, 230), 30, EDGE + (255,))
    dot(board, (pill[0] + 36, (pill[1] + pill[3]) / 2), 9, FADED)
    sub(board, "EPIC ONLINE SERVICES  ·  COMING SOON", pill[0] + 62, (pill[1] + pill[3]) / 2, 24, GREY,
        anchor="lm", bold=True)

    kit.panel(board, SESSIONS, 230)
    heading(board, "SESSIONS", SESSIONS[0] + 36, SESSIONS[1] + 56, 28, ORANGE)
    cx = (SESSIONS[0] + SESSIONS[2]) / 2
    heading(board, "NO ONLINE SESSIONS YET", cx, 640, 56, PAPER, anchor="ms")
    y = 700
    for line in kit.wrap("Online play through Epic Online Services is on its way: the games being hosted, your "
                         "friends and join codes will show here. Until then, join a friend by their address.",
                         30, 1100):
        sub(board, line, cx, y, 30, GREY, anchor="ms")
        y += 44

    kit.panel(board, JOIN, 230)
    heading(board, "JOIN BY ADDRESS", JOIN[0] + 36, JOIN[1] + 46, 26, PAPER)
    sub(board, "A friend's IP address, like 203.0.113.7, or a computer's name on your network",
        JOIN[0] + 36, JOIN[1] + 74, 22, GREY)
    edit = (JOIN[2] - 24 - 560, JOIN[1] + 16, JOIN[2] - 24, JOIN[3] - 16)
    box(board, edit, (30, 31, 36, 255), 8, ORANGE + (255,))
    keys = {"Type": "EDITBOX", "Xywh": xywh(edit), "MaxLen": "120", "TextLMargin": "20", "TextTMargin": "0",
            "AnimNorm": screen.picture("MEditAddress", under(board, edit))}
    keys.update(font(32, PAPER))
    screen.add("EditAddress", **keys)

    kit.panel(board, ABOUT, 230)
    heading(board, "PLAYING ONLINE NOW", ABOUT[0] + 36, ABOUT[1] + 56, 28, ORANGE)
    y = ABOUT[1] + 112
    for title, words in (("JOIN", "Type the host's address and press Join."),
                         ("HOST", "Host makes the game on this computer. Friends join with your public IP "
                                  "address; your router has to let DirectPlay through: TCP 47624, and TCP and "
                                  "UDP 2300 to 2400."),
                         ("SOON", "With Epic Online Services: no addresses and no ports, a code to join a "
                                  "friend, and a list of the games being hosted.")):
        heading(board, title, ABOUT[0] + 36, y, 24, PAPER)
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

    for name in ("Back", "ButHost", "ButJoin"):
        screen.add(name, Type="BUTTON")
    footer(screen, board, [("Back", "BACK")], [("ButJoin", "JOIN", 540, True), ("ButHost", "HOST", 400, False)])
    full_info(screen, board)
    return screen, board, None


def main(ui, out, previews):
    ui, out, previews = Path(ui), Path(out), Path(previews)
    kit.fonts_from(ui / "modern" / "fonts")
    screen = build_online()
    out.mkdir(parents=True, exist_ok=True)
    previews.mkdir(parents=True, exist_ok=True)
    data = screen[0].pack()
    (out / "ONLINE.DAT").write_bytes(data)
    print("ONLINE   %3d pictures, %d bytes" % (len(screen[0].pictures), len(data)))
    photo = sorted(previews.parent.glob("menu photo *.jpg"))[0]
    preview(photo, [screen]).save(previews / "reborn online.png")


if __name__ == "__main__":
    main(*sys.argv[1:4])
