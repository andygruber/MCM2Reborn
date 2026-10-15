r"""The modern main menu: build_mainmenu.py <original MAINMENU.DAT> <out MAINMENU.DAT> <preview dir>

The menu over the photos (photos.py): the renderer draws them under it, crossfading, wherever
the menu's pictures hold HOLE. Rows are type on the photo, the chosen one a slanted blue bar
with a light blue glow; the less used things are small buttons at the bottom; the rider's card
is at the bottom right. The title is tools/modern/title.png when there is one.

Draws the menu at the art's size, cuts each control's pictures from it, rewrites the original
layout with the same controls at their new places, and packs it all as an RS2 archive the game
reads from ui\modern\. Places here are on the 1920x1080 grid.
"""
import struct
import subprocess
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

sys.path.insert(0, str(Path(__file__).parent))
from mainmenu_art import H, W, font, lean, s, text, width_of  # noqa: E402

FOLDER = "02_Modern\\"
HERE = Path(__file__).parent
VERSION = "0.1"

MAGENTA = (255, 0, 255)                 # the colour key: see-through and not clickable
MATTE = (12, 13, 16)                    # what soft edges are mixed with: the photos' dark left
CREAM = (236, 238, 242)
SOFT = (150, 156, 166)                  # the small lines under the rows
BLUE = (34, 110, 255)
BLUE_DEEP = (18, 70, 210)
BLUE_DOWN = (16, 56, 168)
GLOW = (110, 185, 255)
PALE = (205, 228, 255)
DIM = (92, 96, 104)
CARD = (17, 18, 22)
EDGE = (58, 62, 70)

# The main rows, top to bottom: control, title, what it holds.
MAIN_ITEMS = [
    ("MenuSingle", "SINGLE PLAYER", "BAJA · STUNT QUARRY · ENDURO · NATIONALS · SUPERCROSS"),
    ("MenuMulti", "MULTIPLAYER", "LOCAL NETWORK · INTERNET · MSN GAMING ZONE · MODEM"),
    ("MenuReplay", "REPLAYS", "WATCH YOUR SAVED RACES"),
    ("MenuHighScore", "HIGH SCORES", "BEST LAPS · BEST SCORES · PRO CIRCUIT"),
    ("MenuOptions", "OPTIONS", "CONTROLS · AUDIO · GRAPHICS"),
]
ROWS_X, ROWS_Y, ROW_W, ROW_H = 84, 330, 640, 92
CHIPS = [("MenuHelp", "HELP", (84, 820, 110, 44)), ("MenuCredits", "CREDITS", (204, 820, 140, 44)),
         ("Menuexit", "EXIT", (354, 820, 100, 44))]
PROFILE_CARD = (1290, 868, 560, 130)
CURRENT_PROFILE = (1318, 906, 470, 52)

# The pull-down panels: the panel control, its place, its title, and its buttons.
SINGLE = ("MenuSingleAnimation", (760, 330, 790, 420), "SELECT EVENT",
          [("ButBaja", "BAJA"), ("ButStuntQuarry", "STUNT QUARRY"), ("ButEnduro", "ENDURO"),
           ("ButNationals", "NATIONALS"), ("ButSupercross", "SUPERCROSS"),
           ("ButProCircuit", "PRO-CIRCUIT")])
MULTI = ("MenuMultiAnimation", (760, 422, 440, 400), "MULTIPLAYER",
         [("ButIPX", "LOCAL NETWORK"), ("ButTCP", "INTERNET  TCP/IP"), ("ButZone", "MSN GAMING ZONE"),
          ("ButCable", "SERIAL CABLE"), ("ButModem", "MODEM")])
USER = ("MenuUserAnimation", (1290, 410, 560, 440), "RIDER PROFILES",
        [("ButNewProfile", "NEW"), ("ButLoadProfile", "LOAD"), ("ButRemoveProfile", "REMOVE"),
         ("ButCancelProfile", "CLOSE")])
