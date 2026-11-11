# Probar Josts en Windows Sandbox

1. Extrae todo `Josts_Sandbox.zip` en una carpeta donde puedas guardar archivos.
2. Haz doble clic en **Probar_Josts.cmd**. No abras directamente Josts.exe fuera de Sandbox si quieres una prueba aislada.
3. Espera a que abra Windows Sandbox. Josts se copia al escritorio de Sandbox, en **Josts-Prueba**, y se abre desde ahí. La ventana se abre sin forzar elevación. Al aplicar cambios, acepta el permiso de administrador dentro de Sandbox si aparece.
4. Comprueba que inicia en español. El botón **English** cambia el idioma y **Español (Chile)** permite volver.
5. Para probar el flujo rápido, pulsa **Aplicar y cerrar automáticamente**. Debe mostrar el éxito y una cuenta regresiva de 3 segundos. **Mantener abierto** cancela el cierre.
6. Para comprobar la bienvenida, vuelve a abrir Josts después de aplicar la precarga: debe indicar que el parche está al día y cerrar en 5 segundos sin volver a escribir hosts. Usa **Mantener abierto** para cancelar ese cierre.
7. Puedes volver a abrir `Josts-Prueba\Josts.exe` dentro de Sandbox y usar **Quitar bloqueos de Josts**. En modo avanzado puedes revisar las entradas y el registro.
8. Cierra Windows Sandbox cuando termines; se descartarán los cambios y respaldos que se crearon dentro de él.

El paquete contiene el mismo ejecutable portable x86 de la entrega, con los idiomas y la lista incorporados. No necesita un `hosts.txt` externo para esta prueba. Las carpetas del equipo anfitrión se comparten en **solo lectura**; Josts trabaja desde una copia dentro de Sandbox. La red está habilitada para comprobar los sitios bloqueados. Google Drive no está en la precarga de bloqueo.

`Preparar_Sandbox.ps1` genera `Josts_Sandbox.wsb` con la ubicación actual de la carpeta. Si mueves el paquete, vuelve a abrir **Probar_Josts.cmd** para actualizar las rutas. Ese archivo generado no se debe subir a GitHub porque contiene rutas locales.

Requiere Windows Sandbox instalado y habilitado. Como ya lo usaste en este equipo, no hace falta instalar otra versión de Josts. Si Windows muestra «Editor desconocido», es porque el ejecutable aún no tiene una firma digital de confianza.

Documentación: [configuración de Windows Sandbox](https://learn.microsoft.com/en-us/windows/security/application-security/application-isolation/windows-sandbox/windows-sandbox-configure-using-wsb-file).

La sesión predeterminada de Sandbox no sustituye la prueba de introducir credenciales desde una cuenta estudiante. Para esa aceptación usa una VM Windows 10 con cuentas Administrador y Estudiante; consulta `docs/Josts-v1.1-revision.md` en las fuentes.
