r"""MCM2:Reborn's online and LAN screens: build_online.py <game ui folder> <out folder> <preview dir>

The game has no such screens, so their layouts start empty here: ONLINE.DAT (read as Online.dtm)
for Internet play through Epic Online Services, and LAN.DAT (Lan.dtm) for the local network.
Each is the game's sessions screen in MCM2:Reborn's look: a card for each game hosted (the game
finds five at most), the one picked shown in full on the right with the riders on its gate,
Host to make one and Join to join the one picked. Online's lobbies carry their race, so its
cards show the event's picture and track and its tabs sort them by event.

dlgprocs.c, online_handle, runs them on the game's own sessions code: its list (LstSessions)
is here but out of sight, and the cards, pictures and texts are shown and filled from it each
second. The events' pictures are the game's own loading screens.
"""
import io
import sys
from pathlib import Path

from PIL import Image, ImageDraw

import reborn_kit as kit
from build_mainmenu import VERSION, rs2
from reborn_kit import (EDGE, GREY, INK, LEFT, ORANGE, PANEL, PAPER, RAISED, RIGHT, SELECTED, TOP, box, dot,
                        heading, heading_ink, shape, slanted, sub, tag)
from reborn_layout import NOTHING, STATES, clear, font, on_board, rs2_files, under, xywh
from reborn_pages import PLACE, footer, full_info, full_screen, preview

EMPTY_LAYOUT = "\r\n".join(["[Set_Control]", "", "[Set_Information]", "Type = <Unknown>", "ScreenWidth = 640",
                            "ScreenHeight = 480", "NoScale = 0", "PopUp = 0", "", "[Set_Anim]", ""])

GREEN = (52, 199, 89)
CARDS = 5                               # the game keeps five sessions found
CARD_TOP, CARD_H, CARD_GAP = 372, 143, 14
CARD_RIGHT = 1600
LIST_BOTTOM = CARD_TOP + CARDS * (CARD_H + CARD_GAP) - CARD_GAP
HINT = (LEFT, LIST_BOTTOM + 20, CARD_RIGHT, 1268)     # where the mockup's join code will be
DETAIL = (1640, CARD_TOP, RIGHT, HINT[3])
THUMB = (20, 14, 210, CARD_H - 14)      # in a card
RIDERS = 8                              # the gate's slots, and a lobby's riders at most
PIP_W, PIP_H, PIP_GAP = 22, 16, 7
TABS_Y = (PLACE[1], PLACE[1] + 60)
PILL_Y = (TOP + 60, TOP + 124)

# The game's events by race_kind (menus.h), their names and loading screens.
KINDS = [(0, "STUNT QUARRY", "LOADQUAR", "stuntquarry"), (1, "BAJA", "LOADBAJA", "baja"),
         (2, "NATIONALS", "LOADNAT", "nationals"), (3, "SUPERCROSS", "LOADSX", "supercross"),
         (4, "TAG", "LOADTAG", "tag"), (5, "ENDURO", "LOADEND", "enduro")]
# Online's tabs: the control and its words; which events each holds is dlgprocs.c's.
TABS = [("TabAll", "ALL"), ("TabRaces", "RACES"), ("TabQuarry", "STUNT QUARRY"), ("TabTag", "TAG")]

# Each screen: its layout, title, the service tag and its dot, its tabs, and the line under the gate.
MODES = {
    "ONLINE": ("Online.dtm", "ONLINE", GREEN, True, "PEER TO PEER  ·  NO PORT FORWARDING"),
    "LAN": ("Lan.dtm", "LAN", ORANGE, False, "THE COMPUTERS ON YOUR NETWORK"),
}


def text(screen, name, area, size, colour=PAPER, align="LEFT", bold=True):
    """A line the code writes."""
    keys = {"Type": "STATICTEXT", "Xywh": xywh(area), "TextLMargin": "0", "TextTMargin": "0", "Text": '""'}
    keys.update(font(size, colour, align, bold))
    screen.add(name, **keys)


def picture(screen, name, area, img):
    """A picture the code shows or hides."""
    screen.add(name, Type="STATIC", Xywh=xywh(area), AnimNorm=screen.picture("M" + name, img))
    screen.shown.append((img, (int(area[0]), int(area[1]))))


def event_pictures(ui):
    """Each event's picture, from its loading screen."""
    pictures = {}
    for kind, _, archive, stem in KINDS:
        files = dict(rs2_files((ui / (archive + ".DAT")).read_bytes()))
        pictures[kind] = Image.open(io.BytesIO(files["Loading\\%s.tga" % stem])).convert("RGBA")
    return pictures


def rounded(img, size, radius):
    """`img` cut to `size` with its corners rounded."""
    out = kit.cover(img.convert("RGB"), size).convert("RGBA")
    mask = Image.new("L", size, 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, size[0] - 1, size[1] - 1), radius, fill=255)
    out.putalpha(mask)
    return out


