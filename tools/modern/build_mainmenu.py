r"""The modern main menu: build_mainmenu.py <original MAINMENU.DAT> <out MAINMENU.DAT> <preview dir>

Draws the menu (mainmenu_art's look) at the art's size, cuts each control's pictures from it,
rewrites the original layout with the same controls at their new places, and packs it all as an
RS2 archive the game reads from ui\modern\. Places here are on the 1920x1080 grid.

Every picture is an opaque 16-bit TGA: the menus key out magenta (0xFF00FF) and nothing else,
so each button carries its own piece of the backdrop, drawn in each of its four states.
"""
import struct
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, str(Path(__file__).parent))
from mainmenu_art import (EMBER, ICE, WHITE, H, W, chain_line, chosen_strip, glass_title,  # noqa: E402
                          gradient, label, pressed_strip, s, smoked_glass, solid)

FOLDER = "02_Modern\\"
SOFT = (222, 232, 248)          # an item at rest
SOFT_ICE = (196, 222, 252)
GREY = (104, 124, 156)
MAGENTA = (255, 0, 255)

# The left pane's items, top to bottom: control, label.
MAIN_ITEMS = [("MenuSingle", "SINGLE PLAYER"), ("MenuMulti", "MULTIPLAYER"), ("MenuOptions", "OPTIONS"),
              ("MenuUserProfile", "RIDER PROFILE"), ("MenuHighScore", "HIGH SCORES"),
              ("MenuReplay", "REPLAYS"), ("Menuexit", "EXIT")]
STATUS_ITEMS = [("MenuHelp", "HELP", (1540, 1014, 150, 58)), ("MenuCredits", "CREDITS", (1700, 1014, 190, 58))]

# The pull-down panels: the panel control, its place, its title, and its buttons.
PANEL_X, PANEL_Y = 660, 400
SINGLE = ("MenuSingleAnimation", (PANEL_X, PANEL_Y, 860, 440), "SELECT EVENT",
          [("ButBaja", "BAJA"), ("ButStuntQuarry", "STUNT QUARRY"), ("ButEnduro", "ENDURO"),
           ("ButNationals", "NATIONALS"), ("ButSupercross", "SUPERCROSS"), ("ButProCircuit", "PRO-CIRCUIT")])
MULTI = ("MenuMultiAnimation", (PANEL_X, PANEL_Y, 480, 430), "MULTIPLAYER",
         [("ButIPX", "LOCAL NETWORK"), ("ButTCP", "INTERNET  TCP/IP"), ("ButZone", "MSN GAMING ZONE"),
          ("ButCable", "SERIAL CABLE"), ("ButModem", "MODEM")])
USER = ("MenuUserAnimation", (PANEL_X, PANEL_Y, 760, 470), "RIDER PROFILES",
        [("ButNewProfile", "NEW"), ("ButLoadProfile", "LOAD"), ("ButRemoveProfile", "REMOVE"),
         ("ButCancelProfile", "CANCEL")])
MOVIE_FRAME = (430, 92, 400, 290)           # ScreenOverCtl, on the single-player panel
PROFILE_LIST = (30, 86, 370, 356)
SCROLL_X, SCROLL_W = 408, 34
CURRENT_PROFILE = (170, 1016, 640, 52)


def px(box):
    """A box on the grid, in pixels."""
    return tuple(s(v) for v in box)


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


def rs2_entries(data):
    count, = struct.unpack_from("<I", data, 4)
    at, found = 8, {}
    for _ in range(count):
        length, = struct.unpack_from("<I", data, at)
        name = data[at + 4:at + 4 + length].rstrip(b"\0").decode("latin-1")
        offset, size = struct.unpack_from("<II", data, at + 4 + length)
        found[name.lower()] = data[offset:offset + size]
        at += 4 + length + 8
    return found


# ---- The art ----

