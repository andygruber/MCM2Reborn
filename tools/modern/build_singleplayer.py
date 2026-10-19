r"""MCM2:Reborn's single player screen: build_singleplayer.py <game ui folder> <out folder> <preview dir>

SPBase is the screen around the three pages (reborn_pages.py). The pages:

  the event page: event cards in place of the event list (dlgprocs.c keeps the two together),
  the tracks as a list (TrackList, which dlgprocs.c keeps on the game's track), the track's
  picture, and the race's settings down the right;
  the bike page (reborn_pages.py);
  the race information page: the last race's results.

Writes SPBASE.DAT, SPEVENT.DAT, SPBIKER.DAT and SPRINFO.DAT for the game's ui\modern\.
"""
import io
import sys
from pathlib import Path

from PIL import Image

import reborn_kit as kit
from build_mainmenu import rs2_files
from reborn_kit import (EDGE, GREY, LEFT, ORANGE, PANEL, PAPER, RIGHT, SOFT, box, heading, outline, shape,
                        slanted, sub)
from reborn_layout import (LABEL, Screen, arrow_states, button_states, colorref, font, on_board, segment_states,
                           under, xywh)
from reborn_pages import (PLACE, build_base, build_bike, chip, combo, ground, keyed_outside, local, page_info,
                          preview, radio, scroll_bar, stepper, table, toggle)

SELECTED = (42, 33, 22)

TABS = ["EventTab", "BikeRiderTab", "RaceInfoTab"]
TITLES = ["CHOOSE YOUR EVENT", "BIKE & RIDER", "RACE INFO"]

# The event cards: control, the event list's kind, title, what it is, the loading screen with its art.
CARDS = [("CardSupercross", 3, "SUPERCROSS", "STADIUM", "LOADSX"),
         ("CardNationals", 2, "NATIONALS", "OUTDOOR MX", "LOADNAT"),
         ("CardBaja", 1, "BAJA", "POINT TO POINT", "LOADBAJA"),
         ("CardEnduro", 5, "ENDURO", "CHECKPOINTS", "LOADEND"),
         ("CardQuarry", 0, "STUNT QUARRY", "SCORE ATTACK", "LOADQUAR")]
CARD_GROUP = 150
TRACK_GROUP = 151


# ---- The event page ----

def event_art(ui, stem):
    """The event's gold rider, from its loading screen."""
    for name, blob in rs2_files((ui / (stem + ".DAT")).read_bytes()):
        if name.lower().endswith(".tga") and "bar" not in name.lower():
            return Image.open(io.BytesIO(blob)).convert("RGB").crop((20, 72, 620, 420))
    raise FileNotFoundError(stem)


def event_cards(screen, board, ui):
    top, bottom = PLACE[1], PLACE[1] + 300
    gap = 22
    lean = (bottom - top) * kit.SLANT
    card_w = (PLACE[2] - PLACE[0] - lean - (len(CARDS) - 1) * gap) / len(CARDS)
    for i, (name, _, title, kind, stem) in enumerate(CARDS):
        x0 = PLACE[0] + i * (card_w + gap)
        area = (int(x0), top, int(x0 + card_w + lean), bottom)
        points = slanted(area)
        face = kit.fade_down(kit.cover(event_art(ui, stem), (area[2] - area[0], bottom - top)))
        pictures = {}
        for state, chosen in (("On", True), ("Off", False)):
            canvas = on_board(board, area)
            art = face if chosen else Image.alpha_composite(face, Image.new("RGBA", face.size, (12, 12, 14, 140)))
            kit.paste_shape(canvas, art, area)
            if chosen:
                outline(canvas, points, ORANGE, 6)
                shape(canvas, slanted((area[0] + 28, bottom - 12, area[2] - 2, bottom)), ORANGE)
            size = kit.fit(title, 38, card_w - 70)
            heading(canvas, title, area[0] + 44, bottom - 62, size, PAPER if chosen else SOFT)
            sub(canvas, kind, area[0] + 36, bottom - 28, 22, ORANGE if chosen else (120, 122, 128))
            pictures[state] = keyed_outside(under(canvas, area), points, area[:2])
        pictures["OnGrey"], pictures["OffGrey"] = pictures["On"], pictures["Off"]
        radio(screen, name, pictures, area, chosen=i == 1, AttachId=str(CARD_GROUP), FitToAnimOn="0")


