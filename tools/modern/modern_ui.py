r"""What the modern menus are drawn with: the colours, the shapes, and the 16-bit encoding.

The menus' pictures are 16-bit, so the renderer reads the colours beside the colour key as
inks the photos show through (code/mcm2/ddraw.c, photo_pixel): HOLE is clear, and red 29, 30
and 31 with no green are shadow, blue and light blue at the strength their blue gives. encode()
turns an RGBA picture into that; shown() undoes it, for the previews.
"""
import struct

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

CREAM = (236, 238, 242)
SOFT = (150, 156, 166)
BLUE = (34, 110, 255)
BLUE_DEEP = (18, 70, 210)
BLUE_DOWN = (16, 56, 168)
GLOW = (110, 185, 255)
PALE = (205, 228, 255)
DIM = (92, 96, 104)
CARD = (17, 18, 22)
EDGE = (58, 62, 70)
MAGENTA = (255, 0, 255)             # the colour key: see-through and not clickable
MATTE = (12, 13, 16)                # soft edges are mixed with this where there are no inks

HOLE = 0x7C1E                       # x1R5G5B5 31, 0, 30
KEY = 0x7C1F
INKS = np.array([(0, 0, 0), BLUE, GLOW], dtype=np.float64)       # red 29, 30, 31
INK_STEPS = np.array([32, 32, 30])                               # their blue's strengths
INK_NEAR = 30 ** 2 * 3              # how far a soft pixel's colour may be from an ink to be one
BAYER = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]) / 16 - 0.5


