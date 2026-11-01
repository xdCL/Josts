# Josts v1.1.1

**Gestor de hosts para entornos educativos. Desarrollado por xdCL.**

**Uso gratuito para fines o entornos educativos.** Se permite compartir el ejecutable original sin modificaciones, conservando el crédito a xdCL y la licencia. Cada administrador puede adaptar sus listas de sitios. Condiciones completas en [LICENSE.txt](LICENSE.txt); [traducción al inglés](LICENSE_EN.txt). Ambas están incorporadas en **Acerca de**, por lo que sigue bastando distribuir el `.exe`. Es una licencia personalizada de uso educativo, no una licencia de software libre o de código abierto.

Josts es una aplicación Win32 en C++14 dirigida a **Windows 10 x86 y x64**. La entrega es **`dist\Josts.exe`, compilada como x86** para usar un mismo archivo en ambas arquitecturas. El programa, la precarga de `hosts.txt`, las cuatro variantes estáticas de DM Sans, Google Material Icons, sus licencias, el icono, el manifiesto y los metadatos de versión están incorporados. No requiere instalador ni archivos auxiliares para distribuirlo. No instala fuentes. El botón **Quitar noticias de Edge** escribe la política elegida en el Registro de Windows; distribuirlo sigue requiriendo únicamente el `.exe`. Abre sin elevación para consultar y preparar cambios. Solicita permisos UAC al modificar hosts o la política de Edge, mediante una segunda instancia breve del mismo EXE. Puede guardar `hosts.txt` y `logs` junto al ejecutable si la carpeta permite escritura; los respaldos recuperables permanecen junto al hosts del equipo. El estado activo siempre se lee del hosts global. No necesita escribir junto al EXE para aplicar la precarga incorporada.

La interfaz propia de Josts utiliza **un único tema oscuro, activo desde el primer arranque**. No existe selector de tema ni dependencia del modo claro u oscuro configurado en Windows. La ventana principal, tabla, buscador, consola, botones, edición, confirmaciones y «Acerca de» usan la misma paleta. La barra de título, la ventana UAC y los selectores de archivos pertenecen a Windows y pueden conservar el aspecto del sistema. Josts solicita una barra de título oscura cuando la versión de Windows y el tema del sistema lo permiten.

El idioma inicial es **Español (Chile)** en cada arranque. El botón **English** de la parte superior cambia la interfaz al inglés sin reiniciar; luego se llama **Español (Chile)** para volver. Ambos idiomas están incorporados en el `.exe`, incluidos los mensajes, diálogos y la cuenta regresiva. El cambio conserva la lista, las selecciones, el filtro y el modo de vista; el registro conserva los mensajes anteriores en su idioma original. El selector se desactiva durante las operaciones y el cierre automático. Las ventanas del sistema, como UAC y los selectores de archivos, pueden seguir el idioma de Windows.

