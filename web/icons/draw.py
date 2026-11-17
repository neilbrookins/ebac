"""EBaC's icon for the web page's home-screen shortcut (web/README.md):
the red Bible with a gold cross of the Home Assistant app's icon, drawn as
ebac-ha-integration's tools/draw_icon.py draws it, at any size, here on a
white tile, so it is the same picture on an iPhone and on Android:

    python3 web/icons/draw.py

writes, beside this script:
    icon-180.png        iPhone and iPad (apple-touch-icon), and the tab's
    icon-192.png        Android, and the browser's tab
    icon-512.png        Android (its install dialog, its splash screen)
    maskable-512.png    Android's own shapes (circle, squircle): the Bible
                        smaller, within the middle 80% they all keep

Coordinates are in the 128-pixel units of the app's icon.
"""
import os
from PIL import Image, ImageDraw

COVER, SPINE, OUTLINE = (122, 30, 36), (95, 20, 26), (71, 15, 20)
PAGES, PAGE_EDGE, GOLD = (243, 234, 208), (123, 103, 73), (230, 185, 70)
TILE = (255, 255, 255)

def bible(size):
    ss = 4                                  # supersampling
    k = size * ss / 128
    im = Image.new("RGBA", (size * ss, size * ss), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    s = lambda *v: [round(x * k) for x in v]
    d.rounded_rectangle(s(24, 20, 115, 117), radius=round(6 * k), fill=PAGES,
                        outline=PAGE_EDGE, width=round(1.2 * k))
    d.rounded_rectangle(s(15.5, 15.5, 108, 110), radius=round(8 * k), fill=COVER,
                        outline=OUTLINE, width=round(1.5 * k))
    d.rectangle(s(25, 16.5, 30, 109), fill=SPINE)
    d.rectangle(s(64, 30, 75.5, 90), fill=GOLD)
    d.rectangle(s(49, 41, 91, 52.5), fill=GOLD)
    return im.resize((size, size), Image.LANCZOS)

def tile(size, scale):
    """the Bible, SCALE of the tile's width, centred on white"""
    im = Image.new("RGB", (size, size), TILE)
    b = bible(round(size * scale))
    at = (size - b.width) // 2
    im.paste(b, (at, at), b)
    return im

here = os.path.dirname(os.path.abspath(__file__))
# (the app's icon fills its square edge to edge; a home screen's tile wants
# a margin: 0.86, as the app's own margin, 15 of 128, is about 12% a side)
for size, scale, name in [(180, 0.86, "icon-180.png"), (192, 0.86, "icon-192.png"),
                          (512, 0.86, "icon-512.png"), (512, 0.68, "maskable-512.png")]:
    tile(size, scale).save(os.path.join(here, name), optimize=True)
