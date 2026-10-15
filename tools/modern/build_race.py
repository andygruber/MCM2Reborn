r"""MCM2:Reborn's race screens: build_race.py <game ui folder> <out folder> <preview dir>

  Load*: each event's loading screen, its gold rider across the left and the progress along
  the foot. The game fills its progress (dlgprocs.c, loading_open) with the BlueBar animation,
  here an orange bar as wide as StaBarRect.
  VCR: the replay's controls, a solid bar over the race at the foot of the screen; their
  pictures are drawn here (play, pause, the speeds, the ends, save, camera and riders).

Writes LOADSX, LOADNAT, LOADBAJA, LOADEND, LOADQUAR, LOADTAG and VCR for the game's ui\modern\.
"""
import io
import sys
from pathlib import Path

from PIL import Image, ImageDraw

import reborn_kit as kit
from build_mainmenu import rs2_files
from reborn_kit import BOTTOM, DIM, EDGE, GREY, INK, LEFT, ORANGE, PAPER, RAISED, RIGHT, TOP, H, W, box, heading, sub
from reborn_layout import DEEP, STATES, Screen, clear, font, on_board, under, xywh

ART = 3

# The loading screens: the archive, the event's title and what it is.
EVENTS = [("LOADSX", "SUPERCROSS", "STADIUM"), ("LOADNAT", "NATIONALS", "OUTDOOR MOTOCROSS"),
          ("LOADBAJA", "BAJA", "POINT TO POINT"), ("LOADEND", "ENDURO", "CHECKPOINTS"),
          ("LOADQUAR", "STUNT QUARRY", "SCORE ATTACK"), ("LOADTAG", "MOTO-TAG", "KEEP THE BALL")]
BAR = (1400, BOTTOM - 40, RIGHT, BOTTOM - 24)


def art_of(data):
    """The event's gold rider: the archive's picture that is not the progress bar."""
    for name, blob in rs2_files(data):
        if name.lower().endswith(".tga") and "bar" not in name.lower():
            return Image.open(io.BytesIO(blob)).convert("RGB")
    raise FileNotFoundError("no picture")


def build_loading(data, stem, title, kind):
    screen = Screen(data, stem)
    board = Image.new("RGBA", (W, H), (8, 8, 10, 255))
    art = art_of(data).crop((20, 40, 620, 400))          # above the original's own progress bar
    face = kit.cover(art, (1700, H)).convert("RGBA")
    fade = Image.new("L", face.size)
    d = ImageDraw.Draw(fade)
    for x in range(face.width):
        d.line((x, 0, x, face.height), fill=int(255 * min(1.0, max(0.0, (x - 1000) / 700))))
    face = Image.composite(Image.new("RGBA", face.size, (8, 8, 10, 255)), face, fade)
    board.paste(face, (0, 0))
    # Darker at the top, for the title on it.
    shade = Image.new("L", (W, H))
    d = ImageDraw.Draw(shade)
    for y in range(360):
        d.line((0, y, W, y), fill=int(200 * (1 - y / 360)))
    board = Image.composite(Image.new("RGBA", (W, H), (8, 8, 10, 255)), board, shade)

    kit.tag(board, kind, LEFT, TOP, 26)
    heading(board, title, LEFT, TOP + 180, kit.fit(title, 140, 1300), PAPER)
    heading(board, "GET READY", RIGHT, 760, 72, PAPER, anchor="rs")
    sub(board, "THE TRACK IS LOADING", RIGHT, 812, 28, GREY, anchor="rs", bold=True)
    box(board, BAR, RAISED + (255,), 8)
    sub(board, "LOADING", BAR[0], BAR[1] - 22, 22, GREY, bold=True)

    bar = clear((BAR[2] - BAR[0], BAR[3] - BAR[1]))
    ImageDraw.Draw(bar).rounded_rectangle((0, 0, bar.width - 1, bar.height - 1), 8, fill=ORANGE + (255,))
    screen.picture("MLoadBar", bar)
    # The game finds the bar's animation by its name: BlueBar is read from the new picture.
    screen.set("BlueBar", Files1=screen.path("MLoadBar"))
    screen.set("StaBarRect", Xywh=xywh(BAR), AnimNorm='"MLoadBar"')
    screen.set("Set_Information", BackgroundImage=screen.picture("MBack", board), ScreenWidth=str(W),
               ScreenHeight=str(H), ArtScale=str(ART), Xywh=xywh((0, 0, W, H)), FontName='"Helvetica"')
    return screen, board


