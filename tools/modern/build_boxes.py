r"""MCM2:Reborn's message boxes: build_boxes.py <game ui folder> <out folder> <preview dir>

The three message boxes are cards over the screen that asks: the game's title and question,
and its buttons, whose words are the game's too (they change with the question). MessBox1 has
an edit box for an answer. GlobSet, Restore Defaults, chooses between the factory settings and
another profile's.

Writes MESSBOX1, MESSBOX2, MESSBOX3 and GLOBSET for the game's ui\modern\.
"""
import sys
from pathlib import Path

import reborn_kit as kit
from reborn_kit import EDGE, ORANGE, PAPER, box, heading
from reborn_layout import Screen, button_states, colorref, font, segment_states, under, xywh
from reborn_pages import card_ground, card_info, radio, scroll_bar, text_button

CARD = (1500, 620)
PAD = 56


def words(screen, name, area, size, colour, top=False):
    keys = {"Xywh": xywh(area), "TextLMargin": "0", "TextTMargin": "0"}
    keys.update(font(size, colour))
    if top:
        keys["TextAlignV"] = "TOP"
    screen.set(name, ("AnimNorm",), **keys)


def build_message(data, stem, buttons, edit=False):
    """A message: its title and question, an edit box with `edit`, and `buttons` along the foot,
    right to left."""
    screen = Screen(data, stem)
    w, h = CARD
    board = card_ground(w, h)
    words(screen, "TitleText", (PAD, PAD - 8, w - PAD, PAD + 56), 40, ORANGE)
    foot = h - PAD - 72
    bottom = foot - 24 - (92 if edit else 0)
    words(screen, "TxtPrompt", (PAD, PAD + 72, w - PAD, bottom), 32, PAPER, top=True)
    if edit:
        area = (PAD, foot - 24 - 76, w - PAD, foot - 24)
        box(board, area, (30, 31, 36, 255), 8, EDGE + (255,))
        keys = {"Xywh": xywh(area), "TextLMargin": "20", "TextTMargin": "0",
                "AnimNorm": screen.picture("MEditBox", under(board, area))}
        keys.update(font(32, PAPER))
        screen.set("EditBox", **keys)
    bw, gap = 300, 20
    for k, name in enumerate(buttons):
        x1 = w - PAD - k * (bw + gap)
        text_button(screen, board, name, (x1 - bw, foot, x1, foot + 72))
    card_info(screen, board)
    return screen, board


def build_restore(data):
    screen = Screen(data, "GLOBSET")
    w, h = 1100, 900
    board = card_ground(w, h)
    heading(board, "RESTORE SETTINGS", PAD, PAD + 40, 40, PAPER)
    row = (PAD, PAD + 80, w - PAD, PAD + 156)
    box(board, row, (30, 31, 36, 255), 8, EDGE + (255,))
    half = (row[2] - row[0] - 24 - 6) / 2
    for k, (name, label) in enumerate((("ButFactory", "FACTORY DEFAULTS"), ("ButCopyUser", "FROM A PROFILE"))):
        where = (row[0] + 12 + k * (half + 6), row[1] + 12, row[0] + 12 + k * (half + 6) + half, row[3] - 12)
        radio(screen, name, segment_states(board, where, label), where, (0, 0), chosen=k == 0)

    listing = (PAD, row[3] + 24, w - PAD, h - PAD - 72 - 24)
    box(board, listing, (12, 12, 14, 235), 8, EDGE + (255,))
    keys = {"Xywh": xywh((listing[0] + 8, listing[1] + 8, listing[2] - 56, listing[3] - 8)), "TextLMargin": "16",
            "TextTMargin": "0", "SelectColor": colorref(ORANGE)}
    keys.update(font(30, PAPER))
    screen.set("LstProfiles", **keys)
    bar = (listing[2] - 52, listing[1] + 16, listing[2] - 12, listing[3] - 16)
    screen.set("ScrollProfiles", **scroll_bar(screen, board, "ScrollProfiles", bar))

    foot = h - PAD - 72
    for k, (name, label, primary) in enumerate((("ButLoad", "RESTORE", True), ("ButCancel", "CANCEL", False))):
        x1 = w - PAD - k * 320
        where = (x1 - 300, foot, x1, foot + 72)
        screen.button(name, where, button_states(board, where, label, primary=primary, size=32))
    screen.hide("TitleText")
    card_info(screen, board)
    return screen, board


def main(ui, out, previews):
    ui, out, previews = Path(ui), Path(out), Path(previews)
    kit.fonts_from(ui / "modern" / "fonts")
    read = lambda stem: (ui / (stem + ".DAT")).read_bytes()             # noqa: E731
    cards = [build_message(read("MESSBOX1"), "MESSBOX1", ("OKButton", "CancelButton"), edit=True),
             build_message(read("MESSBOX2"), "MESSBOX2", ("ButRight", "ButMiddle", "ButLeft")),
             build_message(read("MESSBOX3"), "MESSBOX3", ("But4", "But3", "But2", "But1")),
             build_restore(read("GLOBSET"))]
    out.mkdir(parents=True, exist_ok=True)
    previews.mkdir(parents=True, exist_ok=True)
    for screen, board in cards:
        data = screen.pack()
        (out / (screen.stem + ".DAT")).write_bytes(data)
        print("%-8s %3d pictures, %d bytes" % (screen.stem, len(screen.pictures), len(data)))
        board.save(previews / ("reborn %s.png" % screen.stem.lower()))


if __name__ == "__main__":
    main(*sys.argv[1:4])