def track_panel(screen, board):
    """The tracks: TrackList, in a group of its own with its scroll bar and arrows (in the game's
    track group, a scroll bar would scroll the track's picture too), kept by dlgprocs.c on the
    track the game's list has."""
    area = (LEFT, 614, 900, PLACE[3])
    box(board, area, PANEL + (255,), 10)
    heading(board, "TRACKS", area[0] + 36, area[1] + 58, 30, ORANGE)
    bar_x0, bar_x1 = area[2] - 56, area[2] - 12
    for k, (name, label) in enumerate((("TrackAll", "ALL"), ("TrackBase", "BASE GAME"), ("TrackCustom", "CUSTOM"))):
        where = (area[0] + 20 + k * 244, area[1] + 76, area[0] + 252 + k * 244, area[1] + 128)
        radio(screen, name, segment_states(board, where, label), where,
              chosen=k == 0, AttachId="152", FitToAnimOn="0")
    listing = (area[0] + 12, area[1] + 144, bar_x0 - 8, area[3] - 12)
    keys = {"Type": "LISTBOX", "AttachId": str(TRACK_GROUP), "Xywh": local(listing), "MaxItems": "2048",
            "Selectable": "1", "AllowWScroll": "0", "TextLMargin": "24", "TextTMargin": "0",
            "SelectColor": colorref(ORANGE), "SelectBoxColor": colorref(SELECTED), "ItemBoxColor": colorref(PANEL)}
    keys.update(font(34, SOFT))
    screen.add("TrackList", **keys)
    # Rows like the game's, for the preview.
    for k, words in enumerate(["Bela's Backwards", "Third Degree Berms", "El Centro"]):
        screen.shown.append((text_row(listing, words, k == 1), (listing[0], int(listing[1] + k * 46))))

    up = (bar_x0, listing[1], bar_x1, listing[1] + 44)
    down = (bar_x0, listing[3] - 44, bar_x1, listing[3])
    for name, where, kind, way in (("TrackUp", up, "SCROLLUP", "up"), ("TrackDown", down, "SCROLLDOWN", "down")):
        screen.add(name, Type=kind, AttachId=str(TRACK_GROUP))
        screen.button(name, where, arrow_states(board, where, way))
    bar = (bar_x0, up[3] + 8, bar_x1, down[1] - 8)
    screen.add("TrackSlider", Type="VSCROLLBAR", AttachId=str(TRACK_GROUP),
               **scroll_bar(screen, board, "TrackSlider", bar))


def text_row(listing, words, chosen):
    row = Image.new("RGBA", (int(listing[2] - listing[0]), 46), (SELECTED if chosen else PANEL) + (255,))
    sub(row, words, 24, 23, 34, ORANGE if chosen else SOFT, anchor="lm", bold=True)
    return row


