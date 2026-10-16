r"""MCM2:Reborn's main menu: build_mainmenu.py <game ui folder> <out MAINMENU.DAT> <preview dir>

The menu over the photos (photos.py): the renderer draws them under it, crossfading, wherever
the menu's pictures are see-through. The title at the top left; the rows below it, the one under
the pointer on a slanted orange bar; Help, Credits and Exit as small buttons under them; the
rider's card at the bottom right. Pro Circuit is a row of its own and Single Player goes straight
to the event screen (dlgprocs.c, menu_click), so the single player panel is never shown.

Draws the menu at 2560x1440 with reborn_kit, cuts each control's pictures from it (on what is
under the control, as the game draws them), rewrites the original layout with the same controls
at their new places, and packs it all as an RS2 archive the game reads from ui\modern\.
The fonts come from the game's ui\modern\fonts, which the dev build loads too.
"""
import struct
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw

sys.path.insert(0, str(Path(__file__).parent))
import reborn_kit as kit                                                              # noqa: E402
from reborn_kit import (BOTTOM, DIM, EDGE, GREY, INK, LEFT, ORANGE, PAPER, RIGHT, TOP, H, W,  # noqa: E402
                        box, cap_height, heading, heading_ink, keys, outline, panel, shape, slanted,
                        sub, sub_fit)
from modern_ui import shown, tga16                                                    # noqa: E402

FOLDER = "02_Modern\\"
HERE = Path(__file__).parent
VERSION = "0.1"
TITLE = "MCM2:REBORN"

# The rows, top to bottom: control, title, what it holds.
ROWS = [("ButProCircuit", "PRO CIRCUIT", "YOUR CAREER · SPONSORS · PRIZE MONEY"),
        ("MenuSingle", "SINGLE PLAYER", "SUPERCROSS · NATIONALS · BAJA · ENDURO · STUNT QUARRY"),
        ("MenuMulti", "MULTIPLAYER", "LOCAL NETWORK · INTERNET"),
        ("MenuReplay", "REPLAYS", "WATCH YOUR SAVED RACES"),
        ("MenuHighScore", "HIGH SCORES", "BEST LAPS · BEST SCORES · PRO CIRCUIT"),
        ("MenuOptions", "OPTIONS", "CONTROLS · AUDIO · GRAPHICS")]
ROW_X, ROW_TOP, ROW_W, ROW_H, ROW_SIZE = LEFT - 24, 430, 1120, 128, 58
SMALL = [("MenuHelp", "HELP"), ("MenuCredits", "CREDITS"), ("Menuexit", "EXIT")]
SMALL_TOP, SMALL_H = ROW_TOP + len(ROWS) * ROW_H + 24, 54
CARD = (1904, 1060, RIGHT, 1290)                    # the rider's card
NAME = (1936, 1124, RIGHT - 32, 1196)               # the rider's name in it, the game's own text

# The panels the menu still opens: control, its place, its title, and its buttons.
# LAN is TCP/IP over the local network; Internet is a control of the look's own, opening the
# online screen (dlgprocs.c). The other connections go: no one has IPX, a cable or a modem now.
MULTI = ("MenuMultiAnimation", (1240, 556, 2000, 556 + 88 + 2 * 74 + 24), "MULTIPLAYER",
         [("ButTCP", "LAN"), ("ButInternet", "INTERNET")])
GONE = ["ButIPX", "ButZone", "ButCable", "ButModem"]
ADDED = {"ButInternet": {"Type": "BUTTON", "Anchor": "MenuMultiAnimation", "RelAnchor": "1"}}
USER = ("MenuUserAnimation", (1904, 390, RIGHT, 1030), "RIDER PROFILES",
        [("ButNewProfile", "NEW"), ("ButLoadProfile", "LOAD"), ("ButRemoveProfile", "REMOVE"),
         ("ButCancelProfile", "CLOSE")])
# The single player panel's buttons, never shown now that Single Player goes straight on.
UNUSED = ("MenuSingleAnimation", ["ButBaja", "ButStuntQuarry", "ButEnduro", "ButNationals", "ButSupercross"])

STATES = ("Up", "Lit", "Down", "Grey")


