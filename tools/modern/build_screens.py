r"""The modern look for the rest of the menus: build_screens.py <game ui folder> <out folder> <preview folder>

Each screen keeps its layout - every control where the original put it, so the game's code finds
what it expects - and is drawn again at ART times its size (its dtm says ArtScale, and the dev
build draws it at the menus' scale over that, so 1:1 at 1440 high). Every picture is made new
in mainmenu_art's look: flat dark grounds, grey fields with a blue edge, yellow for what is
chosen or on, cream type. Buttons whose label was painted into their picture get it back from
their Text, or from their name. The original's pictures and sounds stay in the archive, under
their own names, for anything that asks for them.
"""
import re
import sys
from pathlib import Path

from PIL import Image, ImageDraw

sys.path.insert(0, str(Path(__file__).parent))
import mainmenu_art as art                                                         # noqa: E402
from build_mainmenu import read_dtm, rs2, rs2_entries, rs2_files, set_keys, tga16              # noqa: E402
from mainmenu_art import (BLUE, CARD, CREAM, DIM, INK, ROW, ROW_LINE, YELLOW,       # noqa: E402
                          YELLOW_DOWN, text, width_of)

ART = 3                         # the pictures' pixels to the original's
SIDE = 320                      # what a full screen gains each side: 1920 wide becomes 2560
GROUND = (24, 26, 30)           # behind everything on a full screen
PLATE = (44, 47, 53)            # a group's plate (Opponents, Laps...), on a card
LIT_YELLOW = (255, 226, 92)
PRIMARY = re.compile(r"(Start|Race(?!s)|Continue|Accept|Done|Play|OK$|Ok$)")
PLATE_NAME = re.compile(r"(Box|Plate|Panel)$")
FOLDER = "03_Modern\\"
CREAM_BGR = str(0xE0ECF0)       # COLORREF of CREAM
INK_BGR = str(0x141212)

SKIP = {"MAINMENU", "GLOBAL", "CREDITS", "INTROVID", "INTRO1", "INTRO2", "INTRO3"}
SKIP_PREFIX = ("LOAD",)         # the tracks' loading pictures stay as they are

TITLES = {"GRBASE": "GHOSTS AND REPLAYS", "HISCORES": "HIGH SCORES", "MPBASE": "MULTIPLAYER",
          "OPTADVG": "ADVANCED GRAPHICS", "OPTBASE": "OPTIONS", "PCC": "PRO CIRCUIT",
          "PCDEAL": "PRO CIRCUIT", "PCDONE": "PRO CIRCUIT", "PCFINISH": "RESULTS",
          "PCLRACE": "LAST RACE", "PCSTART": "PRO CIRCUIT", "PCTRANS": "PRO CIRCUIT",
          "SPBASE": "SINGLE PLAYER"}


def a(v):
    """A length in the original's pixels, in the new pictures'."""
    return round(v * ART)


def _bahn(size, style="Bold Condensed"):
    f = art.ImageFont.truetype(art.BAHNSCHRIFT, max(a(size), 6))
    f.set_variation_by_name(style)
    return f


def mono_a(size):
    return art.ImageFont.truetype(art.CONSOLAS, max(a(size), 6))


# ---- Reading a layout ----

def value(lines, key):
    for line in lines:
        if "=" in line and line.split("=", 1)[0].strip().lower() == key.lower():
            return line.split("=", 1)[1].split("'")[0].strip().strip('"')
    return None


def numbers(text):
    return [int(float(n)) for n in re.findall(r"-?\d+(?:\.\d+)?", text or "")]


def label_of(name, lines):
    """What a button says: its Text, or its name with the prefix taken off and the words split."""
    words = value(lines, "Text")
    if words:
        return words.replace("&", "").upper()
    name = re.sub(r"^(But|Btn|Button|Menu|Cmd)", "", name)
    name = re.sub(r"(?<=[a-z])(?=[A-Z0-9])|_", " ", name)
    return name.strip().upper() or "OK"


