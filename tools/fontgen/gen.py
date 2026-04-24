"""Rasterize one or more TTFs into a BitmapFont header for P-OBC.

Usage: python gen.py

Output: include/Fonts.h, src/Fonts.cpp  (one combined file per font configured).
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

# (ttf_path, symbol_prefix, monospace_digits, target_heights, variation_name)
FONTS = [
    ("DSEG7Classic-Bold.ttf",  "DSEG7C",   True,  [120], None),
    ("DSEG7Modern-Bold.ttf",   "DSEG7M",   True,  [120], None),
    ("Orbitron[wght].ttf",     "ORBITRON", True,  [120], b"Black"),
    ("Teko[wght].ttf",         "TEKO",     True,  [120], b"Bold"),
]
CHARS = "0123456789:.-/ "
OUT_HEADER = Path("../../include/Fonts.h")
OUT_SOURCE = Path("../../src/Fonts.cpp")


def measure_digit_height(font):
    img = Image.new("L", (font.size * 3, font.size * 3), 0)
    d = ImageDraw.Draw(img)
    d.text((font.size, font.size), "8", font=font, fill=255)
    bbox = img.getbbox()
    return bbox[3] - bbox[1] if bbox else 0


def load_font(path, size, variation=None):
    f = ImageFont.truetype(path, size)
    if variation is not None:
        f.set_variation_by_name(variation)
    return f


def pick_size_for_digit_height(font_path, target_h, variation=None):
    lo, hi = 8, target_h * 3
    best = lo
    while lo <= hi:
        mid = (lo + hi) // 2
        f = load_font(font_path, mid, variation)
        h = measure_digit_height(f)
        if h <= target_h:
            best = mid
            lo = mid + 1
        else:
            hi = mid - 1
    return best


def rasterize(font, chars, monospace_digits):
    ascent, descent = font.getmetrics()
    line_h = ascent + descent

    canvas_w = font.size * 3
    canvas_h = line_h + font.size

    # First pass: raw rasterization.
    raw = {}
    for ch in chars:
        img = Image.new("L", (canvas_w, canvas_h), 0)
        d = ImageDraw.Draw(img)
        d.text((font.size, 0), ch, font=font, fill=255)
        bbox = img.getbbox()
        advance = int(round(font.getlength(ch)))
        if bbox is None:
            raw[ch] = dict(w=0, h=0, advance=advance, ox=0, oy=0, bits=b"")
            continue
        l, t, r, b = bbox
        w, h = r - l, b - t
        crop = img.crop(bbox).point(lambda p: 255 if p >= 128 else 0, mode="1")
        stride = (w + 7) // 8
        data = bytearray(stride * h)
        px = crop.load()
        for y in range(h):
            for x in range(w):
                if px[x, y]:
                    data[y * stride + (x >> 3)] |= 0x80 >> (x & 7)
        raw[ch] = dict(
            w=w, h=h, advance=advance,
            ox=l - font.size,  # bearing from pen
            oy=t,
            bits=bytes(data),
        )

    # Normalize vertical: align top of tallest digit to y=0, and clamp line_h
    # to the actual digit extent. Variable fonts (Orbitron, Teko) declare large
    # ascent/descent for text use, which would push digits way down on the
    # canvas and leave huge dead space below.
    digit_keys = [c for c in "0123456789" if c in raw and raw[c]["w"] > 0]
    if digit_keys:
        top_offset = min(raw[c]["oy"] for c in digit_keys)
        for g in raw.values():
            g["oy"] -= top_offset
        line_h = max(raw[c]["oy"] + raw[c]["h"] for c in digit_keys)

    # Optional: force digits 0-9 to share a common monospace cell.
    if monospace_digits and digit_keys:
        cell_w = max(raw[c]["advance"] for c in digit_keys)
        cell_w = max(cell_w, max(raw[c]["w"] + raw[c]["ox"] for c in digit_keys))
        for c in digit_keys:
            g = raw[c]
            g["ox"] = (cell_w - g["w"]) // 2
            g["advance"] = cell_w

    return line_h, raw


def emit_bitmaps(s, prefix, target_h, glyphs):
    """Emit static bitmap arrays for one font variant inside its own namespace."""
    first = min(ord(c) for c in glyphs)
    last = max(ord(c) for c in glyphs)
    total = sum(len(g["bits"]) for g in glyphs.values())
    ns = f"ns_{prefix}_{target_h}"

    s.append(f"// === {prefix}_{target_h}  bitmaps={total}B ===")
    s.append(f"namespace {ns} {{")
    s.append("")

    for ch in sorted(glyphs, key=ord):
        g = glyphs[ch]
        if g["w"] == 0:
            continue
        sym = f"bm_{ord(ch):02X}"
        stride = (g["w"] + 7) // 8
        s.append(f"// '{ch}'  {g['w']}x{g['h']}  adv={g['advance']}")
        s.append(f"constexpr uint8_t {sym}[] = {{")
        for y in range(g["h"]):
            row = g["bits"][y * stride:(y + 1) * stride]
            s.append("    " + ", ".join(f"0x{b:02X}" for b in row) + ",")
        s.append("};")
        s.append("")

    s.append("constexpr BitmapGlyph glyphs[] = {")
    for code in range(first, last + 1):
        ch = chr(code)
        if ch in glyphs and glyphs[ch]["w"] > 0:
            g = glyphs[ch]
            sym = f"bm_{code:02X}"
            s.append(
                f"    {{ {g['w']:3d}, {g['h']:3d}, {g['advance']:3d}, "
                f"{g['ox']:3d}, {g['oy']:3d}, {sym} }},  // '{ch}'"
            )
        else:
            adv = glyphs.get(ch, {"advance": 0})["advance"]
            s.append(
                f"    {{   0,   0, {adv:3d},   0,   0, nullptr }},  "
                f"// code {code}"
            )
    s.append("};")
    s.append("")
    s.append(f"}} // namespace {ns}")
    s.append("")
    return first, last, ns


def main():
    header = [
        "#pragma once",
        "",
        '#include "BitmapFont.h"',
        "",
        "// Generated by tools/fontgen/gen.py.",
        "",
    ]
    source = [
        '#include "Fonts.h"',
        "",
        "// Generated by tools/fontgen/gen.py.",
        f"// Chars: {CHARS!r}",
        "",
    ]

    for ttf, prefix, mono, targets, variation in FONTS:
        for target in targets:
            size = pick_size_for_digit_height(ttf, target, variation)
            font = load_font(ttf, size, variation)
            actual = measure_digit_height(font)
            line_h, glyphs = rasterize(font, CHARS, mono)
            name = f"{prefix}_{target}"
            bytes_total = sum(len(g["bits"]) for g in glyphs.values())
            print(f"{name}: font={ttf} size={size} digit_h={actual} "
                  f"line_h={line_h} bytes={bytes_total} mono={mono}")
            first, last, ns = emit_bitmaps(source, prefix, target, glyphs)
            source.append(f"const BitmapFont {name} = {{")
            source.append(f"    {line_h}, {first}, {last}, {ns}::glyphs")
            source.append("};")
            source.append("")
            header.append(f"extern const BitmapFont {name};  // {ttf}")

    header.append("")
    OUT_HEADER.write_text("\n".join(header))
    OUT_SOURCE.write_text("\n".join(source))
    print(f"Wrote {OUT_HEADER}")
    print(f"Wrote {OUT_SOURCE}")


if __name__ == "__main__":
    main()
