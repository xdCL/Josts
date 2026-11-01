# Josts Sync — v1.3.0

Josts Sync mantiene actualizada la batería de dominios de Josts sin convertir el programa en una herramienta de administración remota.

## Arquitectura

- El ejecutable sigue siendo un único `Josts.exe`.
- Al activar Sync una vez, Josts instala una copia protegida en `C:\ProgramData\Josts\Josts.exe`.
- Windows crea la tarea programada `Josts Sync`, ejecutada como `SYSTEM` cada 60 minutos.
- La batería oficial vive en la rama `sync-data`, archivo `sync/hosts-pack.txt`.
- El cliente sólo consulta rutas HTTPS fijas en `raw.githubusercontent.com/xdCL/Josts/sync-data/`.
- No hay comandos remotos, scripts descargables, escucha de red, descubrimiento de equipos ni almacenamiento de credenciales.

## Cadena de confianza

Cada actualización usa:

1. HTTPS hacia un origen fijo.
2. SHA-256 de `hosts-pack.txt`.
3. Firma RSA-2048/SHA-256 del mensaje canónico:

```
JOSTS-SYNC-1
<version>
<sha256>
```

La clave pública está compilada dentro de Josts. La clave privada nunca forma parte del repositorio ni del ejecutable.

Josts rechaza una actualización si falla el hash o la firma.

## Publicar una nueva batería

Edita únicamente:

`sync/hosts-pack.txt`

Haz commit sobre la rama `sync-data`. GitHub Actions:

1. calcula SHA-256;
2. usa el secreto `JOSTS_SYNC_SIGNING_KEY`;
3. firma el manifiesto;
4. actualiza `sync/manifest.json`.

Los equipos sincronizados detectarán la nueva versión en su siguiente ciclo.

## Secreto de GitHub requerido

En el repositorio:

**Settings → Secrets and variables → Actions → New repository secret**

Nombre:

`JOSTS_SYNC_SIGNING_KEY`

Valor: la clave privada PEM completa correspondiente a la clave pública compilada en Josts.

Nunca agregues esa clave como archivo del repositorio.

## Caché y fallos de red

La última batería válida queda almacenada bajo `C:\ProgramData\Josts`. Si el origen no está disponible, Josts puede reaplicar la última batería que ya fue validada anteriormente.

## Límites deliberados

Josts Sync no actualiza el propio ejecutable. Una actualización de `Josts.exe` sigue requiriendo una release/versionado independiente.
