r"""MCM2:Reborn's multiplayer: build_multiplayer.py <game ui folder> <out folder> <preview dir>

MPBase is the screen round four pages (event, bike and rider, race information, options),
with the riders in the session and their Ready buttons, and the chat, down the right. Its pages
are narrower than single player's (MP_PLACE). HostJoin and SerPopup are cards over the menu.

Writes MPBASE, MPEVENT, MPBIKER, MPRINFO, MPOPT, HOSTJOIN and SERPOPUP for the game's ui\modern\.
"""
import sys
from pathlib import Path

import reborn_kit as kit
from build_online import text
from reborn_kit import EDGE, GREY, LEFT, ORANGE, PAPER, RIGHT, box, heading, sub
from reborn_layout import (LABEL, Screen, arrow_states, button_states, colorref, font, segment_states, under,
                           xywh)
from reborn_pages import (bike_picker, build_base, card_ground, card_info, chip, combo, engine_row, finish_bike,
                          ground, page_info, plate_row, preview, radio, scroll_bar, stepper, table,
                          toggle, bike_page)

MP_PLACE = (LEFT, 274, 1700, 1264)          # the pages' room: the riders and the chat take the right
MP_STAGE = (LEFT, 274, 1700, 760)           # the bike page's 3D
RIDERS = (1740, 274, RIGHT, 820)
CHAT = (1740, 860, RIGHT, 1264)
TABS = ["EventTab", "BikeRiderTab", "RaceInfoTab", "OptionsTab"]     # selectgamepicprocs.c's pages
TITLES = ["EVENT", "BIKE & RIDER", "RACE INFO", "OPTIONS"]
WELL = (12, 12, 14, 235)
ROW_H, PITCH = 76, 88


def mp_page(data, stem):
    return Screen(data, stem, MP_PLACE[:2]), ground()


def rows(area, top, count):
    return [(area[0] + 24, top + k * PITCH, area[2] - 24, top + k * PITCH + ROW_H) for k in range(count)]


def drop(screen, board, name, row, label, list_h=230, start=0.42):
    chip(board, row, label)
    side = 52
    inset = (row[3] - row[1] - side) / 2
    combo(screen, name, board, (row[0] + (row[2] - row[0]) * start, row[1] + inset, row[2] - 12 - side - 1,
                                row[3] - inset), list_h=list_h)


# ---- MPBase ----

LOBBY = 30                                  # a lobby's riders (code/eos/protocol.h, EOS_RIDERS_MOST)
LOBBY_ROWS, LOBBY_PITCH, LOBBY_ROW_H = 15, 31, 27


def keys_of(screen, section):
    """A control of the original's, its keys but its place and pictures, to make more like it."""
    keys = {}
    for line in dict(screen.sections)[section]:
        key, _, value = line.partition("=")
        key = key.strip()
        if key and key != "Xywh" and not key.startswith("Anim"):
            keys[key] = value.strip()
    return keys


