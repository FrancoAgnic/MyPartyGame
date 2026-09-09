from pathlib import Path
from PIL import Image, ImageDraw
import math

ROOT = Path(__file__).resolve().parent
OUT = ROOT.parent.parent / 'Content' / 'UI' / 'NetworkStatus'
OUT.mkdir(parents=True, exist_ok=True)
S = 8
INK = '#171D29'

def make(name, color, shapes):
    image = Image.new('RGBA', (24*S,24*S))
    draw = ImageDraw.Draw(image)
    svg = ['<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24">']
    def line(points, width, col):
        coords = [(round(x*S),round(y*S)) for x,y in points]
        draw.line(coords,fill=col,width=round(width*S),joint='curve')
        for x,y in coords:
            r=width*S/2
            draw.ellipse((x-r,y-r,x+r,y+r),fill=col)
        svg.append(f'<polyline points="{" ".join(f"{x},{y}" for x,y in points)}" fill="none" stroke="{col}" stroke-width="{width}" stroke-linecap="round" stroke-linejoin="round"/>')
    for kind, data in shapes:
        if kind=='line':
            line(data,4.1,INK);line(data,2.2,color)
        elif kind=='circle':
            x,y,r=data
            points=[(x+r*math.cos(a*math.pi/32),y+r*math.sin(a*math.pi/32)) for a in range(65)]
            line(points,4.1,INK);line(points,2.2,color)
        elif kind=='box':
            x,y,w,h=data
            draw.rounded_rectangle(((x-.8)*S,(y-.8)*S,(x+w+.8)*S,(y+h+.8)*S),radius=1.5*S,fill=INK)
            draw.rounded_rectangle((x*S,y*S,(x+w)*S,(y+h)*S),radius=.65*S,fill=color)
            svg.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx=".65" fill="{color}" stroke="{INK}" stroke-width="1.2" paint-order="stroke"/>')
        elif kind=='dot':
            x,y,r=data
            draw.ellipse(((x-r-.8)*S,(y-r-.8)*S,(x+r+.8)*S,(y+r+.8)*S),fill=INK)
            draw.ellipse(((x-r)*S,(y-r)*S,(x+r)*S,(y+r)*S),fill=color)
            svg.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="{color}" stroke="{INK}" stroke-width="1.2" paint-order="stroke"/>')
    svg.append('</svg>')
    (ROOT / (name+'.svg')).write_text('\n'.join(svg))
    image=image.resize((24,24),Image.Resampling.LANCZOS)
    image.save(OUT/(name+'.png'))
    assert image.getbbox() and image.getpixel((0,0))[3]==0
    return image

icons = [
    make('T_Net_HighLatency_24','#FFD34E',[
        ('circle',(12,12,8)),('line',[(12,7),(12,12),(16,14)])]),
    make('T_Net_Disconnected_24','#FF5964',[
        ('line',[(3,8),(5,6.5),(8,5.1),(12,4.5),(16,5.1),(19,6.5),(21,8)]),
        ('line',[(6,12),(8,10.5),(12,9.5),(16,10.5),(18,12)]),
        ('dot',(12,18,1.65)),('line',[(4,3),(21,20)])]),
    make('T_Net_PacketLoss_24','#FF9B40',[
        ('box',(2,6,5,6)),('box',(17,6,5,6)),
        ('line',[(10,6),(10,7)]),('line',[(14,6),(14,7)]),
        ('line',[(10,11),(10,12)]),('line',[(14,11),(14,12)]),
        ('line',[(12,15),(12,20)]),('line',[(9.5,18),(12,20.5),(14.5,18)])])
]

preview=Image.new('RGB',(660,260),'#101723')
d=ImageDraw.Draw(preview)
labels=['LATENCIA ALTA','SIN CONEXION','PERDIDA DE PAQUETES']
for i,(icon,label) in enumerate(zip(icons,labels)):
    x=i*220
    preview.paste(icon.resize((96,96),Image.Resampling.NEAREST),(x+62,20),icon.resize((96,96),Image.Resampling.NEAREST))
    d.text((x+28,132),label,fill='white')
    preview.paste(icon,(x+98,162),icon)
    d.rectangle((x+12,202,x+207,248),fill='#F0EEE7')
    preview.paste(icon,(x+98,213),icon)
preview.save(ROOT/'Preview_NetworkStatus.png')
print('Created 3 transparent 24x24 PNGs and editable SVG sources.')
