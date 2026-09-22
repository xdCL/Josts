# Josts 1.1: privilegios y estado compartido

## Diagnóstico del código de 1.0, antes del parche

Revisión realizada el 22 de septiembre de 2026. Los síntomas de los HP son reportados por el usuario: no se dispone de sus políticas, registros ni sesiones para atribuirles una causa única comprobada.

1. **Elevación de toda la aplicación.** `res/josts.manifest` declara `requireAdministrator`. Windows decide si autoriza el proceso **antes** de `wWinMain`. `src/main.cpp` no tiene `ShellExecuteExW`, `runas` ni proceso auxiliar. `Ui::create_controls` solo consulta `CheckTokenMembership` para mostrar un indicador; no solicita elevación. Las acciones llaman directamente a `HostsManager`. Si una directiva rechaza el arranque, Josts ni siquiera puede explicar el error. Un ejecutable con ese manifiesto debería pedir credenciales a un usuario estándar cuando las políticas lo permiten; no es correcto afirmar que el código prueba que el HP tenía una política concreta.
2. **Ruta global, no por usuario.** `HostsManager::HostsManager` resuelve `GetSystemDirectoryW` y conserva la ruta canónica `System32\drivers\etc\hosts`. Ese subdirectorio está exento de la redirección WOW64, por lo que no se necesita el alias virtual `Sysnative`. No se usa APPDATA, LOCALAPPDATA, USERPROFILE ni HKCU.
3. **Tabla confundida con preset.** `Ui::load_preload` sí lee un snapshot del hosts global, pero `WM_ACTION_DONE` reconstruye `entries_` exclusivamente desde el `hosts.txt` junto al ejecutable. Dos copias portables con presets distintos pueden mostrar listas distintas aunque el hosts sea exactamente el mismo. La bienvenida compara ese preset con el bloque del equipo y llama «incompleto o desactualizado» a cualquier diferencia. `entry_status` comprueba solo el dominio, no la IP. Esta es una causa comprobada de inconsistencias visuales; no prueba que el hosts de los HP haya sido borrado al cerrar sesión.
4. **Lectura con efecto de escritura.** `HostsManager::read_current` intenta crear hosts cuando no existe. Por tanto, consultar el estado no era estrictamente una operación de lectura.
5. **Respaldo dependiente del pendrive.** `ensure_backups` exige poder crear `backups/hosts_original.bak` junto al EXE. `restore` puede recurrir a ese respaldo si falta el del sistema: un pendrive usado en varias máquinas puede llevar el respaldo de otra. Los logs están junto al EXE en `logs/josts.log`; no determinan el estado activo. No hay JSON, INI, DB ni preferencias persistidas por usuario. La política de Edge es HKLM, vista nativa de 64 bits.
6. **Integridad parcial.** Había respaldo inicial y comparación previa al reemplazo, pero temporales con nombre fijo, ausencia de comprobación exacta posterior en el núcleo, sin respaldo del estado inmediatamente anterior a cada cambio y sin recarga por modificaciones externas mientras la ventana estaba abierta. No había vaciado de caché DNS.

