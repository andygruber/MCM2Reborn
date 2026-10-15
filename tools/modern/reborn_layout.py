r"""A screen in MCM2:Reborn's look, from the original's archive: its controls moved and given new
pictures, new controls added, the ones the look has no use for put out of sight.

The builders draw a screen at 2560x1440 with reborn_kit and cut each control's pictures from
what is under it, because the game draws a control's picture over the screen's, see-through
parts and all. A page drawn in another screen (Place) is drawn on the same 2560x1440 and cut
out of it, so both share one set of coordinates.
"""
from PIL import Image, ImageDraw

from build_mainmenu import read_dtm, rs2, rs2_files, set_keys
from modern_ui import tga16
from reborn_kit import (DIM, EDGE, GREY, INK, ORANGE, PANEL, PAPER, RAISED, SOFT, W, H, box, cap_height,
                        heading, heading_ink, outline, shape, slanted, sub, sub_width)

FOLDER = "04_Modern\\"
STATES = ("Up", "Lit", "Down", "Grey")
DEEP = (214, 110, 0)                    # orange, pressed
TEXT_KEYS = ("Text", "LocalTextId")
NOTHING = "MNothing"


def colorref(rgb):
    return str(rgb[0] | rgb[1] << 8 | rgb[2] << 16)


def clear(size=(W, H)):
    return Image.new("RGBA", size, (0, 0, 0, 0))


def under(board, area):
    """What `board` holds under `area`."""
    x0, y0, x1, y1 = (int(round(v)) for v in area)
    return board.crop((x0, y0, x1, y1)).convert("RGBA")


def on_board(board, area):
    """A full-size canvas with what `board` holds under `area`, to draw a state on."""
    canvas = clear(board.size)
    canvas.alpha_composite(under(board, area), (int(round(area[0])), int(round(area[1]))))
    return canvas


def xywh(area, origin=(0, 0)):
    x0, y0, x1, y1 = (int(round(v)) for v in area)
    return "%d, %d, %d, %d" % (x0 - origin[0], y0 - origin[1], x1 - x0, y1 - y0)


def font(size, colour=PAPER, align="LEFT", bold=True):
    """The keys for the game's own text in a control: the text face, at `size` layout pixels."""
    return {"Font_UseDefault": "0", "FontName": '"Helvetica"', "FontHeight": str(size),
            "Bold": "700" if bold else "400",
            "Italic": "-1", "TextColor": colorref(colour), "TextDrop": "0", "TextAlign": align,
            "TextAlignV": "MIDDLE"}


# ---- The controls' states ----

def button_states(board, area, words, primary=False, size=None):
    """A slanted button: orange when primary, an outlined ghost filled orange when chosen."""
    x0, y0, x1, y1 = area
    size = size or int((y1 - y0) * 0.46)
    pictures = {}
    for state in STATES:
        canvas = on_board(board, area)
        lit = state in ("Lit", "Down")
        if primary and state != "Grey":
            shape(canvas, slanted(area), {"Up": ORANGE, "Lit": (255, 158, 40), "Down": DEEP}[state])
            colour = INK
        elif lit:
            shape(canvas, slanted(area), ORANGE if state == "Lit" else DEEP)
            colour = INK
        else:
            shape(canvas, slanted(area), PANEL, 220)
            outline(canvas, slanted(area), EDGE, 2)
            colour = DIM if state == "Grey" else PAPER
        chevron = primary and words
        total = heading_ink(words, size) + (14 + sub_width("›", int(size * 1.15), bold=True) if chevron else 0)
        x = (x0 + x1) / 2 - total / 2
        baseline = (y0 + y1) / 2 + cap_height(size) / 2
        heading(canvas, words, x, baseline, size, colour)
        if chevron:
            sub(canvas, "›", x + heading_ink(words, size) + 14, baseline + 2, int(size * 1.15), colour,
                bold=True)
        pictures[state] = under(canvas, area)
    return pictures


