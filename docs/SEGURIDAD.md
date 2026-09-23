# Modelo de seguridad de Josts

## Alcance

Josts administra únicamente el computador local donde se ejecuta. No incluye administración remota, descubrimiento de equipos, sincronización en red, telemetría ni almacenamiento de credenciales.

## Principio de privilegio mínimo

La interfaz se inicia con el nivel de privilegios del usuario actual mediante un manifiesto `asInvoker`. Consultar el estado, preparar una lista o editar entradas no requiere elevar toda la aplicación.

Cuando el usuario confirma una operación que sí requiere privilegios, Josts solicita a Windows iniciar un auxiliar mediante `ShellExecuteExW` con el verbo `runas`. El cuadro de credenciales pertenece a Windows.

Josts:

- no recibe la contraseña escrita en UAC;
- no almacena credenciales;
- no intenta eludir UAC;
- no cambia las políticas UAC del equipo;
- rechaza la operación privilegiada si el auxiliar no posee un token elevado.

## Comunicación con el auxiliar

La instancia normal crea un named pipe local para intercambiar exclusivamente la solicitud administrativa y su resultado. El canal utiliza identificadores aleatorios y una autenticación efímera entre ambas instancias.

El pipe rechaza clientes remotos. La solicitud acepta un conjunto limitado de operaciones y valida los datos recibidos antes de utilizarlos.

## Escritura de hosts

Josts obtiene la ruta del sistema con APIs de Windows y trabaja sobre `System32\drivers\etc\hosts`.

Antes de reemplazarlo:

1. comprueba que se trate de un archivo normal y no de un reparse point;
2. compara la revisión leída para detectar cambios externos;
3. conserva respaldos;
4. escribe primero un archivo temporal exclusivo;
5. verifica los bytes preparados;
6. reemplaza el archivo preservando su seguridad;
7. vuelve a leer el resultado antes de declarar éxito.

Josts delimita sus propias entradas para poder retirarlas sin eliminar reglas ajenas.

## Política de Microsoft Edge

La función «Quitar noticias de Edge» establece la política local `NewTabPageContentEnabled` en `0` bajo `HKLM\SOFTWARE\Policies\Microsoft\Edge` y vuelve a leer el valor para verificarlo.

No cambia el navegador predeterminado ni pretende bloquear publicidad general.

## Red y privacidad

Josts no necesita un servicio remoto propio para administrar `hosts`. No envía listas, registros, credenciales ni información del equipo a xdCL.

Las conexiones realizadas posteriormente por el navegador u otras aplicaciones no forman parte de Josts.

## Antivirus y reputación

Las operaciones legítimas de Josts —elevación bajo demanda y modificación de un archivo protegido del sistema— pueden ser examinadas especialmente por productos de seguridad. Una detección debe investigarse, no omitirse desactivando el antivirus.

La versión candidata 1.1.1 fue remitida a Microsoft para análisis después de una detección `Trojan:Win32/Wacatac.B!ml`. La documentación pública debe reflejar la determinación final cuando esté disponible.

## Firma

Los metadatos del ejecutable identifican el producto y a xdCL, pero esto no equivale a una firma digital. Mientras el ejecutable no tenga una firma Authenticode confiable, Windows puede mostrar «Editor desconocido».
