# Josts v1.2.0 RC1

**Pre-release / Release Candidate**

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
