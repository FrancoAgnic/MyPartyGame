# Relay del modo online (Sculpturillo)

El juego se conecta a este relay y recibe un **código de sala** (ej. `KQZT`). Los jugadores abren
`https://play.TU-DOMINIO.com/KQZT` en el celular (o escanean el QR de la TV). El relay solo reenvía
mensajes: toda la lógica de la partida corre en el juego. También sirve la página del celular.

Corre en Docker junto a un **Cloudflare Tunnel**:
- **Sin puertos abiertos:** el túnel se conecta hacia afuera a Cloudflare.
- **Sin certificados:** Cloudflare pone el HTTPS.
- **IP oculta:** la de la VPS no queda expuesta.

Consume ~50 MB de RAM y convive sin problema con otros servicios.

## Instalación (una sola vez)

### 1. Crear el túnel en Cloudflare
1. En el panel de Cloudflare, abre **Zero Trust** → **Networks** → **Tunnels** → **Create a tunnel**.
2. Elige el tipo **Cloudflared** y ponle de nombre `sculpturillo-relay`.
3. En "Install and run a connector", copia **el token**: el texto largo que viene después de
   `--token`. No hace falta instalar nada de lo que muestra la página.
4. **Next** → pestaña **Public Hostname**:
   - Subdomain: `play`
   - Domain: tu dominio
   - Service: **HTTP** → `relay:8080`
5. Guarda.

### 2. Subir esta carpeta a la VPS
**Opción A (PowerShell en Windows, recomendada).** Parado en la raíz del repo. En la primera línea va
tu IP; las demás se pegan tal cual. Si tu clave SSH tiene contraseña, te la pide en cada `scp` / `ssh`.

```powershell
$ip = "PONÉ.ACÁ.TU.IP"
tar --exclude=node_modules --exclude=.env -czf "$env:TEMPelay.tgz" -C Tools/Relay .
scp "$env:TEMPelay.tgz" "root@${ip}:/tmp/relay.tgz"
ssh "root@$ip" "mkdir -p /opt/sculpturillo-relay && tar -xzf /tmp/relay.tgz -C /opt/sculpturillo-relay && rm /tmp/relay.tgz && ls /opt/sculpturillo-relay"
```
El último comando lista los archivos que llegaron: `Dockerfile`, `docker-compose.yml`, `server.js`,
`public`, etc.

**Opción B (Git Bash).** Primero la variable con tu IP:

```bash
IP=PONÉ.ACÁ.TU.IP
```

Después, en una línea, crear la carpeta y subir el contenido:

```bash
ssh root@$IP "mkdir -p /opt/sculpturillo-relay" && tar --exclude=node_modules --exclude=.env -czf - -C Tools/Relay . | ssh root@$IP "tar -xzf - -C /opt/sculpturillo-relay"
```

### 3. Levantarlo en la VPS
```bash
cd /opt/sculpturillo-relay
cp .env.example .env
nano .env            # pegar el token en TUNNEL_TOKEN=..., guardar (Ctrl+O, Enter, Ctrl+X)
docker compose up -d --build
```

### 4. Verificar
```bash
docker compose ps                 # relay y tunnel en "running"
docker compose logs -f --tail=50  # Ctrl+C para salir
curl https://play.TU-DOMINIO.com/health   # → {"ok":true,"rooms":0}
```

### 5. Configurar el juego
En `Config/DefaultGame.ini`:
```ini
[LocalParty]
RelayUrl=wss://play.TU-DOMINIO.com
```

## Actualizar
Si cambió la página del celular (`Content/LocalParty/Web/index.html`) o el relay:
1. En la PC, corre `sh Tools/Relay/sync-page.sh`.
2. Repite el paso 2 (subir la carpeta).
3. En la VPS:
   ```bash
   cd /opt/sculpturillo-relay && docker compose up -d --build
   ```

## Útil
- **Ver logs:** `docker compose logs -f relay`
- **Reiniciar:** `docker compose restart`
- **Apagar:** `docker compose down`
- **Límites:** en `.env` (`MAX_ROOMS`, `MAX_PHONES_PER_ROOM`, `HOST_GRACE_SECONDS`).
- **Probar en local, sin Cloudflare:** `npm install && node server.js`, y lanzar el juego con
  `-ini:Game:[LocalParty]:RelayUrl=ws://127.0.0.1:8080` y el comando `PTOnline`.
