# Reborn: Recolor existing ControlBarPro quit DDS backgrounds without generating or recompressing artwork.
from pathlib import Path
import argparse
import struct

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
# Reborn: Build separate Generals atlases while leaving both Zero Hour source atlases untouched.
#-------------------------------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--repo', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    for size in ('1080', '2160'):
        source = args.repo / f'build/shared/Art/Textures/RebornCBP_{size}_quitwindowpro_1024_2048.dds'
        target = args.output / f'RebornCBP_{size}_QuitWindowProGen_1024_2048.dds'
        recolor(source, target)

if __name__ == '__main__':
    main()