def track_detail(screen, board):
    area = (940, 614, 1860, PLACE[3])
    kit.panel(board, area, 230)
    picture = (948, 622, 1852, 1062)
    box(board, picture, (10, 10, 12, 255), 8)
    screen.set("PictureBox", Xywh=local(picture), TextLMargin="0", TextTMargin="0",
               AnimNorm=screen.picture("MPictureBox", under(board, picture)))
    screen.shown.append((under(board, picture), picture[:2]))

    strip = (948, 1074, 1852, 1142)
    chip(board, strip)
    for name, where, way in (("TrackLeft", (956, 1082, 1008, 1134), "left"),
                             ("TrackRight", (1792, 1082, 1844, 1134), "right")):
        screen.button(name, where, arrow_states(board, where, way))
    combo(screen, "DDLTextBox", board, (1016, 1078, 1784, 1138), arrow=False, list_h=360, size=34, align="CENTER")

    row = (948, 1160, 1852, 1240)
    # Baja's waypoint gates: random or not, how many, and the seed.
    screen.set("WaypointBox", Xywh=local(row), AnimNorm=screen.picture("MWaypointBox", under(board, row)),
               FXAnimIn='"MWaypointBox"', FXAnimOut='"MWaypointBox"')
    toggle_w = 344
    toggle(screen, board, "ChkRandomGates", (row[0], row[1], row[0] + toggle_w, row[3]), "RANDOM GATES", row[:2])
    screen.hide("rbwLabel1")
    chip_w = (row[2] - row[0] - toggle_w - 32) / 2
    for k, (label, names, part) in enumerate((
            ("GATES", ("RandomGatesLeftArrow", "LstNumGates", "RandomGatesRightArrow"), "GatesChip"),
            ("SEED", ("SeedLeft", "EditSeed", "SeedRight"), "SeedChip"))):
        x0 = row[0] + toggle_w + 16 + k * (chip_w + 16)
        area = (x0, row[1], x0 + chip_w, row[3])
        chip_board = board.copy()
        chip(chip_board, area, label)
        arrows = stepper(screen, chip_board, area, names, row[:2], side=40, number=56)
        assert x0 + 22 + kit.sub_width(label, LABEL, bold=True) + 10 < arrows, label
        # The well and its name, under the arrows and the number (their sections come after it)
        # and shown with them (dlgprocs.c).
        screen.add(part, before=names[1], Type="STATIC", Anchor="WaypointBox", RelAnchor="1",
                   Xywh=xywh(area, row[:2]), AnimNorm=screen.picture("M%s" % part, under(chip_board, area)))
        screen.shown.insert(len(screen.shown) - 3, (under(chip_board, area), (int(area[0]), int(area[1]))))

    # A ghost race's ghost, in the same place (only one of the two shows, and the previews show
    # the gates).
    previewed = len(screen.shown)
    ghost_board = board.copy()
    chip(ghost_board, row, "GHOST")
    screen.set("GhostBox", Xywh=local(row), AnimNorm=screen.picture("MGhostBox", under(ghost_board, row)))
    left = (row[0] + 140, row[1] + 14, row[0] + 192, row[3] - 14)
    right = (row[2] - 64, row[1] + 14, row[2] - 12, row[3] - 14)
    screen.button("GhostLeft", left, arrow_states(ghost_board, left, "left"), row[:2])
    screen.button("GhostRight", right, arrow_states(ghost_board, right, "right"), row[:2])
    combo(screen, "DDLGhostRaces", ghost_board, (left[2] + 8, row[1] + 12, right[0] - 8, row[3] - 12), arrow=False,
          list_h=220, size=28, align="CENTER", origin=row[:2])
    del screen.shown[previewed:]

    for corner in ("CornerLR", "CornerUL", "CornerUR", "CornerLL"):
        screen.hide(corner)


def settings_column(screen, board):
    area = (1900, 614, RIGHT, PLACE[3])
    kit.panel(board, area, 230)
    x0, x1 = area[0] + 16, area[2] - 16
    rows = [(x0, 630 + k * 88, x1, 630 + k * 88 + 76) for k in range(7)]

    mode = rows[0]
    chip(board, mode, "MODE")
    side = 52
    inset = (mode[3] - mode[1] - side) / 2
    combo(screen, "RaceModeDropDown", board, (mode[0] + 150, mode[1] + inset, mode[2] - 12 - side - 1,
                                              mode[3] - inset), list_h=176)
    for hidden in ("EventTypeDropDown", "EventTypeText", "RaceModeText", "OpponentsLabel", "rbwLabel2"):
        screen.hide(hidden)

    for row, box_name, label, names, words in (
            (rows[1], "OpponentsControlBox", None, ("OpponentsLeftarrow", "OpponentsListBox", "OpponentsrightArrow"),
             "OPPONENTS"),
            (rows[2], "LapsControlBox", "LapsLabel", ("LapsLeftArrow", "LapsListBox", "LapsrightArrow"), None),
            (rows[3], "racesControlBox", "RacesLabel", ("RacesLeftArrow", "RacesListBox", "RacesRightArrow"), None)):
        chip_board = board.copy()
        chip(chip_board, row, words)
        screen.set(box_name, Xywh=local(row), AnimNorm=screen.picture("M%s" % box_name, under(chip_board, row)),
                   FXAnimIn='"M%s"' % box_name, FXAnimOut='"M%s"' % box_name)
        screen.shown.append((under(chip_board, row), (int(row[0]), int(row[1]))))
        stepper(screen, chip_board, row, names, row[:2])
        if label:
            # The game's own words: "Laps" or "Length", "Races" or the race of the series.
            keys = {"Xywh": xywh((row[0] + 22, row[1], row[2] - 12 - 52 - 84 - 52 - 8, row[3]), row[:2]),
                    "TextLMargin": "0", "TextTMargin": "0"}
            keys.update(font(LABEL, GREY))
            screen.set(label, **keys)

    skill = rows[4]
    chip(board, skill, "SKILL")
    span = (skill[0] + 120, skill[2] - 12)
    gap = 6
    w = (span[1] - span[0] - 2 * gap) / 3
    for k, (name, words) in enumerate((("RadLODEasy", "EASY"), ("RadLODMedium", "NORMAL"), ("RadLODHard", "HARD"))):
        where = (span[0] + k * (w + gap), skill[1] + 12, span[0] + k * (w + gap) + w, skill[3] - 12)
        radio(screen, name, segment_states(board, where, words), where, chosen=k == 1)

    toggle(screen, board, "ChkTreeCollision", rows[5], "TREE COLLISION")
    toggle(screen, board, "ChkRecordRace", rows[6], "RECORD RACE")


