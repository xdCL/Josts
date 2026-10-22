# Josts v1.2.0 RC1

**Pre-release / Release Candidate**

## Fe de erratas de trazabilidad — 22 de octubre de 2026

La etiqueta publicada `v1.2.0-rc1` apunta a `648d0d19f80ce7a9124a650e013c874ba9d5ce98`, cuyo árbol contiene únicamente el README antiguo. Por ello, los archivos automáticos **Source code (zip/tar.gz)** de esa release no corresponden al ejecutable RC1.

El código que produjo el EXE es [`850047ebb45cc9b28a04b13e53130083036cfb35`](https://github.com/xdCL/Josts/tree/850047ebb45cc9b28a04b13e53130083036cfb35), conservado también en la rama `release/v1.2.0-rc1`. Su [ejecución de CI](https://github.com/xdCL/Josts/actions/runs/36142708345) compiló x86 con MSVC y aprobó las cuatro suites. El SHA-256 del EXE del artefacto coincide con el digest del EXE publicado:

`fb6afdaa4c6bb32fc3edaee3f892ed11a85446f817aa45a75f6c2803be52f8ac`

Se conserva la etiqueta histórica; esta corrección documental no reemplaza el EXE ni declara la versión estable. Las notas originales declaran pruebas en terreno, cuyo alcance detallado se registra por separado en [VALIDACION_v1.2.0.md](VALIDACION_v1.2.0.md).

## Notas originales de RC1

Esta versión candidata incorpora correcciones derivadas de pruebas reales en equipos Windows 10 de entorno educativo.

## Cambios principales

- Los conflictos con entradas externas de `hosts` ya no interrumpen la aplicación completa de la batería.
- Si un dominio ya está bloqueado externamente mediante `0.0.0.0`, `127.0.0.1` o `::1`, Josts lo reconoce como un bloqueo equivalente y evita duplicarlo.
- Si existe un mapeo externo realmente diferente, Josts conserva esa configuración, omite solo ese dominio y continúa con el resto de la lista.
- El modo avanzado distingue ahora entre **Bloqueada fuera de Josts** y **Conflicto externo**.
- Si existen conflictos, Josts permanece abierto para permitir su revisión en vez de cerrarse automáticamente.

## Lectura y diagnóstico de hosts

- Se reforzó la inspección de `C:\Windows\System32\drivers\etc\hosts`.
- Josts distingue entre archivo inexistente, carpeta, reparse point/enlace y error real de acceso.
- Los fallos de lectura ahora incluyen un diagnóstico Win32 más preciso en el log.
- La ruta canónica se construye explícitamente desde el directorio de Windows hacia `System32\drivers\etc\hosts`.

## Seguridad

Josts continúa siendo una herramienta **local**:

- no administra otros computadores;
- no incorpora funciones remotas;
- no almacena credenciales;
- usa Windows UAC para las operaciones que requieren privilegios;
- conserva las entradas externas del archivo `hosts` en lugar de sobrescribirlas silenciosamente.

## Estado

La rama `dev/v1.2.0` compiló correctamente y el flujo de GitHub Actions finalizó en verde para el commit:

`850047ebb45cc9b28a04b13e53130083036cfb35`

Esta versión se publica como **RC1 / pre-release** para ampliar la validación en terreno antes de declarar v1.2.0 estable.

---

Josts © 2026 xdCL