def encode(image, inks=True, window=None):
    """An RGBA picture as x1R5G5B5 pixels (rows top first). With `inks`, what is see-through is
    HOLE or an ink; without, it is mixed with MATTE (HOLE where it is nearly clear). Where the
    mask `window` is set, what shows under the picture is the game's 3D, which inks would hide:
    there it is the colour key where mostly clear, and solid on MATTE where not."""
    rgba = np.asarray(image.convert("RGBA"), dtype=np.float64)
    alpha = rgba[..., 3] / 255.0
    rgb = rgba[..., :3]
    keyed = (rgba[..., 3] == 255) & (rgba[..., :3] == MAGENTA).all(-1)
    over_3d = np.asarray(window) > 127 if window is not None else np.zeros(alpha.shape, bool)
    if not inks:
        rgb = rgb * alpha[..., None] + np.array(MATTE) * (1 - alpha[..., None])
    else:
        solid = rgb * alpha[..., None] + np.array(MATTE) * (1 - alpha[..., None])
        rgb = np.where(over_3d[..., None], solid, rgb)
        keyed |= over_3d & (alpha < 0.5)
        alpha = np.where(over_3d, 1.0, alpha)
    h, w = alpha.shape
    dither = np.tile(BAYER, (h // 4 + 1, w // 4 + 1))[:h, :w, None]       # no banding in gradients
    five = np.clip(np.round(rgb / 255.0 * 31 + dither), 0, 31).astype(np.uint16)
    five[..., 1] = np.where((five[..., 0] >= 29) & (five[..., 1] == 0), 1, five[..., 1])   # clear of the inks
    pixels = (five[..., 0] << 10) | (five[..., 1] << 5) | five[..., 2]
    if inks:
        # A soft pixel near an ink's colour is that ink; another is solid when it is mostly
        # there (type's edges), else a faint shadow.
        distance = ((rgb[..., None, :] - INKS) ** 2).sum(-1)
        ink = distance.argmin(-1)
        near = distance.min(-1) <= INK_NEAR
        ink = np.where(near, ink, 0)
        level = np.round(alpha * INK_STEPS[ink]).astype(int) - 1
        coded = ((29 + ink) << 10 | np.clip(level, 0, 31)).astype(np.uint16)
        soft = (alpha < 0.97) & (near | (alpha < 0.5))
        pixels = np.where(soft, np.where(level < 0, HOLE, coded), pixels)
    else:
        pixels = np.where(alpha < 0.125, HOLE, pixels)
    return np.where(keyed, KEY, pixels).astype(np.uint16)


def tga16(image, inks=True, window=None):
    """encode() as an uncompressed 16-bit TGA, bottom row first."""
    pixels = encode(image, inks, window)
    h, w = pixels.shape
    header = struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, w, h, 16, 0)
    return header + pixels[::-1].astype("<u2").tobytes()


def shown(image, inks=True, window=None):
    """What the renderer makes of encode(image): RGBA, for laying over a photo in a preview."""
    p = encode(image, inks, window).astype(int)
    r, g, b = p >> 10 & 31, p >> 5 & 31, p & 31
    out = np.zeros(p.shape + (4,))
    out[..., :3] = np.stack([r, g, b], -1) * 255.0 / 31
    out[..., 3] = 255
    ink = (r >= 29) & (g == 0) & ~((r == 31) & (b >= 30))
    which = np.clip(r - 29, 0, 2)
    amount = (b + 1) * 255.0 / INK_STEPS[which]
    out[ink, :3] = INKS[which[ink]]
    out[ink, 3] = amount[ink]
    out[p == HOLE] = 0
    out[p == KEY] = (*MAGENTA, 255)
    return Image.fromarray(np.clip(np.round(out), 0, 255).astype(np.uint8), "RGBA")


# ---- Shapes ----

SUPER = 4                           # masks are drawn this many times over and brought down


def clear(size):
    return Image.new("RGBA", size, (0, 0, 0, 0))


def rounded(size, radius, slant=0):
    """A smooth mask `size`: a rounded rectangle, leaning forward `slant` pixels over its height."""
    w, h = size
    big = Image.new("L", (w * SUPER, h * SUPER))
    inset = slant * SUPER
    ImageDraw.Draw(big).rounded_rectangle((0, 0, (w - slant) * SUPER - 1, h * SUPER - 1),
                                          radius=radius * SUPER, fill=255)
    if slant:
        # x' = x + slant * (1 - y / h): the top goes right, the bottom stays.
        big = big.transform(big.size, Image.AFFINE, (1, inset / big.height, -inset, 0, 1, 0),
                            Image.BICUBIC)
    return big.resize(size, Image.LANCZOS)


def paint(canvas, mask, colour, at=(0, 0), opacity=1.0):
    """`colour` laid on `canvas` through `mask`, which may be a gradient image the mask's size."""
    if isinstance(colour, Image.Image):
        layer = colour.convert("RGBA")
    else:
        layer = Image.new("RGBA", mask.size, tuple(colour) + (255,))
    alpha = mask if opacity >= 1 else mask.point(lambda v: int(v * opacity))
    layer.putalpha(alpha)
    canvas.alpha_composite(layer, at)


def rim(mask):
    """The outermost pixel or so of `mask`, as a mask (its own edges count as outside)."""
    padded = Image.new("L", (mask.width + 2, mask.height + 2))
    padded.paste(mask, (1, 1))
    inner = np.asarray(padded.filter(ImageFilter.MinFilter(3)), int)[1:-1, 1:-1]
    return Image.fromarray(np.clip(np.asarray(mask, int) - inner, 0, 255).astype(np.uint8))


def gradient(size, left, right, gloss=0.0):
    """Left to right from `left` to `right`, lighter towards the top by `gloss`."""
    w, h = size
    across = np.linspace(0, 1, w)[None, :, None]
    rows = np.array(left) * (1 - across) + np.array(right) * across
    down = np.linspace(1, 0, h)[:, None, None] ** 2 * gloss
    rgb = rows + (255 - rows) * down
    return Image.fromarray(np.clip(rgb, 0, 255).astype(np.uint8), "RGB")


def glow(canvas, mask, at, colour, sigma, strength):
    """A soft light round `mask` placed at `at` on `canvas`."""
    halo = Image.new("L", canvas.size)
    halo.paste(mask, at)
    halo = halo.filter(ImageFilter.GaussianBlur(sigma)).point(lambda v: min(255, int(v * strength)))
    paint(canvas, halo, colour)


def lit_bar(canvas, box, state, radius, slant=0, halo=0):
    """The chosen thing's bar on `canvas`: a rounded blue gradient, a light edge, and with `halo`
    a light blue glow that far round it. `state` is Lit or Down."""
    x, y, w, h = box
    mask = rounded((w, h), radius, slant)
    if halo and state == "Lit":
        glow(canvas, mask, (x, y), GLOW, halo / 2.2, 0.9)
    left, right = (BLUE, BLUE_DEEP) if state == "Lit" else (BLUE_DOWN, BLUE_DOWN)
    paint(canvas, mask, gradient((w, h), left, right, 0.18 if state == "Lit" else 0), (x, y))
    if state == "Lit":
        paint(canvas, rim(mask), PALE, (x, y), 0.55)


def glass(canvas, box, radius, opacity, edge=None):
    """A dark see-through panel on `canvas`, with a fine `edge` line when given."""
    x, y, w, h = box
    mask = rounded((w, h), radius)
    paint(canvas, mask, (0, 0, 0), (x, y), opacity)
    if edge:
        paint(canvas, rim(mask), edge, (x, y))


def card(canvas, box, radius, colour=CARD, edge=None):
    """An opaque panel with rounded corners."""
    x, y, w, h = box
    mask = rounded((w, h), radius)
    paint(canvas, mask, colour, (x, y))
    if edge:
        paint(canvas, rim(mask), edge, (x, y))
