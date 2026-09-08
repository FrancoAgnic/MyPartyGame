from pathlib import Path
from urllib.request import urlopen, Request
from io import BytesIO
from PIL import Image

root = Path(__file__).resolve().parent
out = root.parent.parent / 'Content' / 'UI' / 'Flags'
out.mkdir(parents=True, exist_ok=True)
for language, country in {'ES':'es','EN':'gb','PT':'pt','DE':'de','FR':'fr','IT':'it'}.items():
    url = f'https://flagcdn.com/w320/{country}.png'
    source = root / f'{country}_source.png'
    if not source.exists():
        source.write_bytes(urlopen(Request(url, headers={'User-Agent':'FlagAssetExport/1.0'}),timeout=30).read())
    flag = Image.open(BytesIO(source.read_bytes())).convert('RGBA')
    flag.thumbnail((64,64), Image.Resampling.LANCZOS)
    canvas = Image.new('RGBA',(64,64),(0,0,0,0))
    canvas.paste(flag,((64-flag.width)//2,(64-flag.height)//2))
    target=out / f'Flag_{language}.png'
    canvas.save(target)
    with Image.open(target) as check:
        assert check.size == (64,64) and check.mode == 'RGBA'
    print(f'{language}: 64 x 64 RGBA')