Los créditos y metadatos identifican al autor como **xdCL**. El ejecutable está **sin firma digital**: cambiar `CompanyName` o los créditos no elimina «Editor desconocido» de Windows. Eso requiere una firma Authenticode con un certificado de editor emitido por una autoridad de confianza; el nombre que Windows verifique procederá del certificado. Véase la [documentación de Microsoft sobre Authenticode](https://learn.microsoft.com/en-us/windows/win32/dxtecharts/authenticode-signing-for-game-developers).

Josts abre con una **bienvenida en modo simple** que comprueba el parche de hosts y muestra si está ausente, al día, incompleto/desactualizado o si no se pudo verificar. Si ya está al día, indica que **no es necesario hacer cambios** y muestra una cuenta regresiva visible de **5 segundos** para cerrar. **Mantener abierto** cancela el cierre y permite acceder a las herramientas, incluido Edge. La comprobación no aplica ni restaura el parche.

Para considerar el parche al día, debe existir un bloque válido de Josts y una precarga válida y no vacía; deben estar presentes todos sus pares de IP/dominio, sin IP en conflicto ni dominios antiguos dentro del bloque de Josts. Se respeta la convivencia normal de `127.0.0.1 localhost` y `::1 localhost`. Una lista vacía, errores de lectura, un bloque incompleto o una precarga diferente mantienen el programa abierto. Se compara con la precarga que Josts está usando: `hosts.txt` junto al ejecutable, si existe, o la lista incorporada. El cierre de bienvenida solo se programa en la comprobación inicial, no al recargar manualmente la lista. Este estado corresponde a hosts; no certifica el estado de la política de Edge.

Si falta aplicar o actualizar el parche, la bienvenida ofrece **Aplicar y cerrar automáticamente**. Un clic aplica la precarga sin una confirmación adicional, vuelve a leer `hosts` para comprobar las entradas y muestra **«¡Precarga aplicada correctamente!»** junto con una cuenta regresiva visible de **3, 2, 1 segundos**, tras la cual cierra el programa. **Mantener abierto** cancela ese cierre. Si falla la escritura o la verificación, muestra el error y permanece abierto. Las entradas ajenas con otra IP se detectan antes de aplicar; el programa pide revisarlas en modo avanzado y no inicia el temporizador.

El modo simple conserva **Aplicar lista precargada** (con confirmación y sin cierre automático) y **Quitar bloqueos de Josts**. **Modo avanzado** muestra la lista completa, el buscador, la edición, la importación, la exportación, el registro y las operaciones de restauración. Se cambia de modo con el botón de la parte superior. Las ediciones permanecen en la sesión al cambiar de vista; los botones del modo simple usan siempre la precarga leída al abrir o al usar **Recargar precarga**. Las demás operaciones sobre el `hosts` del sistema conservan sus confirmaciones.

Ambos modos muestran una **barra de progreso animada** y el nombre de la operación durante la lectura de la precarga, importación, exportación, aplicación, retirada y restauración. La barra pasa a verde al completar o a rojo ante un error. No muestra porcentajes estimados. Estas tareas se ejecutan en segundo plano para mantener la ventana activa; los controles se bloquean mientras trabajan para evitar operaciones simultáneas. El resultado queda visible al terminar y las acciones rápidas de edición también muestran su finalización.

## Noticias de Edge

El botón **Quitar noticias de Edge**, visible en los modos simple y avanzado, aplica la configuración con un clic y muestra progreso y resultado. Esta acción es independiente de la aplicación de hosts y mantiene Josts abierto. La frase **«No entenderás su utilidad hasta que lo uses»** está visible junto al botón y en **Acerca de**, con traducción al cambiar al inglés.

La función independiente [`BloquearContenidoNoticiasEdge()`](src/edge_settings.cpp), declarada en [`edge_settings.h`](src/edge_settings.h), devuelve `bool` y deja el código de error en `GetLastError()`. Usa `RegCreateKeyExW` y `RegSetValueExW` para establecer `NewTabPageContentEnabled` como `REG_DWORD` de valor `0` en `HKEY_LOCAL_MACHINE\SOFTWARE\Policies\Microsoft\Edge`. Abre la clave con `KEY_SET_VALUE | KEY_QUERY_VALUE | KEY_WOW64_64KEY`, vuelve a leer tipo, tamaño y valor para verificar la escritura, y cierra el identificador conservando los errores. Requiere enlazar `Advapi32.lib`, ya incluida en la compilación de Josts. No necesita archivos `.reg`, PowerShell ni DLL externas para aplicar la política.

[Microsoft documenta esta política desde Edge 91](https://learn.microsoft.com/en-us/deployedge/microsoft-edge-policies/newtabpagecontentenabled): desactiva el contenido de Microsoft en la nueva pestaña. Después de aplicar, abre una pestaña nueva o reinicia Edge si todavía ves noticias. Para comprobar que Edge la recibió, revisa `edge://policy` y busca `NewTabPageContentEnabled = false` sin errores. La verificación de Josts comprueba el Registro; no inspecciona el navegador. No cambia el navegador predeterminado ni promete eliminar toda la publicidad de Internet.

La bandera [`KEY_WOW64_64KEY`](https://learn.microsoft.com/en-us/windows/win32/sysinfo/registry-key-security-and-access-rights) selecciona la vista de 64 bits incluso desde el ejecutable x86; Windows de 32 bits la ignora. La configuración permanece en el equipo después de cerrar o retirar el ejecutable. Si más adelante deseas deshacerla, un administrador puede quitar únicamente el valor `NewTabPageContentEnabled` para volver a la política no configurada, o restablecer el valor anterior si existía una configuración propia de la escuela. Una política administrada centralmente puede volver a imponer su configuración.

Las pruebas de esta función simulan las API del Registro: comprueban ruta y permisos exactos, escritura DWORD, lectura de verificación, repetición y errores. La prueba de interfaz simula el resultado y comprueba progreso y mensajes en ambos idiomas. No modifican la política de Edge de esta estación; la comprobación en un navegador real queda para Sandbox o un equipo de prueba.

## Estructura

```text
Josts/
├── src/                  código C++14
├── res/
│   ├── DM_Sans/          TTF extraídos para compilar; no se distribuyen
│   ├── MaterialIcons-Regular.ttf
│   ├── MaterialIcons-Regular.codepoints
│   ├── icon.ico
│   ├── josts.manifest
│   └── josts.rc
├── tests/core_tests.cpp
├── tests/ui_tests.cpp     progreso, aplicación rápida y cierre automático con hosts de prueba
├── DM_Sans.zip           archivo original de Google Fonts
├── LICENSE_DM_Sans.txt   licencia SIL OFL 1.1 original del ZIP
├── LICENSE_Material_Icons.txt  licencia Apache 2.0 de Google
├── LICENSE_LLVM_MinGW.txt     licencias de componentes del compilador/runtime
├── hosts.txt             precarga, con la lista existente conservada
├── preparar_fuentes.ps1
├── compilar_portable.ps1  compila, prueba y verifica la entrega x86
├── dist/Josts.exe         único archivo para copiar a los equipos
├── sandbox/              inicio de una prueba aislada en Windows Sandbox
└── CMakeLists.txt
```

El archivo `hosts.txt` que ya existía en este proyecto se conservó, recibió un encabezado de Josts y se amplió con dominios de juegos de clics, pruebas de velocidad del mouse, contadores de teclado, juegos competitivos de mecanografía, catálogos de descarga y almacenamiento de archivos. Se puede reemplazar por cualquier lista propia con líneas `dominio.com` o `IP dominio.com`. CMake copia este archivo junto a cada ejecutable compilado y también lo embebe como precarga de reserva. Al arrancar, Josts usa primero el `hosts.txt` que esté junto al `.exe`; si falta, carga la lista incorporada y trata de crear una copia editable junto al programa. Por eso **copiar solo el `.exe` también precarga los sitios**. Si el pendrive conserva una copia anterior de `hosts.txt`, sustitúyala por la nueva para ver los dominios añadidos.

La ampliación incluye, entre otros, [Cookie Clicker](https://cookieclicker.com/~web/), [Clicker Heroes](https://clickerheroes.com/play), [CPS Test](https://cpstest.org/), [Click Speed Test](https://clickspeedtest.com/), [Spacebar Counter](https://spacebarcounter.org/), [Nitro Type](https://www.nitrotype.com/), [TypeRacer](https://play.typeracer.com/) y [TypingGames.Zone](https://www.typinggames.zone/). Los sitios se revisaron en sus propias páginas el 13 de septiembre de 2026. Nitro Type y otros juegos de escritura pueden usarse con fines didácticos; quite esas entradas si son parte de una clase. `hosts` bloquea nombres de dominio concretos, no rutas de una página ni todas las variantes futuras de un juego. Por eso no se agregaron portales de uso general que también alojan contenido no relacionado.

También se precargaron **Uptodown**, **Softonic**, [TLauncher y los espejos que publica](https://tlauncher.org/en/official-domain-list.html), **MEGA**, [MediaFire](https://www.mediafire.com/), [OneDrive personal y sus enlaces `1drv.ms`](https://support.microsoft.com/en-us/onedrive/share-files-and-folders-in-microsoft-onedrive) y [CrazyGames](https://www.crazygames.com/). **Google Drive no está en la lista de bloqueo.** Uptodown y Softonic publican fichas en subdominios particulares de cada aplicación: se incluyeron los de [Minecraft en Uptodown](https://minecraft.uptodown.com/windows) y [Minecraft en Softonic](https://minecraft.softonic.com/), pero `hosts` no permite cubrir automáticamente todos los futuros subdominios. Para OneDrive empresarial no se bloquearon dominios compartidos de Microsoft 365 o SharePoint que podrían afectar otros servicios.

## Probar en Windows Sandbox

En un Windows 10/11 compatible, active **Windows Sandbox** desde «Activar o desactivar las características de Windows» y reinicie si Windows se lo pide. Se requiere una edición compatible y virtualización habilitada; vea los [requisitos de Microsoft](https://learn.microsoft.com/en-us/windows/security/application-security/application-isolation/windows-sandbox/windows-sandbox-install).

Extraiga **Josts_Sandbox.zip** y haga doble clic en **Probar_Josts.cmd**. Desde el código fuente también puede usar `sandbox\Probar_Josts.cmd`, después de compilar la entrega en `dist`. El acceso genera las rutas de la configuración según la carpeta actual, comparte las carpetas en **solo lectura**, copia únicamente `Josts.exe` al escritorio *dentro* de Sandbox y lo abre desde esa copia editable. La lista inicial sale del propio ejecutable. La consulta inicial no solicita elevación; acepte la petición de administrador **dentro de Sandbox al aplicar cambios**, si aparece. La red queda habilitada para comprobar los sitios en el navegador de la máquina aislada. Si mueve el paquete, vuelva a abrir el `.cmd`. El archivo `sandbox\Probar_Josts.wsb` conserva un acceso directo para la ubicación local `C:\Josts`.

Para una prueba pequeña, abra **Modo avanzado**, seleccione `crazygames.com` en Josts, pulse **Aplicar selección**, compruebe que el sitio deja de abrir dentro del Sandbox y que `drive.google.com` sí abre. Después pruebe **Desparchear solo mis entradas**. Al cerrar Windows Sandbox se descartan sus cambios, incluidos los hechos al `hosts` de la máquina aislada; los respaldos y registros de esta prueba también se pierden. [Microsoft documenta los archivos `.wsb` y las carpetas montadas en solo lectura](https://learn.microsoft.com/en-us/windows/security/application-security/application-isolation/windows-sandbox/windows-sandbox-configure-using-wsb-file).

Windows Sandbox usa la misma compilación de Windows que el equipo anfitrión. Esta prueba permite revisar el programa y el parche en aislamiento, pero **no verifica su compatibilidad con un sistema operativo x86 ni con todas las compilaciones de Windows 10**. El ejecutable actual de `build` y `dist` es x86.

## Compartir en GitHub

Consulte [COMPARTIR_EN_GITHUB.md](COMPARTIR_EN_GITHUB.md) para crear el repositorio y publicar únicamente **Josts.exe** como descarga en Releases. La carpeta `dist` contiene la aplicación portable completa. El paquete de Sandbox se conserva para pruebas locales y no se incluye en la publicación de GitHub.

## Compilación final para Windows 10 x86 y x64

La entrega actual usa **LLVM-MinGW 20260908 (Clang 23.1.1), objetivo i686/UCRT**, con las bibliotecas de C++ y del compilador enlazadas estáticamente. Las DLL importadas pertenecen a Windows: [UCRT forma parte de Windows 10](https://learn.microsoft.com/en-us/cpp/windows/universal-crt-deployment) y [WOW64 ejecuta aplicaciones x86 en Windows x64](https://learn.microsoft.com/es-es/windows/win32/winprog64/running-32-bit-applications). No se necesita instalar Visual C++ Redistributable ni copiar DLL a los HP.

Para reproducir esta compilación, se requiere CMake 3.24 o posterior, Ninja y el [paquete oficial LLVM-MinGW 20260908 UCRT para anfitrión x64](https://github.com/mstorsjo/llvm-mingw/releases/tag/20260908), extraído en `tools\llvm-mingw-20260908-ucrt-x86_64`. El nombre x64 del paquete identifica al equipo compilador; el script selecciona el compilador de destino **i686/x86**. SHA256 del ZIP descargado: `1bcf74d06b724aeecaa6412ca85f5b26fb1da770e7cdcefa9263c9c5c3ad34b6`.

En PowerShell, desde el proyecto:

```powershell
powershell -ExecutionPolicy Bypass -File .\preparar_fuentes.ps1
powershell -ExecutionPolicy Bypass -File .\compilar_portable.ps1
```

El script acepta `-ToolchainRoot`, `-CMakePath` y `-NinjaPath` para instalaciones en otras rutas. Configura `build` como x86, ejecuta las pruebas con sus comprobaciones activas incluso en Release, rechaza arquitecturas o DLL inesperadas y comprueba el límite de 4 MB. Repite las pruebas en una carpeta sin `hosts.txt`, fuentes ni DLL auxiliares, con un `PATH` que solo contiene Windows. Solo después copia el resultado a **`dist\Josts.exe`** y guarda arquitectura, tamaño, SHA256 y dependencias en `build\portabilidad.json`. Los compiladores y pruebas no se distribuyen con la aplicación.

Como alternativa, el proyecto conserva soporte para Visual Studio 2019, toolset v142, con el runtime estático `/MT`:

```powershell
cmake -S . -B build-msvc -G "Visual Studio 16 2019" -A Win32 -DJOSTS_REQUIRE_X86=ON -DJOSTS_BUILD_TESTS=ON
cmake --build build-msvc --config Release
ctest --test-dir build-msvc -C Release --output-on-failure
```

CMake fija C++14, `WINVER=0x0601` y `_WIN32_WINNT=0x0601` para mantener el código dentro de API antiguas; **el alcance de distribución es Windows 10**. La opción `JOSTS_REQUIRE_X86` rechaza por error una compilación de 64 bits destinada a la entrega portable.

### Compilación verificada en esta estación

La entrega es un ejecutable nativo **PE32/i386 de menos de 4 MB**; su tamaño exacto y SHA256 quedan en `build\portabilidad.json`. Se revisaron sus importaciones y se ejecutaron las pruebas x86 en el anfitrión Windows x64 (compilación 26200.9445). Pasaron el parsing de 50.000 dominios, la escritura/restauración de archivos de prueba, la conservación de codificación y contenido ajeno, la resolución canónica de `System32\drivers\etc\hosts` bajo Windows x86/WOW64, y la carga de precarga, fuentes, iconos, manifiesto y licencias desde el `.exe`. La copia sin archivos auxiliares pasó las mismas comprobaciones.

Las pruebas de interfaz usan ventanas ocultas y archivos propios: comprueban la animación mientras una lectura espera, el bloqueo de doble clic y cierre durante el trabajo, la aplicación con respaldo y lectura de verificación, los tres valores visibles del temporizador y el cierre al transcurrir 3 segundos. También comprueban **Mantener abierto**, un `hosts` de solo lectura, entradas ajenas incompatibles y una precarga inválida. La bienvenida se verifica con archivos ya parchados, antiguos, incompletos, conflictos de IP, entradas obsoletas y listas vacías o inválidas; se comprueba el cierre de 5 segundos sin escribir hosts, su cancelación y que una recarga posterior no reactive el cierre. Generan imágenes de las vistas simple, avanzada, de trabajo, éxito y error para revisar su distribución visual. **Ninguna de estas pruebas modifica el `hosts` real.** La suite `privileged` agrega el intercambio entre dos procesos mediante named pipe y simula la respuesta al lanzamiento UAC con archivos de prueba; no sustituye la validación de credenciales entre dos cuentas reales.

**Límite de la verificación:** esto valida el binario x86 y su funcionamiento bajo WOW64; no equivale a haber probado todas las compilaciones de Windows 10 ni un Windows 10 x86 real. La validación de campo pendiente es abrirlo y probar aplicar/quitar bloqueos en los HP de cada arquitectura.

## Corrección de cuentas estándar en 1.1

Consulta el [diagnóstico, arquitectura, cambios antes/después y pruebas A–F](docs/Josts-v1.1-revision.md). El manifiesto es `asInvoker`; solo las operaciones administrativas usan `runas`. Las credenciales pertenecen al diálogo UAC de Windows. Si la política institucional deniega la elevación, Josts lo informa sin alterarla. La tabla incorpora las reglas que ya existen en el hosts aunque una copia portable tenga otra precarga.

## Uso para el personal

1. Copie **solo `dist\Josts.exe`** a una carpeta del pendrive. Lleva la lista inicial incorporada; copie además `hosts.txt` únicamente si quiere sustituir la precarga por una lista propia. Puede añadir una carpeta `listas` con archivos `.txt` y `.md`. No copie compiladores, ZIP, archivos TTF ni DLL.
2. Abra Josts normalmente, incluso desde una cuenta estudiante. Al aplicar o quitar cambios, Windows solicitará consentimiento o credenciales administrativas según la cuenta y las políticas del equipo. En **modo simple** verá el estado del equipo y cuántos dominios contiene la precarga. Para hacerlo en un clic, pulse **Aplicar y cerrar automáticamente**: tras verificar el éxito, se cierra en 3 segundos. Puede cancelar el cierre con **Mantener abierto**. Para aplicar sin cerrar, use **Aplicar lista precargada** y confirme. **Quitar bloqueos de Josts** retira solo el bloqueo propio.
3. Si necesita revisar o cambiar sitios, abra **Modo avanzado**. La lista indica cada dominio como **No aplicada**, **Activa fuera de Josts**, **Aplicada por Josts**, **Modificada** o **Estado no disponible**. Use **Agregar**, **Editar** (o doble clic), **Eliminar**, el buscador y **Subir/Bajar selección**. **Cargar lista** permite varios archivos y muestra nuevas, duplicadas y errores antes de añadir o reemplazar la lista. **Exportar** guarda la lista actual.
4. En modo avanzado, pulse **Aplicar parche** para escribir la lista editada, o **Aplicar selección** para escribir solo las filas elegidas. Confirme el cambio. Los dominios ya activos en entradas ajenas se omiten para evitar duplicados.
5. **Desparchear solo mis entradas** quita el bloque de Josts y conserva el resto. **Restablecer hosts** recupera el respaldo original completo y elimina también cambios ajenos; exige confirmación.

Antes de la primera modificación se guarda `hosts.bak_original` junto al hosts del sistema; antes de cada cambio efectivo se verifica `hosts.bak_previous` con el estado anterior. `hosts.josts.lock` coordina las instancias. Los respaldos portables antiguos se conservan, pero no se restauran automáticamente porque podrían pertenecer a otro PC. El log rotativo está en `logs\josts.log` cuando la carpeta permite escritura (máximo aproximado de 1 MB por archivo, más una copia `.1`). Si falta el permiso administrativo, falla el respaldo o cambia hosts durante la operación, se informa y no se declara éxito. Los fallos de verificación posterior se notifican con la ubicación de recuperación.

## Decisiones técnicas

- **Win32 y C++14** reducen tamaño y dependencias. Las tareas de importación y escritura de hosts se ejecutan en un hilo secundario; la lista virtual evita crear 50.000 controles individuales.
- **Tema oscuro fijo**: los controles propios se pintan con GDI y una paleta de grafito, azul y texto claro sin depender de APIs modernas de modo oscuro. La barra de título oscura se solicita de forma opcional mediante carga dinámica; Windows decide si admite y aplica ese atributo.
- **AddFontMemResourceEx** carga DM Sans y Material Icons como recursos privados del proceso. Los botones dibujan glifos de Google Material Icons según los códigos oficiales, con texto en DM Sans. Si falla DM Sans, los controles propios usan Segoe UI; si falla Material Icons, los botones conservan sus etiquetas de texto. Ambas incidencias se registran. Los cuadros de selección de archivos pertenecen a Windows y conservan la tipografía del sistema.
- La ruta de `hosts` se obtiene con `GetSystemDirectoryW` y se conserva como `System32\drivers\etc\hosts`. Ese subdirectorio está exento de la redirección WOW64, por lo que Josts ya no necesita exponer el alias virtual `Sysnative`. No se fija la letra de la unidad.
- La aplicación detecta UTF-8 con y sin BOM, UTF-16 LE/BE con BOM y ANSI. Reescribe con la codificación y el salto de línea detectados. Si la página ANSI no puede representar los caracteres del bloque firmado, rechaza el cambio. Solo sustituye su bloque delimitado. Escribe un temporal aleatorio exclusivo, lo vacía a disco, verifica su contenido y reemplaza hosts con `ReplaceFileW`, conservando su seguridad. Antes compara la revisión observada; después verifica los bytes guardados. Un auxiliar privilegiado vacía la caché DNS y comunica cualquier fallo por separado.
- El manifiesto declara compatibilidad con Windows 10 y conserva identificadores para versiones anteriores. Esto no amplía el alcance de distribución ni sustituye las pruebas en Windows 10 x86 y x64.

Crédito de Josts: **© xdCL**. Los elementos propios se ofrecen según la [licencia de uso educativo y distribución sin modificaciones](LICENSE.txt). Las licencias de terceros conservan sus permisos originales. DM Sans pertenece a sus autores respectivos y se distribuye conforme a la [SIL Open Font License 1.1](LICENSE_DM_Sans.txt). Material Icons es de Google y se distribuye conforme a la [Apache License 2.0](LICENSE_Material_Icons.txt). También se incluyen las [licencias de LLVM-MinGW](LICENSE_LLVM_MinGW.txt). Todas están incorporadas en el ejecutable y disponibles en **Acerca de**.
