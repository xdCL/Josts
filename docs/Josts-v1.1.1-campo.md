# Josts 1.1.1 — prueba de campo UAC

Esta revisión responde al caso real detectado en Windows 10 cuando Josts se abre desde una cuenta estándar y UAC recibe credenciales de una cuenta administradora distinta.

## Cambios

- El `hosts` se resuelve por la ruta canónica `System32\drivers\etc\hosts`; no se expone `Sysnative`.
- El broker ya no necesita abrir ni consultar el proceso elevado desde la cuenta estándar.
- El auxiliar elevado valida el PID del servidor del named pipe y demuestra posesión de un secreto aleatorio independiente antes de aceptar la operación.
- Los fallos IPC/UAC registran la etapa y el código Win32 para identificar exactamente dónde se corta el flujo.
- Se mantiene `asInvoker`: Josts abre sin privilegios y solicita UAC sólo al aplicar/quitar/restaurar o cambiar la política Edge.

## Prueba recomendada

1. Abrir Josts desde la misma cuenta de estudiante donde falló 1.1.0.
2. Confirmar `Admin: al aplicar` y que el estado del hosts se lee.
3. Pulsar **Aplicar lista precargada**.
4. Introducir las credenciales de la cuenta administradora cuando Windows las solicite.
5. Confirmar el mensaje de éxito y volver a leer `C:\Windows\System32\drivers\etc\hosts`.
6. Cerrar sesión y volver a la cuenta estudiante; verificar que los bloqueos siguen activos, porque `hosts` es global al equipo.
7. Probar **Quitar bloqueos de Josts** con el mismo flujo.

Si falla, adjuntar `logs\josts.log`. En 1.1.1 el log distingue `[UAC]`, `[IPC]` y `[HELPER]`, junto a la etapa y el código Windows.

> Esta prueba no cambia políticas UAC, ACL del hosts ni permisos de las cuentas.
