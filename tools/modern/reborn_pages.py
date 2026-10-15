r"""The parts of MCM2:Reborn's screens with pages (single player's, the pro circuit's): the
screen around the pages, its controls' looks, the bike page, a table, and the previews.

The screen around the pages is a full 2560x1440: its kicker, the title of the page shown (each
a control, Title0 to Title2, that dlgprocs.c shows for its page), the steps as its tabs, and the
buttons along the foot. Each page is drawn in the room under the header (PLACE, the page's
Place) on the same 2560x1440, and cut out of it.
"""
import numpy as np
from PIL import Image, ImageDraw

import reborn_kit as kit
from modern_ui import shown
from reborn_kit import (BOTTOM, EDGE, GREY, LEFT, ORANGE, PAPER, RAISED, RIGHT, TOP, H, W, box, heading,
                        heading_ink, shape, sub)
from reborn_layout import (NOTHING, Screen, arrow_states, button_states, clear, colorref, font, label_at,
                           on_board, toggle_states, under, xywh)

ART = 3
PLACE = (LEFT, 274, RIGHT, 1264)            # the pages' room, under the header and over the foot
FOOT = (BOTTOM - 72, BOTTOM)
SHADE = (8, 8, 10, 170)
STAGE = (LEFT, PLACE[1], 1640, PLACE[3])    # the bike page's 3D


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
    origin = origin or screen.origin
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
    screen.set(name, ("Text", "LocalTextId"), Xywh=xywh(area, origin or screen.origin), TextDrop="0", **keys)


def radio(screen, name, pictures, area, origin=None, chosen=False, **more):
    """A radio button; the previews show it `chosen` or not."""
    keys = screen.states(name, pictures, {"AnimOn": "On", "AnimOff": "Off", "AnimOnDisable": "OnGrey",
                                          "AnimOffDisable": "OffGrey"}, at=area[:2], show="On" if chosen else "Off")
    if name in dict(screen.sections):
        screen.set(name, ("Text", "LocalTextId"), Xywh=xywh(area, origin or screen.origin), **keys, **more)
    else:
        screen.add(name, Type="RADIOBUTTON", Xywh=xywh(area, origin or screen.origin), **keys, **more)



# ---- The screen around the pages ----

def stage_window():
    """The mask of the bike page's stage, where the 3D shows."""
    window = Image.new("L", (W, H))
    ImageDraw.Draw(window).rectangle((STAGE[0], STAGE[1], STAGE[2] - 1, STAGE[3] - 1), fill=255)
    return window


def build_base(data, stem, kicker, titles, tabs, left, right):
    """The screen around the pages: `titles` for its pages, `tabs` its tab controls, `left` the
    (control, words) of the buttons from the foot's left, `right` (control, words, width,
    primary) from its right."""
    screen = Screen(data, stem)
    board = ground()
    kit.tag(board, kicker, LEFT, TOP, 26)
    kit.build_mark(board)

    # The bike page's stage shows the 3D through here too.
    window = stage_window()
    board.paste((0, 0, 0, 0), mask=window)

    areas = kit.step_areas(names=[("%02d" % (i + 1), words) for i, words in enumerate(titles)])
    title_area = (LEFT - 8, TOP + 50, areas[0][0] - 40, TOP + 186)
    for number, words in enumerate(titles):
        canvas = on_board(board, title_area)
        size = kit.fit(words, 92, title_area[2] - title_area[0] - 24)
        heading(canvas, words, LEFT, TOP + 140, size, PAPER)
        shape(canvas, [(LEFT + 2, TOP + 162), (LEFT + 600, TOP + 162), (LEFT + 594, TOP + 170), (LEFT, TOP + 170)],
              ORANGE)
        name = "Title%d" % number
        screen.add(name, Type="STATIC", Xywh=xywh(title_area),
                   AnimNorm=screen.picture("M%s" % name, under(canvas, title_area)))
        if not number:
            screen.shown.append((under(canvas, title_area), title_area[:2]))

    names = [("%02d" % (i + 1), words) for i, words in enumerate(titles)]
    for i, (name, area) in enumerate(zip(tabs, areas)):
        on, off = on_board(board, area), on_board(board, area)
        kit.steps(on, i, names=names)
        kit.steps(off, -1, names=names)
        pictures = {"On": under(on, area), "Off": under(off, area)}
        pictures["OnGrey"] = pictures["Off"]
        pictures["OffGrey"] = pictures["Off"]
        radio(screen, name, pictures, area, (0, 0), chosen=i == 0, FitToAnimOn="0")

    footer(screen, board, left, right)
    full_info(screen, board, window)
    return screen, board, window


