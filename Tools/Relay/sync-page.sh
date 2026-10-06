#!/bin/sh
# Copia la página del celular (fuente única: Content/LocalParty/Web/index.html) al relay.
# Correr antes de subir la carpeta a la VPS si se cambió la página.
cd "$(dirname "$0")" && mkdir -p public/assets && cp ../../Content/LocalParty/Web/index.html public/index.html && cp ../../Content/LocalParty/Web/assets/* public/assets/ && echo "public/ actualizado (index.html + assets)"