def picture_size(entries, globals_, anims, anim):
    """The original picture `anim` names, as (width, height), or None."""
    for line in anims.get(anim.lower(), []):
        if line.lower().startswith("files1"):
            path = line.split("=", 1)[1].strip().strip('"').lower()
            data = entries.get(path) or globals_.get(path)
            if data and len(data) >= 18:
                return int.from_bytes(data[12:14], "little"), int.from_bytes(data[14:16], "little")
    return None


# ---- Drawing ----

def new(size, ground):
    return Image.new("RGBA", (max(size[0], 1), max(size[1], 1)), ground + (255,))


def fill(img, colour, box=None):
    ImageDraw.Draw(img).rectangle(box or (0, 0, img.width - 1, img.height - 1), fill=colour + (255,))


def outline(img, colour, width=None):
    ImageDraw.Draw(img).rectangle((0, 0, img.width - 1, img.height - 1), outline=colour + (255,),
                                  width=width or max(a(0.6), 1))


def fit_font(words, w, h, most):
    """A condensed italic size for `words` that fits w x h pixels, at most `most` (original px)."""
    size = most
    while size > 6:
        f = _bahn(size)
        if width_of(words, f) <= w * 0.82 and a(size) <= h * 0.86:
            return f
        size -= 0.5
    return _bahn(6)


def centred_text(img, words, f, colour, italic=True, left=None):
    ascent, descent = f.getmetrics()
    w = width_of(words, f)
    x = left if left is not None else (img.width - w) / 2
    y = (img.height + ascent - descent) / 2
    text(img, words, x, y, f, colour, italic=italic)


