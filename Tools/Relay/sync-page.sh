#!/bin/sh
# Copia la página del celular (fuente única: Content/LocalParty/Web/index.html) al relay.
# Correr antes de subir la carpeta a la VPS si se cambió la página.
cd "$(dirname "$0")" && cp ../../Content/LocalParty/Web/index.html public/index.html && echo "public/index.html actualizado"
