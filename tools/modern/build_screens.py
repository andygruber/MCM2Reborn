r"""The modern look for the rest of the menus: build_screens.py <game ui folder> <out folder> <preview folder>

Each screen keeps its layout - every control where the original put it, so the game's code finds
what it expects - and is drawn again at ART times its size (its dtm says ArtScale, and the dev
build draws it at the menus' scale over that, so 1:1 at 1440 high). The look is the main menu's
(modern_ui): the photos behind, dark glass over them, cream type, rounded blue bars for what is
chosen. A full screen is 16:9, its title on glass at the left and its 4:3 on a glass panel in
the middle; the screens drawn inside it are the same glass, so they sit in it seamlessly.

The game draws a control's picture over what is under it, holes and all, so each picture is
drawn on the part of its screen it covers. Buttons whose label was painted into their picture
get it back from their Text, or from their name. The original's pictures and sounds stay in the
archive, under their own names, for anything that asks for them.
"""
import re
import sys
from pathlib import Path

from PIL import Image, ImageDraw

sys.path.insert(0, str(Path(__file__).parent))
import mainmenu_art as art                                                         # noqa: E402
from build_mainmenu import read_dtm, rs2, rs2_entries, rs2_files, set_keys          # noqa: E402
from mainmenu_art import text, width_of                                             # noqa: E402
from modern_ui import (BLUE, CREAM, DIM, EDGE, GLOW, PALE, SOFT, clear, glass, lit_bar,  # noqa: E402
                       paint, rim, rounded, shown, tga16)

ART = 3                         # the pictures' pixels to the original's
SIDE = 320                      # what a full screen gains each side: 1920 wide becomes 2560
PANEL = 0.58                    # the glass a screen's 4:3, and the screens in it, are on
POPUP = 0.9                     # a message's glass: the screen under it hardly shows
POPUPS = re.compile(r"^(MESSBOX\d|PCNEW|SERPOPUP|HOSTJOIN|GLOBSET)$")
PRIMARY = re.compile(r"(Start|Race(?!s)|Continue|Accept|Done|Play|OK$|Ok$)")
PLATE_NAME = re.compile(r"(Box|Plate|Panel)$")
FOLDER = "03_Modern\\"


def colorref(rgb):
    r, g, b = rgb
    return str(b << 16 | g << 8 | r)


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


def bahn(size, style="Bold Condensed"):
    f = art.ImageFont.truetype(art.BAHNSCHRIFT, max(a(size), 6))
    f.set_variation_by_name(style)
    return f


# ---- Reading a layout ----

def value(lines, key):
    for line in lines:
        if "=" in line and line.split("=", 1)[0].strip().lower() == key.lower():
            return line.split("=", 1)[1].split("'")[0].strip().strip('"')
    return None


def numbers(text_):
    return [int(float(n)) for n in re.findall(r"-?\d+(?:\.\d+)?", text_ or "")]


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


def direction_of(name):
    """Which way a button named for a direction points, or None."""
    for word, way in (("Left", "left"), ("Prev", "left"), ("Right", "right"), ("Next", "right"),
                      ("Up", "up"), ("Down", "down")):
        if re.search(word + r"(?![a-z])", name):
            return way
    return None


# ---- The controls, each drawn on `base`: the part of the screen under it ----

STATES = ("Up", "Lit", "Down", "Grey")
RADIUS = a(2.4)


def fit_font(words, w, h, most):
    """A condensed size for `words` that fits w x h pixels, at most `most` (original px)."""
    size = most
    while size > 6:
        f = bahn(size)
        if width_of(words, f) <= w * 0.82 and a(size) <= h * 0.86:
            return f
        size -= 0.5
    return bahn(6)


def centred_text(img, words, f, colour, left=None):
    ascent, descent = f.getmetrics()
    x = left if left is not None else (img.width - width_of(words, f)) / 2
    text(img, words, x, (img.height + ascent - descent) / 2, f, colour, italic=True)


