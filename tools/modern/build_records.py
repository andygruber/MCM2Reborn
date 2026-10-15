r"""MCM2:Reborn's records: build_records.py <game ui folder> <out folder> <preview dir>

  GRBase: the saved ghosts and replays, a page each (reborn_pages.py), and Start;
  GhostFil and ReplyFil: a table of the files and their buttons;
  HiScores: the records by event and track. Its code is still the game's, which words its tabs
  and event buttons, so they are drawn without words for the game's to show.

Writes GRBASE, GHOSTFIL, REPLYFIL and HISCORES for the game's ui\modern\.
"""
import sys
from pathlib import Path

import reborn_kit as kit
from reborn_kit import EDGE, LEFT, ORANGE, PANEL, PAPER, RIGHT, box, heading
from reborn_layout import Screen, button_states, colorref, font, on_board, outline, shape, slanted, under, xywh
from reborn_pages import (PLACE, build_base, footer, full_info, full_screen, ground, page_info, preview,
                          scroll_bar, table)

WELL = (12, 12, 14, 235)


# ---- GRBase and its pages ----

FILES = [("TxtTrack", "LstTrack", 185), ("TxtRider", "LstRider", 90), ("TxtLaps", "LstLaps", 50),
         ("TxtLength", "LstLength", 80), ("TxtDesc", "LstDesc", 160)]


def build_files(data, stem, slider):
    """A page of saved files: their table, its scroll bar, Description and Delete."""
    screen = Screen(data, stem, PLACE[:2])
    board = ground()
    kit.panel(board, PLACE, 230)
    frame = (PLACE[0] + 24, PLACE[1] + 24, PLACE[2] - 24, PLACE[3] - 112)
    box(board, frame, WELL, 8, EDGE + (255,))
    screen.set("Screen", Xywh=xywh(frame, PLACE[:2]), AnimNorm=screen.picture("MScreen", under(board, frame)))
    screen.shown.append((under(board, frame), frame[:2]))
    table(screen, board, (frame[0], frame[1], frame[2] - 60, frame[3]), FILES, frame[:2], anchor="Screen")
    bar = (frame[2] - 56, frame[1] + 100, frame[2] - 12, frame[3] - 16)
    screen.set(slider, **scroll_bar(screen, board, slider, bar, frame[:2], anchor="Screen"))
    screen.hide("TxtTitle")
    for k, (name, words, w) in enumerate((("ButDelete", "DELETE", 280), ("ButDescription", "DESCRIPTION", 380))):
        x1 = PLACE[2] - 24 - k * (280 + 20)
        where = (x1 - w, PLACE[3] - 88, x1, PLACE[3] - 24)
        screen.button(name, where, button_states(board, where, words, size=30))
    page_info(screen, board)
    return screen, board, None


# ---- HiScores ----

EVENTS = ["TabQuarry", "TabTag", "TabSupercross", "TabNationals", "TabBaja", "TabEnduro"]


def worded_tab_states(board, area):
    """A tab the game words: a ghost, outlined and underlined orange when it is the one shown."""
    pictures = {}
    for state, on in (("On", True), ("Off", False)):
        canvas = on_board(board, area)
        shape(canvas, slanted(area), (40, 30, 18) if on else PANEL, 230)
        outline(canvas, slanted(area), ORANGE if on else EDGE, 3 if on else 2)
        if on:
            shape(canvas, slanted((area[0] + 12, area[3] - 8, area[2] - 4, area[3])), ORANGE)
        pictures[state] = under(canvas, area)
    pictures["OnGrey"], pictures["OffGrey"] = pictures["On"], pictures["Off"]
    return pictures


def worded_radio(screen, name, area, pictures, chosen, size=28):
    keys = screen.states(name, pictures, {"AnimOn": "On", "AnimOff": "Off", "AnimOnDisable": "OnGrey",
                                          "AnimOffDisable": "OffGrey"}, at=area[:2], show="On" if chosen else "Off")
    keys.update(font(size, PAPER, "CENTER"))
    screen.set(name, Xywh=xywh(area), FitToAnimOn="0", Anchor="", **keys)