def background():
    canvas = gradient()
    smoked_glass(canvas, px((0, 0, 620, 1080)))
    glass_title(canvas, "MOTOCROSS", s(1000), s(70), 136)
    glass_title(canvas, "MADNESS", s(1060), s(196), 136)
    glass_title(canvas, "2", s(1560), s(54), 300)
    chain_line(canvas, s(1010), s(1590), s(352), s(30))
    ImageDraw.Draw(canvas).line([(s(92), s(180)), (s(560), s(180))], fill=ICE + (170,), width=s(1))
    label(canvas, "MAIN MENU", s(92), s(126), 30, ICE, "SemiBold Condensed", shade=False)
    smoked_glass(canvas, px((0, 1010, 1920, 1080)))
    label(canvas, "RIDER", s(92), s(1022), 22, ICE, "SemiBold Condensed", shade=False)
    return canvas


def panel_card(base, panel):
    """The panel's card over the backdrop: smoked glass, a hairline, its title."""
    _, (x, y, w, h), title, _ = panel
    canvas = base.copy()
    smoked_glass(canvas, px((x, y, x + w, y + h)), radius=s(14))
    label(canvas, title, s(x + 34), s(y + 24), 28, ICE, "SemiBold Condensed", shade=False)
    ImageDraw.Draw(canvas).line([(s(x + 34), s(y + 68)), (s(x + w - 34), s(y + 68))],
                                fill=ICE + (140,), width=s(1))
    return canvas


def states(base, box, text, size, rest_colour, inset=(14, 6)):
    """The four pictures of a button at grid `box` over `base`: Up, Lit, Down, Grey."""
    x, y, w, h = px(box)
    ix, iy = s(inset[0]), s(inset[1])
    margin = s(48)
    region = (max(x - margin, 0), max(y - margin, 0), min(x + w + margin, W), min(y + h + margin, H))
    ox, oy = region[0], region[1]
    tx, ty = x - ox + ix + s(30), y - oy + (h - s(size)) // 2 - s(6)
    pictures = {}
    for state in ("Up", "Lit", "Down", "Grey"):
        local = base.crop(region)
        if state == "Lit":
            chosen_strip(local, x - ox + ix, y - oy + iy, w - 2 * ix, h - 2 * iy)
            label(local, text, tx, ty, size, WHITE)
        elif state == "Down":
            pressed_strip(local, x - ox + ix, y - oy + iy, w - 2 * ix, h - 2 * iy)
            label(local, text, tx + s(1), ty + s(2), size, EMBER)
        elif state == "Grey":
            label(local, text, tx, ty, size, GREY, shade=False)
        else:
            label(local, text, tx, ty, size, rest_colour)
        pictures[state] = local.crop((x - ox, y - oy, x - ox + w, y - oy + h))
    return pictures


def arrow(size, up, colour):
    img = Image.new("RGBA", size, (0, 0, 0, 0))
    w, h = size
    points = [(w * 0.25, h * 0.62), (w * 0.5, h * 0.36), (w * 0.75, h * 0.62)]
    if not up:
        points = [(x, h - y) for x, y in points]
    ImageDraw.Draw(img).line(points, fill=colour + (255,), width=s(3), joint="curve")
    return img


def crop(image, box):
    x, y, w, h = px(box)
    return image.crop((x, y, x + w, y + h))


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
PLACED = {}                                     # a panel button's place on the grid, for previews


