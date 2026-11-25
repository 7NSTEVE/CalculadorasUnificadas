// Lógica de IA local.

#include "ia_local.h"

#include <sstream>

namespace tinta {

std::string GenerarPromptSocratico(const Pagina& pagina,
                                   const std::string& pregunta_usuario) {
  std::stringstream ss;
  ss << "Eres un tutor socrático. Tu objetivo es guiar al estudiante para que "
        "descubra la respuesta por sí mismo mediante pistas o preguntas, SIN "
        "dar la respuesta directa.\n\n"
     << "Contexto de los apuntes:\n";

  std::string contexto_total;
  for (const auto& cuadro : pagina.textos) {
    if (!cuadro.texto.empty()) {
      contexto_total += cuadro.texto + "\n";
    }
  }
  for (const auto& nota : pagina.notas) {
    if (!nota.texto.empty()) {
      contexto_total += nota.texto + "\n";
    }
  }

  // Límite de seguridad para no exceder la ventana de tokens.
  const size_t limite = 2000;
  if (contexto_total.length() > limite) {
    contexto_total = contexto_total.substr(0, limite);
    contexto_total += "... [recortado]";
  }

  if (contexto_total.empty()) {
    ss << "(No hay texto en esta página)\n";
  } else {
    ss << contexto_total;
  }

  ss << "\nPregunta del estudiante: " << pregunta_usuario;

  return ss.str();
}

}  // namespace tinta
