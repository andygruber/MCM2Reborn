r"""The modern main menu: build_mainmenu.py <original MAINMENU.DAT> <out MAINMENU.DAT> <preview dir>

Draws the menu (mainmenu_art's look) at the art's size, cuts each control's pictures from it,
rewrites the original layout with the same controls at their new places, and packs it all as an
RS2 archive the game reads from ui\modern\. Places here are on the 1920x1080 grid.

Every picture is an opaque 16-bit TGA: the menus key out magenta (0xFF00FF) and nothing else,
so each button carries its own piece of the backdrop, drawn in each of its four states.
"""
import struct
import subprocess
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, str(Path(__file__).parent))
from mainmenu_art import (BLUE, CARD, CREAM, DIM, GREY, H, INK, ROW, ROW_LINE, W,  # noqa: E402
                          YELLOW, YELLOW_DOWN, arrow, backdrop, body, dash, exit_icon, font,
                          mono, rect, s, text, width_of)

FOLDER = "02_Modern\\"
MAGENTA = (255, 0, 255)
VERSION = "0.1"
NUMBER_BLUE = (52, 100, 158)    # a row's number at rest

# The main rows, top to bottom: control, title, tag, the line shown when it is chosen.
MAIN_ITEMS = [
    ("MenuSingle", "SINGLE PLAYER", "6 EVENTS", "Pick an event. Drop the gate. Leave your mark."),
    ("MenuMulti", "MULTIPLAYER", "LAN / ONLINE", "Race your friends on a network or online."),
    ("MenuHighScore", "HIGH SCORES", "RECORDS", "The fastest laps and the biggest scores."),
    ("MenuReplay", "REPLAYS", "VCR", "Watch your recorded races again."),
    ("MenuOptions", "OPTIONS", "SETUP", "Controls, graphics and sound."),
]
ROWS_X, ROWS_Y, ROW_W, ROW_H = 67, 430, 546, 84
EXIT = ("Menuexit", (67, ROWS_Y + ROW_H * 5 + 18, 300, 52))
CHIPS = [("MenuHelp", "HELP", (67, 990, 112, 40)), ("MenuCredits", "CREDITS", (191, 990, 150, 40))]
PROFILE_CARD = (1533, 52, 320, 112)
CURRENT_PROFILE = (1623, 84, 215, 34)

# The pull-down panels: the panel control, its place, its title, and its buttons.
SINGLE = ("MenuSingleAnimation", (640, ROWS_Y, 790, 420), "SELECT EVENT",
          [("ButBaja", "BAJA"), ("ButStuntQuarry", "STUNT QUARRY"), ("ButEnduro", "ENDURO"),
           ("ButNationals", "NATIONALS"), ("ButSupercross", "SUPERCROSS"),
           ("ButProCircuit", "PRO-CIRCUIT")])
MULTI = ("MenuMultiAnimation", (640, ROWS_Y, 430, 400), "MULTIPLAYER",
         [("ButIPX", "LOCAL NETWORK"), ("ButTCP", "INTERNET  TCP/IP"), ("ButZone", "MSN GAMING ZONE"),
          ("ButCable", "SERIAL CABLE"), ("ButModem", "MODEM")])
USER = ("MenuUserAnimation", (1153, 186, 700, 420), "RIDER PROFILES",
        [("ButNewProfile", "NEW"), ("ButLoadProfile", "LOAD"), ("ButRemoveProfile", "REMOVE"),
         ("ButCancelProfile", "CLOSE")])
MOVIE_FRAME = (384, 70, 382, 280)           # ScreenOverCtl, on the single-player panel
PROFILE_LIST = (24, 70, 400, 326)
SCROLL_X, SCROLL_W = 430, 30


def px(box):
    """A box on the grid, in pixels."""
    return tuple(s(v) for v in box)


def build_version():
    """The version line's build: VERSION and the dev tree's commit."""
    try:
        commit = subprocess.run(["git", "rev-parse", "--short", "HEAD"], capture_output=True,
                                text=True, cwd=Path(__file__).parent, check=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return VERSION
    return "%s  %s" % (VERSION, commit)


# ---- TGA and RS2 ----

BAYER = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]) / 16.0


