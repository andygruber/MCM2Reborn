r"""MCM2:Reborn's pro circuit: build_procircuit.py <game ui folder> <out folder> <preview dir>

  PCStart: the careers under way and finished, each a table, and a new one;
  PCNew: a new career's name, class, skill and opponents, a card over PCStart;
  PCC: the career under way, around its pages (reborn_pages.py): the next race (its track,
  its fee and purse, your money and the announcements), the bike and rider, the standings;
  PCLRace and PCFinish: a race's results and the final standings, as tables;
  PCDeal: a deal offered (a loan, the next class), its lines and Accept or Decline;
  PCTrans and PCDone: a career's end, its picture and words.

The words the game sets (titles, sums, the announcements) are its own, in the text face; the
names of things are drawn in the pictures. Writes the PC*.DAT files for the game's ui\modern\.
"""
import sys
from pathlib import Path

import reborn_kit as kit
from reborn_kit import EDGE, GREY, LEFT, ORANGE, PAPER, RIGHT, box, heading, sub
from reborn_layout import Screen, button_states, font, segment_states, under, xywh
from reborn_pages import (FOOT, PLACE, build_base, build_bike, card_ground, card_info, chip, footer, full_info,
                          full_screen, ground, info_row, local, page_info, preview, radio, scroll_bar, stepper, table,
                          text_button)

TABS = ["TabNext", "TabBikeRider", "TabStandings"]          # the pages' order (procircuitprocs.c)
TITLES = ["NEXT RACE", "BIKE & RIDER", "STANDINGS"]
WELL = (12, 12, 14, 235)


def frame_well(board, frame):
    box(board, frame, WELL, 8, EDGE + (255,))


def game_title(screen, name, area, size=48, colour=PAPER, align="LEFT"):
    """A title the game writes."""
    keys = {"Xywh": xywh(area, screen.origin), "TextLMargin": "0", "TextTMargin": "0"}
    keys.update(font(size, colour, align))
    screen.set(name, **keys)


def game_text(screen, name, area, size=30, colour=PAPER, anchor=None, origin=None):
    """Words the game writes, wrapped in `area`, from its top."""
    keys = {"Xywh": xywh(area, origin or screen.origin), "TextLMargin": "0", "TextTMargin": "0"}
    keys.update(font(size, colour))
    keys["TextAlignV"] = "TOP"
    if anchor:
        keys.update(Anchor=anchor, RelAnchor="1")
    screen.set(name, ("AnimNorm",), **keys)


# ---- PCStart ----

ACTIVE = [("ButName", "LstName", 163), ("ButRank", "LstRank", 50), ("ButClass", "LstClass", 50),
          ("ButDiff", "LstDiff", 90), ("ButPoints", "LstPoints", 70), ("ButCash", "LstCash", 80)]
FINISHED = [("ButDoneName", "LstDoneName", 139), ("ButDoneRank", "LstDoneRank", 50),
            ("ButDoneClass", "LstDoneClass", 50), ("ButDoneDiff", "LstDoneDiff", 90),
            ("ButDonePoints", "LstDonePoints", 70), ("ButDoneCash", "LstDoneCash", 80),
            ("ButDoneStatus", "LstDoneStatus", 80)]


def careers(screen, board, area, title, columns, slider, buttons):
    """A panel of careers: its title, their table with its scroll bar, its buttons at the foot."""
    kit.panel(board, area, 230)
    heading(board, title, area[0] + 36, area[1] + 56, 30, ORANGE)
    frame = (area[0] + 24, area[1] + 76, area[2] - 24, area[3] - 88)
    frame_well(board, frame)
    bar = (frame[2] - 56, frame[1] + 100, frame[2] - 12, frame[3] - 16)
    screen.set(slider, **scroll_bar(screen, board, slider, bar))
    table(screen, board, (frame[0], frame[1], frame[2] - 60, frame[3]), columns, (0, 0), size=30)
    x = area[2] - 24
    for name, words, w, primary in buttons:
        where = (x - w, area[3] - 72, x, area[3] - 16)
        screen.button(name, where, button_states(board, where, words, primary=primary, size=30))
        x -= w + 20