def build_version():
    """The version line's build: VERSION and the dev tree's commit."""
    try:
        commit = subprocess.run(["git", "rev-parse", "--short", "HEAD"], capture_output=True,
                                text=True, cwd=HERE, check=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return VERSION
    return "%s  %s" % (VERSION, commit)


# ---- RS2 ----

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


def background():
    """What is under everything: the shade the menu reads on, the title and the credit line."""
    canvas = clear((W, H))
    side, foot = Image.new("L", (W, H)), Image.new("L", (W, H))
    d = ImageDraw.Draw(side)
    for x in range(W):
        d.line((x, 0, x, H), fill=int(225 * max(0.0, 1 - x / 1500) ** 1.3))
    d = ImageDraw.Draw(foot)
    for y in range(1000, H):
        d.line((0, y, W, y), fill=int(200 * (y - 1000) / 440))
    canvas.paste((8, 8, 10, 255), mask=ImageChops.lighter(side, foot))

    heading(canvas, "MCM", LEFT - 4, TOP + 118, 124, PAPER)
    x = LEFT - 4 + heading_ink("MCM", 124) + 8
    two = round(124 * kit.figure_scale() * 1.08)
    ImageDraw.Draw(canvas).text((x, TOP + 118), "2:", font=kit.sub_font(two), fill=ORANGE, anchor="ls",
                                stroke_width=max(3, two // 14), stroke_fill=ORANGE)
    heading(canvas, "REBORN", LEFT - 4, TOP + 252, 124, PAPER)
    shape(canvas, [(LEFT, TOP + 282), (LEFT + 640, TOP + 282), (LEFT + 634, TOP + 290), (LEFT - 2, TOP + 290)],
          ORANGE)

    sub(canvas, "%s  ·  OPEN-SOURCE  ·  DEVELOPMENT BUILD %s  ·  NOT AFFILIATED WITH OR ENDORSED "
        "BY THE ORIGINAL CREATORS" % (TITLE, build_version()), LEFT, BOTTOM - 4, 20, DIM)
    keys(canvas, [("ENTER", "SELECT"), ("ESC", "BACK"), ("↑ ↓", "MOVE")], BOTTOM - 18,
         RIGHT - (keys(clear((W, H)), [("ENTER", "SELECT"), ("ESC", "BACK"), ("↑ ↓", "MOVE")], 0, 0)
                  - 52))
    return canvas


def under(board, area):
    """What `board` holds under `area`, to draw a control's picture on."""
    x0, y0, x1, y1 = (int(v) for v in area)
    return board.crop((x0, y0, x1, y1)).convert("RGBA")


def row_states(board, area, words, line):
    """A row: type on the menu's shade; on a slanted orange bar, with a chevron, when chosen."""
    x0, y0, x1, y1 = area
    baseline = y0 + 70
    pictures = {}
    for state in STATES:
        canvas = clear((W, H))
        canvas.alpha_composite(under(board, area), (int(x0), int(y0)))
        on = state in ("Lit", "Down")
        if on:
            w = heading_ink(words, ROW_SIZE) + 96
            shape(canvas, slanted((x0, baseline - 60, x0 + w, baseline + 12)),
                  ORANGE if state == "Lit" else (214, 110, 0))
            heading(canvas, words, x0 + 34, baseline, ROW_SIZE, INK)
            sub(canvas, "›", x0 + w + 20, baseline - 4, 64, ORANGE, bold=True)
        else:
            heading(canvas, words, x0 + 34, baseline, ROW_SIZE, DIM if state == "Grey" else PAPER)
        sub(canvas, line, x0 + 26, baseline + 50, sub_fit(line, 22, x1 - x0 - 40),
            ORANGE if on else DIM if state == "Grey" else GREY)
        pictures[state] = under(canvas, area)
    return pictures


def small_states(board, area, words):
    """A small slanted button: an outlined ghost, filled orange when chosen."""
    pictures = {}
    for state in STATES:
        canvas = clear((W, H))
        canvas.alpha_composite(under(board, area), (int(area[0]), int(area[1])))
        on = state in ("Lit", "Down")
        if on:
            shape(canvas, slanted(area), ORANGE if state == "Lit" else (214, 110, 0))
        else:
            shape(canvas, slanted(area), (16, 17, 20), 200)
            outline(canvas, slanted(area), EDGE, 2)
        heading(canvas, words, (area[0] + area[2]) / 2, (area[1] + area[3]) / 2 + cap_height(26) / 2, 26,
                INK if on else DIM if state == "Grey" else PAPER, anchor="ms")
        pictures[state] = under(canvas, area)
    return pictures


def card_states(board):
    """The rider's card: its label and the way to change; the name is the game's text."""
    x0, y0, x1, y1 = CARD
    pictures = {}
    for state in STATES:
        canvas = clear((W, H))
        canvas.alpha_composite(under(board, CARD), (x0, y0))
        on = state in ("Lit", "Down")
        box(canvas, CARD, (16, 17, 20, 225), 10, (ORANGE if on else EDGE) + (255,))
        heading(canvas, "RIDER", x0 + 32, y0 + 48, 24, ORANGE)
        sub(canvas, "CHANGE RIDER  ›", x1 - 32, y1 - 30, 22, ORANGE if on else PAPER, anchor="rs", bold=True)
        pictures[state] = under(canvas, CARD)
    return pictures


def panel_board(spec):
    """A panel's ground: dark glass, its title at the top."""
    _, area, title, _ = spec
    canvas = clear((W, H))
    panel(canvas, area, 230)
    heading(canvas, title, area[0] + 36, area[1] + 58, 26, ORANGE)
    return canvas


def panel_row_states(board, area, words, size, centred=False):
    """A panel's row: its words (`centred` in it, or from its left) at rest, an orange bar
    under them when chosen."""
    x0, y0, x1, y1 = area
    pictures = {}
    for state in STATES:
        canvas = clear((W, H))
        canvas.alpha_composite(under(board, area), (int(x0), int(y0)))
        on = state in ("Lit", "Down")
        if on:
            shape(canvas, slanted((x0, y0 + 4, x1, y1 - 4), 0.12), ORANGE if state == "Lit" else (214, 110, 0))
        x = (x0 + x1) / 2 - heading_ink(words, size) / 2 if centred else x0 + 30
        heading(canvas, words, x, (y0 + y1) / 2 + cap_height(size) / 2, size,
                INK if on else DIM if state == "Grey" else PAPER)
        pictures[state] = under(canvas, area)
    return pictures


def arrow_states(board, area, up):
    pictures = {}
    for state, colour in zip(STATES, (PAPER, ORANGE, (214, 110, 0), DIM)):
        canvas = clear((W, H))
        canvas.alpha_composite(under(board, area), (int(area[0]), int(area[1])))
        x0, y0, x1, y1 = area
        cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
        d = 8 if up else -8
        ImageDraw.Draw(canvas).line([(cx - 12, cy + d), (cx, cy - d), (cx + 12, cy + d)], fill=colour + (255,),
                                    width=4, joint="curve")
        pictures[state] = under(canvas, area)
    return pictures


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


def xywh(area, origin=(0, 0)):
    x0, y0, x1, y1 = (int(round(v)) for v in area)
    return "%d, %d, %d, %d" % (x0 - origin[0], y0 - origin[1], x1 - x0, y1 - y0)


def anims(name):
    return {"AnimNorm": '"M%sUp"' % name, "AnimFocus": '"M%sLit"' % name,
            "AnimPush": '"M%sDown"' % name, "AnimDisable": '"M%sGrey"' % name}


TEXT_KEYS = ("Text", "LocalTextId", "Font_UseDefault", "FontName", "FontHeight", "Charset", "Bold",
             "Underline", "Italic", "Strikethrough")
PLACED = {}                                     # where each control is, for the previews


def put(pictures, controls, name, area, states, anchor=None, origin=(0, 0)):
    """A button's pictures and keys; `area` is on the screen, `origin` its panel's corner."""
    for state, img in states.items():
        pictures["M%s%s" % (name, state)] = img
    keys_ = dict(anims(name), Xywh=xywh(area, origin))
    if anchor:
        keys_.update(Anchor=anchor, RelAnchor="1")
    controls[name] = keys_
    PLACED[name] = (int(area[0]), int(area[1]))


def user_panel(board, controls, pictures):
    """The profiles panel's list, its frame and its scroll arrows and bar."""
    name, (x0, y0, x1, y1), _, _ = USER
    lst = (x0 + 32, y0 + 92, x1 - 92, y1 - 200)
    box(board, lst, (10, 10, 12, 220), 8, EDGE + (255,))
    controls["LstProfiles"] = {"Xywh": xywh((lst[0] + 16, lst[1] + 10, lst[2] - 16, lst[3] - 10), (x0, y0)),
                               "Anchor": name, "RelAnchor": "1", "TextAlign": "LEFT", "TextDrop": "0",
                               "TextColor": str(0xE6F0F4), "FontName": '"Helvetica"', "FontHeight": "26",
                               "Italic": "-1"}
    sx0, sx1 = x1 - 76, x1 - 32
    up_area, down_area = (sx0, lst[1], sx1, lst[1] + 44), (sx0, lst[3] - 44, sx1, lst[3])
    bar_area = (sx0, lst[1] + 50, sx1, lst[3] - 50)
    for control, area, up in (("ProfileScrollUp", up_area, True), ("ProfileScrollDown", down_area, False)):
        put(pictures, controls, control, area, arrow_states(board, area, up), anchor=name, origin=(x0, y0))
    bar = under(board, bar_area)
    ImageDraw.Draw(bar).line([(bar.width // 2, 0), (bar.width // 2, bar.height)], fill=EDGE + (255,), width=2)
    pictures["MProfileBar"] = bar
    thumb = clear((int(sx1 - sx0), 44))
    ImageDraw.Draw(thumb).rounded_rectangle((10, 2, thumb.width - 10, 42), 6, fill=ORANGE + (255,))
    pictures["MProfileThumb"] = thumb
    controls["SldProfile"] = {"Xywh": xywh(bar_area, (x0, y0)), "AnimNorm": '"MProfileBar"',
                              "AnimThumb": '"MProfileThumb"', "Anchor": name, "RelAnchor": "1"}


def build(original_dat):
    pictures = {}                               # anim name -> image
    controls = {}                               # section -> keys to set
    drops = {}

    board = background()
    pictures["MBackground"] = board
    controls["Set_Information"] = {"BackgroundImage": '"MBackground"', "ScreenWidth": str(W),
                                   "ScreenHeight": str(H), "Xywh": xywh((0, 0, W, H)),
                                   "FontName": '"Helvetica"', "FontHeight": "26", "Photos": "1"}

    for i, (name, words, line) in enumerate(ROWS):
        area = (ROW_X, ROW_TOP + i * ROW_H, ROW_X + ROW_W, ROW_TOP + (i + 1) * ROW_H)
        put(pictures, controls, name, area, row_states(board, area, words, line))
        drops[name] = TEXT_KEYS
    # Pro Circuit leaves the single player panel to be a row of the menu's own.
    controls["ButProCircuit"]["Anchor"] = ""

    x = ROW_X + 24
    for name, words in SMALL:
        w = heading_ink(words, 26) + 80
        area = (x, SMALL_TOP, x + w, SMALL_TOP + SMALL_H)
        put(pictures, controls, name, area, small_states(board, area, words))
        x += w + 16

    put(pictures, controls, "MenuUserProfile", CARD, card_states(board))
    controls["TxtCurrentProfile"] = {"Xywh": xywh(NAME), "TextColor": str(0xE6F0F4), "TextAlign": "LEFT",
                                     "TextAlignV": "MIDDLE", "TextDrop": "0", "FontName": '"AeromoveDemo"',
                                     "FontHeight": "56", "Bold": "400", "Italic": "0"}

    for spec in (MULTI, USER):
        name, area, _, buttons = spec
        x0, y0, x1, y1 = area
        panel_art = panel_board(spec)
        if spec is USER:
            user_panel(panel_art, controls, pictures)
            # Two by two under the list.
            gap, row_h = 12, 64
            w = (x1 - x0 - 64 - gap) / 2
            places = [(x0 + 32 + (k % 2) * (w + gap), y1 - 32 - (2 - k // 2) * row_h - (1 - k // 2) * gap,
                       x0 + 32 + (k % 2) * (w + gap) + w, y1 - 32 - (1 - k // 2) * (row_h + gap))
                      for k in range(len(buttons))]
        else:
            row_h = 74
            places = [(x0 + 24, y0 + 88 + k * row_h, x1 - 24, y0 + 88 + (k + 1) * row_h - 8)
                      for k in range(len(buttons))]
        pictures["M%sPanel" % name] = under(panel_art, area)
        controls[name] = {"Xywh": xywh(area), "AnimNorm": '"M%sPanel"' % name,
                          "FXAnimIn": '"M%sPanel"' % name, "FXAnimOut": '"M%sPanel"' % name}
        # One size for a panel's buttons: the largest every one of them fits.
        size = min(kit.fit(words, 34, place[2] - place[0] - 40) for (_, words), place in zip(buttons, places))
        for (button, words), place in zip(buttons, places):
            put(pictures, controls, button, place,
                panel_row_states(panel_art, place, words, size, centred=spec is USER), anchor=name,
                origin=(x0, y0))
            drops[button] = TEXT_KEYS

    # The single player panel and its event buttons stay in the layout, out of sight.
    name, buttons = UNUSED
    controls[name] = {"Xywh": xywh((0, 0, 4, 4)), "AnimNorm": '"MNothing"', "FXAnimIn": '"MNothing"',
                      "FXAnimOut": '"MNothing"'}
    for button in buttons:
        controls[button] = dict(anims("Nothing"), Xywh="0, 0, 4, 4", Anchor=name, RelAnchor="1")
        drops[button] = TEXT_KEYS
    controls["ScreenOverCtl"] = {"Xywh": "0, 0, 4, 4", "AnimNorm": '"MNothing"', "Anchor": name, "RelAnchor": "1"}
    for state in [""] + list(STATES):
        pictures["MNothing" + state] = clear((4, 4))

    original = rs2_entries(original_dat)
    sections = read_dtm(original["mainmenu.dtm"].decode("latin-1"))
    for button in GONE:
        controls[button] = dict(anims("Nothing"), Xywh="0, 0, 4, 4", Anchor=name, RelAnchor="1")
        drops[button] = TEXT_KEYS
    missing = [c for c in controls if c not in {n for n, _ in sections} and c not in ADDED]
    assert not missing, missing

    # The original's sections stay in their order, its anims and sounds among them; the new
    # pictures go in at the start of Set_Anim, and the original's files stay in the archive.
    out = []
    for name, lines in sections:
        if name == "rbwLabel1":
            continue
        if name.lower() == "set_anim":
            for added, added_keys in ADDED.items():
                keys_ = dict(added_keys, **controls[added])
                out += ["[%s]" % added] + ["%s = %s" % kv for kv in keys_.items()] + [""]
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
    return rs2(entries), board, pictures


def preview(board, pictures, lit, panel_spec, photo):
    """The menu as it would show over `photo`: every control at rest, `lit` lit, a panel open."""
    canvas = kit.cover(Image.open(photo).convert("RGB"), (W, H)).convert("RGBA")
    layer = shown(board)
    visible = [r[0] for r in ROWS] + ["MenuUserProfile"] + [c[0] for c in SMALL]
    if panel_spec:
        name, area, _, buttons = panel_spec
        layer.alpha_composite(shown(pictures["M%sPanel" % name]), PLACED.get(name, (int(area[0]), int(area[1]))))
        visible += [b for b, _ in buttons]
    for name in visible:
        picture = pictures["M%s%s" % (name, "Lit" if name == lit else "Up")]
        layer.paste(shown(picture), PLACED[name])       # drawn over what is there, as the game does
    heading(layer, "ROOKIE", NAME[0], (NAME[1] + NAME[3]) / 2 + cap_height(56) / 2, 56, PAPER)
    canvas.alpha_composite(layer)
    return canvas.convert("RGB")


if __name__ == "__main__":
    ui, out_dat, out = Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3])
    kit.fonts_from(ui / "modern" / "fonts")
    data, board, pictures = build((ui / "MAINMENU.DAT").read_bytes())
    out_dat.write_bytes(data)
    out.mkdir(parents=True, exist_ok=True)
    photos = sorted(out.parent.glob("menu photo *.jpg"))
    for name, spec in ((None, None), ("MenuMulti", MULTI), ("ButLoadProfile", USER)):
        preview(board, pictures, name or "MenuSingle", spec, photos[0]).save(
            out / ("reborn mainmenu%s.png" % ("" if not spec else " - " + spec[2].lower())))
    print("wrote", out_dat, len(data), "bytes,", len(pictures), "pictures")
