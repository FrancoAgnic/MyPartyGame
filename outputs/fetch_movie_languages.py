import urllib.request,urllib.error,re,json,time,csv
from pathlib import Path
from html import unescape
out=Path('outputs/movie_languages');out.mkdir(exist_ok=True)
mapping=dict(line.split('|') for line in Path('outputs/movie_ids_200.txt').read_text(encoding='utf-8').splitlines())
rows=list(csv.reader(open('outputs/Bancos_200/WB_Peliculas_200.csv',encoding='utf-8-sig')))
assert set(mapping)=={r[0] for r in rows}
for i,(title,mid) in enumerate(mapping.items()):
    p=out/f'{mid}.json'
    if p.exists():continue
    req=urllib.request.Request(f'https://www.themoviedb.org/movie/{mid}/translations',headers={'User-Agent':'Mozilla/5.0'})
    try: html=urllib.request.urlopen(req,timeout=30).read().decode('utf-8')
    except urllib.error.HTTPError as ex:
        print('HTTP error',ex.code,'at',i,flush=True);break
    result={}
    for lang in ['en-US','pt-BR','de-DE','fr-FR','it-IT']:
        match=re.search(r'<div id="'+lang+r'"[^>]*>(.*?)(?=<div id="[a-z]{2}-[A-Z]{2}"|$)',html,re.S)
        heading=re.search(r'<h3[^>]*>(.*?)</h3>',match[1],re.S) if match else None
        text=unescape(re.sub('<[^>]*>','',heading[1])).strip() if heading else ''
        result[lang]=text if text!='—' else ''
    p.write_text(json.dumps(result,ensure_ascii=False),encoding='utf-8')
    if (i+1)%10==0:print('Movies retrieved:',i+1,flush=True)
    time.sleep(0.6)
print('Cached movies:',len(list(out.glob('*.json'))),flush=True)
