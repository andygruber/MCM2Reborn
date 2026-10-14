"""The modern menus' look, drawn from nothing.

The Glassed look - text over the backdrop, smoked glass, hairline edges, the chosen item on a
strip of clear glass with a warm glow - over a dark-to-light blue gradient. Everything is laid
out on a 1920x1080 grid and drawn at SCALE times that, so the pictures are sharp at the size the
menus show at (2560x1440: SCALE 4/3).
"""
import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

SCALE = 4 / 3
W, H = round(1920 * SCALE), round(1080 * SCALE)
FONT = r"C:\Windows\Fonts\bahnschrift.ttf"

WHITE = (255, 255, 255)
ICE = (176, 214, 255)           # hairlines and headings
EMBER = (255, 168, 64)          # the glow round the chosen item
INK = (2, 8, 24)
NAVY = (6, 14, 36)


def s(value):
    """A length on the 1920x1080 grid, in pixels."""
    return round(value * SCALE)


def font(size, style="Bold Condensed"):
    f = ImageFont.truetype(FONT, s(size))
    f.set_variation_by_name(style)
    return f


def stops(colours, n):
    """n values blended through (position 0..1, rgb) stops."""
    t = np.linspace(0, 1, n)[:, None]
    out = np.zeros((n, 3))
    for (p0, c0), (p1, c1) in zip(colours, colours[1:]):
        k = np.clip((t - p0) / max(p1 - p0, 1e-6), 0, 1)
        inside = (t >= p0) & (t <= p1)
        out = np.where(inside, np.array(c0) + (np.array(c1) - np.array(c0)) * k, out)
    return out


def to_image(array, mode="RGB"):
    return Image.fromarray(np.clip(array, 0, 255).astype(np.uint8), mode)


def solid(size, colour, alpha):
    """`colour` through the `alpha` mask, as an RGBA layer."""
    layer = Image.new("RGBA", size, colour + (0,))
    layer.putalpha(alpha)
    return layer


def lean(img, slant=0.2):
    """Lean an image forward, as the type does."""
    w, h = img.size
    extra = int(h * slant)
    return img.transform((w + extra, h), Image.AFFINE, (1, slant, -extra, 0, 1, 0), Image.BICUBIC)


def text_mask(text, size, style="Bold Condensed"):
    f = font(size, style)
    pad = s(16)
    box = f.getbbox(text)
    mask = Image.new("L", (box[2] - box[0] + pad * 2, box[3] - box[1] + pad * 2))
    ImageDraw.Draw(mask).text((pad - box[0], pad - box[1]), text, font=f, fill=255)
    return mask


def gradient():
    """Dark blue at the top to light blue at the bottom, nothing else."""
    sky = stops([(0, (4, 10, 30)), (0.45, (16, 52, 112)), (1, (96, 160, 226))], H)
    return to_image(np.repeat(sky[:, None, :], W, 1)).convert("RGBA")


def smoked_glass(canvas, box, radius=0, tint=NAVY, dim=0.55, edge=ICE):
    """Smoked glass over `box` (pixels): what is behind it darkened, a sheen across the top, a
    hairline round it."""
    x0, y0, x1, y1 = box
    behind = canvas.crop(box).convert("RGB")
    dark = Image.blend(behind, Image.new("RGB", behind.size, tint), dim)
    sheen = stops([(0, (255, 255, 255)), (0.18, (0, 0, 0)), (1, (0, 0, 0))], y1 - y0)
    sheen = to_image(np.repeat(sheen[:, None, :], x1 - x0, 1) * 0.08)
    glass = ImageChops.add(dark, sheen).convert("RGBA")
    mask = Image.new("L", glass.size)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, x1 - x0 - 1, y1 - y0 - 1), radius, fill=255)
    canvas.paste(glass, (x0, y0), mask)
    line = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    ImageDraw.Draw(line).rounded_rectangle(box, radius, outline=edge + (150,), width=max(1, s(1)))
    canvas.alpha_composite(line)


