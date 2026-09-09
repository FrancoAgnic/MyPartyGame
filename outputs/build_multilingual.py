import csv,json,re,unicodedata
from pathlib import Path
root=Path('outputs')
langs=['en-US','pt-BR','de-DE','fr-FR','it-IT']
movie={}
for line in (root/'movie_ids_200.txt').read_text(encoding='utf-8').splitlines():
    es,mid=line.split('|'); d=json.loads((root/'movie_languages'/f'{mid}.json').read_text(encoding='utf-8'))
    original={'Metrópolis':'Metropolis','Amores perros':'Amores perros','Nosotros los nobles':'Nosotros los nobles'}.get(es,d['en-US'])
    vals=[d[l] or original for l in langs]
    vals=[re.split(r'\s[-–]\s|\s*:\s*',v)[0].strip() for v in vals]
    movie[es]=[es]+vals
overrides='''Alien|Alien|Alien|Alien|Alien|Alien
Amélie|Amélie|Amélie|Amélie|Amélie|Amélie
Avengers|Avengers|Vingadores|Avengers|Avengers|Avengers
Batman|Batman|Batman|Batman|Batman|Batman
Birdman|Birdman|Birdman|Birdman|Birdman|Birdman
Blancanieves|Snow White|Branca de Neve|Schneewittchen|Blanche-Neige|Biancaneve
Capitán América|Captain America|Capitão América|Captain America|Captain America|Captain America
Carrie|Carrie|Carrie|Carrie|Carrie|Carrie
Cars|Cars|Carros|Cars|Cars|Cars
Casino Royale|Casino Royale|Cassino Royale|Casino Royale|Casino Royale|Casino Royale
Casper|Casper|Gasparzinho|Casper|Casper|Casper
Click|Click|Click|Klick|Click|Cambia la tua vita con un click
Coco|Coco|Viva|Coco|Coco|Coco
Coraline|Coraline|Coraline|Coraline|Coraline|Coraline
Crepúsculo|Twilight|Crepúsculo|Twilight|Twilight|Twilight
Drácula|Dracula|Drácula|Dracula|Dracula|Dracula
El Grinch|The Grinch|O Grinch|Der Grinch|Le Grinch|Il Grinch
El Perfecto Asesino|Léon|O Profissional|Léon|Léon|Léon
Encanto|Encanto|Encanto|Encanto|Encanto|Encanto
Escuela de Rock|School of Rock|Escola de Rock|School of Rock|Rock Academy|School of Rock
E.T.|E.T.|E.T.|E.T.|E.T.|E.T.
Hulk|Hulk|Hulk|Hulk|Hulk|Hulk
Harry Potter|Harry Potter|Harry Potter|Harry Potter|Harry Potter|Harry Potter
Hombres de negro|Men in Black|Homens de Preto|Men in Black|Men in Black|Men in Black
Indiana Jones|Indiana Jones|Indiana Jones|Indiana Jones|Indiana Jones|Indiana Jones
It|It|It|Es|Ça|It
Kill Bill|Kill Bill|Kill Bill|Kill Bill|Kill Bill|Kill Bill
La Cenicienta|Cinderella|Cinderela|Aschenputtel|Cendrillon|Cenerentola
Los Cazafantasmas|Ghostbusters|Os Caça-Fantasmas|Ghostbusters|S.O.S. Fantômes|Ghostbusters
Misión imposible|Mission Impossible|Missão Impossível|Mission Impossible|Mission Impossible|Mission Impossible
Moana|Moana|Moana|Vaiana|Vaiana|Oceania
Piratas del Caribe|Pirates of the Caribbean|Piratas do Caribe|Fluch der Karibik|Pirates des Caraïbes|Pirati dei Caraibi
Pocahontas|Pocahontas|Pocahontas|Pocahontas|Pocahontas|Pocahontas
Rambo|Rambo|Rambo|Rambo|Rambo|Rambo
Sonic|Sonic|Sonic|Sonic|Sonic|Sonic
Star Wars|Star Wars|Star Wars|Star Wars|Star Wars|Star Wars
Super Mario Bros.|Super Mario Bros.|Super Mario Bros.|Super Mario Bros.|Super Mario Bros.|Super Mario Bros.
Terminator|Terminator|O Exterminador do Futuro|Terminator|Terminator|Terminator
WALL-E|WALL-E|WALL-E|WALL-E|WALL-E|WALL-E'''
for line in overrides.splitlines():
    cells=line.split('|');movie[cells[0]]=cells
movie['Bichos'][3]='Das große Krabbeln'
movie['Arma Mortal'][3]='Lethal Weapon'
movie['Nosotros los nobles'][2]='Los Nobles'
movie['La Máscara'][2]='O Máskara'
movie['La Máscara'][5]='The Mask'
movie['Los increíbles'][5]='Gli Incredibili'
movie['Karate Kid'][5]='The Karate Kid'
tables={'Peliculas':movie}
for kind,name in [('Verbos','verbs'),('Variado','varied')]:
    cells=[line.split('|') for line in (root/f'{name}_translations.txt').read_text(encoding='utf-8').splitlines()]
    assert all(len(r)==6 for r in cells)
    tables[kind]={r[0]:r for r in cells}
tables['Verbos']['abrazar'][4]='prendre dans ses bras'
tables['Verbos']['guisar'][2]='cozinhar ensopado'
out=root/'Bancos_200_6_idiomas';out.mkdir(exist_ok=True)
header=next(csv.reader(open('C:/Users/franc/Desktop/plantilla_palabras (1).csv',encoding='utf-8-sig')))
assert header==['ES','EN','PT','DE','FR','IT']
def norm(s):return ''.join(c for c in unicodedata.normalize('NFD',s.casefold()) if not unicodedata.combining(c))
for kind,table in tables.items():
    source=list(csv.reader(open(root/'Bancos_200'/f'WB_{kind}_200.csv',encoding='utf-8-sig')))
    rows=[table[r[0]] for r in source]
    assert len(rows)==200 and len({norm(r[0]) for r in rows})==200
    assert all(len(r)==6 and all(c.strip() for c in r) for r in rows)
    assert not any(any(bad in c for bad in ['\ufffd','Ã¡','Ã©','Ã±','Â¿']) for r in rows for c in r)
    path=out/f'WB_{kind}_200_6_idiomas.csv'
    with path.open('w',encoding='utf-8-sig',newline='') as f:
        w=csv.writer(f);w.writerow(header);w.writerows(rows)
    back=list(csv.reader(path.open(encoding='utf-8-sig',newline='')))
    assert back==[header]+rows and path.read_bytes().startswith(b'\xef\xbb\xbf')
    print(f'{path}: 200 rows, 6 complete columns, UTF-8 BOM, Spanish preserved, validated')