def footer(screen, board, left, right):
    """The buttons along the foot: `left` the (control, words) from its left, `right` the
    (control, words, width, primary) from its right."""
    top, bottom = FOOT
    x = LEFT
    for name, words in left:
        w = heading_ink(words, 28) + 96
        area = (x, top, x + w, bottom)
        screen.button(name, area, button_states(board, area, words, size=28))
        x += w + 16
    end = x
    x = RIGHT
    for name, words, w, primary in right:
        area = (x - w, top, x, bottom)
        screen.button(name, area, button_states(board, area, words, primary=primary, size=44 if primary else 36))
        x -= w + 24
        assert x > end, words


def full_info(screen, board, window=None):
    """A whole screen's own keys: its ground and size, over the photos."""
    screen.set("Set_Information", BackgroundImage=screen.picture("MBack", board, window), ScreenWidth=str(W),
               ScreenHeight=str(H), ArtScale=str(ART), Photos="1", Xywh=xywh((0, 0, W, H)), FontName='"Helvetica"')


def full_screen(data, stem, kicker, title):
    """A whole screen of its own: its kicker and title, on the shade. Its controls' places are
    from the screen's corner; it is finished with full_info()."""
    screen = Screen(data, stem)
    board = ground()
    kit.page_title(board, kicker, title)
    kit.build_mark(board)
    return screen, board


def card_ground(w, h):
    """A card's ground: solid, with an orange edge (what is see-through shows the photos, not
    the screen under it)."""
    board = clear((w, h))
    box(board, (0, 0, w - 1, h - 1), (14, 15, 18, 255), 4, ORANGE + (255,))
    return board


def card_info(screen, board):
    """A card's own keys: its ground and size, in the middle of the screen it opens over."""
    w, h = board.size
    screen.set("Set_Information", BackgroundImage=screen.picture("MBack", board), ScreenWidth=str(w),
               ScreenHeight=str(h), ArtScale=str(ART), Xywh=xywh((0, 0, w, h)), FontName='"Helvetica"',
               DlgAlignH="CENTER", DlgAlignV="MIDDLE")


