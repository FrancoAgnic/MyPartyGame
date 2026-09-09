from pathlib import Path
from urllib.request import urlopen, Request
from PIL import Image

root = Path(__file__).resolve().parent
out = root.parent.parent / 'Content' / 'UI' / 'Flags'
for language, country in {'ES':'es','EN':'gb','PT':'pt','DE':'de','FR':'fr','IT':'it'}.items():
    source = root / f'{country}_source_1280.png'
    if not source.exists():
        source.write_bytes(urlopen(Request(f'https://flagcdn.com/w1280/{country}.png',
            headers={'User-Agent':'FlagAssetExport/1.0'}), timeout=30).read())
    flag = Image.open(source).convert('RGBA')
    assert flag.width >= 512
    flag.thumbnail((512,512), Image.Resampling.LANCZOS)
    canvas = Image.new('RGBA',(512,512),(0,0,0,0))
    canvas.paste(flag,((512-flag.width)//2,(512-flag.height)//2))
    target = out / f'Flag_{language}_512.png'
    canvas.save(target)
    with Image.open(target) as check:
        assert check.size == (512,512) and check.mode == 'RGBA'
    print(f'{language}: 512 x 512 RGBA from {Image.open(source).size}')
