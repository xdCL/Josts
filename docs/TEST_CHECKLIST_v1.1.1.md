# Checklist de publicación — Josts v1.1.1

Este documento define los mínimos que deben cumplirse antes de marcar v1.1.1 como estable.

## 1. Compilación

- [ ] El commit final está identificado y congelado.
- [ ] GitHub Actions termina con estado `success`.
- [ ] Todas las pruebas automatizadas pasan.
- [ ] El artefacto corresponde a Win32/x86.
- [ ] Se conserva el SHA-256 del ejecutable final.
- [ ] El ejecutable final coincide exactamente con el artefacto que se publicará en Releases.

## 2. Microsoft Defender

- [ ] Existe determinación final de Microsoft para la muestra enviada.
- [ ] Si Microsoft la reclasifica como limpia, las definiciones de Defender están actualizadas antes de repetir la prueba.
- [ ] El ejecutable final puede descargarse y conservarse con Defender activo.
- [ ] No se requieren exclusiones ni desactivar protección en tiempo real.
- [ ] Si la determinación no es favorable, se detiene la publicación estable y se investiga antes de continuar.

## 3. Cuenta estándar

En un equipo de prueba con UAC correctamente configurado:

- [ ] Josts abre normalmente desde una cuenta estándar sin mostrar UAC al iniciar.
- [ ] Consultar el estado no requiere elevación.
- [ ] Al pulsar **Aplicar**, Windows muestra UAC.
- [ ] Las credenciales son introducidas únicamente en el cuadro de Windows.
- [ ] Tras autorizar, Josts aplica el cambio y verifica el resultado.
- [ ] Cancelar UAC no modifica `hosts` y muestra un resultado comprensible.

## 4. Cuenta administrativa

- [ ] Josts abre normalmente sin requerir ejecución manual como administrador.
- [ ] Una operación privilegiada solicita el comportamiento UAC esperado del equipo.
- [ ] La operación termina con verificación correcta.

## 5. Operaciones sobre hosts

- [ ] Aplicar precarga crea el bloque de Josts.
- [ ] Volver a aplicar no genera duplicados.
- [ ] Quitar bloqueos de Josts conserva entradas externas.
- [ ] Restaurar utiliza el respaldo correcto del mismo equipo.
- [ ] `hosts.bak_original` se conserva.
- [ ] `hosts.bak_previous` corresponde al estado anterior.
- [ ] Un cambio externo concurrente impide sobrescritura silenciosa.
- [ ] Un archivo de solo lectura o inválido produce error sin declarar éxito.

## 6. Modo avanzado

- [ ] Buscar y filtrar funciona.
- [ ] Agregar, editar y eliminar entradas funciona.
- [ ] Importar lista valida entradas y duplicados.
- [ ] Exportar lista produce un archivo legible.
- [ ] Aplicar selección modifica únicamente lo esperado.
- [ ] Cambiar entre modo simple y avanzado conserva el estado de sesión esperado.

## 7. Microsoft Edge

- [ ] **Quitar noticias de Edge** solicita privilegios cuando corresponde.
- [ ] La política queda escrita como `NewTabPageContentEnabled = 0`.
- [ ] Josts verifica el valor después de escribirlo.
- [ ] `edge://policy` refleja la política después de actualizar/reiniciar Edge cuando corresponde.

## 8. Presentación y usabilidad

- [ ] Español (Chile) funciona.
- [ ] English funciona.
- [ ] El cambio de idioma no pierde la lista ni la selección actual.
- [ ] Icono, nombre de producto, versión y copyright son correctos.
- [ ] La interfaz no presenta controles cortados en el equipo de prueba.
- [ ] Los mensajes de error importantes son legibles y comprensibles.

## 9. Documentación

- [ ] README corresponde al comportamiento del ejecutable final.
- [ ] `docs/SEGURIDAD.md` corresponde al comportamiento del ejecutable final.
- [ ] `docs/TROUBLESHOOTING.md` incluye los problemas observados en campo.
- [ ] CHANGELOG está actualizado.
- [ ] Las notas de lanzamiento ya no contienen estados pendientes.
- [ ] La documentación no describe Josts como Open Source si la licencia vigente no concede esos permisos.

## 10. Release

- [ ] Se crea tag `v1.1.1` desde el commit final aprobado.
- [ ] La Release no queda marcada como pre-release.
- [ ] Se adjunta únicamente el ejecutable aprobado y los archivos que realmente correspondan a la distribución.
- [ ] Se publica el SHA-256.
- [ ] Se revisa la Release pública desde una sesión sin autenticación.
- [ ] Se confirma que la descarga publicada corresponde byte a byte al ejecutable validado.

## Criterio de cierre

v1.1.1 puede declararse estable únicamente cuando los puntos críticos de compilación, Defender, elevación UAC y operaciones de `hosts` estén completos. Los ítems cosméticos pueden postergarse solo si no afectan seguridad, instalación, comprensión del usuario ni integridad del sistema.
