# Solución de problemas — Josts v1.1.1

## Al pulsar Aplicar no aparece UAC

Josts depende del mecanismo de elevación de Windows. Si tampoco aparece UAC al usar «Ejecutar como administrador» con una aplicación normal de Windows, el problema se encuentra en la configuración o política del sistema y no en la lista de Josts.

No introduzcas contraseñas administrativas dentro de Josts y no las guardes en scripts para solucionar este caso. La configuración UAC debe ser administrada por una persona autorizada mediante las herramientas normales de Windows.

Después de corregir una configuración UAC que requiera reinicio, vuelve a abrir Josts normalmente desde la cuenta cotidiana y prueba la operación.

## Windows muestra «Editor desconocido»

La versión actual no está firmada con Authenticode. Los campos de producto, compañía y copyright no sustituyen una firma digital de editor.

## Defender detecta Josts

No desactives Microsoft Defender ni agregues una exclusión solamente para ejecutar Josts.

Durante las pruebas de v1.1.1, Defender clasificó una compilación candidata como `Trojan:Win32/Wacatac.B!ml`. La muestra exacta fue enviada a Microsoft como posible detección incorrecta y se espera su determinación.

Si estás probando una compilación distinta, conserva el nombre de detección, la versión de inteligencia de seguridad y el SHA-256 del archivo para poder comparar los resultados.

## Josts dice que hosts cambió externamente

Josts compara el contenido que leyó con el existente justo antes de escribir. Si otra aplicación o administrador modifica `hosts` durante ese intervalo, Josts cancela la operación para evitar sobrescribir cambios que no revisó.

Recarga el estado, revisa las diferencias y vuelve a aplicar solo si corresponde.

## Hay entradas que Josts no creó

Josts distingue su bloque de las entradas ajenas. Una regla externa compatible puede conservarse sin duplicarla. Si existe un dominio externo con una IP incompatible, Josts detiene la aplicación para que pueda revisarse.

## Quiero retirar Josts sin borrar otras reglas

Utiliza **Quitar bloqueos de Josts** o **Desparchear solo mis entradas**. Estas operaciones retiran el bloque delimitado de Josts y conservan las entradas externas.

**Restablecer hosts** tiene un alcance diferente: recupera el respaldo original completo del computador y puede retirar cambios posteriores de terceros. Úsalo únicamente cuando ese sea el resultado deseado.

## Edge todavía muestra noticias

La función de Edge configura una política del Registro y verifica que el valor haya quedado escrito. Abre una pestaña nueva o reinicia Edge y revisa `edge://policy`.

Una política administrada por la institución puede volver a imponer otro valor.

## El programa no puede escribir un log junto al EXE

La aplicación puede ejecutarse desde una ubicación donde la cuenta estándar no tenga permiso de escritura. Eso no impide por sí solo aplicar la precarga incorporada. Los respaldos críticos de `hosts` permanecen junto al archivo de sistema, no junto al ejecutable portable.

## Antes de reportar un problema

Anota la versión de Josts, edición/versión de Windows, tipo de cuenta desde la que se abrió, acción realizada y mensaje exacto mostrado por Josts o Windows. No compartas contraseñas ni otras credenciales.
