#include "ajustes.h"

#include <algorithm>
#include <vector>

namespace tinta {
namespace {

// ---------------------------------------------------------------------------
// Utilidades de texto UTF-8
// ---------------------------------------------------------------------------

// Las vocales acentuadas y la eñe ocupan dos bytes en UTF-8 y empiezan todas por
// 0xC3. No hace falta una tabla Unicode entera: con el suplemento Latin-1 se
// cubre el español completo.
char SinAcento(unsigned char segundo) {
  switch (segundo) {
    case 0xA0: case 0xA1: case 0xA2: case 0xA3: case 0xA4: case 0xA5:
      return 'a';
    case 0xA8: case 0xA9: case 0xAA: case 0xAB:
      return 'e';
    case 0xAC: case 0xAD: case 0xAE: case 0xAF:
      return 'i';
    case 0xB1:
      return 'n';
    case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6:
      return 'o';
    case 0xB9: case 0xBA: case 0xBB: case 0xBC:
      return 'u';
    case 0xBD: case 0xBF:
      return 'y';
    default:
      return 0;
  }
}

char Minuscula(char c) {
  if (c >= 'A' && c <= 'Z') return static_cast<char>(c - 'A' + 'a');
  return c;
}

bool EsEspacio(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

// Mayúscula de la primera letra, también si lleva tilde.
void PonerMayuscula(std::string* texto, size_t desde) {
  if (!texto || desde >= texto->size()) return;
  unsigned char primero = static_cast<unsigned char>((*texto)[desde]);
  if (primero >= 'a' && primero <= 'z') {
    (*texto)[desde] = static_cast<char>(primero - 'a' + 'A');
    return;
  }
  // Á es 0xC3 0x81 y á es 0xC3 0xA1: la mayúscula está 0x20 por debajo.
  if (primero == 0xC3 && desde + 1 < texto->size()) {
    unsigned char segundo = static_cast<unsigned char>((*texto)[desde + 1]);
    if (segundo >= 0xA0 && segundo <= 0xBE && segundo != 0xB7) {
      (*texto)[desde + 1] = static_cast<char>(segundo - 0x20);
    }
  }
}

// Si lo último escrito cierra una frase, lo siguiente empieza en mayúscula.
bool EmpiezaFrase(const std::string& salida) {
  for (auto it = salida.rbegin(); it != salida.rend(); ++it) {
    const char c = *it;
    if (c == ' ') continue;
    return c == '.' || c == '?' || c == '!' || c == '\n';
  }
  return true;  // no hay nada escrito todavía
}

// ---------------------------------------------------------------------------
// Puntuación dictada
// ---------------------------------------------------------------------------

struct SignoDictado {
  const char* frase;
  const char* texto;
  int saltos;          // saltos de línea que añade después
  bool pega_izquierda; // sin espacio antes: los signos de cierre
  bool pega_derecha;   // sin espacio después: los de apertura
};

// Solo español. El reconocedor devuelve las palabras tal cual se dicen y el
// acento ya se pliega antes de comparar, así que «línea» y «linea» entran las
// dos por la misma puerta.
const SignoDictado kSignos[] = {
    {"punto y aparte", ".", 2, true, false},
    {"punto y seguido", ".", 0, true, false},
    {"punto y coma", ";", 0, true, false},
    {"puntos suspensivos", "...", 0, true, false},
    {"signo de interrogacion", "?", 0, true, false},
    {"signo de exclamacion", "!", 0, true, false},
    {"abre interrogacion", "\xC2\xBF", 0, false, true},
    {"abre exclamacion", "\xC2\xA1", 0, false, true},
    {"cierra interrogacion", "?", 0, true, false},
    {"cierra exclamacion", "!", 0, true, false},
    {"abre parentesis", "(", 0, false, true},
    {"cierra parentesis", ")", 0, true, false},
    {"salto de linea", "", 1, true, false},
    {"nueva linea", "", 1, true, false},
    {"nuevo parrafo", "", 2, true, false},
    {"dos puntos", ":", 0, true, false},
    {"interrogacion", "?", 0, true, false},
    {"exclamacion", "!", 0, true, false},
    {"parentesis", "(", 0, false, true},
    {"comillas", "\"", 0, true, false},
    {"guion", "-", 0, true, false},
    {"punto", ".", 0, true, false},
    {"coma", ",", 0, true, false},
};

const SignoDictado* BuscarSigno(const std::string& clave) {
  for (const SignoDictado& s : kSignos) {
    if (clave == s.frase) return &s;
  }
  return nullptr;
}

}  // namespace

// ---------------------------------------------------------------------------
// Puntero
// ---------------------------------------------------------------------------

float AjustesPuntero::RadioEfectivo() const {
  return std::clamp(radio, kRadioPunteroMinimo, kRadioPunteroMaximo);
}

float AjustesPuntero::GrosorAnilloEfectivo() const {
  // El anillo nunca puede ser más grueso que el propio radio: pasado ese punto
  // deja de ser un anillo y es un disco mal dibujado.
  return std::clamp(grosor_anillo, 0.5f, RadioEfectivo());
}

uint32_t AjustesPuntero::ColorEfectivo(uint32_t color_pluma) const {
  if (!seguir_color_pluma) return color_argb;
  // Se toma el color de la pluma pero se conserva la transparencia del puntero:
  // un puntero opaco tapa justo lo que se quiere mirar.
  const uint32_t alfa = color_argb & 0xFF000000u;
  return alfa | (color_pluma & 0x00FFFFFFu);
}

bool AjustesPuntero::DebeVerse(bool hay_hover, bool dibujando) const {
  if (!visible || !hay_hover) return false;
  if (dibujando && !mostrar_al_dibujar) return false;
  return true;
}

// ---------------------------------------------------------------------------
// Pantalla
// ---------------------------------------------------------------------------

bool AjustesPantalla::Debe(bool grabando, bool escribiendo) const {
  if (siempre_activa) return true;
  if (grabando && siempre_activa_al_grabar) return true;
  if (escribiendo && siempre_activa_al_escribir) return true;
  return false;
}

// ---------------------------------------------------------------------------
// Panel lateral
// ---------------------------------------------------------------------------

bool PanelCubreTodo(float ancho_total, const AjustesPanel& a) {
  if (ancho_total <= 0.0f) return true;
  if (ancho_total < a.ancho_minimo_para_dividir) return true;
  // El mínimo del panel no cabe dentro de su fracción máxima: repartir daría
  // una calculadora ilegible o un cuaderno sin sitio para escribir.
  return a.min_pt > a.max_fraccion * ancho_total;
}

float AnchoPanel(float ancho_total, const AjustesPanel& a) {
  if (!a.visible || ancho_total <= 0.0f) return 0.0f;
  if (PanelCubreTodo(ancho_total, a)) return ancho_total;

  const float maximo = a.max_fraccion * ancho_total;
  float ancho = a.fraccion * ancho_total;
  if (ancho < a.min_pt) ancho = a.min_pt;
  if (ancho > maximo) ancho = maximo;
  return ancho;
}

float AnchoLienzo(float ancho_total, const AjustesPanel& a) {
  if (ancho_total <= 0.0f) return 0.0f;
  // Cubriendo todo, el lienzo no se encoge: se queda detrás, entero. Encogerlo
  // reflowearía los apuntes cada vez que se abre la calculadora.
  if (a.visible && PanelCubreTodo(ancho_total, a)) return ancho_total;
  return ancho_total - AnchoPanel(ancho_total, a);
}

float OrigenPanel(float ancho_total, const AjustesPanel& a) {
  if (!a.visible) return ancho_total;              // fuera de pantalla
  if (PanelCubreTodo(ancho_total, a)) return 0.0f;
  return a.lado == LadoPanel::Derecha ? ancho_total - AnchoPanel(ancho_total, a)
                                      : 0.0f;
}

float OrigenLienzo(float ancho_total, const AjustesPanel& a) {
  if (!a.visible) return 0.0f;
  if (PanelCubreTodo(ancho_total, a)) return 0.0f;
  return a.lado == LadoPanel::Izquierda ? AnchoPanel(ancho_total, a) : 0.0f;
}

float FraccionAlSoltar(float fraccion_cruda, const AjustesPanel& a) {
  if (fraccion_cruda <= 0.0f) return 0.0f;

  const float candidatos[] = {1.0f / 3.0f, 0.5f, 2.0f / 3.0f};

  // Los repartes que la configuración no permite no se ofrecen. `min_pt` no
  // entra aquí: depende del ancho de pantalla, que este cálculo no conoce, y
  // AnchoPanel ya lo aplica después.
  float mejor = -1.0f;
  float distancia = 0.0f;
  for (const float c : candidatos) {
    // Con holgura: un máximo escrito a mano como 0,66 no debe dejar fuera el
    // reparto de dos tercios por redondeo. La diferencia es menos de un punto
    // en una pantalla de 1366, y AnchoPanel recorta después de todos modos.
    if (c > a.max_fraccion + 0.001f) continue;
    const float d = c > fraccion_cruda ? c - fraccion_cruda : fraccion_cruda - c;
    if (mejor < 0.0f || d < distancia) {
      mejor = c;
      distancia = d;
    }
  }
  if (mejor < 0.0f) mejor = a.max_fraccion;   // ni el reparto más pequeño cabe

  // Arrastrar casi hasta el borde cierra el panel: es el gesto natural y evita
  // tener que ir a buscar un botón. El umbral es la mitad del reparto más
  // pequeño que se ofrece, para que cerrar no compita con «un tercio».
  if (fraccion_cruda < mejor * 0.5f) return 0.0f;
  return mejor;
}

// ---------------------------------------------------------------------------
// Dictado
// ---------------------------------------------------------------------------

std::string PlegarAcentos(const std::string& texto) {
  std::string salida;
  salida.reserve(texto.size());
  for (size_t i = 0; i < texto.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(texto[i]);
    if (c == 0xC3 && i + 1 < texto.size()) {
      unsigned char segundo = static_cast<unsigned char>(texto[i + 1]);
      // Las mayúsculas acentuadas están 0x20 por debajo de las minúsculas: se
      // baja a minúscula para buscar en la tabla y se devuelve la caja que
      // tenía, que plegar el acento no es cambiar de caja.
      const bool era_mayuscula = (segundo >= 0x80 && segundo <= 0x9E);
      if (era_mayuscula) segundo = static_cast<unsigned char>(segundo + 0x20);
      char plano = SinAcento(segundo);
      if (plano != 0) {
        if (era_mayuscula) plano = static_cast<char>(plano - 'a' + 'A');
        salida.push_back(plano);
        ++i;
        continue;
      }
    }
    salida.push_back(static_cast<char>(c));
  }
  return salida;
}

std::string LimpiarDictado(const std::string& crudo,
                           const AjustesDictado& ajustes) {
  // Se parte en palabras y se guarda cada una dos veces: como se dijo, y
  // plegada a minúsculas sin acentos para poder compararla.
  std::vector<std::string> palabras;
  std::vector<std::string> claves;
  {
    std::string actual;
    for (char c : crudo) {
      if (EsEspacio(c)) {
        if (!actual.empty()) {
          palabras.push_back(actual);
          actual.clear();
        }
      } else {
        actual.push_back(c);
      }
    }
    if (!actual.empty()) palabras.push_back(actual);
  }
  claves.reserve(palabras.size());
  for (const std::string& p : palabras) {
    std::string clave = PlegarAcentos(p);
    for (char& c : clave) c = Minuscula(c);
    claves.push_back(clave);
  }

  std::string salida;
  salida.reserve(crudo.size() + 8);
  bool sin_espacio = true;  // al principio no se pone espacio delante

  auto anadir = [&](const std::string& trozo, bool pega_izquierda,
                    bool mayuscula) {
    if (trozo.empty()) return;
    if (!sin_espacio && !pega_izquierda) salida.push_back(' ');
    const size_t inicio = salida.size();
    salida += trozo;
    if (mayuscula) PonerMayuscula(&salida, inicio);
    sin_espacio = false;
  };

  for (size_t i = 0; i < palabras.size();) {
    const SignoDictado* signo = nullptr;
    size_t consumidas = 1;

    if (ajustes.puntuacion_hablada) {
      // De la frase más larga a la más corta: «punto y coma» no puede ganarla
      // «punto», que también encaja.
      for (size_t n = 3; n >= 1 && !signo; --n) {
        if (i + n > palabras.size()) continue;
        std::string frase = claves[i];
        for (size_t k = 1; k < n; ++k) frase += " " + claves[i + k];
        signo = BuscarSigno(frase);
        if (signo) consumidas = n;
      }
    }

    if (signo) {
      if (signo->texto[0] != '\0') {
        anadir(signo->texto, signo->pega_izquierda, false);
      }
      for (int s = 0; s < signo->saltos; ++s) salida.push_back('\n');
      if (signo->saltos > 0 || signo->pega_derecha) sin_espacio = true;
    } else {
      anadir(palabras[i], false,
             ajustes.mayuscula_inicial && EmpiezaFrase(salida));
    }
    i += consumidas;
  }

  // Un salto de línea final es ruido: el cursor ya está donde toca.
  while (!salida.empty() && (salida.back() == '\n' || salida.back() == ' ')) {
    salida.pop_back();
  }
  return salida;
}

}  // namespace tinta
