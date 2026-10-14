"""The modern main menu's art, drawn from nothing: mainmenu_art.py <out.png>

1920x1080. The Glassed look - text over the scene, smoked glass, hairline edges, the chosen item
on a strip of clear glass - in MCM2's colours: a purple dusk over dirt-track country, gold for
the chain's metal, ice blue for the readouts.
"""
import math
import sys

import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

W, H = 1920, 1080
FONT = r"C:\Windows\Fonts\bahnschrift.ttf"
WHITE = (255, 255, 255)
GOLD = (236, 190, 92)
EMBER = (255, 168, 64)          # the glow round the chosen item
ICE = (170, 215, 255)
INK = (8, 6, 18)


def font(size, style="Bold Condensed"):
    f = ImageFont.truetype(FONT, size)
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


def text_mask(text, size, style="Bold Condensed", pad=16):
    f = font(size, style)
    box = f.getbbox(text)
    mask = Image.new("L", (box[2] - box[0] + pad * 2, box[3] - box[1] + pad * 2))
    ImageDraw.Draw(mask).text((pad - box[0], pad - box[1]), text, font=f, fill=255)
    return mask


def scene():
    """Dusk over dirt-track country: purple sky, a low gold sun behind haze, hills, a jump."""
    sky = stops([(0, (10, 6, 28)), (0.35, (40, 20, 74)), (0.58, (104, 52, 112)), (0.68, (196, 112, 98)),
                 (0.72, (236, 168, 104)), (1, (236, 168, 104))], H)
    img = to_image(np.repeat(sky[:, None, :], W, 1))
    d = ImageDraw.Draw(img)

    sun = Image.new("L", (W, H))                           # the sun's bloom, low on the right
    ImageDraw.Draw(sun).ellipse((1320, 600, 1520, 800), fill=255)
    img = Image.composite(Image.new("RGB", (W, H), (255, 214, 150)), img,
                          sun.filter(ImageFilter.GaussianBlur(90)))
    d = ImageDraw.Draw(img)

    def ridge(base, waves, colour):
        points = [(0, H)]
        for x in range(0, W + 20, 20):
            points.append((x, int(base + sum(a * math.sin(x * f + p) for a, f, p in waves))))
        points.append((W, H))
        d.polygon(points, fill=colour)

    ridge(760, [(28, 0.003, 0.4), (12, 0.009, 2.0)], (70, 40, 86))       # far hills in haze
    ridge(820, [(20, 0.004, 1.7), (10, 0.013, 0.3)], (40, 22, 52))
    ridge(890, [(14, 0.005, 3.1), (6, 0.02, 1.0)], (20, 12, 28))          # the track's banks

    jump = [(960, 900)]                                                   # a tabletop: lip, deck, landing
    jump += [(960 + i * 14, 900 - int(110 * (i / 18) ** 1.6)) for i in range(19)]
    jump += [(1380, 790), (1400, 794)]
    jump += [(1400 + i * 18, 794 + int(106 * (i / 14) ** 0.8)) for i in range(15)]
    d.polygon(jump, fill=(14, 8, 20))
    for x in range(560, 1900, 220):                                        # stadium lights' bloom
        d.line([(x, 640), (x, 900)], fill=(16, 10, 24), width=6)
        d.rectangle((x - 18, 630, x + 18, 642), fill=(255, 236, 200))
    lights = img.filter(ImageFilter.GaussianBlur(16))
    img = ImageChops.screen(img, ImageChops.multiply(lights, Image.new("RGB", (W, H), (120, 100, 80))))

    rng = np.random.default_rng(11)                                       # dust caught in the light
    dust = Image.new("L", (W, H))
    dd = ImageDraw.Draw(dust)
    for _ in range(700):
        x, y = rng.integers(0, W), int(560 + rng.beta(2, 3) * 420)
        r = rng.random() * 2.2
        dd.ellipse((x - r, y - r, x + r, y + r), fill=int(rng.integers(40, 160)))
    img = Image.composite(Image.new("RGB", (W, H), (255, 220, 170)), img, dust.filter(ImageFilter.GaussianBlur(1)))
    return img.convert("RGBA")


