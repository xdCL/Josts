# Changelog

Todos los cambios relevantes de Josts se documentan aquí.

## [1.1.1] — candidato

### Mejoras principales

- La aplicación puede abrirse normalmente desde una cuenta estándar de Windows y solicitar elevación solo cuando una operación realmente necesita privilegios administrativos.
- Se incorporó un broker privilegiado de una sola ejecución para aplicar, retirar o restaurar cambios sin mantener toda la interfaz elevada.
- La comunicación entre la instancia normal y la instancia elevada utiliza un named pipe local autenticado.
- El auxiliar comprueba que su token sea realmente elevado antes de ejecutar una operación administrativa.
- Se reforzó la resolución canónica de `System32\drivers\etc\hosts` para la entrega x86 bajo Windows x64.
- Se mejoró la detección de cambios externos en `hosts` antes de escribir.
- Se reforzó la escritura atómica y la verificación posterior.
- Se mantienen respaldos `hosts.bak_original` y `hosts.bak_previous` junto al archivo de sistema.
- Se añadió manejo más claro de errores de UAC, IPC y operaciones privilegiadas.
- La función de Edge verifica la política escrita después de modificar el Registro.
- Se mantienen los modos simple y avanzado, la precarga incorporada, importación/exportación y soporte Español (Chile) / English.

### Seguridad

- Josts no almacena ni recibe contraseñas administrativas.
- Las credenciales son gestionadas exclusivamente por Windows UAC.
- No se incorporaron funciones de administración remota ni telemetría.
- El broker privilegiado rechaza operaciones si no posee un token elevado.
- El canal IPC utiliza valores aleatorios y rechaza clientes remotos.

### Compatibilidad

- Entrega Windows x86 para funcionar tanto en Windows 10 x86 como en Windows 10 x64 mediante WOW64.
- GitHub Actions compila con MSVC en Win32 y ejecuta la suite automatizada antes de producir el artefacto.

### Validación de campo

- Probado en Windows 10 desde una cuenta estándar con credenciales de una cuenta administrativa distinta.
- Validado el flujo: abrir sin elevación → solicitar UAC al aplicar → ejecutar operación privilegiada → verificar el estado final de `hosts`.
- Se detectó que una política/configuración de UAC del propio equipo puede impedir que Windows muestre la solicitud de elevación. Josts no modifica esa política.

### Pendiente antes de declarar estable

- Microsoft Defender clasificó una compilación candidata como `Trojan:Win32/Wacatac.B!ml`.
- La muestra exacta fue remitida a Microsoft como posible detección incorrecta y la determinación final continúa pendiente.
- Se requiere una última validación de campo de la versión que vaya a publicarse como estable.