def chevron(img, area, way, colour, width=5):
    """A chevron pointing `way` ("left", "right", "up" or "down") in the middle of `area`."""
    x0, y0, x1, y1 = area
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    r = min(x1 - x0, y1 - y0) * 0.17
    points = {"left": [(cx + r * 0.6, cy - r), (cx - r * 0.6, cy), (cx + r * 0.6, cy + r)],
              "right": [(cx - r * 0.6, cy - r), (cx + r * 0.6, cy), (cx - r * 0.6, cy + r)],
              "up": [(cx - r, cy + r * 0.6), (cx, cy - r * 0.6), (cx + r, cy + r * 0.6)],
              "down": [(cx - r, cy - r * 0.6), (cx, cy + r * 0.6), (cx + r, cy - r * 0.6)]}[way]
    ImageDraw.Draw(img).line(points, fill=colour + (255,), width=width, joint="curve")


def arrow_states(board, area, way, well=True):
    """A small step button: a chevron, on a raised well when `well`; orange when chosen."""
    pictures = {}
    for state in STATES:
        canvas = on_board(board, area)
        lit = state in ("Lit", "Down")
        if well:
            box(canvas, area, ((ORANGE if state == "Lit" else DEEP) if lit else RAISED) + (255,), 8,
                None if lit else EDGE + (255,))
            colour = INK if lit else DIM if state == "Grey" else PAPER
        else:
            colour = (ORANGE if state == "Lit" else DEEP) if lit else DIM if state == "Grey" else PAPER
        chevron(canvas, area, way, colour)
        pictures[state] = under(canvas, area)
    return pictures


def disc_states(area, way):
    """A round step button laid over the game's own picture: what is round it is the colour key,
    so the picture under it still shows."""
    x0, y0, x1, y1 = area
    size = (int(x1 - x0), int(y1 - y0))
    pictures = {}
    for state in STATES:
        img = Image.new("RGBA", size, (255, 0, 255, 255))
        lit = state in ("Lit", "Down")
        ImageDraw.Draw(img).ellipse((0, 0, size[0] - 1, size[1] - 1),
                                    fill=((ORANGE if state == "Lit" else DEEP) if lit else (12, 12, 14)) + (255,))
        chevron(img, (0, 0) + size, way, INK if lit else DIM if state == "Grey" else PAPER, 6)
        pictures[state] = img
    return pictures