Windows puede denegar automáticamente la elevación de usuarios estándar mediante `ConsentPromptBehaviorUser=0`. Josts no cambia esa política. [Configuración oficial de UAC](https://learn.microsoft.com/en-us/windows/security/application-security/application-control/user-account-control/settings-and-configuration).

## Arquitectura implementada

```text
Josts.exe (asInvoker, cuenta que abrió la ventana)
  lee el hosts global → muestra estado + prepara lista
  Aplicar / Quitar / Restaurar / Edge
    → ShellExecuteExW("runas", mismo Josts.exe)
    → Windows solicita consentimiento o credenciales según sus políticas
    → auxiliar elevado verifica token y canal
    → recibe operación limitada + lista + SHA-256 del hosts observado
    → comprueba estado actual, respalda, escribe y verifica
    → ipconfig /flushdns, para operaciones de hosts
    → responde por canal local y termina
  la ventana original vuelve a leer el hosts y actualiza la tabla
```

No instala servicio ni programa auxiliar. La segunda instancia sale antes de inicializar fuentes, UI o mutex de instancia única. Si la UI ya se abrió elevada, ejecuta la misma operación directamente sin un segundo UAC. El programa no recibe ni almacena contraseñas.

- **`asInvoker`**: apropiado para leer y preparar cambios como estudiante. Es la opción elegida.
- **`highestAvailable`**: no garantiza credenciales para una cuenta estándar; no resuelve por sí solo este flujo.
- **`requireAdministrator`**: pide elevación para todo el programa antes de mostrar UI; se reemplazó.
- **`runas`**: solicita a Windows la elevación únicamente al ejecutar una acción. La API devuelve códigos de error y un handle de proceso. [ShellExecuteExW](https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shellexecuteexw).

El canal es un named pipe local con nombre aleatorio criptográfico, primera instancia exclusiva, ACL para el usuario solicitante, administradores y SYSTEM, y rechazo de clientes remotos. Desde 1.1.1 el auxiliar valida el PID del servidor mediante `GetNamedPipeServerProcessId` y autentica el intercambio con un segundo secreto aleatorio independiente entregado únicamente al lanzamiento elevado. Se eliminó la dependencia de `OpenProcess`/`GetProcessId` entre cuentas, porque puede fallar cuando un estudiante introduce credenciales de **otra** cuenta administradora. El auxiliar no acepta rutas de hosts, comandos de shell ni ubicaciones de respuesta aportadas por el cliente. Solo acepta cuatro operaciones y pares IP/dominio validados, con límites de tamaño. El cliente elevado usa `SECURITY_IDENTIFICATION`; las transferencias tienen tiempo límite y cierre controlado.

La elevación de un EXE portable desde una carpeta modificable presupone que el administrador confía en ese archivo al aprobar UAC. El canal no convierte una carpeta de usuario en una ubicación de código confiable. No se ha incorporado firma digital.

## Fuente de verdad y almacenamiento

| Dato | Ubicación y propósito |
|---|---|
| Reglas activas | hosts nativo del sistema; siempre se vuelve a leer |
| Bloque propio | delimitadores `INICIO Josts` / `FIN Josts` dentro de ese hosts |
| Precarga | recurso del EXE o `hosts.txt` junto a él; es una lista propuesta, no evidencia de que esté aplicada |
| Original recuperable | `hosts.bak_original`, junto al hosts de ese computador |
| Estado anterior | `hosts.bak_previous`, junto al hosts; se actualiza antes de cada modificación efectiva |
| Serialización entre instancias | `hosts.josts.lock`, en la misma carpeta protegida; se abre sin compartir mientras dura la operación |
| Logs | `logs/josts.log` junto al EXE, cuando la cuenta tiene permiso de escritura |
| Preferencias | idioma y modo en memoria; español por defecto en cada sesión |

No se agrega ProgramData ni se amplían ACL. Los respaldos se escriben con privilegios en la carpeta del sistema y corresponden a ese equipo. Los viejos respaldos portables se conservan, pero ya no se usan automáticamente para restaurar. No poder escribir logs o materializar `hosts.txt` no impide usar la precarga incorporada. Si existe un `hosts.txt` ilegible o inválido se informa; las reglas globales leídas siguen visibles, y no se aplica esa precarga defectuosa.

Al inicio se fusionan las entradas globales con la lista visible. Los estados «Aplicada por Josts» y «Activa fuera de Josts» se calculan por **IP y dominio**. «No aplicada» describe esa fila propuesta. Una lista diferente muestra «Josts activo; lista diferente», no ausencia del parche. Una precarga inválida no oculta un bloque global que sí fue leído. La presencia de una entrada no certifica por sí sola el bloqueo efectivo en todos los navegadores ni resuelve conflictos con políticas DNS externas.

Un temporizador consulta cambios de fecha del archivo cada dos segundos y recarga su contenido sin descartar la lista preparada; también se comprueba al recuperar el foco. Antes de cualquier escritura se compara SHA-256, aunque una herramienta externa haya preservado la fecha. La recarga cancela un cierre automático pendiente si detecta cambio. El cierre de cinco segundos solo se inicia cuando la precarga válida coincide con el estado leído.

## Integridad y errores

Las acciones de hosts se serializan mediante un archivo de bloqueo en la carpeta protegida. Se valida el bloque, los dominios, las IP y los conflictos con entradas externas. La aplicación deduplica dominios iguales con la misma IP y rechaza IP contradictorias; respeta el caso habitual de localhost IPv4/IPv6.

El núcleo conserva los bytes ajenos, la codificación y el estilo de saltos de línea. Crea respaldos, escribe un temporal aleatorio con `CREATE_NEW`, lo vacía a disco y comprueba su contenido. Relee hosts para detectar cambios antes de `ReplaceFileW`, que conserva la seguridad del archivo, y compara byte a byte el resultado. No hay fallback que relaje permisos. [ReplaceFileW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-replacefilew).

Se rechazan hosts inexistentes, enlaces/reparse points, bloques incompletos, errores de lectura, archivo de solo lectura, fallos de respaldo y estados antiguos. La lectura inicial ya no crea archivos de sistema. Una falla de reemplazo o verificación informa la ubicación de recuperación; no realiza un rollback ciego sobre un posible cambio de otro administrador. **Restablecer hosts** sigue siendo una restauración completa y conserva su confirmación explícita, que advierte que reemplaza cambios externos.

No existe una operación Windows de «reemplazar solo si el hash sigue siendo X» frente a todos los escritores externos. La exclusión coordina instancias de Josts; la comprobación inmediatamente anterior y la verificación posterior detectan cambios observables, pero no convierten a otros programas en participantes del bloqueo. Las políticas institucionales o software de restauración pueden reescribir hosts posteriormente.

| Situación | Resultado comunicado |
|---|---|
| Datos inválidos o fallo al preparar canal | No se solicitó elevación; no se modificó el sistema |
| `ERROR_CANCELLED` sin directiva de denegación detectada | Solicitud cancelada; Windows devolvió ese código |
| Cuenta estándar y `ConsentPromptBehaviorUser=0` tras fallar la solicitud | La directiva no permite introducir credenciales |
| Acceso denegado / bloqueo por política sin diagnóstico más específico | Windows denegó la elevación, con error del sistema |
| Proceso auxiliar falla o no entrega respuesta verificable | No se puede confirmar la operación; releer hosts, nunca asumir éxito |
| Escritura y lectura posterior coinciden | Cambio confirmado |
| Fallo de `ipconfig /flushdns` | Cambio guardado, con advertencia separada sobre DNS |

Los códigos UAC no revelan siempre la razón administrativa completa. Si no se puede leer la política o intervienen AppLocker/WDAC u otras restricciones, se informa la denegación sin afirmar que el usuario pulsó Cancelar ni inventar una política específica. No se modifican políticas para conseguir la elevación.

## Ruta x86/x64

Se mantiene el EXE x86 para ambas arquitecturas. `GetSystemDirectoryW` evita fijar `C:` y la ruta visible se conserva como `System32\drivers\etc\hosts`. Windows excluye `System32\drivers\etc` de la redirección WOW64, por lo que no se usa `Sysnative` ni se desactiva WOW64 globalmente. [Redirección del sistema de archivos](https://learn.microsoft.com/en-us/windows/win32/winprog64/file-system-redirector).

## Cambios por archivo y fragmentos representativos

| Archivo | Funciones/clases y motivo |
|---|---|
| `res/josts.manifest`, `res/josts.rc` | `asInvoker` y metadatos 1.1 |
| `src/main.cpp` | `wWinMain`: despachar auxiliar antes del mutex y de la UI |
| `src/privileged_ops.h/.cpp` (nuevo) | `request_privileged`, `privileged_entry`, `execute_privileged`, protocolo y clasificación de errores UAC |
| `src/hosts_manager.h/.cpp` | Snapshot con mappings completos, revisión y fecha; lectura pura; bloqueo, validación, respaldos locales al equipo, reemplazo y verificación |
| `src/common.h/.cpp` | SHA-256 y nombres aleatorios con BCrypt |
| `src/ui.h/.cpp` | acciones por broker; reconstrucción desde hosts; estado por IP/dominio; recarga externa; errores sin cerrar la ventana |
| `src/translations.inc`, `src/log.cpp` | mensajes integrados y versión; español/inglés |
| `CMakeLists.txt`, `compilar_portable.ps1` | módulo nuevo, bibliotecas nativas Windows BCrypt/COM, cuarto conjunto de pruebas |
| `tests/core_tests.cpp`, `tests/ui_tests.cpp`, `tests/privileged_tests.cpp` | integridad, lectura compartida, estados de UI, protocolo y ciclo de dos procesos |
| `sandbox/iniciar.ps1` | abrir normalmente; pedir UAC al actuar, no al lanzar |
| `README.md`, `COMPARTIR_EN_GITHUB.md` | flujo de uso y distribución 1.1 |

**Manifiesto:** antes `level="requireAdministrator"`; ahora `level="asInvoker"`.

**Aplicación desde UI:** antes:

```cpp
result.ok = manager.apply(unique, error);
```

Ahora:

```cpp
PrivilegedRequest request;
request.operation = PrivilegedOperation::Apply;
request.revision = revision; // snapshot mostrado al preparar el cambio
request.entries = chosen;
auto outcome = action(target, request); // broker en producción
result.snapshot_valid = manager.snapshot(result.snapshot, error);
```

**Arranque:** antes empezaba creando el mutex de ventana; ahora:

```cpp
int privileged_result = 0;
if (josts::privileged_entry(privileged_result)) return privileged_result;
// Después se crea el mutex y la interfaz normal.
```

**Estado por fila:** antes `snapshot_.own.count(e.domain)`; ahora `snapshot_.own_mappings.count(e.ip + L"\n" + lower(e.domain))`. Se añaden a la tabla las entradas activas que el preset no contenía.

**Lectura:** antes `ERROR_FILE_NOT_FOUND` llevaba a `atomic_replace("", initial, error)`; ahora devuelve un error sin crear hosts.

**Restauración:** antes `exists(system_backup_) ? system_backup_ : portable_backup_`; ahora solo un respaldo válido del sistema actual. Los archivos originales previos a esta corrección se guardaron en `build/v1.1-before` para comparar localmente.

## Cómo probar A–F en Windows 10

Usar una máquina de prueba o una VM con dos cuentas. Copiar el EXE 1.1 a una carpeta accesible para ambas. No cambiar políticas institucionales para probar. Anotar la ruta que Josts muestra en su consola y el SHA-256 del EXE. Conservar un respaldo administrativo antes de la prueba de restauración completa.

| Test | Pasos y resultado esperado |
|---|---|
| A | Administrador abre normalmente, aplica, cierra y reabre: detecta el bloque global; si la precarga coincide, cierre en cinco segundos sin reescribir |
| B | Administrador aplica y cierra sesión. Estudiante abre otra copia con `hosts.txt` distinto: aparecen las reglas globales como aplicadas; la bienvenida explica la diferencia de lista |
| C | Estudiante prepara/aplica: UAC solicita credenciales si la política lo permite; tras autorización vuelve a la ventana original, con reglas verificadas y respaldo del equipo |
| D | Estudiante cancela UAC: la UI sigue abierta, no inicia cierre automático, hosts conserva su contenido y se muestra cancelación |
| E | Equipo configurado por su administrador para denegar elevación: la UI abre para consultar, Aplicar informa denegación/política; no se cambian políticas ni se afirma éxito |
| F | Tras aplicar, `type C:\Windows\System32\drivers\etc\hosts`: un bloque propio, dominios deduplicados, entradas externas intactas; quitar el bloque conserva las líneas ajenas |

Pruebas adicionales: cambiar hosts externamente con Josts abierto; cancelar cierre automático; archivo de solo lectura; desconectar el medio portable; abrir desde ruta con espacios y acentos; permisos de lectura del EXE para la cuenta administradora; presionar Aplicar después de que otra sesión cambie hosts; restaurar original con la confirmación correspondiente; repetir en Windows 10 x86 real y x64 con EXE x86.

## Alcance de la validación automatizada

`compilar_portable.ps1` compila x86, corre CTest e inspecciona dependencias y recursos incorporados. Cuatro suites: núcleo, UI oculta, política Edge con Registro simulado y broker. El broker se prueba con **dos procesos reales y named pipe**, pero se sustituye `runas` por `CreateProcessW` únicamente en el binario de pruebas y se usan archivos de fixture. Se cubren errores de lanzamiento, cancelación, denegación, muerte del auxiliar, mensajes inválidos, revisión obsoleta, restauración y distintas ubicaciones de datos auxiliares. Las pruebas de producción no incluyen los hooks de prueba.

Estas pruebas no introducen credenciales reales, no editan el hosts del computador de desarrollo, no cambian políticas UAC ni vacían su DNS. **No certifican todavía el diálogo UAC con otra cuenta ni los tests A–F en los HP**: esa aceptación requiere las sesiones reales o una VM Windows 10 equivalente. Los resultados automáticos se documentan en `build/portabilidad.json` y el registro CTest.