def card_states(board, area):
    """A lobby's card: dark, and lit with an orange edge under the pointer."""
    pictures = {}
    for state in STATES:
        canvas = on_board(board, area)
        lit = state in ("Lit", "Down")
        box(canvas, area, (SELECTED if lit else PANEL) + (235,), 10, (ORANGE if lit else EDGE) + (255,))
        pictures[state] = under(canvas, area)
    return pictures


def tab_states(board, area, words, on):
    """A tab, its words at the left and its count written after them; `on` is the chosen one's."""
    pictures = {}
    for state in STATES:
        canvas = on_board(board, area)
        lit = state in ("Lit", "Down")
        if on:
            shape(canvas, slanted(area), ORANGE)
        else:
            shape(canvas, slanted(area), (40, 30, 18) if lit else PANEL, 230)
            kit.outline(canvas, slanted(area), ORANGE if lit else EDGE, 2)
        heading(canvas, words, area[0] + 34, (area[1] + area[3]) / 2 + kit.cap_height(26) / 2, 26,
                INK if on else PAPER)
        pictures[state] = under(canvas, area)
    return pictures


def pips(count):
    """`count` of eight riders, as a row of squares."""
    img = clear((RIDERS * (PIP_W + PIP_GAP), PIP_H))
    d = ImageDraw.Draw(img)
    for k in range(RIDERS):
        x = k * (PIP_W + PIP_GAP)
        if k < count:
            d.rounded_rectangle((x, 0, x + PIP_W - 1, PIP_H - 1), 3, fill=ORANGE + (255,))
        else:
            d.rounded_rectangle((x, 0, x + PIP_W - 1, PIP_H - 1), 3, outline=EDGE + (255,), width=2)
    return img


def header_picture(img, size, words):
    """The detail's picture: the event's, fading into the panel, its name on an orange tag."""
    out = rounded(img, size, 12)
    fade = Image.new("RGBA", size, (0, 0, 0, 0))
    d = ImageDraw.Draw(fade)
    for y in range(size[1]):
        d.line((0, y, size[0], y), fill=PANEL + (int(245 * max(0, y / size[1] - 0.35) / 0.65),))
    out.alpha_composite(fade)
    tag(out, words, 24, 24, 26)
    return out


def build_tabs(screen, board):
    x = LEFT
    for name, words in TABS:
        w = heading_ink(words, 26) + 34 + 90
        area = (x, TABS_Y[0], x + w, TABS_Y[1])
        for suffix, on in (("", False), ("On", True)):
            screen.add(name + suffix, Type="BUTTON")
            screen.button(name + suffix, area, tab_states(board, area, words, on))
        count = (area[2] - 76, area[1], area[2] - 20, area[3])
        text(screen, name + "Count", count, 26, ORANGE)
        x += w + 18


def build_cards(screen, board, events):
    thumb_size = (THUMB[2] - THUMB[0], THUMB[3] - THUMB[1])
    for i in range(CARDS):
        x0, y0, x1 = LEFT, CARD_TOP + i * (CARD_H + CARD_GAP), CARD_RIGHT
        area = (x0, y0, x1, y0 + CARD_H)
        name = "Lobby%d" % i
        screen.add(name, Type="BUTTON")
        screen.button(name, area, card_states(board, area))
        # The chosen card's edge, over it.
        edge = clear((x1 - x0, CARD_H))
        ImageDraw.Draw(edge).rounded_rectangle((0, 0, x1 - x0 - 1, CARD_H - 1), 10, outline=ORANGE + (255,), width=4)
        screen.add(name + "Chosen", Type="STATIC", Xywh=xywh(area), AnimNorm=screen.picture("M%sChosen" % name, edge))
        thumb = (x0 + THUMB[0], y0 + THUMB[1], x0 + THUMB[2], y0 + THUMB[3])
        for kind, img in events.items():
            picture(screen, "%sPic%d" % (name, kind), thumb, rounded(img, thumb_size, 8))
        tx = thumb[2] + 34
        text(screen, name + "Name", (tx, y0 + 22, x0 + 1000, y0 + 84), 42)
        text(screen, name + "Info", (tx, y0 + 88, x0 + 1060, y0 + 124), 26, GREY, bold=False)
        text(screen, name + "Riders", (x0 + 1080, y0 + 26, x1 - 40, y0 + 70), 26, PAPER)
        for count in range(RIDERS + 1):
            row = pips(count)
            where = (x0 + 1080, y0 + 86, x0 + 1080 + row.width, y0 + 86 + row.height)
            screen.add("%sPips%d" % (name, count), Type="STATIC", Xywh=xywh(where),
                       AnimNorm=screen.picture("M%sPips%d" % (name, count), row))
        text(screen, name + "Tag", (x1 - 260, y0 + 80, x1 - 40, y0 + 124), 24, ORANGE, "RIGHT")
    text(screen, "TxtEmpty", (LEFT, CARD_TOP + 300, CARD_RIGHT, CARD_TOP + 380), 40, PAPER, "CENTER")