def build_event(data, ui):
    screen = Screen(data, "SPEVENT", PLACE[:2])
    board = ground()
    event_cards(screen, board, ui)
    track_panel(screen, board)
    track_detail(screen, board)
    settings_column(screen, board)
    page_info(screen, board)
    return screen, board, None



# ---- The race information page ----

def build_results(data):
    screen = Screen(data, "SPRINFO", PLACE[:2])
    board = ground()
    area = PLACE
    kit.panel(board, area, 230)
    title = (area[0] + 40, area[1] + 24, area[2] - 40, area[1] + 96)
    keys = {"Xywh": local(title), "TextLMargin": "0", "TextTMargin": "0"}
    keys.update(font(40, ORANGE))
    screen.set("TitleText", **keys)

    frame = (area[0] + 24, area[1] + 112, area[2] - 24, area[3] - 112)
    box(board, frame, (12, 12, 14, 235), 8, EDGE + (255,))
    # The columns as wide as the original has them.
    table(screen, board, frame, [("ButStats%d" % k, "LstStats%d" % k, share)
                                 for k, share in zip(range(1, 5), (141, 56, 136, 112))], frame[:2])
    screen.set("rbwFrame1", Xywh=local(frame), AnimNorm=screen.picture("MFrame", under(board, frame)))
    screen.shown.append((under(board, frame), frame[:2]))

    replay = (area[2] - 40 - 440, area[3] - 92, area[2] - 40, area[3] - 20)
    screen.button("ButViewReplay", replay, button_states(board, replay, "VIEW REPLAY", size=30),
                  Font_UseDefault="0")
    page_info(screen, board)
    return screen, board, None


def main(ui, out, previews):
    ui, out, previews = Path(ui), Path(out), Path(previews)
    kit.fonts_from(ui / "modern" / "fonts")
    read = lambda stem: (ui / (stem + ".DAT")).read_bytes()             # noqa: E731
    base = build_base(read("SPBASE"), "SPBASE", "SINGLE PLAYER", TITLES, TABS,
                      [("Back", "BACK"), ("Help", "HELP"), ("Options", "OPTIONS"), ("Joystick", "GAMEPAD")],
                      [("Start", "RIDE", 540, True)])
    pages = [build_event(read("SPEVENT"), ui), build_bike(read("SPBIKER"), "SPBIKER"),
             build_results(read("SPRINFO"))]
    out.mkdir(parents=True, exist_ok=True)
    previews.mkdir(parents=True, exist_ok=True)
    for screen, _, _ in [base] + pages:
        data = screen.pack()
        (out / (screen.stem + ".DAT")).write_bytes(data)
        print("%-8s %3d pictures, %d bytes" % (screen.stem, len(screen.pictures), len(data)))
    photo = sorted(previews.parent.glob("menu photo *.jpg"))[0]
    for page in pages:
        preview(photo, [base, page]).save(previews / ("reborn %s.png" % page[0].stem.lower()))


if __name__ == "__main__":
    main(*sys.argv[1:4])