def chosen_strip(canvas, x, y, w, h):
    """The chosen item's ground (pixels): a strip of clear glass fading to the right, a tick at
    its left, and a warm light along its edges."""
    across = np.linspace(1, 0, w) ** 1.6
    strip = Image.new("L", (w, h))
    strip.putdata([int(150 * a) for _ in range(h) for a in across])
    edge = Image.new("L", (w, h))
    ImageDraw.Draw(edge).rectangle((0, 0, w - 1, h - 1), outline=255, width=s(3))
    reach = Image.new("L", (w, h))                         # the edge stays lit most of the way
    reach.putdata([int(255 * min(1, (1 - i / w) * 1.6)) for _ in range(h) for i in range(w)])
    edge = ImageChops.multiply(edge, reach)

    glow = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    glow.paste(solid((w, h), EMBER, edge), (x, y))
    for radius, times in ((22, 2), (9, 2), (3, 1)):        # a wide warm bloom, then a tight one
        bloom = glow.filter(ImageFilter.GaussianBlur(s(radius)))
        for _ in range(times):
            canvas.alpha_composite(bloom)
    canvas.alpha_composite(solid((w, h), (255, 236, 210), strip.point(lambda v: v // 4)), (x, y))
    canvas.alpha_composite(solid((w, h), (255, 230, 190), edge), (x, y))
    ImageDraw.Draw(canvas).rectangle((x, y + s(4), x + s(3), y + h - s(5)), fill=EMBER + (255,))


def pressed_strip(canvas, x, y, w, h):
    """Pushed (pixels): the strip without its bloom, deeper, the tick still lit."""
    across = np.linspace(1, 0, w) ** 1.2
    strip = Image.new("L", (w, h))
    strip.putdata([int(120 * a) for _ in range(h) for a in across])
    canvas.alpha_composite(solid((w, h), (60, 28, 10), strip), (x, y))
    edge = Image.new("L", (w, h))
    ImageDraw.Draw(edge).rectangle((0, 0, w - 1, h - 1), outline=170, width=s(2))
    canvas.alpha_composite(solid((w, h), EMBER, edge), (x, y))
    ImageDraw.Draw(canvas).rectangle((x, y + s(4), x + s(3), y + h - s(5)), fill=EMBER + (255,))


def label(canvas, text, x, y, size, colour, style="Bold Condensed", shade=True):
    """`text` at (x, y) pixels, `size` on the grid."""
    mask = lean(text_mask(text, size, style))
    if shade:                                              # keeps type readable over light blue
        canvas.alpha_composite(solid(mask.size, INK, mask.filter(ImageFilter.GaussianBlur(s(4)))),
                               (x + s(2), y + s(3)))
    canvas.alpha_composite(solid(mask.size, colour, mask), (x, y))


def glass_title(canvas, text, x, y, size):
    """Glass letters (pixels, grid size): dark glass with a bright upper edge, an ice rim and a
    band of light."""
    mask = lean(text_mask(text, size), 0.16)
    w, h = mask.size
    body = to_image(np.repeat(stops([(0, (244, 250, 255)), (0.46, (130, 170, 224)), (0.5, (14, 34, 78)),
                                     (1, (84, 130, 196))], h)[:, None, :], w, 1))
    band = Image.new("L", (w, h))                          # the light caught across the letters
    ImageDraw.Draw(band).polygon([(w * 0.22, 0), (w * 0.32, 0), (w * 0.24, h), (w * 0.14, h)], fill=110)
    body = ImageChops.add(body, Image.merge("RGB", [band.filter(ImageFilter.GaussianBlur(s(6)))] * 3))
    rim = ImageChops.subtract(mask.filter(ImageFilter.MaxFilter(2 * s(2) + 1)), mask)
    canvas.alpha_composite(solid((w, h), INK, mask.filter(ImageFilter.GaussianBlur(s(10)))), (x + s(4), y + s(8)))
    canvas.alpha_composite(solid((w, h), ICE, rim.filter(ImageFilter.GaussianBlur(s(6)))), (x, y))
    letters = body.convert("RGBA")
    letters.putalpha(mask)
    canvas.alpha_composite(letters, (x, y))
    canvas.alpha_composite(solid((w, h), (230, 242, 255), rim), (x, y))


def chain_line(canvas, x0, x1, y, pitch):
    """A drive chain drawn in hairlines (pixels) - outer and inner link plates in turn, a pin at
    each joint: the original's chain, as a fine rule."""
    layer = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    for i, x in enumerate(range(x0, x1, pitch)):
        half = s(9) if i % 2 == 0 else s(6)                # outer plates stand taller
        d.rounded_rectangle((x - s(4), y - half, x + pitch + s(4), y + half), half,
                            outline=ICE + (230,), width=s(2))
    for x in range(x0, x1 + 1, pitch):
        d.ellipse((x - s(3), y - s(3), x + s(3), y + s(3)), fill=(236, 246, 255, 255))
    canvas.alpha_composite(layer.filter(ImageFilter.GaussianBlur(s(4))))
    canvas.alpha_composite(layer)