def build_start(data):
    screen, board = full_screen(data, "PCSTART", "PRO CIRCUIT", "YOUR CAREERS")
    sub(board, "Compete for points and cash as you build a pro riding career.",
        LEFT + kit.heading_ink("YOUR CAREERS", 92) + 48, kit.TOP + 140, 30, GREY)
    careers(screen, board, (LEFT, 274, RIGHT, 764), "UNDER WAY", ACTIVE, "rbwVertSlider2",
            [("ButContinue", "CONTINUE", 360, True), ("ButDelete", "DELETE", 240, False)])
    careers(screen, board, (LEFT, 794, RIGHT, 1264), "FINISHED", FINISHED, "rbwVertSlider1",
            [("ButDoneDelete", "DELETE", 240, False)])
    # The original's panels behind its two tables, and its words, are drawn in the pictures.
    for hidden in ("LstCurrent", "LstFinished", "rbwLabel1", "rbwLabel2", "rbwLabel3", "rbwLabel4"):
        screen.hide(hidden)
    footer(screen, board, [("Back", "BACK")], [("ButNew", "NEW CIRCUIT", 540, True)])
    full_info(screen, board)
    return screen, board, None


# ---- PCNew: a card over PCStart ----

CARD = (1600, 760)                      # the original's 432 by 172, given room for its choices


def build_new(data):
    screen = Screen(data, "PCNEW")
    w, h = CARD
    board = card_ground(w, h)
    heading(board, "NEW PRO CIRCUIT", 48, 72, 40, PAPER)
    rows = [(48, 120 + k * 92, w - 48, 120 + k * 92 + 76) for k in range(4)]

    name = rows[0]
    chip(board, name, "NAME")
    edit = (name[0] + 200, name[1] + 12, name[2] - 12, name[3] - 12)
    box(board, edit, (14, 15, 18, 255), 6, EDGE + (255,))
    keys = {"Xywh": xywh(edit), "AnimNorm": screen.picture("MEditBox", under(board, edit)), "TextLMargin": "16",
            "TextTMargin": "0"}
    keys.update(font(32, PAPER))
    screen.set("EditBox", **keys)

    for row, label, choices in ((rows[1], "CLASS", (("RadClass125", "125"), ("RadClass250", "250"),
                                                     ("RadClassOpen", "OPEN"))),
                                (rows[2], "SKILL", (("RadLODEasy", "EASY"), ("RadLODMedium", "NORMAL"),
                                                     ("RadLODHard", "HARD")))):
        chip(board, row, label)
        span = (row[0] + 200, row[2] - 12)
        gap = 6
        cw = (span[1] - span[0] - 2 * gap) / 3
        for k, (control, words) in enumerate(choices):
            where = (span[0] + k * (cw + gap), row[1] + 12, span[0] + k * (cw + gap) + cw, row[3] - 12)
            radio(screen, control, segment_states(board, where, words), where, (0, 0), chosen=k == 1)

    opponents = rows[3]
    chip(board, opponents, "OPPONENTS")
    stepper(screen, board, opponents, ("OpponentsLeftArrow", "OpponentsListBox", "OpponentsRightArrow"), (0, 0))

    for hidden in ("TitleText", "rbwLabel1", "rbwLabel2", "rbwLabel3", "rbwLabel4"):
        screen.hide(hidden)
    y = h - 48 - 72
    for k, (control, words, primary) in enumerate((("OKButton", "CREATE", True), ("CancelButton", "CANCEL", False))):
        x1 = w - 48 - k * 340
        where = (x1 - 320, y, x1, y + 72)
        screen.button(control, where, button_states(board, where, words, primary=primary, size=32))
    card_info(screen, board)
    return screen, board, None


# ---- PCC and its pages ----

