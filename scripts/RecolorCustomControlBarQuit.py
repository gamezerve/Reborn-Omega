# Reborn: Recolor existing ControlBarPro quit DDS backgrounds without generating or recompressing artwork.
from pathlib import Path
import argparse
import struct
from io import BytesIO
from PIL import Image
from collections import deque

#-------------------------------------------------------------------------------------------------
# Reborn: Convert only blue RGB565 endpoints to Generals gold, retaining their original brightness.
#-------------------------------------------------------------------------------------------------
def gold_endpoint(value):
    red = ((value >> 11) & 31) * 255 / 31
    green = ((value >> 5) & 63) * 255 / 63
    blue = (value & 31) * 255 / 31
    if blue <= red + 3 or blue <= green + 3:
        return value
    low, high = min(red, green, blue), max(red, green, blue)
    gold_green = low + (high - low) * (190 / 255)
    return (round(high * 31 / 255) << 11) | (round(gold_green * 63 / 255) << 5) | round(low * 31 / 255)

#-------------------------------------------------------------------------------------------------
# Reborn: Preserve logo blocks, alpha, compression, indices and atlas coordinates byte-for-byte.
#-------------------------------------------------------------------------------------------------
def recolor(source, target):
    original = source.read_bytes()
    data = bytearray(original)
    assert data[:4] == b'DDS ' and data[84:88] == b'DXT5'
    height, width = struct.unpack_from('<II', data, 12)
    assert (width, height) in ((512, 1024), (1024, 2048))
    assert struct.unpack_from('<I', data, 28)[0] in (0, 1)
    scale = width // 512
    logos = [(36 * scale, 36 * scale, 444 * scale, 184 * scale),
             (36 * scale, 520 * scale, 444 * scale, 672 * scale)]
    changed = 0
    for row in range(height // 4):
        for col in range(width // 4):
            x, y = col * 4, row * 4
            if x >= 480 * scale:
                continue
            if any(x < right and x + 4 > left and y < bottom and y + 4 > top
                   for left, top, right, bottom in logos):
                continue
            offset = 128 + (row * (width // 4) + col) * 16
            first, second = struct.unpack_from('<HH', data, offset + 8)
            recolored = (gold_endpoint(first), gold_endpoint(second))
            if recolored != (first, second):
                struct.pack_into('<HH', data, offset + 8, *recolored)
                changed += 1
            assert data[offset:offset+8] == original[offset:offset+8]
            assert data[offset+12:offset+16] == original[offset+12:offset+16]
    assert len(data) == len(original) and data[:128] == original[:128] and changed > 0
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(data)
    print(f'{target.name}: {changed} recolored blocks; dimensions, alpha, logo blocks and DDS size preserved')

#-------------------------------------------------------------------------------------------------
# Reborn: Replace only the old logo regions with the existing Steam Generals logo, preserving all other DDS blocks.
#-------------------------------------------------------------------------------------------------
# Reborn: Remove the faint outer Steam shadow without removing dark metal details enclosed by the logo.
#-------------------------------------------------------------------------------------------------
def clean_logo(logo):
    pixels = list(logo.getdata())
    width, height = logo.size
    outside = bytearray(width * height)
    pending = deque()
    for y in range(height):
        pending.extend((y * width, y * width + width - 1))
    pending.extend(range(width))
    pending.extend(range((height - 1) * width, height * width))
    while pending:
        index = pending.popleft()
        if outside[index]:
            continue
        red, green, blue, alpha = pixels[index]
        if alpha >= 32 and max(red, green, blue) >= 40:
            continue
        outside[index] = 1
        x, y = index % width, index // width
        if x: pending.append(index - 1)
        if x + 1 < width: pending.append(index + 1)
        if y: pending.append(index - width)
        if y + 1 < height: pending.append(index + width)
    logo.putdata([(0, 0, 0, 0) if outside[index] else pixel for index, pixel in enumerate(pixels)])
    return logo.crop(logo.getbbox())

#-------------------------------------------------------------------------------------------------
# Reborn: Align the cleaned visible logo bounds to the same grid rectangle in each quit menu.
#-------------------------------------------------------------------------------------------------
def replace_logo(target, logo_path):
    original = target.read_bytes()
    atlas = Image.open(target).convert('RGBA')
    scale = atlas.width // 512
    logo = Image.open(logo_path).convert('RGBA')
    # Reborn: Use the manually cleaned source alpha unchanged instead of applying another shadow-removal pass.
    logo = logo.crop(logo.getbbox())
    width = 400 * scale
    height = 140 * scale
    logo = logo.resize((width, height), Image.Resampling.LANCZOS)
    assert height <= 148 * scale
    regions = [(36 * scale, 36 * scale, 444 * scale, 184 * scale),
               (36 * scale, 520 * scale, 444 * scale, 672 * scale)]
    for index, rect in enumerate(regions):
        left, top, right, bottom = rect
        # Reborn: Match scanline brightness/alpha to this exact row instead of copying a differently shaded rectangle.
        blank = Image.new('RGBA', (right - left, bottom - top))
        period = 40 * scale
        for y in range(top, bottom):
            # Reborn: Sample only an uninterrupted grid strip; never inherit gaps from the button rows.
            template_y = 200 * scale + y % (4 * scale)
            base = atlas.getpixel((20 * scale, y))
            template_base = atlas.getpixel((20 * scale, template_y))
            for x in range(left, right):
                pattern = atlas.getpixel((40 * scale + x % period, template_y))
                rgb = tuple(min(255, base[channel] + max(0, pattern[channel] - template_base[channel]))
                            for channel in range(3))
                blank.putpixel((x - left, y - top), (*rgb, base[3]))
        atlas.paste(blank, (left, top))
        atlas.alpha_composite(logo, (40 * scale, top + 4 * scale))
    compressed = BytesIO()
    atlas.save(compressed, format='DDS', pixel_format='DXT5')
    encoded = compressed.getvalue()
    assert len(encoded) == len(original)
    data = bytearray(original)
    changed = 0
    for row in range(atlas.height // 4):
        for col in range(atlas.width // 4):
            x, y = col * 4, row * 4
            offset = 128 + (row * (atlas.width // 4) + col) * 16
            affected = any(x < right and x + 4 > left and y < bottom and y + 4 > top
                           for left, top, right, bottom in regions)
            if affected:
                data[offset:offset+16] = encoded[offset:offset+16]
                changed += 1
            else:
                assert data[offset:offset+16] == original[offset:offset+16]
    target.write_bytes(data)
    print(f'{target.name}: Generals HD logo installed in {changed} blocks; all other DDS blocks unchanged')

#-------------------------------------------------------------------------------------------------
# Reborn: Build separate Generals atlases while leaving both Zero Hour source atlases untouched.
#-------------------------------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--repo', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--logo', type=Path)
    args = parser.parse_args()
    for size in ('1080', '2160'):
        source = args.repo / f'build/shared/Art/Textures/RebornCBP_{size}_quitwindowpro_1024_2048.dds'
        target = args.output / f'RebornCBP_{size}_QuitWindowProGen_1024_2048.dds'
        recolor(source, target)
        replace_logo(target, args.logo or args.repo / 'scripts/assets/GeneralsHDLogoSteam.png')

if __name__ == '__main__':
    main()