def button_states(size, ground, words, primary=False):
    """Up, Lit, Down, Grey of a button: a grey field with a blue edge, yellow when chosen; a small
    one is an outlined chip."""
    w, h = size
    chip = w < 2.6 * h or h < a(16)
    if chip:                                    # the chip itself: compact, in the middle
        f = fit_font(words, w - a(6), min(h, a(22)), 12)
        cw = min(w, int(width_of(words, f)) + a(18))
        ch = min(h, a(22))
        chip_box = ((w - cw) // 2, (h - ch) // 2, (w - cw) // 2 + cw - 1, (h - ch) // 2 + ch - 1)
    else:
        f = fit_font(words, w - a(8), h, 16)
    pictures = {}
    for state in ("Up", "Lit", "Down", "Grey"):
        img = new(size, ground)
        if chip:
            edge = {"Up": (78, 82, 88), "Lit": YELLOW, "Down": YELLOW, "Grey": (50, 52, 56)}[state]
            ink = {"Up": CREAM, "Lit": YELLOW, "Down": YELLOW_DOWN, "Grey": DIM}[state]
            d = ImageDraw.Draw(img)
            if primary and state != "Grey":
                d.rectangle(chip_box, fill={"Up": YELLOW, "Lit": LIT_YELLOW,
                                            "Down": YELLOW_DOWN}[state] + (255,))
                ink = INK
            else:
                if state == "Lit":
                    d.rectangle(chip_box, fill=(40, 42, 46, 255))
                d.rectangle(chip_box, outline=edge + (255,), width=max(a(0.7), 2))
        else:
            if state in ("Lit", "Down") or (primary and state == "Up"):
                fill(img, {"Up": YELLOW, "Lit": LIT_YELLOW if primary else YELLOW,
                           "Down": YELLOW_DOWN}[state])
                ink = INK
            else:
                fill(img, ROW)
                fill(img, BLUE if state == "Up" else DIM, (0, 0, a(1.4) - 1, h - 1))
                ink = CREAM if state == "Up" else DIM
        centred_text(img, words, f, ink, left=None if chip else a(9))
        pictures[state] = img
    return pictures


def direction_of(name):
    """Which way a button named for a direction points, or None."""
    for word, way in (("Left", "left"), ("Prev", "left"), ("Right", "right"), ("Next", "right"),
                      ("Up", "up"), ("Down", "down")):
        if re.search(word + r"(?![a-z])", name):
            return way
    return None


def arrow_states(size, ground, way):
    """A chevron on a grey square, pointing `way`."""
    pictures = {}
    for state, colour in (("Up", BLUE), ("Lit", YELLOW), ("Down", CREAM), ("Grey", DIM)):
        img = new(size, ground)
        fill(img, ROW)
        w, h = img.size
        r = min(w, h) * 0.2
        cx, cy = w / 2, h / 2
        pts = {"up": [(cx - r, cy + r / 2), (cx, cy - r / 2), (cx + r, cy + r / 2)],
               "down": [(cx - r, cy - r / 2), (cx, cy + r / 2), (cx + r, cy - r / 2)],
               "left": [(cx + r / 2, cy - r), (cx - r / 2, cy), (cx + r / 2, cy + r)],
               "right": [(cx - r / 2, cy - r), (cx + r / 2, cy), (cx - r / 2, cy + r)]}[way]
        ImageDraw.Draw(img).line(pts, fill=colour + (255,), width=max(a(0.9), 2), joint="curve")
        pictures[state] = img
    return pictures


def field(size, ground, edge=BLUE, underline=True):
    """A grey field, a blue rule along its bottom: list, edit box and text backs."""
    img = new(size, ground)
    fill(img, ROW)
    if underline:
        fill(img, edge, (0, img.height - max(a(0.6), 2), img.width - 1, img.height - 1))
    return img


def track(size, ground):
    """A scroll bar's track: a fine rule along its length."""
    img = new(size, ground)
    fill(img, (38, 40, 45))
    w, h = img.size
    d = ImageDraw.Draw(img)
    if w >= h:
        d.line([(0, h // 2), (w, h // 2)], fill=ROW_LINE + (255,), width=max(a(0.5), 1))
    else:
        d.line([(w // 2, 0), (w // 2, h)], fill=ROW_LINE + (255,), width=max(a(0.5), 1))
    return img


def thumb(size, ground):
    img = new(size, ground)
    w, h = img.size
    inset = max(min(w, h) // 5, 1)
    fill(img, YELLOW, (inset, inset, w - 1 - inset, h - 1 - inset))
    return img


def radio(size, ground, on, enabled):
    """A radio button's mark at its left: a yellow square when on, an outline when off. The game
    draws its text."""
    img = new(size, ground)
    side = min(img.height - a(2), a(9))
    top = (img.height - side) // 2
    box = (a(1), top, a(1) + side - 1, top + side - 1)
    d = ImageDraw.Draw(img)
    colour = (YELLOW if on else (110, 114, 120)) if enabled else DIM
    d.rectangle(box, outline=colour + (255,), width=max(a(0.6), 2))
    if on:
        inner = max(side // 4, 2)
        d.rectangle((box[0] + inner, box[1] + inner, box[2] - inner, box[3] - inner),
                    fill=colour + (255,))
    return img


def backdrop(size, card, side, title):
    """A screen's ground, or a panel's card."""
    img = new(size, CARD if card else GROUND)
    w, h = img.size
    if card:
        fill(img, BLUE, (0, 0, a(1.2) - 1, h - 1))
        accent = Image.new("L", (w, max(a(0.8), 2)))
        accent.putdata([int(255 * max(0.0, 1 - x / (w * 0.6))) for x in range(w)] * accent.height)
        layer = Image.new("RGBA", accent.size, BLUE + (0,))
        layer.putalpha(accent)
        img.alpha_composite(layer)
        return img
    if side:
        side_panel(img, side, title)
    return img


def side_panel(img, side, title):
    """The left of a full screen: the game's name small, the screen's large, its last word yellow;
    a hairline down each side of the 4:3."""
    w, h = img.size
    d = ImageDraw.Draw(img)
    bar = max(a(0.9), 3) - 1
    d.rectangle((0, 0, side - 1, bar), fill=YELLOW + (255,))
    d.rectangle((w - side, 0, w - 1, bar), fill=YELLOW + (255,))
    d.line([(side - 1, a(6)), (side - 1, h - a(6))], fill=ROW_LINE + (255,), width=1)
    d.line([(w - side, a(6)), (w - side, h - a(6))], fill=ROW_LINE + (255,), width=1)
    art.dash(img, art.s(36), art.s(70), 24, YELLOW)
    text(img, "MOTOCROSS MADNESS 2", art.s(70), art.s(76), art.mono(12), (150, 152, 156), spacing=1)
    if not title:
        return
    words = title.split()
    size = 76
    while size > 30 and max(width_of(word, art.font(size)) for word in words) > side - art.s(56):
        size -= 2
    f = art.font(size)
    y = art.s(110) + f.getmetrics()[0]
    for i, word in enumerate(words):
        text(img, word, art.s(32), y, f, YELLOW if i == len(words) - 1 else CREAM, italic=True)
        y += int(f.getmetrics()[0] * 1.02)
    text(img, "OPENMCM2  /  DEVELOPMENT BUILD", w - art.s(28), h - art.s(30), art.mono(11), BLUE,
         right=True, spacing=0.5)


def plate(size):
    """A group's plate: a lighter field with a blue edge."""
    img = new(size, PLATE)
    fill(img, BLUE, (0, 0, max(a(0.8), 2) - 1, img.height - 1))
    return img


TITLE_BOX = (8, 4, 220, 22)                     # where a title goes, in the original's pixels


def draw_title(img, title, side):
    """The screen's title as a label at its top left."""
    ImageDraw.Draw(img).rectangle((side + a(10), a(12), side + a(16) - 1, a(13) - 1),
                                  fill=BLUE + (255,))
    text(img, title, side + a(20), a(15), mono_a(6), BLUE, spacing=0.3)


def overlaps(box, other):
    return not (box[2] <= other[0] or other[2] <= box[0] or box[3] <= other[1] or other[3] <= box[1])


# ---- A screen ----

def scale_xywh(text_value, shift=0):
    """An Xywh at ART times, moved `shift` pixels across."""
    n = [a(v) for v in numbers(text_value)]
    if n:
        n[0] += shift
    return ", ".join(str(v) for v in n)


def theme(dat, globals_, preview_dir):
    files = rs2_files(dat.read_bytes())
    entries = {n.lower(): blob for n, blob in files}
    layout = next(n for n, _ in files if n.lower().endswith(".dtm"))
    sections = read_dtm(entries[layout.lower()].decode("latin-1"))
    info = dict(sections).get("Set_Information", [])
    width = int(value(info, "ScreenWidth") or 640)
    height = int(value(info, "ScreenHeight") or 480)
    background = value(info, "BackgroundImage")
    full = width >= 640 and height >= 480
    card = bool(background) and background.lower() != "none" and not full
    ground = CARD if card else GROUND
    # A full screen is made 16:9, its 4:3 in the middle, so it covers the screen beside it too.
    side = SIDE if full and background and background.lower() != "none" else 0
    anims = {n.lower(): l for n, l in sections}
    stem = dat.stem.upper()

    pictures, keys, drops = {}, {}, {}
    if background and background.lower() != "none":
        pictures["MBack"] = backdrop((a(width) + 2 * side, a(height)), card, side,
                                     TITLES.get(stem) if side else None)
    info_keys = {"ScreenWidth": str(a(width) + 2 * side), "ScreenHeight": str(a(height)),
                 "ArtScale": str(ART),
                 "FontName": '"Bahnschrift"', "TextColor": CREAM_BGR}
    if "MBack" in pictures:
        info_keys["BackgroundImage"] = '"MBack"'
    if value(info, "Xywh"):
        info_keys["Xywh"] = scale_xywh(value(info, "Xywh"))
    if value(info, "FontHeight"):
        info_keys["FontHeight"] = "%g" % (float(value(info, "FontHeight")) * ART)
    keys["Set_Information"] = info_keys

    placed = []                                     # (picture, absolute box) for the preview
    plates = set()                                  # the groups' plates, by lower-case name
    boxes = []                                      # every control's, in the original's pixels
    origins = {}
    for name, lines in sections:
        kind = (value(lines, "Type") or "").upper()
        box = numbers(value(lines, "Xywh"))
        if not kind or name == "Set_Information" or len(box) < 4:
            continue
        x, y, w, h = box[:4]
        anchor = value(lines, "Anchor")
        ox, oy = origins.get(anchor.lower(), (0, 0)) if anchor else (0, 0)
        origins[name.lower()] = (ox + x, oy + y)
        boxes.append((ox + x, oy + y, ox + x + w, oy + y + h))
        size = (a(w), a(h))
        on = PLATE if anchor and anchor.lower() in plates else ground
        k = {"Xywh": scale_xywh(value(lines, "Xywh"), 0 if anchor else side)}
        if value(lines, "FontHeight"):
            k["FontHeight"] = "%g" % (float(value(lines, "FontHeight")) * ART)
        for colour_key in ("TextColor",):
            if value(lines, colour_key) is not None:
                k[colour_key] = CREAM_BGR
        if value(lines, "TextDrop") is not None:
            k["TextDrop"] = "0"
        if value(lines, "SelectColor") is not None:
            k["SelectColor"] = str(0x27D3F5)        # yellow, as a COLORREF
        has = lambda key: (value(lines, key) or "0") not in ("0", "")   # noqa: E731
        made = {}

        def picture_of(key, original_size=False):
            if original_size:
                found = picture_size(entries, globals_, anims, value(lines, key))
                return (a(found[0]), a(found[1])) if found else size
            return size

        if kind == "BUTTON" and has("AnimNorm"):
            made = {"AnimNorm": "Up", "AnimFocus": "Lit", "AnimPush": "Down", "AnimDisable": "Grey"}
            way = direction_of(name)
            if way and w < 2.4 * h:
                states = arrow_states(size, on, way)
            else:
                states = button_states(size, on, label_of(name, lines),
                                       bool(PRIMARY.search(name)))
            drops[name] = ("Text", "LocalTextId")
            for key, state in made.items():
                pictures["M%s%s" % (name, state)] = states[state]
        elif kind in ("SCROLLUP", "SCROLLDOWN") and has("AnimNorm"):
            made = {"AnimNorm": "Up", "AnimFocus": "Lit", "AnimPush": "Down", "AnimDisable": "Grey"}
            states = arrow_states(size, on, "up" if kind == "SCROLLUP" else "down")
            for key, state in made.items():
                pictures["M%s%s" % (name, state)] = states[state]
        elif kind == "RADIOBUTTON" and has("AnimOn"):
            made = {"AnimOn": "On", "AnimOff": "Off", "AnimOnDisable": "OnGrey", "AnimOffDisable": "OffGrey"}
            for key, state in made.items():
                pictures["M%s%s" % (name, state)] = radio(picture_of(key, True), on,
                                                          state.startswith("On"), "Grey" not in state)
        elif kind in ("VSCROLLBAR", "HSCROLLBAR"):
            for key, maker in (("AnimNorm", track), ("AnimThumb", thumb), ("AnimFocus", track),
                               ("AnimPush", track), ("AnimDisable", track)):
                if has(key):
                    made[key] = key[4:]
                    pictures["M%s%s" % (name, key[4:])] = maker(picture_of(key, key == "AnimThumb"), on)
        elif kind == "DROPDOWNLIST":
            for key in ("AnimNorm", "AnimFocus", "AnimPush", "AnimDisable"):
                if has(key):
                    made[key] = key[4:]
                    img = field(size, on, YELLOW if key == "AnimFocus" else BLUE)
                    w2, h2 = img.size
                    d = ImageDraw.Draw(img)
                    cx, cy = w2 - h2 // 2, h2 // 2
                    d.line([(cx - h2 * 0.16, cy - h2 * 0.08), (cx, cy + h2 * 0.1),
                            (cx + h2 * 0.16, cy - h2 * 0.08)], fill=CREAM + (255,),
                           width=max(a(0.8), 2), joint="curve")
                    pictures["M%s%s" % (name, key[4:])] = img
            for key, maker in (("AnimThumb", thumb), ("AnimListBack", lambda s, g: field(s, g, underline=False)),
                               ("AnimScrollBack", track), ("AnimTextBack", lambda s, g: field(s, g, underline=False))):
                if has(key):
                    made[key] = key[4:]
                    pictures["M%s%s" % (name, key[4:])] = maker(picture_of(key, True), on)
        elif kind in ("LISTBOX", "EDITBOX", "STATICTEXT") and has("AnimNorm"):
            made = {"AnimNorm": "Back"}
            pictures["M%sBack" % name] = field(size, on, underline=kind == "EDITBOX")
        elif kind == "STATIC" and has("AnimNorm") and PLATE_NAME.search(name):
            made = {"AnimNorm": "Plate"}
            pictures["M%sPlate" % name] = plate(size)
            plates.add(name.lower())
            for fx in ("FXAnimIn", "FXAnimOut"):
                if has(fx):
                    k[fx] = '"M%sPlate"' % name
        elif kind == "STATIC" and has("AnimNorm"):
            # Other decoration: none.
            drops[name] = ("AnimNorm", "FXAnimIn", "FXAnimOut")
        for key, state in made.items():
            k[key] = '"M%s%s"' % (name, state)
        keys[name] = k
        if "AnimNorm" in made or "AnimOff" in made:
            first = "M%s%s" % (name, made.get("AnimNorm") or made.get("AnimOff"))
            placed.append((pictures[first], (a(ox + x) + side, a(oy + y))))


    out = []
    for name, lines in sections:
        if name in keys:
            lines = set_keys(lines, keys[name], drops.get(name, ()))
        out += ["[%s]" % name] + [l for l in lines if l.strip()]
        if name.lower() == "set_anim":
            for anim in pictures:
                out += ["", "[%s]" % anim, "Files1 = %s%s.tga" % (FOLDER, anim), "MSecDelay = 0",
                        "LoopCount = 0"]
        out.append("")
    if not any(n.lower() == "set_anim" for n, _ in sections):
        out += ["[Set_Anim]", ""]
        for anim in pictures:
            out += ["[%s]" % anim, "Files1 = %s%s.tga" % (FOLDER, anim), "MSecDelay = 0",
                    "LoopCount = 0", ""]

    kept = [(n, blob) for n, blob in files if not n.lower().endswith(".dtm")]
    new_entries = kept + [(FOLDER + anim + ".tga", tga16(img)) for anim, img in pictures.items()]
    new_entries.append((layout, "\r\n".join(out).encode("latin-1")))

    if preview_dir:
        canvas = (pictures.get("MBack") or new((a(width) + 2 * side, a(height)), GROUND)).copy()
        for img, at in placed:
            canvas.alpha_composite(img, at)
        rgb = canvas.convert("RGB")
        px = rgb.load()
        w, h = rgb.size
        for yy in range(0, h):
            for xx in range(0, w):
                if px[xx, yy] == (255, 0, 255):
                    px[xx, yy] = ground
        rgb.save(Path(preview_dir) / ("modern %s.png" % stem.lower()))
    return rs2(new_entries), len(pictures)


def main(ui, out, preview_dir):
    ui, out = Path(ui), Path(out)
    globals_ = rs2_entries((ui / "GLOBAL.DAT").read_bytes())
    out.mkdir(parents=True, exist_ok=True)
    for dat in sorted(ui.glob("*.DAT")):
        stem = dat.stem.upper()
        if stem in SKIP or stem.startswith(SKIP_PREFIX):
            continue
        data, count = theme(dat, globals_, preview_dir)
        (out / dat.name).write_bytes(data)
        print("%-13s %3d pictures, %d bytes" % (dat.name, count, len(data)))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else None)
