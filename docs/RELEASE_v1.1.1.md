# Notas de lanzamiento — Josts v1.1.1

> **Estado:** borrador para publicación estable. No publicar como versión final hasta completar el checklist de validación.

Josts v1.1.1 se centra en hacer más seguro y predecible el uso del programa en computadores educativos donde la sesión cotidiana no posee privilegios administrativos.

## Qué cambia

La interfaz de Josts se abre con los permisos normales de la cuenta actual. Cuando el usuario decide aplicar, retirar o restaurar cambios que requieren privilegios, Windows solicita elevación mediante UAC.

Esto permite trabajar desde una cuenta estándar sin abrir toda la aplicación como administrador y sin entregar credenciales al programa.

## Seguridad y comportamiento

- Josts no solicita ni almacena contraseñas.
- El cuadro de credenciales pertenece a Windows.
- El auxiliar administrativo verifica que realmente se esté ejecutando elevado.
- La comunicación entre procesos es local y autenticada.
- Josts no incorpora administración remota, telemetría ni funciones de control de otros equipos.
- Las escrituras sobre `hosts` se realizan con respaldo, comprobación de cambios externos, archivo temporal y verificación posterior.

## Windows hosts

Josts administra su propio bloque dentro de `System32\drivers\etc\hosts` y procura conservar las entradas externas existentes.

Antes de cada modificación efectiva mantiene respaldos locales del equipo para facilitar recuperación y restauración.

## Microsoft Edge

La opción **Quitar noticias de Edge** aplica y verifica la política `NewTabPageContentEnabled = 0` en el equipo local. Esta función es independiente de los cambios realizados sobre `hosts`.

## Compatibilidad

La entrega portable se compila como x86 para Windows 10 y puede ejecutarse tanto en Windows 10 de 32 bits como en Windows 10 de 64 bits mediante WOW64.

## Estado de Microsoft Defender

Durante la validación de la versión candidata, Microsoft Defender clasificó una compilación como `Trojan:Win32/Wacatac.B!ml`.

La muestra exacta del build afectado fue enviada a Microsoft como posible detección incorrecta. Mientras exista una determinación pendiente:

- no se recomienda desactivar Defender;
- no se recomienda crear exclusiones únicamente para ejecutar Josts;
- esta versión no debe considerarse lista para despliegue general.

Esta sección debe actualizarse antes de publicar v1.1.1 estable.

## Crédito y licencia

Josts © 2026 xdCL.

Uso gratuito para fines o entornos educativos bajo la licencia personalizada incluida en el repositorio y dentro de la aplicación.