# ---- VCR ----

VCR = (1600, 200)


def icon(img, area, kind, colour):
    """The replay's symbols, drawn in the middle of `area`."""
    d = ImageDraw.Draw(img)
    x0, y0, x1, y1 = area
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    s = min(x1 - x0, y1 - y0) * 0.22
    fill = colour + (255,)

    def tri(x, way):                    # a triangle pointing `way` (1 right, -1 left) from x
        d.polygon([(x - way * s * 0.6, cy - s), (x + way * s * 0.8, cy), (x - way * s * 0.6, cy + s)], fill=fill)

    def bar(x):
        d.rectangle((x - s * 0.18, cy - s, x + s * 0.18, cy + s), fill=fill)

    if kind == "play":
        tri(cx, 1)
    elif kind == "pause":
        bar(cx - s * 0.45)
        bar(cx + s * 0.45)
    elif kind == "reverse":
        tri(cx - s * 0.5, -1)
        tri(cx + s * 0.6, -1)
    elif kind == "ff":
        tri(cx - s * 0.6, 1)
        tri(cx + s * 0.5, 1)
    elif kind == "fff":
        for k in (-1.1, 0, 1.1):
            tri(cx + k * s, 1)
    elif kind == "beginning":
        bar(cx - s * 0.9)
        tri(cx + s * 0.3, -1)
    elif kind == "end":
        tri(cx - s * 0.3, 1)
        bar(cx + s * 0.9)
    elif kind == "frame":
        tri(cx - s * 0.2, 1)
        bar(cx + s * 0.8)
    elif kind == "save":
        d.rounded_rectangle((cx - s, cy - s, cx + s, cy + s), 3, outline=fill, width=3)
        d.rectangle((cx - s * 0.5, cy - s, cx + s * 0.5, cy - s * 0.3), fill=fill)
        d.rectangle((cx - s * 0.55, cy + s * 0.2, cx + s * 0.55, cy + s * 0.75), fill=fill)
    elif kind == "camera":
        d.rounded_rectangle((cx - s * 1.1, cy - s * 0.7, cx + s * 0.5, cy + s * 0.7), 3, fill=fill)
        d.polygon([(cx + s * 0.5, cy), (cx + s * 1.2, cy - s * 0.6), (cx + s * 1.2, cy + s * 0.6)], fill=fill)
    elif kind in ("prev", "next"):
        way = -1 if kind == "prev" else 1
        d.line([(cx - way * s * 0.4, cy - s), (cx + way * s * 0.5, cy), (cx - way * s * 0.4, cy + s)], fill=fill,
               width=5, joint="curve")
    elif kind == "exit":
        d.line([(cx - s * 0.7, cy - s * 0.7), (cx + s * 0.7, cy + s * 0.7)], fill=fill, width=5)
        d.line([(cx - s * 0.7, cy + s * 0.7), (cx + s * 0.7, cy - s * 0.7)], fill=fill, width=5)


def key_states(board, area, kind):
    """A replay button: a raised key with its symbol, orange when chosen."""
    pictures = {}
    for state in STATES:
        canvas = on_board(board, area)
        lit = state in ("Lit", "Down")
        box(canvas, area, ((ORANGE if state == "Lit" else DEEP) if lit else RAISED) + (255,), 8,
            None if lit else EDGE + (255,))
        icon(canvas, area, kind, INK if lit else DIM if state == "Grey" else PAPER)
        pictures[state] = under(canvas, area)
    return pictures