def build(original_dat):
    pictures = {}                               # anim name -> image
    controls = {}                               # section -> keys to set
    drops = {}

    bg = background()
    pictures["MBackground"] = bg
    controls["Set_Information"] = {"BackgroundImage": '"MBackground"', "ScreenWidth": str(W),
                                   "ScreenHeight": str(H), "Xywh": xywh((0, 0, 1920, 1080)),
                                   "FontName": '"Bahnschrift"', "FontHeight": str(s(26))}

    for i, (name, text) in enumerate(MAIN_ITEMS):
        box = (40, 192 + i * 86, 560, 80)
        for state, img in states(bg, box, text, 44, SOFT).items():
            pictures["M%s%s" % (name, state)] = img
        controls[name] = dict(anims(name), Xywh=xywh(box))
        PLACED[name] = box[:2]
    for name, text, box in STATUS_ITEMS:
        for state, img in states(bg, box, text, 32, SOFT, (6, 6)).items():
            pictures["M%s%s" % (name, state)] = img
        controls[name] = dict(anims(name), Xywh=xywh(box))
        PLACED[name] = box[:2]
    controls["TxtCurrentProfile"] = {"Xywh": xywh(CURRENT_PROFILE), "TextColor": "16777215",
                                     "TextAlign": "LEFT"}

    for panel in (SINGLE, MULTI, USER):
        name, (x, y, w, h), _, buttons = panel
        card = panel_card(bg, panel)
        if panel is USER:
            lx, ly, lw, lh = PROFILE_LIST
            ImageDraw.Draw(card).rounded_rectangle(px((x + lx, y + ly, x + lx + lw, y + ly + lh)), s(8),
                                                   outline=ICE + (90,), width=s(1))
        pictures["M%sPanel" % name] = crop(card, (x, y, w, h))
        controls[name] = {"Xywh": xywh((x, y, w, h)), "AnimNorm": '"M%sPanel"' % name,
                          "FXAnimIn": '"M%sPanel"' % name, "FXAnimOut": '"M%sPanel"' % name}

        if panel is USER:
            column = [(470, 86 + i * 72, 260, 60) for i in range(len(buttons))]
        else:
            step = 58 if panel is SINGLE else 64
            column = [(20, 86 + i * step, 400, 54) for i in range(len(buttons))]
        for (button, text), (bx, by, bw, bh) in zip(buttons, column):
            for state, img in states(card, (x + bx, y + by, bw, bh), text, 34, SOFT_ICE, (6, 4)).items():
                pictures["M%s%s" % (button, state)] = img
            controls[button] = dict(anims(button), Xywh=xywh((bx, by, bw, bh)), Anchor=name,
                                    RelAnchor="1")
            drops[button] = TEXT_KEYS
            PLACED[button] = (x + bx, y + by)

        if panel is SINGLE:                     # the events movie's frame, its window keyed out
            fx, fy, fw, fh = MOVIE_FRAME
            frame = crop(card, (x + fx, y + fy, fw, fh))
            d = ImageDraw.Draw(frame)
            d.rectangle((0, 0, frame.width - 1, frame.height - 1), outline=ICE, width=s(2))
            d.rectangle((s(4), s(4), frame.width - s(5), frame.height - s(5)), fill=MAGENTA)
            pictures["MScreenOver"] = frame
            controls["ScreenOverCtl"] = {"Xywh": xywh(MOVIE_FRAME), "AnimNorm": '"MScreenOver"',
                                         "Anchor": name, "RelAnchor": "1"}
        if panel is USER:
            controls["LstProfiles"] = {"Xywh": xywh((lx + 12, ly + 8, lw - 24, lh - 16)), "Anchor": name,
                                       "RelAnchor": "1", "TextAlign": "LEFT"}
            up_box = (SCROLL_X, ly, SCROLL_W, 34)
            down_box = (SCROLL_X, ly + lh - 34, SCROLL_W, 34)
            bar_box = (SCROLL_X, ly + 36, SCROLL_W, lh - 72)
            for control, (bx, by, bw, bh), up in (("ProfileScrollUp", up_box, True),
                                                  ("ProfileScrollDown", down_box, False)):
                base = crop(card, (x + bx, y + by, bw, bh))
                for state, colour in (("Up", ICE), ("Lit", EMBER), ("Down", WHITE), ("Grey", GREY)):
                    img = base.copy()
                    img.alpha_composite(arrow(img.size, up, colour))
                    pictures["M%s%s" % (control, state)] = img
                controls[control] = dict(anims(control), Xywh=xywh((bx, by, bw, bh)), Anchor=name,
                                         RelAnchor="1")
            bar = crop(card, (x + bar_box[0], y + bar_box[1], bar_box[2], bar_box[3]))
            ImageDraw.Draw(bar).line([(bar.width // 2, 0), (bar.width // 2, bar.height)], fill=ICE + (110,))
            pictures["MProfileBar"] = bar
            thumb = Image.new("RGBA", (s(SCROLL_W), s(40)), (14, 30, 66, 255))
            ImageDraw.Draw(thumb).rounded_rectangle((s(6), s(2), s(SCROLL_W - 7), s(37)), s(6),
                                                    fill=ICE + (255,))
            pictures["MProfileThumb"] = thumb
            controls["SldProfile"] = {"Xywh": xywh(bar_box), "AnimNorm": '"MProfileBar"',
                                      "AnimThumb": '"MProfileThumb"', "Anchor": name, "RelAnchor": "1"}

    original = rs2_entries(original_dat)
    sections = read_dtm(original["mainmenu.dtm"].decode("latin-1"))
    missing = [c for c in controls if c not in {n for n, _ in sections}]
    assert not missing, missing
    out = []
    for name, lines in sections:
        if name in ("Set_Anim", "Set_Sound", "rbwLabel1"):
            continue
        if name in controls:
            lines = set_keys(lines, controls[name], drops.get(name, ()))
        out += ["[%s]" % name] + [l for l in lines if l.strip()] + [""]
    out += ["[Set_Anim]", ""]
    for anim in pictures:
        out += ["[%s]" % anim, "Files1 = %s%s.tga" % (FOLDER, anim), "MSecDelay = 0", "LoopCount = 0", ""]
    out += ["[Set_Sound]", ""]

    entries = [(FOLDER + anim + ".tga", tga16(img)) for anim, img in pictures.items()]
    entries.append(("MainMenu.dtm", "\r\n".join(out).encode("latin-1")))
    return rs2(entries), bg, pictures


def preview(bg, pictures, lit, panel):
    """The menu as it would show: every button at rest, `lit` lit, `panel` open."""
    canvas = bg.copy()
    shown = [name for name, _ in MAIN_ITEMS] + [name for name, _, _ in STATUS_ITEMS]
    if panel:
        name, (x, y, _, _), _, buttons = panel
        canvas.alpha_composite(pictures["M%sPanel" % name], (s(x), s(y)))
        shown += [button for button, _ in buttons]
        if panel is SINGLE:
            canvas.alpha_composite(pictures["MScreenOver"], (s(x + MOVIE_FRAME[0]), s(y + MOVIE_FRAME[1])))
    for name in shown:
        gx, gy = PLACED[name]
        canvas.alpha_composite(pictures["M%s%s" % (name, "Lit" if name == lit else "Up")], (s(gx), s(gy)))
    rgb = np.asarray(canvas.convert("RGB")).copy()
    rgb[(rgb == MAGENTA).all(-1)] = (8, 16, 36)        # where the events movie plays
    return Image.fromarray(rgb)


if __name__ == "__main__":
    data, bg, pictures = build(open(sys.argv[1], "rb").read())
    open(sys.argv[2], "wb").write(data)
    out = Path(sys.argv[3])
    preview(bg, pictures, "MenuSingle", SINGLE).save(out / "modern mainmenu - single open.png")
    preview(bg, pictures, "MenuUserProfile", USER).save(out / "modern mainmenu - profiles open.png")
    states_row = [pictures["MMenuSingle" + state] for state in ("Up", "Lit", "Down", "Grey")]
    sheet = Image.new("RGBA", (states_row[0].width * 4, states_row[0].height))
    for i, img in enumerate(states_row):
        sheet.alpha_composite(img, (i * img.width, 0))
    sheet.convert("RGB").save(out / "modern mainmenu - button states.png")
    print("wrote", sys.argv[2], len(data), "bytes,", len(pictures), "pictures")
