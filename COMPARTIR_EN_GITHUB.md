# Publicar Josts portable en GitHub

El repositorio público es [xdCL/Josts](https://github.com/xdCL/Josts). El programa se distribuye como un solo EXE x86 con precarga, idiomas, fuentes, iconos y licencias incorporados. El SHA-256 y el manifiesto de compilación sirven para verificar la entrega; no son necesarios para ejecutar Josts.

## Preparación de v1.2.0

1. Usar la rama `release/v1.2.0`, basada en `a8e53ca88020865e8671c874b3705b8b51aadd02`. Este commit conserva el código validado de RC1 y agrega sus notas. No iniciar desde la etiqueta histórica `v1.2.0-rc1`, que apunta al README antiguo.
2. Integrar mediante un PR hacia `main` las correcciones de versión, documentación y empaquetado. El PR #1 de `dev/v1.1.1` queda reemplazado; sus commits ya están contenidos en esta historia.
3. Completar [VALIDACION_v1.2.0.md](docs/VALIDACION_v1.2.0.md), recuperando los resultados de terreno existentes y probando solo los escenarios no acreditados. No publicar como estable mientras haya fallos bloqueantes o el alcance anunciado siga sin acreditar o delimitar.
4. Tras integrar, anotar el SHA final de `main`. **Actions → Windows build → Run workflow**, sobre `main`, permite repetir la compilación si hace falta; los pushes a `main` también la ejecutan. Comprobar que el run corresponde al SHA final y que sus cuatro suites pasan.

## Etiqueta y ejecutable de la misma revisión

1. Crear una etiqueta nueva **`v1.2.0` en el SHA final verificado**, sin reutilizar ni mover la etiqueta de RC1. Antes de publicar, comprobar que el árbol de esa etiqueta contiene `src/`, `res/`, pruebas y documentación.
2. El push del tag ejecuta [`.github/workflows/windows-build.yml`](.github/workflows/windows-build.yml). Esperar el resultado correcto y descargar el artefacto `Josts-Windows-x86` de **ese run**, no el de otra rama.
3. Verificar en `build-info.json` que `sourceCommit` coincide con el SHA del tag, la versión es `1.2.0` y el archivo es `Josts-v1.2.0.exe`. Comprobar el SHA-256 con `SHA256.txt` y los metadatos del EXE `1.2.0.0`. Conservar el enlace al run y `test-results.xml`.
4. Hacer la comprobación básica del ejecutable que se distribuirá: apertura, versión visible y flujo de aplicar/quitar en el equipo de prueba. Una recompilación puede cambiar el hash; comprobar el EXE concreto, sin asumir identidad por compartir el código.
5. Crear la release **Josts v1.2.0** usando el tag existente. Adjuntar `Josts-v1.2.0.exe`, `SHA256.txt` y `build-info.json`. Incluir cambios, SHA del código, enlace al CI y alcance real de terreno. Desmarcar **Set as a pre-release** solo cuando se cumplan los criterios del registro.

Las compilaciones de ramas/PR se llaman `Josts-v1.2.0-dev.exe`; no anunciarlas como una release estable. El workflow comprueba la correspondencia entre la versión de CMake, los metadatos de Windows y la etiqueta, y no publica releases automáticamente.

## Registro histórico de RC1

La [fe de erratas](docs/RELEASE_v1.2.0-rc1.md#fe-de-erratas-de-trazabilidad--22-de-octubre-de-2026) identifica las fuentes correctas y el hash del EXE publicado. Conservar la publicación y su etiqueta históricas; enlazar la corrección en sus notas. Los archivos automáticos de fuentes de esa etiqueta antigua no representan RC1.

## Distribución y licencia

Compartir el enlace público de la release permite descargar el EXE sin acceder a los artefactos de Actions. No adjuntar compiladores, fuentes TTF sueltas ni paquetes de Sandbox como si fueran dependencias de la aplicación.

**Uso gratuito para fines o entornos educativos.** Se permite compartir el ejecutable original sin modificaciones, conservando el crédito a xdCL y su licencia. Los administradores pueden adaptar sus listas. Consultar [LICENSE.txt](LICENSE.txt), su [traducción al inglés](LICENSE_EN.txt) o **Acerca de**. Es una licencia personalizada de uso educativo; no presentarla como una licencia de software libre o de código abierto. Las licencias de terceros conservan sus permisos originales.

El EXE continúa sin firma digital. Abrirlo normalmente; Windows solicita UAC al aplicar cambios. Los respaldos de hosts se conservan junto al archivo del equipo y los registros se guardan junto al EXE cuando la carpeta permite escritura.