def scroll_bar(screen, board, name, bar, origin=None, anchor=None):
    """A slider (VSCROLLBAR, or HSCROLLBAR when `bar` is wider than tall) in `bar`: a groove and
    an orange thumb, the thumb cut from the groove's middle (see-through, it would show the
    photos through what it is on)."""
    groove = under(board, bar)
    w, h = groove.size
    d = ImageDraw.Draw(groove)
    if w > h:
        d.rounded_rectangle((0, h // 2 - 3, w - 1, h // 2 + 3), 3, fill=EDGE + (255,))
        middle = w // 2 - 18
        thumb = groove.crop((middle, 0, middle + 36, h))
        ImageDraw.Draw(thumb).rounded_rectangle((4, h // 2 - 16, 31, h // 2 + 16), 8, fill=ORANGE + (255,))
    else:
        d.rounded_rectangle((w // 2 - 3, 0, w // 2 + 3, h - 1), 3, fill=EDGE + (255,))
        middle = h // 2 - 36
        thumb = groove.crop((0, middle, w, middle + 72))
        ImageDraw.Draw(thumb).rounded_rectangle((w // 2 - 7, 0, w // 2 + 7, 71), 7, fill=ORANGE + (255,))
    track = screen.picture("M%sBar" % name, groove)
    keys = dict(Xywh=xywh(bar, origin or screen.origin), AnimNorm=track, AnimFocus=track, AnimPush=track,
                AnimDisable=track, AnimThumb=screen.picture("M%sThumb" % name, thumb))
    if anchor:
        keys.update(Anchor=anchor, RelAnchor="1")
    screen.shown += [(groove, (int(bar[0]), int(bar[1]))), (thumb, (int(bar[0]), int(bar[1])))]
    return keys


def info_row(screen, board, area, label, value, origin=None, colour=PAPER, size=30):
    """A line of information: its name in a well, and the game's words for it (`value`, a
    STATICTEXT) at the well's right."""
    chip(board, area, label)
    keys = {"Xywh": xywh((area[0] + 260, area[1], area[2] - 22, area[3]), origin or screen.origin),
            "TextLMargin": "0", "TextTMargin": "0"}
    keys.update(font(size, colour, "RIGHT"))
    screen.set(value, **keys)


def text_button(screen, board, name, area, origin=None):
    """A button the game writes its words on."""
    keys = {"TextColor": colorref(PAPER)}
    keys.update(font(30, PAPER, "CENTER"))
    screen.button(name, area, button_states(board, area, ""), origin, keep_text=True, **keys)


def build_bike(data, stem):
    """The bike and rider page: the 3D stage (its View, the colour key here and on the screen
    round it) and the rider and machine beside it."""
    screen = Screen(data, stem, PLACE[:2])
    board = ground()
    window = stage_window()
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

    for corner in ("CornerLR", "CornerUL", "CornerUR", "CornerLL"):
        if corner in screen.names:
            screen.hide(corner)
    page_info(screen, board, window)
    return screen, board, window


# ---- A table ----

def table(screen, board, frame, columns, origin, anchor=None, size=32, head=64):
    """Lists side by side under their headings (buttons that sort them): `columns` of (heading
    button, list, share of the width), in `frame` on the screen; their places from `origin`."""
    ImageDraw.Draw(board).line((frame[0] + 24, frame[1] + 20 + head, frame[2] - 24, frame[1] + 20 + head),
                               fill=EDGE + (255,), width=2)
    across = frame[2] - frame[0] - 64
    total = sum(share for _, _, share in columns)
    x = frame[0] + 32
    for button, listing, share in columns:
        w = across * share / total
        more = {"Anchor": anchor, "RelAnchor": "1"} if anchor else {}
        keys = {"Xywh": xywh((x, frame[1] + 20, x + w, frame[1] + 20 + head), origin), "TextLMargin": "8"}
        keys.update(font(26, GREY))
        if button:
            screen.set(button, **keys, **more)
        keys = {"Xywh": xywh((x, frame[1] + 40 + head, x + w, frame[3] - 20), origin), "TextLMargin": "8"}
        keys.update(font(size, PAPER))
        screen.set(listing, **keys, **more)
        x += w


# ---- Previews ----

def preview(photo, layers, place=PLACE):
    """The screen as the game would show it: `layers` of (screen, board, window) drawn in turn,
    each board where its screen draws it (a page in `place`) and then its controls at rest; the
    3D stood in for."""
    canvas = kit.cover(Image.open(photo).convert("RGB"), (W, H)).convert("RGBA")
    out = Image.new("RGBA", (W, H))
    for screen, board, window in layers:
        area = (0, 0, W, H) if screen.origin == (0, 0) else place
        out.paste(shown(board.crop(area), window=window.crop(area) if window else None), area[:2])
        for picture, at in screen.shown:
            out.paste(shown(picture), at)
        stage = window or Image.new("L", (W, H))           # the page's, drawn last, is what shows
    keyed = Image.fromarray(((np.asarray(out)[..., :3] == (255, 0, 255)).all(-1) * 255).astype("uint8"))
    out.paste((0, 0, 0, 0), mask=keyed)
    canvas.alpha_composite(out)
    canvas.paste((40, 44, 52, 255), mask=stage)
    return canvas.convert("RGB")
