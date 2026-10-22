#pragma once

namespace josts {
// Desactiva el contenido de Microsoft en la nueva pestaña de Edge para el equipo.
// Requiere permisos de administrador. GetLastError() contiene el error si falla.
bool BloquearContenidoNoticiasEdge() noexcept;
}