MOVIE_FRAME = (384, 70, 382, 280)           # ScreenOverCtl, on the single-player panel
PROFILE_LIST = (24, 70, 330, 340)
SCROLL_X, SCROLL_W = 360, 30


def px(box):
    """A box on the grid, in pixels."""
    return tuple(s(v) for v in box)


def build_version():
    """The version line's build: VERSION and the dev tree's commit."""
    try:
        commit = subprocess.run(["git", "rev-parse", "--short", "HEAD"], capture_output=True,
                                text=True, cwd=HERE, check=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return VERSION
    return "%s  %s" % (VERSION, commit)


# ---- TGA and RS2 ----

HOLE = 0x7C1E                           # x1R5G5B5 31, 0, 30: where the photos show
KEY = 0x7C1F


def tga16(image):
    """An opaque 16-bit TGA (x1R5G5B5, bottom up). Clear pixels become HOLE, soft ones are
    mixed with MATTE, and the colour key (opaque magenta) stays the key."""
    rgba = np.asarray(image.convert("RGBA"), dtype=np.float64)
    alpha = rgba[..., 3:4] / 255.0
    rgb = rgba[..., :3] * alpha + np.array(MATTE) * (1 - alpha)
    five = np.clip(np.round(rgb / 255.0 * 31), 0, 31).astype(np.uint16)
    pixels = (five[..., 0] << 10) | (five[..., 1] << 5) | five[..., 2]
    pixels = np.where((pixels == HOLE) | (pixels == KEY), 0x7C1D, pixels)
    keyed = (rgba[..., 3] == 255) & (rgba[..., :3] == MAGENTA).all(-1)
    pixels = np.where(keyed, KEY, np.where(rgba[..., 3] < 32, HOLE, pixels))   # the faintest light, see-through
    h, w = pixels.shape
    header = struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, w, h, 16, 0)
    return header + pixels[::-1].astype("<u2").tobytes()


def rs2(entries):
    """The archive: RS2, the count, each entry's name, offset and size, then their bytes."""
    names = [name.encode("latin-1") + b"\0" for name, _ in entries]
    at = 8 + sum(4 + len(n) + 8 for n in names)
    table, blobs = [], []
    for name, (_, blob) in zip(names, entries):
        table.append(struct.pack("<I", len(name)) + name + struct.pack("<II", at, len(blob)))
        blobs.append(blob)
        at += len(blob)
    return b"RS2\0" + struct.pack("<I", len(entries)) + b"".join(table) + b"".join(blobs)


def rs2_files(data):
    """The archive's entries as (name, bytes), the names as they are written."""
    count, = struct.unpack_from("<I", data, 4)
    at, found = 8, []
    for _ in range(count):
        length, = struct.unpack_from("<I", data, at)
        name = data[at + 4:at + 4 + length].rstrip(b"\0").decode("latin-1")
        offset, size = struct.unpack_from("<II", data, at + 4 + length)
        found.append((name, data[offset:offset + size]))
        at += 4 + length + 8
    return found


def rs2_entries(data):
    """The archive's entries by lower-case name."""
    return {name.lower(): blob for name, blob in rs2_files(data)}


# ---- The art ----

def clear(size):
    return Image.new("RGBA", size, (0, 0, 0, 0))


def glow(canvas, mask, colour, radius, strength):
    """A soft light of `colour` round `mask` (same size as canvas)."""
    halo = mask.filter(ImageFilter.GaussianBlur(s(radius))).point(lambda v: min(255, int(v * strength)))
    layer = Image.new("RGBA", canvas.size, colour + (0,))
    layer.putalpha(halo)
    canvas.alpha_composite(layer)


def slant_box(w, h, slant):
    """A parallelogram leaning forward, as a mask w x h."""
    mask = Image.new("L", (w, h))
    ImageDraw.Draw(mask).polygon([(slant, 0), (w, 0), (w - slant, h), (0, h)], fill=255)
    return mask