def build_scores(data):
    screen, board = full_screen(data, "HISCORES", "RECORDS", "HIGH SCORES")
    # What the records are of: the game's title for them, beside the screen's.
    keys = {"Xywh": xywh((LEFT + kit.heading_ink("HIGH SCORES", 92) + 48, kit.TOP + 96, RIGHT, kit.TOP + 150)),
            "TextLMargin": "0", "TextTMargin": "0"}
    keys.update(font(30, (150, 150, 156)))
    screen.set("TxtTitle", **keys)

    x = LEFT
    for k, name in enumerate(("TabLeft", "TabMiddle", "TabRight")):
        area = (x, 274, x + 420, 338)
        worded_radio(screen, name, area, worded_tab_states(board, area), k == 0)
        x += 420 + 16

    events = (LEFT, 370, LEFT + 520, PLACE[3])
    kit.panel(board, events, 230)
    heading(board, "EVENT", events[0] + 36, events[1] + 56, 28, ORANGE)
    for k, name in enumerate(EVENTS):
        area = (events[0] + 24, events[1] + 80 + k * 88, events[2] - 24, events[1] + 80 + k * 88 + 76)
        worded_radio(screen, name, area, worded_tab_states(board, area), k == 2, size=30)

    tracks = (events[2] + 40, 370, events[2] + 40 + 760, PLACE[3])
    kit.panel(board, tracks, 230)
    title = (tracks[0] + 36, tracks[1] + 20, tracks[2] - 36, tracks[1] + 76)
    keys = {"Xywh": xywh(title), "TextLMargin": "0", "TextTMargin": "0", "Anchor": ""}
    keys.update(font(28, ORANGE))
    screen.set("TxtTrack", **keys)
    listing = (tracks[0] + 24, tracks[1] + 84, tracks[2] - 72, tracks[3] - 24)
    box(board, (listing[0], listing[1], tracks[2] - 24, listing[3]), WELL, 8, EDGE + (255,))
    keys = {"Xywh": xywh((listing[0] + 8, listing[1] + 8, listing[2], listing[3] - 8)), "Anchor": "",
            "TextLMargin": "16", "TextTMargin": "0", "SelectColor": colorref(ORANGE)}
    keys.update(font(30, PAPER))
    screen.set("LstTrack", **keys)
    bar = (tracks[2] - 68, listing[1] + 16, tracks[2] - 28, listing[3] - 16)
    screen.set("TrackScroll", **scroll_bar(screen, board, "TrackScroll", bar))

    stats = (tracks[2] + 40, 370, RIGHT, PLACE[3])
    kit.panel(board, stats, 230)
    frame = (stats[0] + 24, stats[1] + 24, stats[2] - 24, stats[3] - 24)
    box(board, frame, WELL, 8, EDGE + (255,))
    table(screen, board, frame, [("TxtStats1", "LstStats1", 92), ("TxtStats2", "LstStats2", 122)], (0, 0))
    for name in ("TxtStats1", "TxtStats2", "LstStats1", "LstStats2"):
        screen.set(name, Anchor="")

    for name in ("BoxLeft", "BoxRight", "BoxScrollBackLeft"):
        screen.hide(name)
    footer(screen, board, [("Help", "HELP")], [("Back", "DONE", 540, True)])
    full_info(screen, board)
    return screen, board, None


def main(ui, out, previews):
    ui, out, previews = Path(ui), Path(out), Path(previews)
    kit.fonts_from(ui / "modern" / "fonts")
    read = lambda stem: (ui / (stem + ".DAT")).read_bytes()             # noqa: E731
    base = build_base(read("GRBASE"), "GRBASE", "REPLAYS", ["GHOSTS", "REPLAYS"], ["TabLeft", "TabRight"],
                      [("Back", "BACK"), ("Help", "HELP")], [("Start", "START", 540, True)])
    pages = [build_files(read("GHOSTFIL"), "GHOSTFIL", "SldGhosts"),
             build_files(read("REPLYFIL"), "REPLYFIL", "SldReplays")]
    scores = build_scores(read("HISCORES"))
    out.mkdir(parents=True, exist_ok=True)
    previews.mkdir(parents=True, exist_ok=True)
    for screen, _, _ in [base] + pages + [scores]:
        data = screen.pack()
        (out / (screen.stem + ".DAT")).write_bytes(data)
        print("%-8s %3d pictures, %d bytes" % (screen.stem, len(screen.pictures), len(data)))
    photo = sorted(previews.parent.glob("menu photo *.jpg"))[0]
    for page in pages:
        preview(photo, [base, page]).save(previews / ("reborn %s.png" % page[0].stem.lower()))
    preview(photo, [scores]).save(previews / "reborn hiscores.png")


if __name__ == "__main__":
    main(*sys.argv[1:4])