def tga16(image):
    """An opaque 16-bit TGA (x1R5G5B5, bottom up), dithered so the gradient does not band."""
    rgb = np.asarray(image.convert("RGB"), dtype=np.float64)
    h, w, _ = rgb.shape
    threshold = np.tile(BAYER, (h // 4 + 1, w // 4 + 1))[:h, :w, None]
    five = np.clip(np.floor(rgb / 255.0 * 31 + threshold), 0, 31).astype(np.uint16)
    pixels = (five[..., 0] << 10) | (five[..., 1] << 5) | five[..., 2]
    keyed = (np.asarray(image.convert("RGB")) == MAGENTA).all(-1)
    pixels = np.where(keyed, 0x7C1F, np.where(pixels == 0x7C1F, 0x7C3F, pixels))
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

def label(canvas, words, x, y, colour=BLUE):
    """A small mono label (pixels, its baseline at y), with its rule before it."""
    dash(canvas, x, y - s(6), 10, colour)
    text(canvas, words, x + s(20), y, mono(13), colour, spacing=1)


def background():
    canvas = backdrop()
    dash(canvas, s(67), s(56), 24, YELLOW)
    text(canvas, "COMMUNITY REBUILT.  WIDE OPEN.", s(101), s(62), mono(13), GREY, spacing=1)
    title = font(126)
    text(canvas, "MOTOCROSS", s(64), s(184), title, CREAM, italic=True)
    text(canvas, "MADNESS", s(64), s(296), title, CREAM, italic=True)
    text(canvas, "2", s(64) + int(width_of("MADNESS", title)) + s(26), s(300), font(140), YELLOW,
         italic=True)
    label(canvas, "MAIN MENU", s(ROWS_X), s(ROWS_Y - 14))
    text(canvas, "OPEN-SOURCE REIMPLEMENTATION  /  DEVELOPMENT BUILD " + build_version(),
         s(1853), s(1008), mono(14), BLUE, right=True, spacing=0.5)
    text(canvas, "An independent community project. Not affiliated with or endorsed by the "
         "original creators.", s(1853), s(1034), body(14), GREY, right=True)
    return canvas


def crop(image, box):
    x, y, w, h = px(box)
    return image.crop((x, y, x + w, y + h))


def row_states(base, box, number, title, tag=None, line=None, size=36):
    """A row's four pictures (Up, Lit, Down, Grey), cut from `base` at grid `box`: grey with a
    blue edge at rest, yellow with an arrow (and `line` under the title) when chosen."""
    x, y, w, h = px(box)
    middle = y + h // 2
    pictures = {}
    for state in ("Up", "Lit", "Down", "Grey"):
        canvas = base.copy()
        chosen = state in ("Lit", "Down")
        if chosen:
            rect(canvas, (x, y, x + w - 1, y + h - 1), YELLOW if state == "Lit" else YELLOW_DOWN)
            ink = number_ink = INK
        else:
            rect(canvas, (x, y, x + w - 1, y + h - 1), ROW, 235)
            rect(canvas, (x, y + h - s(1), x + w - 1, y + h - 1), ROW_LINE)
            rect(canvas, (x, y, x + s(4) - 1, y + h - 1), BLUE if state == "Up" else DIM)
            ink = CREAM if state == "Up" else DIM
            number_ink = NUMBER_BLUE if state == "Up" else DIM
        text(canvas, number, x + s(24), middle + s(5), mono(13), number_ink)
        if chosen and line:
            text(canvas, title, x + s(62), middle + s(6), font(size + 4), ink, italic=True)
            text(canvas, line, x + s(64), middle + s(28), body(15), INK)
        else:
            text(canvas, title, x + s(62), middle + s(size * 0.36), font(size), ink, italic=True)
        if chosen:
            arrow(canvas, x + w - s(42), middle, 24, INK)
        elif tag:
            text(canvas, tag, x + w - s(24), middle + s(5), mono(13),
                 BLUE if state == "Up" else DIM, right=True, spacing=1)
        pictures[state] = canvas.crop((x, y, x + w, y + h))
    return pictures


def exit_states(base, box):
    x, y, w, h = px(box)
    pictures = {}
    for state, ink, icon in (("Up", GREY, GREY), ("Lit", CREAM, YELLOW), ("Down", YELLOW, YELLOW),
                             ("Grey", DIM, DIM)):
        canvas = base.copy()
        exit_icon(canvas, x + s(28), y + h // 2 - s(9), icon)
        text(canvas, "EXIT GAME", x + s(60), y + h // 2 + s(10), font(28), ink, italic=True)
        pictures[state] = canvas.crop((x, y, x + w, y + h))
    return pictures


def chip_states(base, box, words):
    """A small outlined text button."""
    x, y, w, h = px(box)
    f = mono(14)
    pictures = {}
    for state, edge, ink in (("Up", (70, 74, 80), GREY), ("Lit", YELLOW, CREAM),
                             ("Down", YELLOW, YELLOW), ("Grey", (50, 52, 56), DIM)):
        canvas = base.copy()
        ImageDraw.Draw(canvas).rectangle((x, y, x + w - 1, y + h - 1), outline=edge + (255,),
                                         width=max(s(1.5), 1))
        text(canvas, words, x + (w - int(width_of(words, f, 1))) // 2, y + h // 2 + s(5), f, ink,
             spacing=1)
        pictures[state] = canvas.crop((x, y, x + w, y + h))
    return pictures


def profile_states(base, box):
    """The rider card: its tile, label and "Change rider". The name is the game's own text."""
    x, y, w, h = px(box)
    pictures = {}
    for state in ("Up", "Lit", "Down", "Grey"):
        canvas = base.copy()
        rect(canvas, (x, y, x + w - 1, y + h - 1), CARD)
        if state in ("Lit", "Down"):
            ImageDraw.Draw(canvas).rectangle((x, y, x + w - 1, y + h - 1), outline=YELLOW + (255,),
                                             width=max(s(2), 2))
        tile = {"Down": YELLOW_DOWN, "Grey": DIM}.get(state, YELLOW)
        rect(canvas, (x + s(20), y + s(26), x + s(76) - 1, y + s(86) - 1), tile)
        text(canvas, "MX", x + s(25), y + s(70), font(34), INK, italic=True)
        text(canvas, "LOCAL RIDER", x + s(90), y + s(30), mono(11), BLUE, spacing=1)
        ink = CREAM if state == "Lit" else YELLOW
        f = body(14, semibold=True)
        text(canvas, "Change rider", x + s(90), y + s(98), f, ink)
        corner = (x + s(90) + int(width_of("Change rider", f)) + s(18), y + s(87))
        d = ImageDraw.Draw(canvas)
        d.line([(corner[0] - s(8), corner[1] + s(8)), corner], fill=ink + (255,), width=max(s(1.6), 2))
        d.line([(corner[0] - s(6), corner[1]), corner, (corner[0], corner[1] + s(6))],
               fill=ink + (255,), width=max(s(1.6), 2))
        pictures[state] = canvas.crop((x, y, x + w, y + h))
    return pictures


def panel_card(base, panel):
    """A pull-down panel's card: dark, edged in blue on the left, its label at the top."""
    _, (x, y, w, h), title, _ = panel
    canvas = base.copy()
    rect(canvas, px((x, y, x + w, y + h)), CARD, 245)
    rect(canvas, px((x, y, x + 4, y + h)), BLUE)
    label(canvas, title, s(x + 24), s(y + 40))
    return canvas


def scroll_arrow(size, up, colour):
    img = Image.new("RGBA", size, (0, 0, 0, 0))
    w, h = size
    points = [(w * 0.28, h * 0.62), (w * 0.5, h * 0.38), (w * 0.72, h * 0.62)]
    if not up:
        points = [(x, h - y) for x, y in points]
    ImageDraw.Draw(img).line(points, fill=colour + (255,), width=max(s(2.5), 2), joint="curve")
    return img


# ---- The layout ----

def read_dtm(text):
    """The dtm's sections in order: [name, [lines]]."""
    sections = []
    for line in text.replace("\r\n", "\n").split("\n"):
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


def user_panel(card, controls, pictures, name, x, y):
    """The profile list's frame, its scroll arrows and bar, on the profiles panel."""
    lx, ly, lw, lh = PROFILE_LIST
    ImageDraw.Draw(card).rectangle(px((x + lx, y + ly, x + lx + lw, y + ly + lh)),
                                   outline=ROW_LINE + (255,), width=max(s(1), 1))
    controls["LstProfiles"] = {"Xywh": xywh((lx + 12, ly + 8, lw - 24, lh - 16)), "Anchor": name,
                               "RelAnchor": "1", "TextAlign": "LEFT", "TextDrop": "0",
                               "TextColor": "14740720"}
    up_box = (SCROLL_X, ly, SCROLL_W, 34)
    down_box = (SCROLL_X, ly + lh - 34, SCROLL_W, 34)
    bar_box = (SCROLL_X, ly + 36, SCROLL_W, lh - 72)
    for control, (bx, by, bw, bh), up in (("ProfileScrollUp", up_box, True),
                                          ("ProfileScrollDown", down_box, False)):
        base = crop(card, (x + bx, y + by, bw, bh))
        for state, colour in (("Up", BLUE), ("Lit", YELLOW), ("Down", CREAM), ("Grey", DIM)):
            img = base.copy()
            img.alpha_composite(scroll_arrow(img.size, up, colour))
            pictures["M%s%s" % (control, state)] = img
        controls[control] = dict(anims(control), Xywh=xywh((bx, by, bw, bh)), Anchor=name,
                                 RelAnchor="1")
    bar = crop(card, (x + bar_box[0], y + bar_box[1], bar_box[2], bar_box[3]))
    ImageDraw.Draw(bar).line([(bar.width // 2, 0), (bar.width // 2, bar.height)],
                             fill=ROW_LINE + (255,), width=max(s(1), 1))
    pictures["MProfileBar"] = bar
    thumb = Image.new("RGBA", (s(SCROLL_W), s(40)), CARD + (255,))
    ImageDraw.Draw(thumb).rectangle((s(9), s(2), s(SCROLL_W - 10), s(37)), fill=YELLOW + (255,))
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
                                   "FontName": '"Bahnschrift"', "FontHeight": str(s(26))}

    for i, (name, title, tag, line) in enumerate(MAIN_ITEMS):
        box = (ROWS_X, ROWS_Y + i * ROW_H, ROW_W, ROW_H)
        put(pictures, controls, name, box, row_states(bg, box, "%02d" % (i + 1), title, tag, line))
    put(pictures, controls, EXIT[0], EXIT[1], exit_states(bg, EXIT[1]))
    for name, words, box in CHIPS:
        put(pictures, controls, name, box, chip_states(bg, box, words))
    put(pictures, controls, "MenuUserProfile", PROFILE_CARD, profile_states(bg, PROFILE_CARD))
    controls["TxtCurrentProfile"] = {"Xywh": xywh(CURRENT_PROFILE), "TextColor": "14740720",
                                     "TextAlign": "LEFT", "TextDrop": "0",
                                     "FontName": '"Bahnschrift SemiBold Condensed"',
                                     "FontHeight": str(s(30)), "Bold": "700"}

    for panel in (SINGLE, MULTI, USER):
        name, (x, y, w, h), _, buttons = panel
        card = panel_card(bg, panel)
        if panel is USER:
            user_panel(card, controls, pictures, name, x, y)
            column = [(x + 448, y + 70 + i * 64, 228, 54) for i in range(len(buttons))]
        else:
            step = 56 if panel is SINGLE else 62
            width = 340 if panel is SINGLE else w - 48
            column = [(x + 24, y + 70 + i * step, width, step - 4) for i in range(len(buttons))]
        pictures["M%sPanel" % name] = crop(card, (x, y, w, h))
        controls[name] = {"Xywh": xywh((x, y, w, h)), "AnimNorm": '"M%sPanel"' % name,
                          "FXAnimIn": '"M%sPanel"' % name, "FXAnimOut": '"M%sPanel"' % name}
        for i, ((button, words), box) in enumerate(zip(buttons, column)):
            put(pictures, controls, button, box,
                row_states(card, box, "%02d" % (i + 1), words, size=26), anchor=name, origin=(x, y))
            drops[button] = TEXT_KEYS
        if panel is SINGLE:                     # the events movie's frame, its window keyed out
            fx, fy, fw, fh = MOVIE_FRAME
            frame = crop(card, (x + fx, y + fy, fw, fh))
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


def preview(bg, pictures, lit, panel):
    """The menu as it would show: every button at rest, `lit` lit, `panel` open, a sample name."""
    canvas = bg.copy()
    shown = [item[0] for item in MAIN_ITEMS] + [EXIT[0], "MenuUserProfile"] + [c[0] for c in CHIPS]
    if panel:
        name, (x, y, _, _), _, buttons = panel
        canvas.alpha_composite(pictures["M%sPanel" % name], (s(x), s(y)))
        shown += [button for button, _ in buttons]
        if panel is SINGLE:
            canvas.alpha_composite(pictures["MScreenOver"], (s(x + MOVIE_FRAME[0]), s(y + MOVIE_FRAME[1])))
    for name in shown:
        gx, gy = PLACED[name]
        canvas.alpha_composite(pictures["M%s%s" % (name, "Lit" if name == lit else "Up")], (s(gx), s(gy)))
    text(canvas, "ROOKIE", s(CURRENT_PROFILE[0]), s(CURRENT_PROFILE[1] + 28),
         font(30, style="SemiBold Condensed"), CREAM)
    rgb = np.asarray(canvas.convert("RGB")).copy()
    rgb[(rgb == MAGENTA).all(-1)] = (10, 11, 13)        # where the events movie plays
    return Image.fromarray(rgb)


if __name__ == "__main__":
    data, bg, pictures = build(open(sys.argv[1], "rb").read())
    open(sys.argv[2], "wb").write(data)
    out = Path(sys.argv[3])
    preview(bg, pictures, "MenuSingle", None).save(out / "modern mainmenu.png")
    preview(bg, pictures, "ButEnduro", SINGLE).save(out / "modern mainmenu - single open.png")
    preview(bg, pictures, "ButLoadProfile", USER).save(out / "modern mainmenu - profiles open.png")
    print("wrote", sys.argv[2], len(data), "bytes,", len(pictures), "pictures")