def chosen_bar(canvas, box, fill_from, fill_to, shine=True):
    """The chosen row's bar on `canvas` (pixels): a blue gradient parallelogram, its light."""
    x, y, w, h = box
    slant = int(h * 0.22)
    mask = Image.new("L", canvas.size)
    mask.paste(slant_box(w, h, slant), (x, y))
    if shine:
        glow(canvas, mask, GLOW, 12, 1.1)
    ramp = np.linspace(0, 1, w)[None, :, None]
    bar = (np.array(fill_from) * (1 - ramp) + np.array(fill_to) * ramp).repeat(h, 0)
    layer = clear(canvas.size)
    layer.paste(Image.fromarray(bar.astype(np.uint8)).convert("RGBA"), (x, y))
    layer.putalpha(mask)
    canvas.alpha_composite(layer)
    if shine:
        edge = Image.new("L", canvas.size)
        ImageDraw.Draw(edge).line([(x + slant, y), (x + w, y)], fill=200, width=max(s(2), 2))
        light = Image.new("RGBA", canvas.size, PALE + (0,))
        light.putalpha(edge)
        canvas.alpha_composite(light)


def shadowed(canvas, words, x, y, f, colour, italic=True, spacing=0):
    """Type with a soft dark shadow, to read over the photos."""
    layer = clear(canvas.size)
    text(layer, words, x + s(2), y + s(3), f, (0, 0, 0), italic=italic, spacing=spacing)
    shade = layer.split()[3].filter(ImageFilter.GaussianBlur(s(4))).point(lambda v: int(v * 0.8))
    black = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    black.putalpha(shade)
    canvas.alpha_composite(black)
    text(canvas, words, x, y, f, colour, italic=italic, spacing=spacing)


def title(canvas):
    """The title: tools/modern/title.png fitted to its place, or the game's name in type."""
    place = px((70, 40, 700, 250))
    made = HERE / "title.png"
    if made.exists():
        art = Image.open(made).convert("RGBA")
        art.thumbnail((place[2], place[3]), Image.LANCZOS)
        canvas.alpha_composite(art, (place[0], place[1]))
        return
    shadowed(canvas, "MOTOCROSS", s(76), s(150), font(128), CREAM)
    shadowed(canvas, "MADNESS", s(76), s(262), font(128), CREAM)
    shadowed(canvas, "2", s(76) + int(width_of("MADNESS", font(128))) + s(26), s(266), font(140), BLUE)


def background():
    canvas = clear((W, H))
    title(canvas)
    small = font(15, style="SemiBold Condensed")
    shadowed(canvas, "OPEN-SOURCE REIMPLEMENTATION  ·  DEVELOPMENT BUILD " + build_version()
             + "  ·  NOT AFFILIATED WITH OR ENDORSED BY THE ORIGINAL CREATORS",
             s(84), s(1046), small, SOFT, italic=False, spacing=0.6)
    return canvas


def crop(image, box):
    x, y, w, h = px(box)
    return image.crop((x, y, x + w, y + h))