def toggle_states(board, area, label):
    """An on/off setting (MULTISTATE): its name, and a switch at its right."""
    x0, y0, x1, y1 = area
    pictures = {}
    for key, on, enabled in (("State0", False, True), ("State1", True, True), ("StateDisable0", False, False),
                             ("StateDisable1", True, False)):
        canvas = on_board(board, area)
        box(canvas, area, RAISED + (255,), 8, EDGE + (255,))
        label_at(canvas, area, label, enabled)
        sw, sh = 76, 36
        assert x0 + 22 + sub_width(label, LABEL, bold=True) + 20 < x1 - 20 - sw, label
        sx, sy = x1 - 20 - sw, (y0 + y1) / 2 - sh / 2
        fill = (ORANGE if enabled else DEEP) if on else (52, 54, 60)
        box(canvas, (sx, sy, sx + sw, sy + sh), fill + (255,), sh // 2)
        knob = sx + sw - sh + 4 if on else sx + 4
        box(canvas, (knob, sy + 4, knob + sh - 8, sy + sh - 4), (INK if on else (150, 150, 156)) + (255,),
            (sh - 8) // 2)
        pictures[key] = under(canvas, area)
    return pictures


def segment_states(board, area, words):
    """One of a row of choices (RADIOBUTTON): orange when it is the one chosen."""
    x0, y0, x1, y1 = area
    pictures = {}
    for key, on, enabled in (("On", True, True), ("Off", False, True), ("OnGrey", True, False),
                             ("OffGrey", False, False)):
        canvas = on_board(board, area)
        if on:
            box(canvas, area, (ORANGE if enabled else (92, 64, 30)) + (255,), 6)
        size = 22
        colour = (INK if on else SOFT) if enabled else DIM
        heading(canvas, words, (x0 + x1) / 2, (y0 + y1) / 2 + cap_height(size) / 2, size, colour, anchor="ms")
        pictures[key] = under(canvas, area)
    return pictures


LABEL = 22                              # a setting's name


def label_at(img, area, words, enabled=True, x=None):
    """A setting's name at the left of its well, middled."""
    sub(img, words, area[0] + 22 if x is None else x, (area[1] + area[3]) / 2, LABEL, GREY if enabled else DIM,
        anchor="lm", bold=True)


def well(board, area, line=True):
    """A setting's ground: a raised well with a fine edge."""
    canvas = on_board(board, area)
    box(canvas, area, RAISED + (255,), 8, EDGE + (255,) if line else None)
    return under(canvas, area)


# ---- The layout ----

class Screen:
    """The original archive `data`, and what is to change in its layout."""

    def __init__(self, data, stem, origin=(0, 0)):
        self.files = rs2_files(data)
        self.layout = next(n for n, _ in self.files if n.lower().endswith(".dtm"))
        blobs = dict(self.files)
        self.sections = read_dtm(blobs[self.layout].decode("latin-1"))
        self.names = {n for n, _ in self.sections}
        self.stem = stem
        self.origin = origin                        # where its controls' places are from, on the screen
        self.pictures = {}                          # anim -> (image, mask of the 3D under it)
        self.keys, self.drops, self.added = {}, {}, []
        self.shown = []                             # (picture, where on the screen), for previews
        self.picture(NOTHING, clear((4, 4)))

    def picture(self, anim, img, window=None):
        self.pictures[anim] = (img, window)
        return '"%s"' % anim

    def states(self, name, pictures, keys, at=None, window=None, show=None):
        """A control's pictures, `keys` mapping each layout key to the state it shows; state `show`
        (or the first) is shown in the previews at `at`."""
        for state, img in pictures.items():
            self.picture("M%s%s" % (name, state), img, window)
        if at is not None:
            self.shown.append((pictures[show or next(iter(keys.values()))], (int(round(at[0])), int(round(at[1])))))
        return {key: '"M%s%s"' % (name, state) for key, state in keys.items()}

    def set(self, section, drop=(), **keys):
        assert section in self.names, section
        self.keys.setdefault(section, {}).update(keys)
        self.drops[section] = tuple(self.drops.get(section, ())) + tuple(drop)

    def add(self, section, before="Set_Anim", **keys):
        """A control of the look's own, its section before `before` (drawn under that control and
        those after it), or by default after the original's controls."""
        self.added.append((section, before, keys))
        self.names.add(section)

    def button(self, name, area, pictures, origin=None, **more):
        """A button (or step arrow) at `area` on the screen, `origin` its anchor's corner."""
        self.set(name, TEXT_KEYS, Xywh=xywh(area, origin or self.origin),
                 **self.states(name, pictures, {"AnimNorm": "Up", "AnimFocus": "Lit", "AnimPush": "Down",
                                                "AnimDisable": "Grey"}, at=area[:2]), **more)

    def hide(self, name):
        """A control put out of sight in the corner, drawing nothing."""
        nothing = '"%s"' % NOTHING
        keys = {k: nothing for k in ("AnimNorm", "AnimFocus", "AnimPush", "AnimDisable", "AnimTextBack",
                                     "AnimListBack", "AnimScrollBack", "AnimThumb", "AnimOn", "AnimOff",
                                     "AnimOnDisable", "AnimOffDisable", "FXAnimIn", "FXAnimOut")
                if self.has(name, k)}
        self.set(name, TEXT_KEYS, Xywh="0, 0, 4, 4", Anchor="", **keys)

    def has(self, section, key):
        lines = dict(self.sections).get(section, [])
        return any(line.split("=", 1)[0].strip().lower() == key.lower() for line in lines if "=" in line)

    def pack(self):
        out = []
        for name, lines in self.sections:
            for added, before, keys in self.added:
                if before.lower() == name.lower():
                    keys = dict(keys, **self.keys.get(added, {}))
                    out += ["[%s]" % added] + ["%s = %s" % kv for kv in keys.items()] + [""]
            if name in self.keys:
                lines = set_keys(lines, self.keys[name], self.drops.get(name, ()))
            out += ["[%s]" % name] + [line for line in lines if line.strip()] + [""]
            if name.lower() == "set_anim":
                for anim in self.pictures:
                    out += ["[%s]" % anim, "Files1 = %s" % self.path(anim), "MSecDelay = 0", "LoopCount = 0", ""]
        entries = [(n, blob) for n, blob in self.files if not n.lower().endswith(".dtm")]
        entries += [(self.path(anim), tga16(img, window=window)) for anim, (img, window) in self.pictures.items()]
        entries.append((self.layout, "\r\n".join(out).encode("latin-1")))
        return rs2(entries)

    def path(self, anim):
        return "%s%s_%s.tga" % (FOLDER, self.stem, anim)
