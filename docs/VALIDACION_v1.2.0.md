# Validación para Josts v1.2.0

Estado al 22 de octubre de 2026: **preparación de estable**. La validación automatizada de RC1 está acreditada; la publicación estable requiere registrar el alcance de terreno y verificar el ejecutable final. Una casilla pendiente significa falta de evidencia detallada, no una prueba fallida.

## Evidencia disponible de RC1

| Elemento | Evidencia |
| --- | --- |
| Código compilado | `850047ebb45cc9b28a04b13e53130083036cfb35` |
| CI | [Windows build, ejecución 36142708345](https://github.com/xdCL/Josts/actions/runs/36142708345), 25-09-2026 |
| Compilación | MSVC, Release, x86; anfitrión Windows Server 2025 |
| Pruebas | `core`, `ui`, `edge_policy`, `privileged`: 4/4 aprobadas |
| Artefacto CI | `Josts-Windows-x86`, ID `10868375841`; contiene `Josts-v1.2.0-dev.exe` |
| EXE publicado | `Josts-v1.2.0-rc1.exe`, 1.077.248 bytes, PE32/i386 |
| SHA-256 del EXE | `fb6afdaa4c6bb32fc3edaee3f892ed11a85446f817aa45a75f6c2803be52f8ac` |
| Correspondencia | El hash recalculado del EXE del artefacto CI coincide con el digest del asset publicado en GitHub |
| Terreno | La [release original](https://github.com/xdCL/Josts/releases/tag/v1.2.0-rc1) declara pruebas en equipos Windows 10; no detalla equipos, arquitectura, casos ni resultados |

La rama `dev/v1.2.0` en `a8e53ca88020865e8671c874b3705b8b51aadd02` añade únicamente las notas de release sobre ese código. La preparación de estable parte de allí. El código del PR #1 está íntegramente contenido en esta historia.

**Discrepancia histórica:** la etiqueta `v1.2.0-rc1` apunta a `648d0d19f80ce7a9124a650e013c874ba9d5ce98`. No representa el código del EXE. Véase la [fe de erratas](RELEASE_v1.2.0-rc1.md#fe-de-erratas-de-trazabilidad--22-de-octubre-de-2026).

## Qué cubren las pruebas automatizadas

- `core`: parsing, recursos incorporados, archivos de prueba, respaldos/restauración, bloqueo externo equivalente, conflicto externo sin detener el resto de la lista y diagnóstico de hosts inexistente o carpeta.
- `ui`: operaciones sobre archivos de prueba, estados, verificación, progreso y cierre automático.
- `edge_policy`: API del Registro simulada; no comprueba una instancia real de Edge.
- `privileged`: protocolo e intercambio entre procesos con archivos de prueba, cancelación y denegación simuladas; no eleva realmente ni prueba credenciales de dos cuentas.

El ejecutable x86 ejecutado en Windows Server x64 no acredita un Windows 10 x86 real. El workflow genera `test-results.xml` y `build-info.json` para relacionar las nuevas compilaciones con su commit y sus resultados.

## Registro de terreno

Recuperar primero los resultados ya obtenidos. Repetir solo los casos no acreditados o afectados por un cambio funcional. Registrar la fecha real de cada prueba; no atribuir pruebas nuevas a la publicación histórica.

| Escenario | Resultado a acreditar | Estado |
| --- | --- | --- |
| Windows 10 x64; cuenta estándar y otra cuenta administradora | Consulta sin elevación; UAC al aplicar; cambio en el hosts global visible al volver a la cuenta estándar | Sin registro detallado |
| Cancelar o denegar UAC | Sin cambios en hosts; diagnóstico comprensible; interfaz utilizable | Sin registro detallado |
| Bloqueos externos equivalentes y mapeo externo diferente | Sin duplicados; mapeo ajeno conservado; resto de la lista aplicado; conflicto visible y sin cierre automático | Sin registro detallado |
| Quitar y restaurar, en equipo de prueba | Quitar conserva entradas externas; restaurar recupera el respaldo original del equipo | Sin registro detallado |
| EXE solo, sin archivos auxiliares | Precarga incorporada disponible y operaciones correctas; comprobar también carpeta sin permiso de escritura | Sin registro detallado |
| Windows 10 x86 real | Aplicar/quitar con el mismo EXE; identificar edición y compilación del sistema | Sin registro detallado |
| Noticias de Edge | Escritura verificada y política reconocida en `edge://policy` | Sin registro detallado |

Anotar Windows Home/Pro y su compilación para describir lo realmente probado, sin presentar una edición como validada por inferencia. Si una arquitectura queda sin comprobar, delimitar expresamente el soporte validado antes de publicar.

Para cada ejecución, completar este registro; no incluir contraseñas ni nombres reales de cuentas:

| Dato | Valor |
| --- | --- |
| Fecha y responsable | Pendiente |
| Identificador no sensible del equipo | Pendiente |
| Windows: edición, compilación y x86/x64 | Pendiente |
| Archivo EXE, SHA-256 y commit/ejecución de CI | Pendiente |
| Tipos de cuenta y flujo UAC | Pendiente |
| Escenarios ejecutados y resultado observado | Pendiente |
| Incidencias y referencia a evidencia sin datos personales | Pendiente |

Las pruebas que alteren entradas externas, restauren respaldos o cambien Edge se realizan en un equipo de prueba. La restauración completa reemplaza también cambios ajenos: no confundirla con quitar solo el bloque de Josts.

## Criterio para publicar estable

- [ ] Registrar o delimitar el alcance de terreno, sin fallos bloqueantes abiertos.
- [ ] Integrar la preparación en `main` y anotar su SHA final.
- [ ] Verificar CI con las cuatro suites aprobadas sobre ese SHA y conservar el enlace al run.
- [ ] Comprobar los metadatos `1.2.0.0`, la versión visible y el funcionamiento básico del EXE final.
- [ ] Crear `v1.2.0` sobre ese mismo SHA; comprobar que el `sourceCommit` del artefacto del tag coincide.
- [ ] Publicar el EXE del tag y su SHA-256 con las notas de validación y `pre-release` desmarcado.

La fe de erratas, la versión interna y la documentación no requieren nuevas funciones. Si aparece un defecto funcional que necesite cambios, corregirlo y validar una nueva candidata antes de estable.