def row_states(box, words, line=None, size=58):
    """A row's four pictures: type on the photo at rest, a blue bar with a glow when chosen."""
    x, y, w, h = px(box)
    pad = s(16)
    big = font(size)
    small = font(15, style="SemiBold Condensed")
    pictures = {}
    for state in ("Up", "Lit", "Down", "Grey"):
        canvas = clear((w + 2 * pad, h + 2 * pad))
        if state in ("Lit", "Down"):
            chosen_bar(canvas, (pad, pad + s(4), w, h - s(8)),
                       BLUE if state == "Lit" else BLUE_DOWN,
                       BLUE_DEEP if state == "Lit" else BLUE_DOWN, shine=state == "Lit")
        ink = {"Up": CREAM, "Lit": (255, 255, 255), "Down": PALE, "Grey": DIM}[state]
        under = {"Up": SOFT, "Lit": PALE, "Down": PALE, "Grey": DIM}[state]
        base = pad + (h // 2 + s(size * 0.30) if not line else h // 2 + s(6))
        shadowed(canvas, words, pad + s(26), base, big, ink)
        if line:
            shadowed(canvas, line, pad + s(30), base + s(26), small, under, italic=False, spacing=0.8)
        pictures[state] = canvas.crop((pad, pad, pad + w, pad + h))
    return pictures


def chip_states(box, words):
    """A small button: dark, edged, its word; blue and lit when chosen."""
    x, y, w, h = px(box)
    f = font(17, style="SemiBold Condensed")
    pictures = {}
    for state in ("Up", "Lit", "Down", "Grey"):
        canvas = Image.new("RGBA", (w, h), CARD + (255,))
        d = ImageDraw.Draw(canvas)
        if state == "Lit":
            d.rectangle((0, 0, w - 1, h - 1), fill=BLUE_DEEP + (255,))
            d.rectangle((0, 0, w - 1, h - 1), outline=GLOW + (255,), width=max(s(1.5), 2))
        elif state == "Down":
            d.rectangle((0, 0, w - 1, h - 1), fill=BLUE_DOWN + (255,))
        else:
            d.rectangle((0, 0, w - 1, h - 1), outline=EDGE + (255,), width=max(s(1), 1))
        ink = {"Up": CREAM, "Lit": (255, 255, 255), "Down": PALE, "Grey": DIM}[state]
        text(canvas, words, (w - int(width_of(words, f, 1))) // 2, h // 2 + s(6), f, ink, spacing=1)
        pictures[state] = canvas
    return pictures


def profile_states(box):
    """The rider card: its label and "change rider"; the name is the game's own text."""
    x, y, w, h = px(box)
    label = font(15, style="SemiBold Condensed")
    pictures = {}
    for state in ("Up", "Lit", "Down", "Grey"):
        canvas = Image.new("RGBA", (w, h), CARD + (255,))
        d = ImageDraw.Draw(canvas)
        d.rectangle((0, 0, s(4) - 1, h - 1), fill=BLUE + (255,))
        if state in ("Lit", "Down"):
            d.rectangle((0, 0, w - 1, h - 1), outline=GLOW + (255,), width=max(s(2), 2))
        text(canvas, "RIDER", s(28), s(30), label, BLUE if state != "Lit" else GLOW, spacing=1.2)
        change = "CHANGE RIDER  ›"
        text(canvas, change, w - s(28) - int(width_of(change, label, 1)), h - s(20), label,
             PALE if state == "Lit" else SOFT, spacing=1)
        pictures[state] = canvas
    return pictures


def panel_card(panel):
    """A pull-down panel's card: dark, a blue edge, its label at the top."""
    _, (x, y, w, h), label, _ = panel
    canvas = Image.new("RGBA", px((0, 0, w, h))[2:], CARD + (255,))
    d = ImageDraw.Draw(canvas)
    d.rectangle((0, 0, s(4) - 1, canvas.height - 1), fill=BLUE + (255,))
    d.rectangle((0, 0, canvas.width - 1, canvas.height - 1), outline=EDGE + (255,), width=1)
    text(canvas, label, s(28), s(42), font(16, style="SemiBold Condensed"), BLUE, spacing=1.2)
    return canvas


def panel_row_states(card, box, words):
    """A panel's row: its word at rest, the blue bar when chosen; on the panel's card."""
    x, y, w, h = px(box)
    f = font(30)
    pictures = {}
    for state in ("Up", "Lit", "Down", "Grey"):
        canvas = card.crop((x, y, x + w, y + h)).convert("RGBA")
        if state in ("Lit", "Down"):
            chosen_bar(canvas, (0, s(3), w, h - s(6)), BLUE if state == "Lit" else BLUE_DOWN,
                       BLUE_DEEP if state == "Lit" else BLUE_DOWN, shine=False)
        ink = {"Up": CREAM, "Lit": (255, 255, 255), "Down": PALE, "Grey": DIM}[state]
        text(canvas, words, s(22), h // 2 + s(11), f, ink, italic=True)
        pictures[state] = canvas
    return pictures


def scroll_arrow(size, up, colour):
    img = Image.new("RGBA", size, (0, 0, 0, 0))
    w, h = size
    points = [(w * 0.28, h * 0.62), (w * 0.5, h * 0.38), (w * 0.72, h * 0.62)]
    if not up:
        points = [(x, h - y) for x, y in points]
    ImageDraw.Draw(img).line(points, fill=colour + (255,), width=max(s(2.5), 2), joint="curve")
    return img


# ---- The layout ----

def read_dtm(text_):
    """The dtm's sections in order: [name, [lines]]."""
    sections = []
    for line in text_.replace("\r\n", "\n").split("\n"):
        if line.startswith("["):
            sections.append([line.strip()[1:-1], []])
        elif sections:
            sections[-1][1].append(line)
    return sections


def key_of(line):
    return line.split("=", 1)[0].strip().lower() if "=" in line else None


def set_keys(lines, values, drop=()):
    """`lines` with each of `values` set (added when missing) and the keys in `drop` removed."""
    out, seen = [], set()
    drop = {d.lower() for d in drop}
    for line in lines:
        key = key_of(line)
        if key in drop:
            continue
        match = next((k for k in values if k.lower() == key), None)
        if match:
            out.append("%s = %s" % (match, values[match]))
            seen.add(match)
        elif line.strip():
            out.append(line)
    out += ["%s = %s" % (k, v) for k, v in values.items() if k not in seen]
    return out


def xywh(box):
    return "%d, %d, %d, %d" % px(box)


def anims(name):
    return {"AnimNorm": '"M%sUp"' % name, "AnimFocus": '"M%sLit"' % name,
            "AnimPush": '"M%sDown"' % name, "AnimDisable": '"M%sGrey"' % name}


TEXT_KEYS = ("Text", "LocalTextId", "Font_UseDefault", "FontName", "FontHeight", "Charset", "Bold",
             "Underline", "Italic", "Strikethrough")
PLACED = {}                                     # where each button is on the grid, for previews


def put(pictures, controls, name, box, states, anchor=None, origin=(0, 0)):
    """A button's pictures and keys; `box` is on the grid, `origin` its panel's corner."""
    for state, img in states.items():
        pictures["M%s%s" % (name, state)] = img
    keys = dict(anims(name), Xywh=xywh((box[0] - origin[0], box[1] - origin[1], box[2], box[3])))
    if anchor:
        keys.update(Anchor=anchor, RelAnchor="1")
    controls[name] = keys
    PLACED[name] = box[:2]


def user_panel(card, controls, pictures, name):
    """The profile list's frame, its scroll arrows and bar, on the profiles panel's card."""
    lx, ly, lw, lh = PROFILE_LIST
    ImageDraw.Draw(card).rectangle(px((lx, ly, lx + lw, ly + lh)), outline=EDGE + (255,),
                                   width=max(s(1), 1))
    controls["LstProfiles"] = {"Xywh": xywh((lx + 12, ly + 8, lw - 24, lh - 16)), "Anchor": name,
                               "RelAnchor": "1", "TextAlign": "LEFT", "TextDrop": "0",
                               "TextColor": str(0xF2EEEC)}
    up_box = (SCROLL_X, ly, SCROLL_W, 34)
    down_box = (SCROLL_X, ly + lh - 34, SCROLL_W, 34)
    bar_box = (SCROLL_X, ly + 36, SCROLL_W, lh - 72)
    for control, (bx, by, bw, bh), up in (("ProfileScrollUp", up_box, True),
                                          ("ProfileScrollDown", down_box, False)):
        base = crop(card, (bx, by, bw, bh))
        for state, colour in (("Up", BLUE), ("Lit", GLOW), ("Down", PALE), ("Grey", DIM)):
            img = base.copy()
            img.alpha_composite(scroll_arrow(img.size, up, colour))
            pictures["M%s%s" % (control, state)] = img
        controls[control] = dict(anims(control), Xywh=xywh((bx, by, bw, bh)), Anchor=name,
                                 RelAnchor="1")
    bar = crop(card, bar_box)
    ImageDraw.Draw(bar).line([(bar.width // 2, 0), (bar.width // 2, bar.height)], fill=EDGE + (255,),
                             width=max(s(1), 1))
    pictures["MProfileBar"] = bar
    thumb = Image.new("RGBA", (s(SCROLL_W), s(40)), CARD + (255,))
    ImageDraw.Draw(thumb).rectangle((s(9), s(2), s(SCROLL_W - 10), s(37)), fill=BLUE + (255,))
    pictures["MProfileThumb"] = thumb
    controls["SldProfile"] = {"Xywh": xywh(bar_box), "AnimNorm": '"MProfileBar"',
                              "AnimThumb": '"MProfileThumb"', "Anchor": name, "RelAnchor": "1"}


def build(original_dat):
    pictures = {}                               # anim name -> image
    controls = {}                               # section -> keys to set
    drops = {}

    bg = background()
    pictures["MBackground"] = bg
    controls["Set_Information"] = {"BackgroundImage": '"MBackground"', "ScreenWidth": str(W),
                                   "ScreenHeight": str(H), "Xywh": xywh((0, 0, 1920, 1080)),
                                   "FontName": '"Bahnschrift"', "FontHeight": str(s(26)),
                                   "Photos": "1"}

    for i, (name, words, line) in enumerate(MAIN_ITEMS):
        box = (ROWS_X, ROWS_Y + i * ROW_H, ROW_W, ROW_H)
        put(pictures, controls, name, box, row_states(box, words, line))
    for name, words, box in CHIPS:
        put(pictures, controls, name, box, chip_states(box, words))
    put(pictures, controls, "MenuUserProfile", PROFILE_CARD, profile_states(PROFILE_CARD))
    controls["TxtCurrentProfile"] = {"Xywh": xywh(CURRENT_PROFILE), "TextColor": str(0xF2EEEC),
                                     "TextAlign": "LEFT", "TextDrop": "0",
                                     "FontName": '"Bahnschrift SemiBold Condensed"',
                                     "FontHeight": str(s(40)), "Bold": "700"}

    for panel in (SINGLE, MULTI, USER):
        name, (x, y, w, h), _, buttons = panel
        card = panel_card(panel)
        if panel is USER:
            user_panel(card, controls, pictures, name)
            column = [(380, 70 + i * 64, 160, 54) for i in range(len(buttons))]
        else:
            step = 56 if panel is SINGLE else 62
            width = 340 if panel is SINGLE else w - 48
            column = [(24, 70 + i * step, width, step - 4) for i in range(len(buttons))]
        pictures["M%sPanel" % name] = card
        controls[name] = {"Xywh": xywh((x, y, w, h)), "AnimNorm": '"M%sPanel"' % name,
                          "FXAnimIn": '"M%sPanel"' % name, "FXAnimOut": '"M%sPanel"' % name}
        for (button, words), (bx, by, bw, bh) in zip(buttons, column):
            put(pictures, controls, button, (x + bx, y + by, bw, bh),
                panel_row_states(card, (bx, by, bw, bh), words), anchor=name, origin=(x, y))
            drops[button] = TEXT_KEYS
        if panel is SINGLE:                     # the events movie's frame, its window keyed out
            fx, fy, fw, fh = MOVIE_FRAME
            frame = crop(card, MOVIE_FRAME)
            d = ImageDraw.Draw(frame)
            d.rectangle((0, 0, frame.width - 1, frame.height - 1), outline=BLUE + (255,), width=s(2))
            d.rectangle((s(4), s(4), frame.width - s(5), frame.height - s(5)), fill=MAGENTA + (255,))
            pictures["MScreenOver"] = frame
            controls["ScreenOverCtl"] = {"Xywh": xywh(MOVIE_FRAME), "AnimNorm": '"MScreenOver"',
                                         "Anchor": name, "RelAnchor": "1"}

    original = rs2_entries(original_dat)
    sections = read_dtm(original["mainmenu.dtm"].decode("latin-1"))
    missing = [c for c in controls if c not in {n for n, _ in sections}]
    assert not missing, missing

    # The original's sections stay in their order, its anims and sounds among them; the new
    # pictures go in at the start of Set_Anim, and the original's files stay in the archive.
    out = []
    for name, lines in sections:
        if name == "rbwLabel1":
            continue
        if name in controls:
            lines = set_keys(lines, controls[name], drops.get(name, ()))
        out += ["[%s]" % name] + [l for l in lines if l.strip()] + [""]
        if name.lower() == "set_anim":
            for anim in pictures:
                out += ["[%s]" % anim, "Files1 = %s%s.tga" % (FOLDER, anim), "MSecDelay = 0",
                        "LoopCount = 0", ""]

    entries = [(n, blob) for n, blob in rs2_files(original_dat) if not n.lower().endswith(".dtm")]
    entries += [(FOLDER + anim + ".tga", tga16(img)) for anim, img in pictures.items()]
    entries.append(("MainMenu.dtm", "\r\n".join(out).encode("latin-1")))
    return rs2(entries), bg, pictures


def preview(bg, pictures, lit, panel, photo):
    """The menu as it would show over `photo`: every button at rest, `lit` lit, `panel` open."""
    canvas = Image.open(photo).convert("RGBA").resize((W, H))
    layer = bg.copy()
    shown = [item[0] for item in MAIN_ITEMS] + ["MenuUserProfile"] + [c[0] for c in CHIPS]
    if panel:
        name, (x, y, _, _), _, buttons = panel
        layer.alpha_composite(pictures["M%sPanel" % name], (s(x), s(y)))
        shown += [button for button, _ in buttons]
        if panel is SINGLE:
            layer.alpha_composite(pictures["MScreenOver"], (s(x + MOVIE_FRAME[0]), s(y + MOVIE_FRAME[1])))
    for name in shown:
        gx, gy = PLACED[name]
        picture = pictures["M%s%s" % (name, "Lit" if name == lit else "Up")]
        spot = layer.crop((s(gx), s(gy), s(gx) + picture.width, s(gy) + picture.height))
        spot.alpha_composite(picture)
        layer.paste(spot, (s(gx), s(gy)))
    text(layer, "ROOKIE", s(CURRENT_PROFILE[0]), s(CURRENT_PROFILE[1] + 38),
         font(40, style="SemiBold Condensed"), CREAM)
    canvas.alpha_composite(layer)
    rgb = np.asarray(canvas.convert("RGB")).copy()
    rgb[(rgb == MAGENTA).all(-1)] = (8, 9, 12)          # where the events movie plays
    return Image.fromarray(rgb)


if __name__ == "__main__":
    data, bg, pictures = build(open(sys.argv[1], "rb").read())
    open(sys.argv[2], "wb").write(data)
    out = Path(sys.argv[3])
    photos = sorted(out.glob("menu photo *.jpg"))
    preview(bg, pictures, "MenuSingle", None, photos[2]).save(out / "modern mainmenu.png")
    preview(bg, pictures, "ButEnduro", SINGLE, photos[0]).save(out / "modern mainmenu - single open.png")
    preview(bg, pictures, "ButLoadProfile", USER, photos[3]).save(out / "modern mainmenu - profiles open.png")
    print("wrote", sys.argv[2], len(data), "bytes,", len(pictures), "pictures")