def build_next(data):
    """The next race: its track and terms on the left, your money and the announcements on the
    right."""
    screen = Screen(data, "PCCNEXT", PLACE[:2])
    board = ground()
    left = (LEFT, PLACE[1], 1048, PLACE[3])
    kit.panel(board, left, 230)
    picture = (left[0] + 24 + (904 - 855) // 2, left[1] + 24, left[0] + 24 + (904 - 855) // 2 + 855, left[1] + 24 + 417)
    box(board, (left[0] + 24, left[1] + 24, left[2] - 24, picture[3]), (10, 10, 12, 255), 8)
    screen.set("NextTrackPic", Xywh=local(picture))
    for corner in ("rbwFrame3", "rbwFrame4", "rbwFrame5", "rbwFrame6"):
        screen.hide(corner)
    rows = [(left[0] + 24, picture[3] + 24 + k * 86, left[2] - 24, picture[3] + 24 + k * 86 + 76) for k in range(5)]
    for row, (label, value) in zip(rows, (("TRACK", "TxtNextTrack"), ("EVENT", "TxtNextType"),
                                          ("RACE", "TxtNextRace"), ("PURSE", "TxtNextPurse"),
                                          ("ENTRY FEE", "TxtNextFee"))):
        info_row(screen, board, row, label, value)

    money = (1088, PLACE[1], RIGHT, 760)
    kit.panel(board, money, 230)
    heading(board, "YOUR MONEY", money[0] + 36, money[1] + 56, 30, ORANGE)
    x0, x1 = money[0] + 24, money[2] - 24
    info_row(screen, board, (x0, money[1] + 84, x1, money[1] + 172), "CASH", "TxtCash", size=40)
    sub(board, "SPENT SO FAR", x0, money[1] + 214, 22, GREY, bold=True)
    for k, (label, value) in enumerate((("ENTRY FEES", "TxtFees"), ("REPAIRS", "TxtRepairs"),
                                        ("MEDICAL", "TxtMedical"))):
        top = money[1] + 234 + k * 86
        info_row(screen, board, (x0, top, x1, top + 76), label, value)

    news = (1088, 800, RIGHT, PLACE[3])
    kit.panel(board, news, 230)
    box(board, (news[0], news[1], news[0] + 6, news[3]), ORANGE + (255,), 3)
    heading(board, "ANNOUNCEMENTS", news[0] + 36, news[1] + 56, 30, ORANGE)
    game_text(screen, "TxtAnnouncement", (news[0] + 36, news[1] + 84, news[2] - 36, news[3] - 24), size=32)
    for hidden in ("rbwLabel%d" % n for n in (1, 2, 3, 4, 5, 6, 12, 13, 14, 15, 16, 18, 22)):
        screen.hide(hidden)
    page_info(screen, board)
    return screen, board, None


STANDINGS = [("ButRank", "LstRank", 50), ("ButName", "LstName", 150), ("ButPoints", "LstPoints", 70),
             ("ButBike", "LstBike", 110), ("ButCash", "LstCash", 90), ("ButWinnings", "LstWinnings", 90),
             ("ButExpenses", "LstExpenses", 90)]


def build_standings(data):
    screen = Screen(data, "PCCSTAND", PLACE[:2])
    board = ground()
    kit.panel(board, PLACE, 230)
    frame = (PLACE[0] + 24, PLACE[1] + 24, PLACE[2] - 24, PLACE[3] - 24)
    frame_well(board, frame)
    table(screen, board, frame, STANDINGS, PLACE[:2])
    page_info(screen, board)
    return screen, board, None


# ---- Results ----

RESULTS = {"PCLRACE": ("LAST RACE", [("ButRank", "LstRank", 50), ("ButName", "LstName", 150),
                                     ("ButPoints", "LstPoints", 70), ("ButBike", "LstBike", 110),
                                     ("ButWinnings", "LstWinnings", 90), ("ButRepairs", "LstRepairs", 90),
                                     ("ButMedical", "LstMedical", 90)]),
           "PCFINISH": ("FINAL STANDINGS", [("ButRank", "LstRank", 50), ("ButName", "LstName", 150),
                                            ("ButPoints", "LstPoints", 70), ("ButBike", "LstBike", 110),
                                            ("ButCash", "LstCash", 90), ("ButRepairs", "LstRepairs", 90),
                                            ("ButMedical", "LstMedical", 90)])}


def build_results(data, stem):
    title, columns = RESULTS[stem]
    screen, board = full_screen(data, stem, "PRO CIRCUIT", title)
    kit.panel(board, PLACE, 230)
    frame = (PLACE[0] + 24, PLACE[1] + 24, PLACE[2] - 24, PLACE[3] - 24)
    frame_well(board, frame)
    table(screen, board, frame, columns, frame[:2], anchor="rbwFrame1")
    screen.set("rbwFrame1", Xywh=xywh(frame), AnimNorm=screen.picture("MFrame", under(board, frame)))
    screen.shown.append((under(board, frame), frame[:2]))
    screen.hide("rbwLabel1")
    footer(screen, board, [], [("Back", "CONTINUE", 540, True)])
    full_info(screen, board)
    return screen, board, None


# ---- A deal, and a career's end ----

def build_deal(data):
    screen, board = full_screen(data, "PCDEAL", "PRO CIRCUIT", "AN OFFER")
    area = (LEFT, PLACE[1], 1600, PLACE[3])
    kit.panel(board, area, 230)
    game_title(screen, "TitleText", (area[0] + 36, area[1] + 24, area[2] - 36, area[1] + 104), colour=ORANGE)
    game_text(screen, "TxtDescription", (area[0] + 36, area[1] + 120, area[2] - 36, area[1] + 520), size=32)
    for k in range(5):
        top = area[1] + 540 + k * 86
        row = (area[0] + 24, top, area[2] - 24, top + 76)
        chip(board, row)
        # The line's name and its sum are both the game's.
        game_title(screen, "TxtItem%d" % (k + 1), (row[0] + 22, row[1], row[0] + 900, row[3]), size=26, colour=GREY)
        keys = {"Xywh": xywh((row[0] + 900, row[1], row[2] - 22, row[3])), "TextLMargin": "0", "TextTMargin": "0"}
        keys.update(font(32, PAPER, "RIGHT"))
        screen.set("Item%d" % (k + 1), **keys)
    footer(screen, board, [("Back", "BACK")], [("ButAccept", "ACCEPT", 420, True), ("ButDecline", "DECLINE", 360,
                                                                                     False)])
    full_info(screen, board)
    return screen, board, None


def build_transfer(data):
    """A career's end: its picture (the game's, three times its 335 by 246) and its words; the
    buttons' words are the game's, which change with the ending."""
    screen, board = full_screen(data, "PCTRANS", "PRO CIRCUIT", "CAREER")
    picture = (LEFT, PLACE[1], LEFT + 1005, PLACE[1] + 738)
    box(board, picture, (10, 10, 12, 255), 8)
    screen.set("Pic", Xywh=xywh(picture))
    area = (picture[2] + 40, PLACE[1], RIGHT, PLACE[3])
    kit.panel(board, area, 230)
    game_title(screen, "TitleText", (area[0] + 36, area[1] + 24, area[2] - 36, area[1] + 104), colour=ORANGE)
    game_text(screen, "TxtDescription", (area[0] + 36, area[1] + 120, area[2] - 36, area[3] - 24), size=32)
    top, bottom = FOOT
    for k, name in enumerate(("ButAccept", "ButDecline")):
        where = (RIGHT - 480 - k * 504, top, RIGHT - k * 504, bottom)
        text_button(screen, board, name, where)
    full_info(screen, board)
    return screen, board, None


def build_done(data):
    screen, board = full_screen(data, "PCDONE", "PRO CIRCUIT", "CAREER OVER")
    area = (LEFT + 300, PLACE[1] + 60, RIGHT - 300, PLACE[3] - 60)
    kit.panel(board, area, 230)
    game_title(screen, "rbwLabel1", (area[0] + 48, area[1] + 36, area[2] - 48, area[1] + 120), colour=ORANGE)
    game_text(screen, "rbwLabel2", (area[0] + 48, area[1] + 150, area[2] - 48, area[3] - 48), size=34)
    footer(screen, board, [], [("Back", "CONTINUE", 540, True)])
    full_info(screen, board)
    return screen, board, None


def main(ui, out, previews):
    ui, out, previews = Path(ui), Path(out), Path(previews)
    kit.fonts_from(ui / "modern" / "fonts")
    read = lambda stem: (ui / (stem + ".DAT")).read_bytes()             # noqa: E731
    base = build_base(read("PCC"), "PCC", "PRO CIRCUIT", TITLES, TABS,
                      [("Back", "BACK"), ("Help", "HELP"), ("Options", "OPTIONS"), ("Controls", "CONTROLS")],
                      [("Start", "START RACE", 540, True), ("Practice", "PRACTICE", 400, False)])
    pages = [build_next(read("PCCNEXT")), build_bike(read("PCCBIKER"), "PCCBIKER"),
             build_standings(read("PCCSTAND"))]
    screens = [build_start(read("PCSTART")), build_results(read("PCLRACE"), "PCLRACE"),
               build_results(read("PCFINISH"), "PCFINISH"), build_deal(read("PCDEAL")),
               build_transfer(read("PCTRANS")), build_done(read("PCDONE"))]
    card = build_new(read("PCNEW"))
    out.mkdir(parents=True, exist_ok=True)
    previews.mkdir(parents=True, exist_ok=True)
    for screen, _, _ in [base] + pages + screens + [card]:
        data = screen.pack()
        (out / (screen.stem + ".DAT")).write_bytes(data)
        print("%-8s %3d pictures, %d bytes" % (screen.stem, len(screen.pictures), len(data)))
    photo = sorted(previews.parent.glob("menu photo *.jpg"))[0]
    for page in pages:
        preview(photo, [base, page]).save(previews / ("reborn %s.png" % page[0].stem.lower()))
    for screen in screens:
        preview(photo, [screen]).save(previews / ("reborn %s.png" % screen[0].stem.lower()))
    card[1].save(previews / "reborn pcnew.png")


if __name__ == "__main__":
    main(*sys.argv[1:4])
