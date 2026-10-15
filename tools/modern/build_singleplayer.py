r"""MCM2:Reborn's single player screen: build_singleplayer.py <game ui folder> <out folder> <preview dir>

SPBase is the screen around the three pages: the title of the page shown (dlgprocs.c shows
the one for the page), the steps as its tabs, and the buttons along the foot. Each page is
drawn in the room under the header (its Place), on the same 2560x1440:

  the event page: event cards in place of the event list (dlgprocs.c keeps the two together),
  the tracks as a list (TrackList, in the game's track list's group, so the two keep the same
  track), the track's picture, and the race's settings down the right;
  the bike page: the 3D stage (its View, the colour key here and in SPBase) and the rider and
  machine beside it;
  the race information page: the last race's results.

Writes SPBASE.DAT, SPEVENT.DAT, SPBIKER.DAT and SPRINFO.DAT for the game's ui\modern\.
"""
import io
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

import reborn_kit as kit
from build_mainmenu import rs2_files
from modern_ui import shown
from reborn_kit import (BOTTOM, EDGE, GREY, LEFT, ORANGE, PANEL, PAPER, RAISED, RIGHT, SOFT, TOP, H, W,
                        box, heading, heading_ink, outline, shape, slanted, sub)
from reborn_layout import (LABEL, NOTHING, Screen, arrow_states, button_states, clear, colorref, font, label_at,
                           on_board, segment_states, toggle_states, under, xywh)

ART = 3
PLACE = (LEFT, 274, RIGHT, 1264)            # the pages' room, under the header and over the foot
FOOT = (BOTTOM - 72, BOTTOM)
SHADE = (8, 8, 10, 170)
SELECTED = (42, 33, 22)

TABS = ["EventTab", "BikeRiderTab", "RaceInfoTab"]
TITLES = [("TitleEvent", "CHOOSE YOUR EVENT"), ("TitleBikeRider", "BIKE & RIDER"), ("TitleRaceInfo", "RACE INFO")]

# The event cards: control, the event list's kind, title, what it is, the loading screen with its art.
CARDS = [("CardSupercross", 3, "SUPERCROSS", "STADIUM", "LOADSX"),
         ("CardNationals", 2, "NATIONALS", "OUTDOOR MX", "LOADNAT"),
         ("CardBaja", 1, "BAJA", "POINT TO POINT", "LOADBAJA"),
         ("CardEnduro", 5, "ENDURO", "CHECKPOINTS", "LOADEND"),
         ("CardQuarry", 0, "STUNT QUARRY", "SCORE ATTACK", "LOADQUAR")]
CARD_GROUP = 150
TRACK_GROUP = 151

# The bike page's stage, where the 3D shows.
STAGE = (LEFT, PLACE[1], 1640, PLACE[3])


def ground():
    """What every part of the screen is on: the photos behind a dark glass."""
    return Image.new("RGBA", (W, H), SHADE)


def page_info(screen, board, window=None):
    """A page's own keys: its ground, size and Place."""
    back = under(board, PLACE)
    screen.set("Set_Information", BackgroundImage=screen.picture("MBack", back,
                                                                 window.crop(PLACE) if window else None),
               ScreenWidth=str(PLACE[2] - PLACE[0]), ScreenHeight=str(PLACE[3] - PLACE[1]), ArtScale=str(ART),
               Place=xywh(PLACE), Xywh=xywh(PLACE), FontName='"Helvetica"')


def local(area):
    """`area` on the page, from the screen."""
    return xywh(area, PLACE[:2])


def chip(img, area, words=None):
    """A setting's well, with its name."""
    box(img, area, RAISED + (255,), 8, EDGE + (255,))
    if words:
        label_at(img, area, words)


def keyed_outside(picture, points, at):
    """`picture` with what is outside the polygon `points` (on the screen) the colour key, so it
    leaves what is under it alone: for shapes whose boxes overlap."""
    mask = Image.new("L", picture.size)
    ImageDraw.Draw(mask).polygon([(x - at[0], y - at[1]) for x, y in points], fill=255)
    out = Image.new("RGBA", picture.size, (255, 0, 255, 255))
    out.paste(picture, (0, 0), mask)
    return out