def build_detail(screen, board, events, gate_note):
    kit.panel(board, DETAIL, 230)
    x0, y0, x1, y1 = DETAIL
    head = (x0, y0, x1, y0 + 240)
    for kind, words, _, _ in KINDS if events else ():
        picture(screen, "DetailPic%d" % kind, head, header_picture(events[kind], (x1 - x0, 240), words))
    text(screen, "TxtDetailName", (x0 + 40, y0 + 168, x1 - 40, y0 + 236), 54)
    text(screen, "TxtDetailInfo", (x0 + 40, y0 + 250, x1 - 40, y0 + 290), 26, GREY, bold=False)
    heading(board, "ON THE GATE", x0 + 40, y0 + 352, 26, PAPER)
    text(screen, "TxtDetailCount", (x1 - 240, y0 + 318, x1 - 40, y0 + 360), 28, ORANGE, "RIGHT")
    slot_w = (x1 - x0 - 80 - 16) / 2
    for k in range(RIDERS):
        sx = x0 + 40 + (k % 2) * (slot_w + 16)
        sy = y0 + 378 + (k // 2) * 70
        box(board, (sx, sy, sx + slot_w, sy + 58), RAISED + (255,), 8, EDGE + (255,))
        text(screen, "TxtRider%d" % k, (sx + 24, sy, sx + slot_w - 24, sy + 58), 28)
    # The race's settings, in boxes as the mockup's.
    box_w = (x1 - x0 - 80 - 3 * 16) / 4
    for k, (name, label) in enumerate((("Laps", "LAPS"), ("Trees", "TREES"), ("Most", "RIDERS"),
                                       ("Version", "VERSION"))):
        bx = x0 + 40 + k * (box_w + 16)
        by = y0 + 378 + 4 * 70 + 18
        box(board, (bx, by, bx + box_w, by + 86), RAISED + (255,), 8, EDGE + (255,))
        sub(board, label, bx + 18, by + 30, 20, GREY)
        if name == "Version":
            heading(board, VERSION, bx + 18, by + 72, 30, PAPER)
        else:
            text(screen, "TxtDetail" + name, (bx + 18, by + 40, bx + box_w - 12, by + 80), 30)
    sub(board, gate_note, x0 + 40, y1 - 32, 24, GREY)


def build(stem, events):
    layout, title, colour, tabs, gate_note = MODES[stem]
    data = rs2([(layout, EMPTY_LAYOUT.encode("latin-1"))])
    screen, board = full_screen(data, stem, "MULTIPLAYER", title)

    # The service, and the rider playing, at the top right.
    pill = (RIGHT - 700, PILL_Y[0], RIGHT, PILL_Y[1])
    box(board, pill, (16, 17, 20, 230), 32, (colour if colour == GREEN else EDGE) + (255,))
    dot(board, (pill[0] + 38, (pill[1] + pill[3]) / 2), 10, colour)
    text(screen, "TxtService", (pill[0] + 64, pill[1], pill[2] - 24, pill[3]), 28, colour if colour == GREEN else PAPER)
    chip = (pill[0] - 24 - 420, PILL_Y[0], pill[0] - 24, PILL_Y[1])
    box(board, chip, (16, 17, 20, 230), 32, EDGE + (255,))
    text(screen, "TxtPlayer", (chip[0] + 32, chip[1], chip[2] - 24, chip[3]), 30)
    text(screen, "TxtStatus", (CARD_RIGHT - 600, TABS_Y[0], RIGHT, TABS_Y[1]), 24, GREY, "RIGHT", False)
    if tabs:
        build_tabs(screen, board)
    else:
        heading(board, "GAMES ON YOUR NETWORK", LEFT, TABS_Y[1] - 14, 30, ORANGE)

    events = events if tabs else {}          # a LAN game's event is not known before joining
    kit.panel(board, HINT, 230)
    heading(board, "HOST OR JOIN", HINT[0] + 36, HINT[1] + 46, 26, PAPER)
    sub(board, "Pick a game and press Join. Host makes one " + ("everyone playing online can see."
        if tabs else "the computers on your network can see."), HINT[0] + 36, HINT[1] + 82, 24, GREY)
    build_cards(screen, board, events)
    build_detail(screen, board, events, gate_note)

    # The game's own list, which the code reads and picks in: out of sight.
    screen.add("LstSessions", Type="LISTBOX", Xywh="0, 0, 4, 4", AnimNorm='"%s"' % NOTHING, TextColor="0")

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
    events = event_pictures(ui)
    photo = sorted(previews.parent.glob("menu photo *.jpg"))[0]
    for stem in MODES:
        screen = build(stem, events)
        data = screen[0].pack()
        (out / (stem + ".DAT")).write_bytes(data)
        print("%-8s %3d pictures, %d bytes" % (stem, len(screen[0].pictures), len(data)))
        preview(photo, [screen]).save(previews / ("reborn %s.png" % stem.lower()))


if __name__ == "__main__":
    main(*sys.argv[1:4])
