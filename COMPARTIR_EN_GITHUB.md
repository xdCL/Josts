# Compartir Josts portable en GitHub

La descarga pública será únicamente **Josts.exe**, listo para usar. La lista inicial, los idiomas, las fuentes, los iconos y sus licencias están incorporados en ese archivo. El paquete de Windows Sandbox queda para las pruebas locales y no forma parte de la publicación.

Los archivos están preparados localmente; aún no se ha creado ni publicado un repositorio.

## 1. Crear el repositorio

Entra con tu cuenta **xdCL**, abre [crear un repositorio](https://github.com/new), escribe **Josts** y elige **Public** para compartirlo. Activa **Add a README file** para crear la primera revisión del repositorio y pulsa **Create repository**. Si el repositorio ya existe, usa ese mismo.

El README puede contener la descripción de abajo y un enlace a Releases. Sube también `LICENSE.txt` y `LICENSE_EN.txt` a la raíz del repositorio, mediante **Add file → Upload files**, para que las condiciones puedan consultarse antes de descargar. La única descarga de la aplicación en Releases seguirá siendo `Josts.exe`; no hace falta subir el código fuente ni la carpeta de trabajo. Usa esta licencia personalizada; no selecciones MIT, GPL u otra licencia que conceda permisos diferentes.

## 2. Publicar Josts.exe

Para la prueba de terreno de esta corrección crea primero una **pre-release** con etiqueta **v1.1.1-rc1** y título **Josts 1.1.1 RC1**. Cuando el flujo cuenta estudiante → UAC con credenciales de otra cuenta administradora quede confirmado, publica **v1.1.1** como versión estable.

Adjunta **Josts.exe**. Si el repositorio contiene el código fuente, el workflow `.github/workflows/build-windows.yml` permite generar el ejecutable x86 desde **Actions → Build Josts Windows x86 → Run workflow** y descargar el artefacto `Josts-Windows-x86`. No adjuntes el paquete de Sandbox, las fuentes ni los archivos de pruebas. Comprueba la descripción y pulsa **Publish release**. Comparte el enlace de esa publicación para que las personas descarguen **Josts.exe**.

GitHub puede mostrar también sus archivos automáticos de código del repositorio; la descarga utilizable del programa es **Josts.exe**.

Guías oficiales: [crear un repositorio](https://docs.github.com/en/repositories/creating-and-managing-repositories/creating-a-new-repository), [publicar Releases y adjuntar binarios](https://docs.github.com/en/repositories/releasing-projects-on-github/managing-releases-in-a-repository).

## Descripción para el README y la publicación

Josts es un gestor portable de hosts para entornos educativos, desarrollado por xdCL.

**Uso gratuito para fines o entornos educativos.** Puedes utilizarlo y compartir el ejecutable original sin modificarlo, conservando el crédito a xdCL y sus condiciones. Puedes adaptar tus listas de sitios bloqueados. Consulta `LICENSE.txt` o **Acerca de** dentro de Josts para leer la licencia completa.

- Un solo ejecutable x86, dirigido a Windows 10 de 32 y 64 bits.
- Lista de dominios incorporada y respaldo antes de modificar hosts.
- Modo simple y avanzado, con tema oscuro y Google Material Icons.
- Español (Chile) por defecto y cambio a inglés.
- Botón para desactivar el contenido de noticias de la nueva pestaña de Edge.
- Bienvenida con el estado del parche: si ya está al día, informa que no hace falta cambiar nada y se cierra en 5 segundos, con opción de mantener abierto.
- Aplicación en un clic, verificación y cierre automático en 3 segundos.

Descarga **Josts.exe** desde Releases, guárdalo en una carpeta o pendrive y ábrelo normalmente; Windows solicitará permisos de administrador al aplicar cambios. No necesita instalador ni archivos adicionales para distribuirlo. Durante el uso puede guardar su lista editable y registros junto al EXE; los respaldos se guardan junto al hosts del equipo.

El ejecutable todavía no tiene firma digital. La compilación x86 y las pruebas locales pasaron en Windows x64; falta validar en terreno las distintas instalaciones de Windows 10 de los equipos destinatarios. Las licencias de DM Sans, Material Icons y LLVM-MinGW están disponibles dentro del programa, en **Acerca de**.

La licencia propia no cambia los permisos de los componentes de terceros. Los repositorios públicos permiten ver y copiar el repositorio mediante las funciones de GitHub, según sus términos; no se debe presentar esta licencia como código abierto. [Guía de licencias de GitHub](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/licensing-a-repository).