def button_states(base, words, primary=False):
    """A button: a pill of faint glass with a fine edge and its word, across the middle of its
    box however tall that is; the blue bar when chosen. A primary one is blue at rest."""
    w, h = base.size
    tall = min(h - a(2), a(20))
    f = fit_font(words, w - a(8), tall, 13)
    box = (a(1), (h - tall) // 2, w - a(2), tall)
    pictures = {}
    for state in STATES:
        img = base.copy()
        if state in ("Lit", "Down") or (primary and state == "Up"):
            lit_bar(img, box, "Lit" if primary and state == "Up" else state, RADIUS)
            ink = (255, 255, 255) if state != "Down" else PALE
        else:
            glass(img, box, RADIUS, 0.22, EDGE if state == "Up" else None)
            ink = CREAM if state == "Up" else DIM
        centred_text(img, words, f, ink)
        pictures[state] = img
    return pictures


def chevron(img, way, colour):
    w, h = img.size
    r = min(w, h) * 0.18
    cx, cy = w / 2, h / 2
    points = {"up": [(cx - r, cy + r / 2), (cx, cy - r / 2), (cx + r, cy + r / 2)],
              "down": [(cx - r, cy - r / 2), (cx, cy + r / 2), (cx + r, cy - r / 2)],
              "left": [(cx + r / 2, cy - r), (cx - r / 2, cy), (cx + r / 2, cy + r)],
              "right": [(cx - r / 2, cy - r), (cx + r / 2, cy), (cx - r / 2, cy + r)]}[way]
    ImageDraw.Draw(img).line(points, fill=colour + (255,), width=max(a(0.9), 2), joint="curve")


def arrow_states(base, way):
    """A chevron on a glass square; on the blue bar when chosen."""
    w, h = base.size
    pictures = {}
    for state in STATES:
        img = base.copy()
        box = (a(1), a(1), w - a(2), h - a(2))
        if state in ("Lit", "Down"):
            lit_bar(img, box, state, RADIUS)
            chevron(img, way, (255, 255, 255))
        else:
            glass(img, box, RADIUS, 0.22, EDGE if state == "Up" else None)
            chevron(img, way, GLOW if state == "Up" else DIM)
        pictures[state] = img
    return pictures


def field(base, edge=None):
    """A darker well with a fine edge for lists, edit boxes and text, a blue rule along the
    bottom with `edge`."""
    img = base.copy()
    w, h = img.size
    glass(img, (0, 0, w, h), a(1.6), 0.3, EDGE)
    if edge:
        paint(img, rounded((w - a(4), max(a(0.6), 2)), 1), edge, (a(2), h - max(a(0.6), 2) - a(1)))
    return img


def drop_list(base):
    """An open list: glass nearly solid, so what it covers hardly shows, with a fine edge."""
    img = base.copy()
    glass(img, (0, 0) + img.size, a(1.6), 0.9, EDGE)
    return img


def track(base):
    """A scroll bar's track: a fine rounded groove."""
    img = base.copy()
    w, h = img.size
    long_way = w >= h
    size = (w - a(2), max(a(0.8), 2)) if long_way else (max(a(0.8), 2), h - a(2))
    at = (a(1), (h - size[1]) // 2) if long_way else ((w - size[0]) // 2, a(1))
    paint(img, rounded(size, 1), EDGE, at)
    return img


def thumb(base):
    img = base.copy()
    w, h = img.size
    inset = max(min(w, h) // 5, 1)
    paint(img, rounded((w - 2 * inset, h - 2 * inset), min(w, h) // 3), BLUE, (inset, inset))
    return img


def radio(base, on, enabled):
    """A radio button's mark at its left: a blue rounded square when on, an outline when off.
    The game draws its text."""
    img = base.copy()
    side = min(img.height - a(2), a(9))
    top = (img.height - side) // 2
    if on:
        lit_bar(img, (a(1), top, side, side), "Lit" if enabled else "Down", a(1.6))
        dot = side // 3
        paint(img, rounded((dot, dot), dot // 2), (255, 255, 255) if enabled else PALE,
              (a(1) + (side - dot) // 2, top + (side - dot) // 2))
    else:
        glass(img, (a(1), top, side, side), a(1.6), 0.4, SOFT if enabled else DIM)
    return img


def plate(base):
    """A group's plate: a little more glass, a blue pill at its left."""
    img = base.copy()
    w, h = img.size
    glass(img, (0, 0, w, h), RADIUS, 0.25)
    paint(img, rounded((max(a(1.2), 3), h - a(6)), a(0.6)), BLUE, (a(1.5), a(3)))
    return img


# ---- A screen's ground ----

def full_backdrop(size, side, title):
    """A full screen: the photo, glass at the left with the title on it, and the 4:3 on a glass
    panel in the middle."""
    img = clear(size)
    w, h = size
    paint(img, Image.new("L", (side, h), 255), (0, 0, 0), (0, 0), 0.45)
    glass(img, (side + a(2), a(2), w - 2 * side - a(4), h - a(4)), a(4), PANEL)
    if title:
        side_title(img, side, title)
    footer = "OPENMCM2  ·  DEVELOPMENT BUILD"
    f = art.font(13, style="SemiBold Condensed")
    text(img, footer, w - art.s(28), h - art.s(26), f, SOFT, right=True, spacing=0.6)
    return img


def side_title(img, side, title):
    """The game's name small, then the screen's large in cream, its last word blue."""
    paint(img, rounded((art.s(4), art.s(22)), art.s(2)), BLUE, (art.s(34), art.s(58)))
    text(img, "MOTOCROSS MADNESS 2", art.s(48), art.s(76), art.font(15, style="SemiBold Condensed"),
         SOFT, spacing=1.2)
    words = title.split()
    size = 76
    while size > 30 and max(width_of(word, art.font(size)) for word in words) > side - art.s(60):
        size -= 2
    f = art.font(size)
    y = art.s(112) + f.getmetrics()[0]
    for i, word in enumerate(words):
        text(img, word, art.s(32), y, f, BLUE if i == len(words) - 1 and len(words) > 1 else CREAM,
             italic=True)
        y += int(f.getmetrics()[0] * 1.02)


def card_backdrop(size, opacity):
    """A screen inside another, or a message: glass edge to edge (no rounded corners, which
    would show the photo through the screen it sits in), a blue rule along its top, and a fine
    edge round a message."""
    img = clear(size)
    w, h = size
    paint(img, Image.new("L", size, 255), (0, 0, 0), (0, 0), opacity)
    if opacity == POPUP:                            # a message floats: a fine edge round it
        paint(img, rim(Image.new("L", size, 255)), EDGE)
    accent = Image.new("L", (w, max(a(0.7), 2)))
    accent.putdata([int(255 * max(0.0, 1 - x / (w * 0.6))) for x in range(w)] * accent.height)
    paint(img, accent, BLUE)
    return img


# ---- A screen ----

def scale_xywh(text_value, shift=0):
    """An Xywh at ART times, moved `shift` pixels across."""
    n = [a(v) for v in numbers(text_value)]
    if n:
        n[0] += shift
    return ", ".join(str(v) for v in n)


def theme(dat, globals_, global_anims, preview_dir, photo):
    files = rs2_files(dat.read_bytes())
    entries = {n.lower(): blob for n, blob in files}
    layout = next(n for n, _ in files if n.lower().endswith(".dtm"))
    sections = read_dtm(entries[layout.lower()].decode("latin-1"))
    info = dict(sections).get("Set_Information", [])
    width = int(value(info, "ScreenWidth") or 640)
    height = int(value(info, "ScreenHeight") or 480)
    background = value(info, "BackgroundImage")
    has_back = bool(background) and background.lower() != "none"
    full = width >= 640 and height >= 480 and has_back
    stem = dat.stem.upper()
    side = SIDE if full else 0
    size = (a(width) + 2 * side, a(height))
    anims = dict(global_anims, **{n.lower(): l for n, l in sections})

    pictures, keys, drops = {}, {}, {}
    if full:
        ground = full_backdrop(size, side, TITLES.get(stem))
    else:
        # A screen drawn in another with no ground of its own is on that one's glass.
        ground = card_backdrop(size, POPUP if POPUPS.match(stem) else PANEL)
    if has_back:
        pictures["MBack"] = ground.copy()
    info_keys = {"ScreenWidth": str(size[0]), "ScreenHeight": str(size[1]), "ArtScale": str(ART),
                 "FontName": '"Bahnschrift"', "TextColor": colorref(CREAM)}
    if full:
        info_keys["Photos"] = "1"
    if has_back:
        info_keys["BackgroundImage"] = '"MBack"'
    if value(info, "Xywh"):
        info_keys["Xywh"] = scale_xywh(value(info, "Xywh"))
    if value(info, "FontHeight"):
        info_keys["FontHeight"] = "%g" % (float(value(info, "FontHeight")) * ART)
    keys["Set_Information"] = info_keys

    placed = []                                     # (picture, place) for the preview
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
        at = (a(ox + x) + side, a(oy + y))

        def base(of=None, offset=(0, 0)):
            """The screen under the control, `of` (w, h) or its own size, `offset` into it."""
            bw, bh = of or (a(w), a(h))
            left, top = at[0] + offset[0], at[1] + offset[1]
            return ground.crop((left, top, left + max(bw, 1), top + max(bh, 1)))

        k = {"Xywh": scale_xywh(value(lines, "Xywh"), 0 if anchor else side)}
        if value(lines, "FontHeight"):
            k["FontHeight"] = "%g" % (float(value(lines, "FontHeight")) * ART)
        if value(lines, "TextColor") is not None:
            k["TextColor"] = colorref(CREAM)
        if value(lines, "TextDrop") is not None:
            k["TextDrop"] = "0"
        if value(lines, "SelectColor") is not None:
            k["SelectColor"] = colorref(GLOW)
        has = lambda key: (value(lines, key) or "0") not in ("0", "")   # noqa: E731
        made = {}

        def original(key):
            """The size of the original picture the control's `key` names, or None."""
            found = has(key) and picture_size(entries, globals_, anims, value(lines, key))
            return (a(found[0]), a(found[1])) if found else None

        if kind == "BUTTON" and has("AnimNorm"):
            made = {"AnimNorm": "Up", "AnimFocus": "Lit", "AnimPush": "Down", "AnimDisable": "Grey"}
            way = direction_of(name)
            if way and w < 2.4 * h:
                states = arrow_states(base(), way)
            else:
                states = button_states(base(), label_of(name, lines), bool(PRIMARY.search(name)))
            drops[name] = ("Text", "LocalTextId")
            for state in made.values():
                pictures["M%s%s" % (name, state)] = states[state]
        elif kind in ("SCROLLUP", "SCROLLDOWN") and has("AnimNorm"):
            made = {"AnimNorm": "Up", "AnimFocus": "Lit", "AnimPush": "Down", "AnimDisable": "Grey"}
            states = arrow_states(base(), "up" if kind == "SCROLLUP" else "down")
            for state in made.values():
                pictures["M%s%s" % (name, state)] = states[state]
        elif kind == "RADIOBUTTON" and has("AnimOn"):
            made = {"AnimOn": "On", "AnimOff": "Off", "AnimOnDisable": "OnGrey", "AnimOffDisable": "OffGrey"}
            for key, state in made.items():
                pictures["M%s%s" % (name, state)] = radio(base(original(key)), state.startswith("On"),
                                                          "Grey" not in state)
        elif kind in ("VSCROLLBAR", "HSCROLLBAR"):
            for key, maker in (("AnimNorm", track), ("AnimThumb", thumb), ("AnimFocus", track),
                               ("AnimPush", track), ("AnimDisable", track)):
                if has(key):
                    made[key] = key[4:]
                    of = original(key) if key == "AnimThumb" else None
                    pictures["M%s%s" % (name, key[4:])] = maker(base(of))
        elif kind == "DROPDOWNLIST":
            # The text's field, the arrow button after it, and the list that opens under it with
            # its scroll bar down the right: each at the original picture's size.
            text_size = original("AnimTextBack") or (a(w), a(h))
            arrow_size = original("AnimNorm") or (max(a(w) - text_size[0], text_size[1]), text_size[1])
            list_size = original("AnimListBack") or (text_size[0], a(80))
            scroll_size = original("AnimScrollBack") or (a(21), list_size[1])
            thumb_size = original("AnimThumb") or (scroll_size[0], scroll_size[0])
            arrows = arrow_states(base(arrow_size, (text_size[0], 0)), "down")
            for key, state in (("AnimNorm", "Up"), ("AnimFocus", "Lit"), ("AnimPush", "Down"),
                               ("AnimDisable", "Grey")):
                if has(key):
                    made[key] = key[4:]
                    pictures["M%s%s" % (name, key[4:])] = arrows[state]
            listing = drop_list(base(list_size, (0, text_size[1])))
            scroll = track(listing.crop((list_size[0] - scroll_size[0], 0) + list_size))
            for key, picture in (("AnimTextBack", field(base(text_size), BLUE)), ("AnimListBack", listing),
                                 ("AnimScrollBack", scroll),
                                 ("AnimThumb", thumb(scroll.crop((0, 0) + thumb_size)))):
                if has(key):
                    made[key] = key[4:]
                    pictures["M%s%s" % (name, key[4:])] = picture
            if has("AnimTextBack"):
                placed.append(("M%sTextBack" % name, at))
            if has("AnimNorm"):
                placed.append(("M%sNorm" % name, (at[0] + text_size[0], at[1])))
        elif kind in ("LISTBOX", "EDITBOX", "STATICTEXT") and has("AnimNorm"):
            made = {"AnimNorm": "Back"}
            pictures["M%sBack" % name] = field(base(), BLUE if kind == "EDITBOX" else None)
        elif kind == "STATIC" and has("AnimNorm") and PLATE_NAME.search(name):
            made = {"AnimNorm": "Plate"}
            pictures["M%sPlate" % name] = plate(base())
            ground.paste(pictures["M%sPlate" % name], at)      # what is on the plate is drawn on it
            for fx in ("FXAnimIn", "FXAnimOut"):
                if has(fx):
                    k[fx] = '"M%sPlate"' % name
        elif kind == "STATIC" and has("AnimNorm"):
            # Other decoration: none.
            drops[name] = ("AnimNorm", "FXAnimIn", "FXAnimOut")
        for key, state in made.items():
            k[key] = '"M%s%s"' % (name, state)
        keys[name] = k
        if kind != "DROPDOWNLIST" and ("AnimNorm" in made or "AnimOff" in made):
            first = "M%s%s" % (name, made.get("AnimNorm") or made.get("AnimOff"))
            placed.append((first, at))

    out = []
    for name, lines in sections:
        if name in keys:
            lines = set_keys(lines, keys[name], drops.get(name, ()))
        out += ["[%s]" % name] + [line for line in lines if line.strip()]
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
        # As the game shows it: over a photo, each picture drawn over what is under it.
        canvas = Image.open(photo).convert("RGBA").resize((a(640) + 2 * SIDE, a(480)), Image.LANCZOS)
        canvas = canvas.crop((0, 0) + size) if not full else canvas
        layer = shown(pictures["MBack"]) if "MBack" in pictures else shown(ground)
        for anim, at in placed:
            layer.paste(shown(pictures[anim]), at)
        canvas.alpha_composite(layer)
        canvas.convert("RGB").save(Path(preview_dir) / ("modern %s.png" % stem.lower()))
    return rs2(new_entries), len(pictures)


def main(ui, out, preview_dir):
    ui, out = Path(ui), Path(out)
    globals_ = rs2_entries((ui / "GLOBAL.DAT").read_bytes())
    layout = next(blob for name, blob in globals_.items() if name.endswith(".dtm"))
    global_anims = {n.lower(): l for n, l in read_dtm(layout.decode("latin-1"))}
    out.mkdir(parents=True, exist_ok=True)
    photos = sorted(Path(preview_dir).parent.glob("menu photo *.jpg")) if preview_dir else []
    for n, dat in enumerate(sorted(ui.glob("*.DAT"))):
        stem = dat.stem.upper()
        if stem in SKIP or stem.startswith(SKIP_PREFIX):
            continue
        data, count = theme(dat, globals_, global_anims, preview_dir,
                            photos[n % len(photos)] if photos else None)
        (out / dat.name).write_bytes(data)
        print("%-13s %3d pictures, %d bytes" % (dat.name, count, len(data)))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else None)