def smoked_glass(canvas, box, radius=0, tint=(14, 10, 26), dim=0.55, edge=GOLD):
    """Smoked glass over `box`: what is behind it blurred and darkened, a sheen across the top,
    a gold hairline round it."""
    x0, y0, x1, y1 = box
    behind = canvas.crop(box).filter(ImageFilter.GaussianBlur(18)).convert("RGB")
    dark = Image.blend(behind, Image.new("RGB", behind.size, tint), dim)
    sheen = stops([(0, (255, 255, 255)), (0.18, (0, 0, 0)), (1, (0, 0, 0))], y1 - y0)
    sheen = to_image(np.repeat(sheen[:, None, :], x1 - x0, 1) * 0.08)
    glass = ImageChops.add(dark, sheen).convert("RGBA")
    mask = Image.new("L", glass.size)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, x1 - x0 - 1, y1 - y0 - 1), radius, fill=255)
    canvas.paste(glass, (x0, y0), mask)
    line = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    ImageDraw.Draw(line).rounded_rectangle(box, radius, outline=edge + (150,), width=1)
    canvas.alpha_composite(line)


def chosen_strip(canvas, x, y, w, h):
    """The chosen item's ground: a strip of clear glass fading to the right, a gold tick at its
    left, and a warm light along its edges."""
    across = np.linspace(1, 0, w) ** 1.6
    strip = Image.new("L", (w, h))
    strip.putdata([int(150 * a) for _ in range(h) for a in across])
    edge = Image.new("L", (w, h))
    ImageDraw.Draw(edge).rectangle((0, 0, w - 1, h - 1), outline=255, width=3)
    reach = Image.new("L", (w, h))                         # the edge stays lit most of the way
    reach.putdata([int(255 * min(1, (1 - i / w) * 1.6)) for _ in range(h) for i in range(w)])
    edge = ImageChops.multiply(edge, reach)

    glow = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    glow.paste(solid((w, h), EMBER, edge), (x, y))
    for radius, times in ((22, 2), (9, 2), (3, 1)):        # a wide warm bloom, then a tight one
        bloom = glow.filter(ImageFilter.GaussianBlur(radius))
        for _ in range(times):
            canvas.alpha_composite(bloom)
    canvas.alpha_composite(solid((w, h), (255, 236, 210), strip.point(lambda v: v // 4)), (x, y))
    canvas.alpha_composite(solid((w, h), (255, 230, 190), edge), (x, y))
    ImageDraw.Draw(canvas).rectangle((x, y + 4, x + 3, y + h - 5), fill=GOLD + (255,))


def label(canvas, text, x, y, size, colour, style="Bold Condensed", shade=True):
    mask = lean(text_mask(text, size, style))
    if shade:                                              # keeps type readable over bright sky
        canvas.alpha_composite(solid(mask.size, INK, mask.filter(ImageFilter.GaussianBlur(4))), (x + 2, y + 3))
    canvas.alpha_composite(solid(mask.size, colour, mask), (x, y))


def glass_title(canvas, text, x, y, size):
    """Glass letters: dark glass with a bright upper edge, a gold rim and a band of light."""
    mask = lean(text_mask(text, size), 0.16)
    w, h = mask.size
    body = to_image(np.repeat(stops([(0, (250, 240, 255)), (0.46, (150, 120, 190)), (0.5, (40, 26, 70)),
                                     (1, (120, 90, 160))], h)[:, None, :], w, 1))
    band = Image.new("L", (w, h))                          # the light caught across the letters
    ImageDraw.Draw(band).polygon([(w * 0.22, 0), (w * 0.32, 0), (w * 0.24, h), (w * 0.14, h)], fill=110)
    body = ImageChops.add(body, Image.merge("RGB", [band.filter(ImageFilter.GaussianBlur(6))] * 3))
    rim = ImageChops.subtract(mask.filter(ImageFilter.MaxFilter(5)), mask)
    canvas.alpha_composite(solid((w, h), INK, mask.filter(ImageFilter.GaussianBlur(10))), (x + 4, y + 8))
    canvas.alpha_composite(solid((w, h), GOLD, rim.filter(ImageFilter.GaussianBlur(6))), (x, y))
    letters = body.convert("RGBA")
    letters.putalpha(mask)
    canvas.alpha_composite(letters, (x, y))
    canvas.alpha_composite(solid((w, h), (255, 225, 160), rim), (x, y))


def chain_line(canvas, x0, x1, y, pitch=30):
    """A drive chain drawn in hairlines - outer and inner link plates in turn, a pin at each
    joint: the original's chain, as a fine rule."""
    layer = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    for i, x in enumerate(range(x0, x1, pitch)):
        half = 9 if i % 2 == 0 else 6                      # outer plates stand taller
        d.rounded_rectangle((x - 4, y - half, x + pitch + 4, y + half), half, outline=GOLD + (230,), width=2)
    for x in range(x0, x1 + 1, pitch):
        d.ellipse((x - 3, y - 3, x + 3, y + 3), fill=(255, 228, 170, 255))
    canvas.alpha_composite(layer.filter(ImageFilter.GaussianBlur(4)))
    canvas.alpha_composite(layer)


def mainmenu():
    canvas = scene()

    smoked_glass(canvas, (0, 0, 620, H), edge=GOLD)                     # the menu's pane
    glass_title(canvas, "MOTOCROSS", 1000, 70, 136)
    glass_title(canvas, "MADNESS", 1060, 196, 136)
    glass_title(canvas, "2", 1560, 54, 300)
    chain_line(canvas, 1010, 1590, 352)

    items = ["SINGLE PLAYER", "MULTIPLAYER", "OPTIONS", "RIDER PROFILE", "HIGH SCORES", "REPLAYS",
             "EXIT"]
    for i, text in enumerate(items):
        y = 210 + i * 86
        if i == 0:
            chosen_strip(canvas, 60, y - 6, 520, 66)
        label(canvas, text, 92, y, 44, WHITE if i == 0 else (225, 214, 236))
    ImageDraw.Draw(canvas).line([(92, 180), (560, 180)], fill=GOLD + (170,), width=1)
    label(canvas, "MAIN MENU", 92, 126, 30, GOLD, "SemiBold Condensed", shade=False)

    smoked_glass(canvas, (660, 190, 1060, 640), radius=14, edge=ICE)     # the event readout
    label(canvas, "SELECT EVENT", 694, 214, 28, ICE, "SemiBold Condensed", shade=False)
    ImageDraw.Draw(canvas).line([(694, 258), (1026, 258)], fill=ICE + (140,), width=1)
    for i, event in enumerate(["BAJA", "STUNT QUARRY", "ENDURO", "NATIONALS", "SUPERCROSS",
                               "PRO-CIRCUIT"]):
        y = 278 + i * 58
        if i == 3:
            chosen_strip(canvas, 680, y - 4, 360, 52)
        label(canvas, event, 704, y, 36, WHITE if i == 3 else (200, 220, 245))

    smoked_glass(canvas, (0, H - 70, W, H), edge=GOLD)                   # the status strip
    label(canvas, "RIDER", 92, H - 58, 22, GOLD, "SemiBold Condensed", shade=False)
    label(canvas, "PPPOOPOO", 168, H - 62, 32, WHITE, shade=False)
    label(canvas, "HELP", W - 330, H - 62, 32, (225, 214, 236), shade=False)
    label(canvas, "CREDITS", W - 210, H - 62, 32, (225, 214, 236), shade=False)
    return canvas


if __name__ == "__main__":
    mainmenu().convert("RGB").save(sys.argv[1])
