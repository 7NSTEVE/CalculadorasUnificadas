// Ajustes que no son del documento sino de cómo se trabaja: el puntero del
// lápiz del iPad, la pantalla siempre encendida y el dictado.
#pragma once

#include <cstdint>
#include <string>

namespace tinta {

// ---------------------------------------------------------------------------
// Puntero del lápiz del iPad
// ---------------------------------------------------------------------------

// El puntero **no es la pluma**.
//
// El grosor de la pluma es el ancho del trazo que queda en el papel. El puntero
// es la marca que señala en el aire dónde está la punta antes de tocar, con el
// hover del lápiz del iPad. Son dos números distintos y confundirlos obliga a
// elegir entre ver dónde vas a escribir y escribir fino.
struct AjustesPuntero {
  bool visible = true;
  // Radio en puntos, independiente del grosor de la pluma.
  float radio = 6.0f;
  uint32_t color_argb = 0x8C0A84FFu;  // azul del sistema, medio transparente
  // Toma el color de la pluma en vez del suyo, conservando su transparencia.
  bool seguir_color_pluma = false;
  // Anillo en vez de disco: deja ver lo que hay debajo de la punta.
  bool anillo = false;
  float grosor_anillo = 1.5f;
  // Mientras se dibuja el puntero estorba: la tinta ya dice dónde está la punta.
  bool mostrar_al_dibujar = false;

  float RadioEfectivo() const;
  float GrosorAnilloEfectivo() const;
  uint32_t ColorEfectivo(uint32_t color_pluma) const;
  bool DebeVerse(bool hay_hover, bool dibujando) const;
};

constexpr float kRadioPunteroMinimo = 1.0f;
constexpr float kRadioPunteroMaximo = 48.0f;

// ---------------------------------------------------------------------------
// Pantalla siempre activa
// ---------------------------------------------------------------------------

// Que la pantalla no se apague sola.
//
// Se separa en tres casos a propósito: dejarla encendida siempre gasta batería
// para nada, pero que se apague en mitad de una grabación de clase, o mientras
// se piensa un ejercicio con el lápiz en la mano, es peor.
struct AjustesPantalla {
  bool siempre_activa = false;
  bool siempre_activa_al_grabar = true;
  bool siempre_activa_al_escribir = true;

  bool Debe(bool grabando, bool escribiendo) const;
};

// ---------------------------------------------------------------------------
// Panel lateral: las Casio dentro del cuaderno
// ---------------------------------------------------------------------------

// Reparto de la pantalla entre el lienzo y el panel de la calculadora.
//
// Está aquí y no en Swift porque es geometría, y la geometría se prueba en
// Windows en segundos. Lo que queda del lado nativo es colocar dos vistas en
// los rectángulos que decide esto.
//
// El caso que obliga a pensarlo es el iPhone en vertical: no caben lienzo y
// calculadora a la vez, y forzarlos deja los dos inservibles. Por debajo de
// cierto ancho el panel pasa a cubrir la pantalla entera, que es lo que hace
// falta de verdad.

enum class LadoPanel { Derecha, Izquierda };

struct AjustesPanel {
  bool visible = false;
  LadoPanel lado = LadoPanel::Derecha;
  // Parte del ancho total que se lleva el panel.
  float fraccion = 0.38f;
  // La ClassWiz con su carátula no se lee por debajo de esto.
  float min_pt = 320.0f;
  // Más de dos tercios y el cuaderno deja de ser el sitio donde se escribe.
  // Es 2/3 exacto y no 0,66: con 0,66 el reparto de dos tercios se quedaba
  // fuera por siete milésimas y el divisor no llegaba nunca a pegarse ahí.
  float max_fraccion = 2.0f / 3.0f;
  // Debajo de este ancho no se reparte: el panel cubre todo.
  float ancho_minimo_para_dividir = 700.0f;
};

// ¿Hay que dejar de repartir y que el panel ocupe la pantalla entera? Es cierto
// tanto si la pantalla es estrecha como si el mínimo del panel no cabe dentro
// de su fracción máxima, que es la misma situación vista desde el otro lado.
bool PanelCubreTodo(float ancho_total, const AjustesPanel& a);

// Los cuatro números que necesita la capa nativa. Con el panel oculto, el
// lienzo se queda con todo y el panel mide cero.
float AnchoPanel(float ancho_total, const AjustesPanel& a);
float AnchoLienzo(float ancho_total, const AjustesPanel& a);
float OrigenPanel(float ancho_total, const AjustesPanel& a);
float OrigenLienzo(float ancho_total, const AjustesPanel& a);

// Al soltar el divisor la fracción se pega al reparto más cercano de
// 1/3, 1/2 y 2/3, para que no queden anchos arbitrarios imposibles de repetir.
// Devuelve 0 si hay que esconder el panel: arrastrarlo casi hasta el borde es
// la forma natural de cerrarlo, y así no hace falta un botón aparte.
float FraccionAlSoltar(float fraccion_cruda, const AjustesPanel& a);

// ---------------------------------------------------------------------------
// Escribir a mano, a teclado o a voz
// ---------------------------------------------------------------------------

enum class ModoEscritura { Lapiz, Teclado, Voz };

struct AjustesDictado {
  // «coma» se convierte en «,». Sin esto hay que dictar la puntuación y
  // corregirla después a mano.
  bool puntuacion_hablada = true;
  bool mayuscula_inicial = true;
  // Reconocimiento en el propio aparato: funciona sin red y no manda la voz a
  // ningún servidor.
  bool en_dispositivo = true;
  std::string idioma = "es-CR";
};

// Limpia lo que devuelve el reconocedor de voz.
//
// Hace tres cosas: convierte la puntuación dictada («coma», «punto y aparte»,
// «nueva linea») en signos de verdad, quita el espacio que quedaría antes del
// signo, y pone mayúscula al empezar y después de cada punto.
//
// Es texto, no audio: se prueba en Windows sin micrófono.
std::string LimpiarDictado(const std::string& crudo,
                           const AjustesDictado& ajustes);

// Quita los acentos de una cadena UTF-8 para poder comparar palabras dictadas.
// «línea» y «linea» tienen que valer lo mismo.
std::string PlegarAcentos(const std::string& texto);

}  // namespace tinta
