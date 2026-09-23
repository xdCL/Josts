# Josts v1.1.1

**Gestor local de hosts para entornos educativos — desarrollado por xdCL.**

Josts es una aplicación portable para Windows que permite consultar, aplicar y retirar listas de dominios en el archivo `hosts` del **mismo computador**. Está pensada para laboratorios y equipos educativos donde el usuario cotidiano puede trabajar con una cuenta estándar.

> **Josts es local.** No administra otros computadores, no incorpora acceso remoto, no recopila telemetría y no almacena credenciales administrativas.

## Cómo funciona

Josts se abre normalmente sin privilegios elevados. Solo cuando una operación necesita modificar `hosts` o una política de Edge, Windows solicita elevación mediante UAC.

- Las credenciales son solicitadas y validadas por **Windows**, no por Josts.
- Josts no guarda contraseñas ni intenta omitir UAC.
- El archivo activo se obtiene desde la ruta de sistema de Windows: `System32\drivers\etc\hosts`.
- Antes de modificar `hosts`, Josts conserva respaldos y verifica el contenido antes y después de escribir.
- El programa modifica únicamente el equipo donde se está ejecutando.

## Uso rápido

1. Descarga el ejecutable desde **Releases**.
2. Abre Josts normalmente. No es necesario usar «Ejecutar como administrador».
3. Revisa el estado que muestra la bienvenida.
4. Si necesitas aplicar la precarga, pulsa **Aplicar**.
5. Cuando Windows muestre UAC, utiliza las credenciales administrativas autorizadas para ese equipo.
6. Josts vuelve a leer `hosts` y solo informa éxito después de verificar el resultado.

Si al solicitar una operación administrativa **Windows no muestra UAC**, revisa [Solución de problemas](docs/TROUBLESHOOTING.md).

## Funciones principales

**Modo simple** permite comprobar rápidamente el estado, aplicar la lista precargada y retirar únicamente el bloque creado por Josts.

**Modo avanzado** permite buscar, agregar, editar, eliminar, importar y exportar entradas, además de aplicar una selección y restaurar respaldos.

**Quitar noticias de Edge** configura `NewTabPageContentEnabled = 0` en la política local de Microsoft Edge. Es una operación independiente de `hosts` y también requiere privilegios administrativos.

La interfaz incluye Español (Chile) e inglés y utiliza un tema oscuro integrado.

## Seguridad y privilegios

El ejecutable principal utiliza un manifiesto `asInvoker`: abrir Josts no debería elevar el proceso. Para una operación privilegiada, Josts inicia una instancia breve del mismo ejecutable mediante el mecanismo `runas` de Windows y se comunica con ella a través de un canal local autenticado.

El auxiliar comprueba que realmente posee un token elevado antes de realizar cambios. Josts no cambia las políticas UAC del computador y no intenta corregirlas automáticamente.

Consulta [SEGURIDAD.md](docs/SEGURIDAD.md) para una descripción más detallada del modelo de seguridad.

## Compatibilidad

La entrega portable se compila como **Windows x86 (32 bits)** para funcionar tanto en Windows 10 x86 como en Windows 10 x64 mediante WOW64. El proyecto utiliza Win32 y C++14 y no requiere instalador.

La validación de campo de v1.1.1 incluye su funcionamiento desde una cuenta estándar de Windows 10 con elevación UAC mediante credenciales de una cuenta administrativa distinta.

## Respaldos

Josts conserva junto al `hosts` del sistema:

- `hosts.bak_original`: respaldo original del equipo.
- `hosts.bak_previous`: estado inmediatamente anterior a una modificación efectiva.
- `hosts.josts.lock`: coordinación temporal para impedir escrituras simultáneas.

Los respaldos portables de otro computador no se utilizan automáticamente para restaurar el sistema.

## Microsoft Defender y firma digital

Josts v1.1.1 se encuentra actualmente **sin firma Authenticode**, por lo que Windows puede mostrar «Editor desconocido».

Durante las pruebas de la versión candidata, una compilación fue clasificada por Microsoft Defender como `Trojan:Win32/Wacatac.B!ml`. La muestra exacta fue remitida a Microsoft para análisis como posible detección incorrecta. Mientras exista una determinación pendiente, no se recomienda desactivar Defender ni crear exclusiones únicamente para ejecutar Josts.

Esta nota se actualizará cuando exista una determinación del proveedor.

## Compilar y probar

El flujo de GitHub Actions compila la entrega Windows x86 con MSVC y ejecuta las pruebas automatizadas antes de publicar el artefacto.

Compilación local con Visual Studio:

```powershell
cmake -S . -B build-msvc -G "Visual Studio 16 2019" -A Win32 -DJOSTS_REQUIRE_X86=ON -DJOSTS_BUILD_TESTS=ON
cmake --build build-msvc --config Release
ctest --test-dir build-msvc -C Release --output-on-failure
```

Las pruebas cubren parsing, escritura/restauración sobre archivos de prueba, interfaz, política de Edge y el broker privilegiado. Las pruebas automatizadas no sustituyen la validación de UAC entre cuentas reales de Windows.

## Licencia

Josts se distribuye gratuitamente para fines educativos o para su uso en entornos educativos bajo una **licencia personalizada de uso educativo y distribución sin modificaciones**.

El código fuente es públicamente visible, pero **Josts no se distribuye bajo una licencia Open Source/Free Software**. Consulta [LICENSE.txt](LICENSE.txt) y su [traducción al inglés](LICENSE_EN.txt).

DM Sans, Material Icons y demás componentes de terceros conservan sus respectivas licencias.

## Documentación

- [Solución de problemas](docs/TROUBLESHOOTING.md)
- [Modelo de seguridad](docs/SEGURIDAD.md)
- [Revisión técnica de cuentas estándar](docs/Josts-v1.1-revision.md)

---

**Josts © 2026 xdCL**  
*No entenderás su utilidad hasta que lo uses.*