def riders_and_chat(screen, board):
    """The riders: the player's Ready, then each other rider's (the host's Kick) and name, 30 in
    two columns; the chat under them. The buttons' words are the game's. The original has rows
    for 8: those past them are made like its last."""
    kit.panel(board, RIDERS, 230)
    heading(board, "RIDERS", RIDERS[0] + 36, RIDERS[1] + 56, 28, ORANGE)
    text(screen, "TxtReadyCount", (RIDERS[2] - 420, RIDERS[1] + 22, RIDERS[2] - 36, RIDERS[1] + 66), 24, GREY,
         "RIGHT")
    x0, x1 = RIDERS[0] + 24, RIDERS[2] - 24
    column_w = (x1 - x0 - 16) / 2
    for k in range(LOBBY):
        left = x0 + (k // LOBBY_ROWS) * (column_w + 16)
        top = RIDERS[1] + 78 + (k % LOBBY_ROWS) * LOBBY_PITCH
        button = "ButRdyUser" if not k else "ButRdyPlayer%d" % (k + 1)
        name = "UserName" if not k else "NamePlayer%d" % (k + 1)
        if k >= 8:
            screen.add(button, **keys_of(screen, "ButRdyPlayer8"))
            screen.add(name, **keys_of(screen, "NamePlayer8"))
        area = (left, top, left + 96, top + LOBBY_ROW_H)
        keys = {"TextColor": colorref(PAPER)}
        keys.update(font(18, PAPER, "CENTER"))
        screen.button(button, area, button_states(board, area, ""), keep_text=True, **keys)
        area = (left + 104, top, left + column_w, top + LOBBY_ROW_H)
        box(board, area, (30, 31, 36, 255), 5, (ORANGE if not k else EDGE) + (255,))
        keys = {"Xywh": xywh(area), "TextLMargin": "10", "TextTMargin": "0",
                "AnimNorm": screen.picture("M%s" % name, under(board, area))}
        keys.update(font(20, PAPER))
        screen.set(name, **keys)
        if k:
            text(screen, "TxtPing%d" % (k + 1), (area[2] - 76, top, area[2] - 10, top + LOBBY_ROW_H), 16, GREY,
                 "RIGHT", bold=False)

    kit.panel(board, CHAT, 230)
    heading(board, "CHAT", CHAT[0] + 36, CHAT[1] + 56, 28, ORANGE)
    listing = (CHAT[0] + 24, CHAT[1] + 76, CHAT[2] - 24, CHAT[3] - 100)
    box(board, listing, WELL, 8, EDGE + (255,))
    keys = {"Xywh": xywh((listing[0] + 8, listing[1] + 8, listing[2] - 56, listing[3] - 8)), "TextLMargin": "12",
            "TextTMargin": "0", "AnimNorm": screen.picture("MListChat", under(board, (
                listing[0] + 8, listing[1] + 8, listing[2] - 56, listing[3] - 8)))}
    keys.update(font(24, PAPER))
    screen.set("ListChat", **keys)
    bar = (listing[2] - 52, listing[1] + 8, listing[2] - 12, listing[3] - 8)
    screen.set("ScrollChat", **scroll_bar(screen, board, "ScrollChat", bar))
    edit = (CHAT[0] + 24, CHAT[3] - 84, CHAT[2] - 184, CHAT[3] - 24)
    box(board, edit, (30, 31, 36, 255), 6, EDGE + (255,))
    keys = {"Xywh": xywh(edit), "TextLMargin": "16", "TextTMargin": "0",
            "AnimNorm": screen.picture("MEditChat", under(board, edit))}
    keys.update(font(26, PAPER))
    screen.set("EditChat", **keys)
    send = (CHAT[2] - 168, CHAT[3] - 84, CHAT[2] - 24, CHAT[3] - 24)
    screen.button("ButChatSend", send, button_states(board, send, "SEND", size=26))


def build_base_mp(data):
    board = ground()
    screen, board, window = build_base(data, "MPBASE", "MULTIPLAYER", TITLES, TABS,
                                       [("Back", "BACK"), ("Help", "HELP"), ("Options", "OPTIONS"),
                                        ("Joystick", "CONTROLS")],
                                       [("Start", "START RACE", 540, True)], stage=MP_STAGE, board=board)
    riders_and_chat(screen, board)
    screen.set("Set_Information", BackgroundImage=screen.picture("MBack", board, window))
    return screen, board, window


# ---- The pages ----

def build_event(data):
    screen, board = mp_page(data, "MPEVENT")
    left = (MP_PLACE[0], MP_PLACE[1], 1048, MP_PLACE[3])
    right = (1088, MP_PLACE[1], MP_PLACE[2], MP_PLACE[3])
    kit.panel(board, left, 230)
    kit.panel(board, right, 230)
    event, mode = rows(left, left[1] + 24, 2)
    drop(screen, board, "EventTypeDropDown", event, "EVENT", list_h=300, start=0.3)
    drop(screen, board, "RaceModeDropDown", mode, "MODE", list_h=230, start=0.3)

    picture = (left[0] + 24, mode[3] + 16, left[0] + 24 + 904, mode[3] + 16 + 440)
    box(board, picture, (10, 10, 12, 255), 8)
    screen.set("PictureBox", Xywh=xywh(picture, screen.origin), TextLMargin="0", TextTMargin="0",
               AnimNorm=screen.picture("MPictureBox", under(board, picture)))
    screen.shown.append((under(board, picture), picture[:2]))
    strip = (left[0] + 24, picture[3] + 16, left[2] - 24, picture[3] + 84)
    chip(board, strip)
    for name, where, way in (("TrackLeft", (strip[0] + 8, strip[1] + 8, strip[0] + 60, strip[3] - 8), "left"),
                             ("TrackRight", (strip[2] - 60, strip[1] + 8, strip[2] - 8, strip[3] - 8), "right")):
        screen.button(name, where, arrow_states(board, where, way))
    combo(screen, "DDLTextBox", board, (strip[0] + 68, strip[1] + 4, strip[2] - 68, strip[3] - 4), arrow=False,
          list_h=360, size=34, align="CENTER")

    top = right[1] + 24
    for row, box_name, label, names, words in zip(
            rows(right, top, 3), ("OpponentsControlBox", "LapsControlBox", "RacesControlBox"),
            (None, "LapsLabel", "RacesLabel"),
            (("OpponentsLeftArrow", "OpponentsListBox", "OpponentsRightArrow"),
             ("LapsLeftArrow", "LapsListBox", "LapRightArrow"),
             ("RacesLeftArrow", "RacesListBox", "RacesRightArrow")),
            ("COMPUTER RIDERS", None, None)):
        chip_board = board.copy()
        chip(chip_board, row, words)
        screen.set(box_name, Xywh=xywh(row, screen.origin),
                   AnimNorm=screen.picture("M%s" % box_name, under(chip_board, row)),
                   FXAnimIn='"M%s"' % box_name, FXAnimOut='"M%s"' % box_name)
        screen.shown.append((under(chip_board, row), (int(row[0]), int(row[1]))))
        stepper(screen, chip_board, row, names, row[:2])
        if label:
            # The game's own words: "Laps" or "Length", "Races" or the race of the series.
            keys = {"Xywh": xywh((row[0] + 22, row[1], row[2] - 12 - 52 - 84 - 52 - 8, row[3]), row[:2]),
                    "TextLMargin": "0", "TextTMargin": "0"}
            keys.update(font(LABEL, GREY))
            screen.set(label, **keys)
    tag, stunt = rows(right, top + 3 * PITCH, 2)
    toggle(screen, board, "ChkTagBall", tag, "TAG BALL")
    toggle(screen, board, "ChkStuntMode", stunt, "STUNT MODE")
    for name in ("EventTypeText", "RaceModeText", "OpponentsLabel", "CornerLL", "CornerLR", "CornerUL", "CornerUR"):
        screen.hide(name)
    page_info(screen, board, place=MP_PLACE)
    return screen, board, None


def build_bike(data):
    """The stage across the top; the rider, bike and machine in two columns under it."""
    screen, board, window = bike_page(data, "MPBIKER", MP_STAGE, MP_PLACE)
    area = (MP_PLACE[0], MP_STAGE[3] + 40, MP_PLACE[2], MP_PLACE[3])
    kit.panel(board, area, 230)
    half = (area[2] - area[0] - 48 - 40) / 2
    left = (area[0] + 24, area[1] + 24, area[0] + 24 + half, area[3])
    right = (left[2] + 40, area[1] + 24, area[2] - 24, area[3])
    rider, bike, engine = [(left[0], left[1] + k * PITCH, left[2], left[1] + k * PITCH + ROW_H) for k in range(3)]
    bike_picker(screen, board, rider, ("RiderLeft", "DDLRiders", "RiderRight"), list_h=360, label="RIDER")
    bike_picker(screen, board, bike, ("BikeLeft", "DDLBikes", "BikeRight"), list_h=300, label="BIKE")
    engine_row(screen, board, engine)
    largest, plate, tune = [(right[0], right[1] + k * PITCH, right[2], right[1] + k * PITCH + ROW_H)
                            for k in range(3)]
    drop(screen, board, "LargestOpponentDropDown", largest, "LARGEST ENGINE", list_h=230, start=0.45)
    plate_row(screen, board, plate)
    screen.button("ButWrench", tune, button_states(board, tune, "TUNE THE BIKE", size=30))
    sub(board, "MOUSE WHEEL ZOOMS THE BIKE", right[0], area[3] - 26, 22, (120, 122, 128), bold=True)
    screen.hide("EventTypeText")
    return finish_bike(screen, board, window, MP_PLACE)


def build_results(data):
    screen, board = mp_page(data, "MPRINFO")
    kit.panel(board, MP_PLACE, 230)
    heading(board, "LAST RACE", MP_PLACE[0] + 40, MP_PLACE[1] + 80, 40, ORANGE)
    frame = (MP_PLACE[0] + 24, MP_PLACE[1] + 112, MP_PLACE[2] - 24, MP_PLACE[3] - 24)
    box(board, frame, WELL, 8, EDGE + (255,))
    table(screen, board, frame, [("ButStats%d" % k, "LstStats%d" % k, share)
                                 for k, share in zip(range(1, 5), (110, 40, 145, 80))], frame[:2],
          anchor="rbwFrame1")
    screen.set("rbwFrame1", Xywh=xywh(frame, screen.origin), AnimNorm=screen.picture("MFrame", under(board, frame)))
    screen.shown.append((under(board, frame), frame[:2]))
    screen.hide("TitleText")
    page_info(screen, board, place=MP_PLACE)
    return screen, board, None


def build_options(data):
    screen, board = mp_page(data, "MPOPT")
    w = (MP_PLACE[2] - MP_PLACE[0] - 40) / 2
    host = (MP_PLACE[0], MP_PLACE[1], MP_PLACE[0] + w, MP_PLACE[3])
    here = (host[2] + 40, MP_PLACE[1], MP_PLACE[2], MP_PLACE[3])
    for area, title in ((host, "THE HOST'S"), (here, "THIS COMPUTER'S")):
        kit.panel(board, area, 230)
        heading(board, title, area[0] + 36, area[1] + 56, 28, ORANGE)
    top = host[1] + 80
    trees, finishes, riders, skill = rows(host, top, 4)
    toggle(screen, board, "ChkTreeCollision", trees, "TREE COLLISION")
    toggle(screen, board, "FastFinishesCheckBox", finishes, "FAST FINISHES")
    toggle(screen, board, "ChkRiderCollision", riders, "RIDER COLLISION")
    chip(board, skill, "SKILL")
    span = (skill[0] + 120, skill[2] - 12)
    sw = (span[1] - span[0] - 12) / 3
    for k, (name, words) in enumerate((("RadLODEasy", "EASY"), ("RadLODMedium", "NORMAL"), ("RadLODHard", "HARD"))):
        where = (span[0] + k * (sw + 6), skill[1] + 12, span[0] + k * (sw + 6) + sw, skill[3] - 12)
        radio(screen, name, segment_states(board, where, words), where, chosen=k == 1)
    record, bikes = rows(here, here[1] + 80, 2)
    toggle(screen, board, "ChkRecordRace", record, "RECORD RACE")
    chip(board, bikes, "AI BIKES TO CONTROL")
    edit = (bikes[2] - 12 - 120, bikes[1] + 12, bikes[2] - 12, bikes[3] - 12)
    box(board, edit, (14, 15, 18, 255), 6, EDGE + (255,))
    keys = {"Xywh": xywh(edit, screen.origin), "TextLMargin": "0", "TextTMargin": "0",
            "AnimNorm": screen.picture("MEditMaxAIBikes", under(board, edit))}
    keys.update(font(30, PAPER, "CENTER"))
    screen.set("EditMaxAIBikes", **keys)
    for name in ("rbwLabel1", "rbwLabel2", "rbwLabel3", "rbwLabel4"):
        screen.hide(name)
    page_info(screen, board, place=MP_PLACE)
    return screen, board, None


# ---- The cards ----

def build_host_join(data):
    screen = Screen(data, "HOSTJOIN")
    w, h = 1300, 820
    board = card_ground(w, h)
    heading(board, "HOST OR JOIN", 56, 96, 40, PAPER)
    x = 56
    for k, (name, words, primary) in enumerate((("ButHost", "HOST", True), ("ButJoin", "JOIN", False))):
        where = (x, 140 + k * 92, x + 340, 140 + k * 92 + 72)
        screen.button(name, where, button_states(board, where, words, primary=primary, size=32))
    where = (x, h - 56 - 72, x + 340, h - 56)
    screen.button("ButCancel", where, button_states(board, where, "CANCEL", size=32))
    listing = (x + 380, 140, w - 56, h - 56)
    box(board, listing, WELL, 8, EDGE + (255,))
    sub(board, "GAMES FOUND", listing[0] + 24, listing[1] - 16, LABEL, GREY, bold=True)
    keys = {"Xywh": xywh((listing[0] + 8, listing[1] + 8, listing[2] - 56, listing[3] - 8)), "TextLMargin": "16",
            "TextTMargin": "0", "SelectColor": colorref(ORANGE)}
    keys.update(font(30, PAPER))
    screen.set("LstSessions", **keys)
    up = (listing[2] - 52, listing[1] + 8, listing[2] - 12, listing[1] + 52)
    down = (listing[2] - 52, listing[3] - 52, listing[2] - 12, listing[3] - 8)
    screen.button("SessionUp", up, arrow_states(board, up, "up"))
    screen.button("SessionDown", down, arrow_states(board, down, "down"))
    bar = (listing[2] - 52, up[3] + 8, listing[2] - 12, down[1] - 8)
    screen.set("ScrollSessions", **scroll_bar(screen, board, "ScrollSessions", bar))
    screen.hide("TitleText")
    card_info(screen, board)
    return screen, board


def build_serial(data):
    screen = Screen(data, "SERPOPUP")
    w, h = 1000, 840
    board = card_ground(w, h)
    heading(board, "SERIAL PORT", 56, 96, 40, PAPER)
    for k, (name, label) in enumerate((("ButPort", "PORT"), ("ButBaud", "BAUD RATE"), ("ButStop", "STOP BITS"),
                                       ("ButParity", "PARITY"), ("ButFlow", "FLOW"))):
        row = (56, 136 + k * PITCH, w - 56, 136 + k * PITCH + ROW_H)
        chip(board, row, label)
        # Each steps through its choices when clicked; the words are the game's.
        keys = {"Xywh": xywh((row[0] + 300, row[1], row[2] - 22, row[3])), "TextLMargin": "0", "TextTMargin": "0"}
        keys.update(font(30, PAPER, "RIGHT"))
        screen.set(name, **keys)
    foot = h - 56 - 72
    for k, (name, words, primary) in enumerate((("ButOk", "OK", True), ("ButCancel", "CANCEL", False))):
        x1 = w - 56 - k * 320
        where = (x1 - 300, foot, x1, foot + 72)
        screen.button(name, where, button_states(board, where, words, primary=primary, size=32))
    for name in ("TitleText", "rbwLabel1", "rbwLabel2", "rbwLabel3", "rbwLabel4", "rbwLabel5"):
        screen.hide(name)
    card_info(screen, board)
    return screen, board


def main(ui, out, previews):
    ui, out, previews = Path(ui), Path(out), Path(previews)
    kit.fonts_from(ui / "modern" / "fonts")
    read = lambda stem: (ui / (stem + ".DAT")).read_bytes()             # noqa: E731
    base = build_base_mp(read("MPBASE"))
    pages = [build_event(read("MPEVENT")), build_bike(read("MPBIKER")), build_results(read("MPRINFO")),
             build_options(read("MPOPT"))]
    cards = [build_host_join(read("HOSTJOIN")), build_serial(read("SERPOPUP"))]
    out.mkdir(parents=True, exist_ok=True)
    previews.mkdir(parents=True, exist_ok=True)
    for screen in [base[0]] + [p[0] for p in pages] + [c[0] for c in cards]:
        data = screen.pack()
        (out / (screen.stem + ".DAT")).write_bytes(data)
        print("%-8s %3d pictures, %d bytes" % (screen.stem, len(screen.pictures), len(data)))
    photo = sorted(previews.parent.glob("menu photo *.jpg"))[0]
    for page in pages:
        preview(photo, [base, page], MP_PLACE).save(previews / ("reborn %s.png" % page[0].stem.lower()))
    for screen, board in cards:
        board.save(previews / ("reborn %s.png" % screen.stem.lower()))


if __name__ == "__main__":
    main(*sys.argv[1:4])