def build_vcr(data):
    screen = Screen(data, "VCR")
    w, h = VCR
    board = clear((w, h))
    box(board, (0, 0, w - 1, h - 1), (14, 15, 18, 255), 0, ORANGE + (255,))

    # The time: where the replay is, the slider through it, its length and a frame on.
    top = 24
    for name, area in (("TxtTime", (24, top, 264, top + 56)), ("TxtTimeEnd", (w - 24 - 72 - 16 - 240, top,
                                                                             w - 24 - 72 - 16, top + 56))):
        keys = {"Xywh": xywh(area), "TextLMargin": "0", "TextTMargin": "0"}
        keys.update(font(30, PAPER, "CENTER"))
        screen.set(name, **keys)
    slider = (280, top + 10, w - 24 - 72 - 16 - 240 - 16, top + 46)
    groove = under(board, slider)
    ImageDraw.Draw(groove).rounded_rectangle((0, groove.height // 2 - 3, groove.width - 1, groove.height // 2 + 3), 3,
                                             fill=EDGE + (255,))
    thumb = groove.crop((groove.width // 2 - 18, 0, groove.width // 2 + 18, groove.height))
    ImageDraw.Draw(thumb).rounded_rectangle((4, 2, 31, thumb.height - 3), 8, fill=ORANGE + (255,))
    track = screen.picture("MSliderTimeBar", groove)
    screen.set("SliderTime", Xywh=xywh(slider), AnimNorm=track, AnimFocus=track, AnimPush=track, AnimDisable=track,
               AnimThumb=screen.picture("MSliderTimeThumb", thumb))
    frame = (w - 24 - 72, top, w - 24, top + 56)
    screen.button("ButFrame", frame, key_states(board, frame, "frame"))

    # The keys: leave, play and the speeds, the ends; save, camera and riders at the right.
    y0, y1 = h - 24 - 84, h - 24
    x = 24
    layout = [("ButExit", "exit", 100, "button"), ("TogPlay", "play", 120, "toggle"),
              ("TogPause", "pause", 84, "toggle"), ("ButBeginning", "beginning", 84, "button"),
              ("TogReverse", "reverse", 84, "toggle"),
              ("TogFF", "ff", 84, "toggle"), ("TogFFF", "fff", 84, "toggle"), ("ButEnd", "end", 84, "button")]
    for name, kind, kw, what in layout:
        place(screen, board, name, (x, y0, x + kw, y1), kind, what)
        x += kw + (24 if name == "ButExit" else 10)
    x = w - 24
    for name, kind in (("ButNextRider", "next"), ("ButPrevRider", "prev"), ("ButCamera", "camera"),
                       ("ButSave", "save")):
        place(screen, board, name, (x - 84, y0, x, y1), kind, "button")
        x -= 84 + 10
    screen.set("Set_Information", BackgroundImage=screen.picture("MBack", board), ScreenWidth=str(w),
               ScreenHeight=str(h), ArtScale=str(ART), Xywh=xywh((0, 0, w, h)), FontName='"Helvetica"')
    return screen, board


def place(screen, board, name, area, kind, what):
    pictures = key_states(board, area, kind)
    if what == "button":
        screen.button(name, area, pictures)
        return
    # A toggle: off at rest, orange while on.
    screen.set(name, Xywh=xywh(area), State0=screen.picture("M%sOff" % name, pictures["Up"]),
               State1=screen.picture("M%sOn" % name, pictures["Lit"]),
               StateDisable0=screen.picture("M%sGrey" % name, pictures["Grey"]),
               StateDisable1=screen.picture("M%sGreyOn" % name, pictures["Grey"]))


def main(ui, out, previews):
    ui, out, previews = Path(ui), Path(out), Path(previews)
    kit.fonts_from(ui / "modern" / "fonts")
    out.mkdir(parents=True, exist_ok=True)
    previews.mkdir(parents=True, exist_ok=True)
    made = [build_loading((ui / (stem + ".DAT")).read_bytes(), stem, title, kind) for stem, title, kind in EVENTS]
    made.append(build_vcr((ui / "VCR.DAT").read_bytes()))
    for screen, board in made:
        data = screen.pack()
        (out / (screen.stem + ".DAT")).write_bytes(data)
        print("%-8s %3d pictures, %d bytes" % (screen.stem, len(screen.pictures), len(data)))
        board.convert("RGB").save(previews / ("reborn %s.png" % screen.stem.lower()))


if __name__ == "__main__":
    main(*sys.argv[1:4])