def combo(screen, name, board, text_area, arrow=True, list_h=170, size=28, align="LEFT", origin=None,
          arrow_w=None):
    """A drop-down list: its text in `text_area`, the arrow just after it when `arrow` (`arrow_w`
    wide, square by default), the list below as wide as the text."""
    x0, y0, x1, y1 = text_area
    origin = origin or PLACE[:2]
    h = arrow_w or y1 - y0
    keys = {"Xywh": xywh(text_area, origin), "ListHeight": str(list_h), "TextLMargin": "18"}
    keys.update(font(size, PAPER, align))
    keys["SelectColor"] = colorref(ORANGE)
    keys["AnimTextBack"] = screen.picture("M%sText" % name, under(board, text_area))
    listing = clear((int(x1 - x0), list_h))
    box(listing, (0, 0, listing.width - 1, list_h - 1), (14, 15, 18, 255), 8, ORANGE + (255,))
    keys["AnimListBack"] = screen.picture("M%sList" % name, listing)
    if arrow:
        area = (x1 + 1, y0, x1 + 1 + h, y1)
        states = arrow_states(board, area, "down", well=arrow_w is None)
        for key, state in (("AnimNorm", "Up"), ("AnimFocus", "Lit"), ("AnimPush", "Down"), ("AnimDisable", "Grey")):
            screen.picture("M%s%s" % (name, state), states[state])
            keys[key] = '"M%s%s"' % (name, state)
        screen.shown.append((states["Up"], (int(area[0]), int(area[1]))))
        track = clear((int(h), list_h - 4))
        ImageDraw.Draw(track).line((h // 2, 10, h // 2, list_h - 14), fill=EDGE + (255,), width=2)
        keys["AnimScrollBack"] = screen.picture("M%sTrack" % name, track)
        thumb = clear((int(h), 40))
        ImageDraw.Draw(thumb).rounded_rectangle((h // 2 - 5, 4, h // 2 + 5, 36), 5, fill=ORANGE + (255,))
        keys["AnimThumb"] = screen.picture("M%sThumb" % name, thumb)
    else:
        for key in ("AnimNorm", "AnimFocus", "AnimPush", "AnimDisable"):
            keys[key] = '""'
        keys["AnimScrollBack"] = keys["AnimThumb"] = '"%s"' % NOTHING
    screen.set(name, **keys)
    screen.shown.append((under(board, text_area), (int(x0), int(y0))))


def stepper(screen, board, area, names, origin, label=None, side=52, number=84):
    """A number stepped by arrows at the right of a well: `names` the left arrow, the number's
    list (or edit box) and the right arrow; the arrows `side` square, the number `number` wide.
    Returns where the arrows begin."""
    left, value, right = names
    x0, y0, x1, y1 = area
    inset = (y1 - y0 - side) / 2
    right_area = (x1 - 12 - side, y0 + inset, x1 - 12, y1 - inset)
    value_area = (right_area[0] - number, y0 + inset, right_area[0], y1 - inset)
    left_area = (value_area[0] - side, y0 + inset, value_area[0], y1 - inset)
    for name, where, way in ((left, left_area, "left"), (right, right_area, "right")):
        screen.button(name, where, arrow_states(board, where, way), origin)
    keys = {"Xywh": xywh(value_area, origin), "TextLMargin": "0", "TextTMargin": "0",
            "AnimNorm": screen.picture("M%sBack" % value, under(board, value_area))}
    keys.update(font(30, PAPER, "CENTER"))
    if screen.has(value, "SelectColor"):
        keys["SelectColor"] = colorref(PAPER)
    screen.set(value, ("Text", "LocalTextId") if label is None else (), **keys)
    screen.shown.append((under(board, value_area), (int(value_area[0]), int(value_area[1]))))
    return left_area[0]


def toggle(screen, board, name, area, words, origin=None):
    keys = screen.states(name, toggle_states(board, area, words),
                         {"State0": "State0", "State1": "State1", "StateDisable0": "StateDisable0",
                          "StateDisable1": "StateDisable1"}, at=area[:2])
    screen.set(name, ("Text", "LocalTextId"), Xywh=xywh(area, origin or PLACE[:2]), TextDrop="0", **keys)


def radio(screen, name, pictures, area, origin=None, chosen=False, **more):
    """A radio button; the previews show it `chosen` or not."""
    keys = screen.states(name, pictures, {"AnimOn": "On", "AnimOff": "Off", "AnimOnDisable": "OnGrey",
                                          "AnimOffDisable": "OffGrey"}, at=area[:2], show="On" if chosen else "Off")
    if name in dict(screen.sections):
        screen.set(name, ("Text", "LocalTextId"), Xywh=xywh(area, origin or PLACE[:2]), **keys, **more)
    else:
        screen.add(name, Type="RADIOBUTTON", Xywh=xywh(area, origin or PLACE[:2]), **keys, **more)


# ---- SPBase: the screen around the pages ----

def build_base(data):
    screen = Screen(data, "SPBASE")
    board = ground()
    kit.tag(board, "SINGLE PLAYER", LEFT, TOP, 26)
    kit.build_mark(board)

    # The bike page's stage shows the 3D through here too.
    window = Image.new("L", (W, H))
    ImageDraw.Draw(window).rectangle((STAGE[0], STAGE[1], STAGE[2] - 1, STAGE[3] - 1), fill=255)
    board.paste((0, 0, 0, 0), mask=window)

    tabs = kit.step_areas()
    title_area = (LEFT - 8, TOP + 50, tabs[0][0] - 40, TOP + 186)
    for (name, words), number in zip(TITLES, range(3)):
        canvas = on_board(board, title_area)
        size = kit.fit(words, 92, title_area[2] - title_area[0] - 24)
        heading(canvas, words, LEFT, TOP + 140, size, PAPER)
        shape(canvas, [(LEFT + 2, TOP + 162), (LEFT + 600, TOP + 162), (LEFT + 594, TOP + 170), (LEFT, TOP + 170)],
              ORANGE)
        picture = screen.picture("M%s" % name, under(canvas, title_area))
        screen.add(name, Type="STATIC", Xywh=xywh(title_area), AnimNorm=picture)
        if not number:
            screen.shown.append((under(canvas, title_area), title_area[:2]))

    for i, (name, area) in enumerate(zip(TABS, tabs)):
        on, off = on_board(board, area), on_board(board, area)
        kit.steps(on, i)
        kit.steps(off, -1)
        pictures = {"On": under(on, area), "Off": under(off, area)}
        pictures["OnGrey"] = pictures["Off"]
        pictures["OffGrey"] = pictures["Off"]
        radio(screen, name, pictures, area, (0, 0), chosen=i == 0, FitToAnimOn="0")

    top, bottom = FOOT
    x = LEFT
    for name, words in (("Back", "BACK"), ("Help", "HELP"), ("Options", "OPTIONS"), ("Joystick", "CONTROLS")):
        w = heading_ink(words, 28) + 96
        area = (x, top, x + w, bottom)
        screen.button(name, area, button_states(board, area, words, size=28))
        x += w + 16
    area = (RIGHT - 540, top, RIGHT, bottom)
    screen.button("Start", area, button_states(board, area, "RIDE", primary=True, size=44))

    screen.set("Set_Information", BackgroundImage=screen.picture("MBack", board, window), ScreenWidth=str(W),
               ScreenHeight=str(H), ArtScale=str(ART), Photos="1", Xywh=xywh((0, 0, W, H)), FontName='"Helvetica"')
    return screen, board, window


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
    listing = (area[0] + 12, area[1] + 84, bar_x0 - 8, area[3] - 12)
    keys = {"Type": "LISTBOX", "AttachId": str(TRACK_GROUP), "Xywh": local(listing), "MaxItems": "100",
            "Selectable": "1", "AllowWScroll": "0", "TextLMargin": "24", "TextTMargin": "0",
            "SelectColor": colorref(ORANGE), "SelectBoxColor": colorref(SELECTED), "ItemBoxColor": colorref(PANEL)}
    keys.update(font(34, SOFT))
    screen.add("TrackList", **keys)
    # Rows like the game's, for the preview.
    for k, words in enumerate(["01   Bela's Backwards", "02   Third Degree Berms", "03   El Centro"]):
        screen.shown.append((text_row(listing, words, k == 1), (listing[0], int(listing[1] + k * 46))))

    up = (bar_x0, listing[1], bar_x1, listing[1] + 44)
    down = (bar_x0, listing[3] - 44, bar_x1, listing[3])
    for name, where, kind, way in (("TrackUp", up, "SCROLLUP", "up"), ("TrackDown", down, "SCROLLDOWN", "down")):
        screen.add(name, Type=kind, AttachId=str(TRACK_GROUP))
        screen.button(name, where, arrow_states(board, where, way))
    bar = (bar_x0, up[3] + 8, bar_x1, down[1] - 8)
    groove = under(board, bar)
    ImageDraw.Draw(groove).rounded_rectangle((groove.width // 2 - 3, 0, groove.width // 2 + 3, groove.height - 1), 3,
                                             fill=RAISED + (255,))
    # The thumb on the groove's middle, which is the same all the way down: see-through, it
    # would show the photos through the panel.
    middle = groove.height // 2 - 36
    thumb = groove.crop((0, middle, groove.width, middle + 72))
    ImageDraw.Draw(thumb).rounded_rectangle((thumb.width // 2 - 7, 0, thumb.width // 2 + 7, 71), 7,
                                            fill=ORANGE + (255,))
    groove_name = screen.picture("MTrackBar", groove)
    screen.add("TrackSlider", Type="VSCROLLBAR", AttachId=str(TRACK_GROUP), Xywh=local(bar), AnimNorm=groove_name,
               AnimFocus=groove_name, AnimPush=groove_name, AnimDisable=groove_name,
               AnimThumb=screen.picture("MTrackThumb", thumb))
    screen.shown += [(groove, bar[:2]), (thumb, bar[:2])]


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


# ---- The bike page ----

def build_bike(data):
    screen = Screen(data, "SPBIKER", PLACE[:2])
    board = ground()
    window = Image.new("L", (W, H))
    ImageDraw.Draw(window).rectangle((STAGE[0], STAGE[1], STAGE[2] - 1, STAGE[3] - 1), fill=255)
    board.paste((0, 0, 0, 0), mask=window)
    screen.set("Set_Information", View=local(STAGE))

    area = (1680, PLACE[1], RIGHT, PLACE[3])
    kit.panel(board, area, 230)
    x0, x1 = area[0] + 24, area[2] - 24
    side = 52

    def picker(top, words, names):
        heading(board, words, x0, top - 20, 28, ORANGE)
        row = (x0, top, x1, top + 84)
        chip(board, row)
        inset = (84 - side) / 2
        left = (row[0] + 12, top + inset, row[0] + 12 + side, top + inset + side)
        right = (row[2] - 12 - side, top + inset, row[2] - 12, top + inset + side)
        screen.button(names[0], left, arrow_states(board, left, "left"))
        screen.button(names[2], right, arrow_states(board, right, "right"))
        combo(screen, names[1], board, (left[2] + 8, top + 10, right[0] - 8, top + 74), arrow=False, list_h=420,
              size=34, align="CENTER")

    picker(PLACE[1] + 92, "RIDER", ("RiderLeft", "DDLRiders", "RiderRight"))
    picker(PLACE[1] + 252, "BIKE", ("BikeLeft", "DDLBikes", "BikeRight"))

    heading(board, "MACHINE", x0, PLACE[1] + 392, 28, ORANGE)
    # The engine's size shows only for a bike that has a choice (dlgprocs.c, with TxtEngineSize):
    # its well is drawn by the two, the name on TxtEngineSize and the rest under the list's text.
    engine = (x0, PLACE[1] + 412, x1, PLACE[1] + 488)
    engine_board = board.copy()
    chip(engine_board, engine, "ENGINE")
    name_area = (engine[0], engine[1], engine[0] + 180, engine[3])
    screen.set("TxtEngineSize", ("Text", "LocalTextId"), Xywh=local(name_area),
               AnimNorm=screen.picture("MTxtEngineSize", under(engine_board, name_area)))
    screen.shown.append((under(engine_board, name_area), name_area[:2]))
    combo(screen, "DDLEngineSize", engine_board, (name_area[2], engine[1], engine[2] - side - 1, engine[3]),
          list_h=230, arrow_w=side)
    inset = (76 - side) / 2

    plate = (x0, engine[3] + 12, x1, engine[3] + 88)
    chip(board, plate, "PLATE NUMBER")
    edit = (plate[2] - 12 - 140, plate[1] + inset, plate[2] - 12, plate[3] - inset)
    box(board, edit, (14, 15, 18, 255), 6, EDGE + (255,))
    keys = {"Xywh": local(edit), "AnimNorm": screen.picture("MEditPlateNumber", under(board, edit)),
            "TextLMargin": "0", "TextTMargin": "0"}
    keys.update(font(32, PAPER, "CENTER"))
    screen.set("EditPlateNumber", **keys)
    screen.hide("rbwLabel1")

    toggle(screen, board, "ChkAutoRotate", (x0, plate[3] + 12, x1, plate[3] + 88), "AUTO ROTATE")
    tune = (x0, plate[3] + 116, x1, plate[3] + 192)
    screen.button("ButWrench", tune, button_states(board, tune, "TUNE THE BIKE", size=30))
    sub(board, "MOUSE WHEEL ZOOMS THE BIKE", x0, area[3] - 30, 22, (120, 122, 128), bold=True)

    page_info(screen, board, window)
    return screen, board, window


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
    head = 64
    ImageDraw.Draw(board).line((frame[0] + 24, frame[1] + 20 + head, frame[2] - 24, frame[1] + 20 + head),
                               fill=EDGE + (255,), width=2)
    screen.set("rbwFrame1", Xywh=local(frame), AnimNorm=screen.picture("MFrame", under(board, frame)))
    screen.shown.append((under(board, frame), frame[:2]))
    # The columns where the original has them, across the frame.
    across = (frame[2] - frame[0] - 64) / 449
    columns = [(8, 137), (149, 52), (205, 129), (341, 108)]
    for k, (x, w) in enumerate(columns):
        cx0 = frame[0] + 32 + (x - 8) * across
        cx1 = cx0 + w * across
        keys = {"Xywh": xywh((cx0, frame[1] + 20, cx1, frame[1] + 20 + head), frame[:2]), "TextLMargin": "8"}
        keys.update(font(26, GREY))
        screen.set("ButStats%d" % (k + 1), **keys)
        keys = {"Xywh": xywh((cx0, frame[1] + 40 + head, cx1, frame[3] - 20), frame[:2]), "TextLMargin": "8"}
        keys.update(font(32, PAPER))
        screen.set("LstStats%d" % (k + 1), **keys)

    replay = (area[2] - 40 - 440, area[3] - 92, area[2] - 40, area[3] - 20)
    screen.button("ButViewReplay", replay, button_states(board, replay, "VIEW REPLAY", size=30),
                  Font_UseDefault="0")
    page_info(screen, board)
    return screen, board, None


# ---- Previews ----

def preview(photo, layers):
    """The screen as the game would show it: `layers` of (screen, board, window) drawn in turn,
    each board where its screen draws it and then its controls at rest; the 3D stood in for."""
    canvas = kit.cover(Image.open(photo).convert("RGB"), (W, H)).convert("RGBA")
    out = Image.new("RGBA", (W, H))
    for screen, board, window in layers:
        whole = screen.stem == "SPBASE"
        area = (0, 0, W, H) if whole else PLACE
        out.paste(shown(board.crop(area), window=window.crop(area) if window else None), area[:2])
        for picture, at in screen.shown:
            out.paste(shown(picture), at)
        stage = window or Image.new("L", (W, H))           # the page's, drawn last, is what shows
    keyed = Image.fromarray(((np.asarray(out)[..., :3] == (255, 0, 255)).all(-1) * 255).astype("uint8"))
    out.paste((0, 0, 0, 0), mask=keyed)
    canvas.alpha_composite(out)
    canvas.paste((40, 44, 52, 255), mask=stage)
    return canvas.convert("RGB")


def main(ui, out, previews):
    ui, out, previews = Path(ui), Path(out), Path(previews)
    kit.fonts_from(ui / "modern" / "fonts")
    read = lambda stem: (ui / (stem + ".DAT")).read_bytes()             # noqa: E731
    base = build_base(read("SPBASE"))
    pages = [build_event(read("SPEVENT"), ui), build_bike(read("SPBIKER")), build_results(read("SPRINFO"))]
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
