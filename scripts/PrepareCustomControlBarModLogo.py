# Reborn: Prepare an uncompressed HD mod-logo layer and remove only its baked DDS copies.
from pathlib import Path
from PIL import Image
from io import BytesIO
import argparse

#-------------------------------------------------------------------------------------------------
# Reborn: Keep the HD source at native detail in a power-of-two 32-bit TGA with transparent gutters.
#-------------------------------------------------------------------------------------------------
def prepare_logo(source, target):
    logo=Image.open(source).convert('RGBA')
    bounds=logo.getchannel('A').point(lambda alpha:255 if alpha>=8 else 0).getbbox()
    logo=logo.crop(bounds)
    logo=logo.resize((2044,178),Image.Resampling.LANCZOS)
    texture=Image.new('RGBA',(2048,256),(0,0,0,0))
    texture.paste(logo,(2,2))
    target.parent.mkdir(parents=True,exist_ok=True)
    texture.save(target,format='TGA',compression=None)
    data=target.read_bytes()
    assert data[2]==2 and data[16]==32
    print(target.name,'PASS: 2048x256 uncompressed 32-bit RGBA, no DDS counterpart')

#-------------------------------------------------------------------------------------------------
# Reborn: Restore uninterrupted same-row grid beneath only the bottom baked mod wordmarks.
#-------------------------------------------------------------------------------------------------
def clear_baked_mod(source,target,scale):
    original=source.read_bytes()
    assert original[:4]==b'DDS ' and original[84:88]==b'DXT5'
    atlas=Image.open(source).convert('RGBA')
    boxes=[tuple(value*scale for value in box) for box in ((36,152,444,184),(36,636,444,672))]
    for left,top,right,bottom in boxes:
        for y in range(top,bottom):
            template_y=200*scale+y%(4*scale)
            base=atlas.getpixel((20*scale,y))
            template_base=atlas.getpixel((20*scale,template_y))
            for x in range(left,right):
                pattern=atlas.getpixel((40*scale+x%(40*scale),template_y))
                rgb=tuple(min(255,base[channel]+max(0,pattern[channel]-template_base[channel])) for channel in range(3))
                atlas.putpixel((x,y),(*rgb,base[3]))
    encoded=BytesIO()
    atlas.save(encoded,format='DDS',pixel_format='DXT5')
    encoded=encoded.getvalue()
    assert len(encoded)==len(original)
    data=bytearray(original)
    for row in range(atlas.height//4):
        for col in range(atlas.width//4):
            x,y=col*4,row*4
            offset=128+(row*(atlas.width//4)+col)*16
            if any(x<right and x+4>left and y<bottom and y+4>top for left,top,right,bottom in boxes):
                data[offset:offset+16]=encoded[offset:offset+16]
            else:
                assert data[offset:offset+16]==original[offset:offset+16]
    target.write_bytes(data)
    print(target.name,'PASS: baked mod text removed; every other DDS block preserved')

#-------------------------------------------------------------------------------------------------
# Reborn: Stage both themes and art sets without modifying source logos or installed files.
#-------------------------------------------------------------------------------------------------
def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--repo',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    prepare_logo(args.repo/'build/shared/Art/Textures/RebornOmegaLogo_HD.png',args.output/'RebornOmegaLogoHD_ControlBarPro.tga')
    for size,scale in ((1080,1),(2160,2)):
        for gen in (False,True):
            name=(f'RebornCBP_{size}_QuitWindowProGen_1024_2048.dds' if gen else f'RebornCBP_{size}_quitwindowpro_1024_2048.dds')
            clear_baked_mod(args.repo/'build/shared/Art/Textures'/name,args.output/name,scale)

if __name__=='__main__':
    main()
