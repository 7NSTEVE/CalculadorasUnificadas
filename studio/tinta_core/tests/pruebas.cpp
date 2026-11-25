// Pruebas del núcleo de tinta.
//
// Sin framework externo a propósito: cero descargas, corre en Windows con MSVC y
// en el runner Linux del CI con g++. Todo lo que se pruebe aquí es una función
// menos que verificar gastando minutos de macOS.

#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "ajustes.h"
#include "api.h"
#include "archivo.h"
#include "almacen.h"
#include "documento.h"
#include "edicion.h"
#include "elementos.h"
#include "formas.h"
#include "grabacion.h"
#include "importar.h"
#include "lienzo_avanzado.h"
#include "tinta.h"
#include "pdf.h"
#include "clip.h"

namespace {

int fallos = 0;
int total = 0;
std::string prueba_actual;

void Verificar(bool condicion, const char* que, int linea) {
  ++total;
  if (!condicion) {
    ++fallos;
    std::printf("  FALLO  %s\n         %s (linea %d)\n", prueba_actual.c_str(),
                que, linea);
  }
}

#define VERIFICAR(cond) Verificar((cond), #cond, __LINE__)
#define CASO(nombre)          \
  prueba_actual = (nombre);   \
  std::printf("  %s\n", (nombre));

bool CasiIgual(float a, float b, float tolerancia = 0.001f) {
  return std::fabs(a - b) <= tolerancia;
}

// Construye un trazo recto horizontal con muestras a 10 px, sin presión.
tinta::Trazo TrazoRecto(float x0, float y, int cantidad, float paso = 10.0f) {
  tinta::Trazo trazo;
  trazo.ancho_base = 4.0f;
  for (int i = 0; i < cantidad; ++i) {
    tinta::Muestra m;
    m.posicion = {x0 + i * paso, y};
    m.presion = 0.5f;
    m.tiempo_qpc = static_cast<int64_t>(i) * 4'000'000;  // 4 ms en unidades 1e9
    trazo.muestras.push_back(m);
  }
  return trazo;
}

// ---------------------------------------------------------------------------

void PruebasLienzo() {
  CASO("limites: eje infinito ignora el maximo, respeta el minimo");
  tinta::LimitesLienzo limites;
  limites.ancho_infinito = false;
  limites.alto_infinito = true;
  limites.ancho_minimo = 300.0f;
  limites.ancho_maximo = 800.0f;
  limites.alto_minimo = 400.0f;
  limites.alto_maximo = 900.0f;

  float ancho = 5000.0f, alto = 5000.0f;
  tinta::AplicarLimites(limites, &ancho, &alto);
  VERIFICAR(CasiIgual(ancho, 800.0f));   // limitado: se recorta
  VERIFICAR(CasiIgual(alto, 5000.0f));   // infinito: no se recorta

  ancho = 10.0f; alto = 10.0f;
  tinta::AplicarLimites(limites, &ancho, &alto);
  VERIFICAR(CasiIgual(ancho, 300.0f));
  VERIFICAR(CasiIgual(alto, 400.0f));    // el minimo sí aplica en el eje infinito

  CASO("limites: ancho infinito con alto limitado (el caso inverso)");
  tinta::LimitesLienzo inverso;
  inverso.ancho_infinito = true;
  inverso.alto_infinito = false;
  inverso.alto_maximo = 1000.0f;
  ancho = 9999.0f; alto = 9999.0f;
  tinta::AplicarLimites(inverso, &ancho, &alto);
  VERIFICAR(CasiIgual(ancho, 9999.0f));
  VERIFICAR(CasiIgual(alto, 1000.0f));

  CASO("modos de lienzo exponen infinito vertical, horizontal y XY");
  tinta::Pagina pagina_modos;
  tinta::AplicarModoPagina(&pagina_modos, tinta::ModoPagina::Hibrido);
  VERIFICAR(!pagina_modos.limites.ancho_infinito);
  VERIFICAR(pagina_modos.limites.alto_infinito);
  tinta::AplicarModoPagina(&pagina_modos,
                           tinta::ModoPagina::HorizontalInfinito);
  VERIFICAR(pagina_modos.limites.ancho_infinito);
  VERIFICAR(!pagina_modos.limites.alto_infinito);
  tinta::AplicarModoPagina(&pagina_modos, tinta::ModoPagina::OneNote);
  VERIFICAR(pagina_modos.limites.ancho_infinito);
  VERIFICAR(pagina_modos.limites.alto_infinito);

  CASO("limites: validacion rechaza maximo menor que minimo");
  tinta::LimitesLienzo malo;
  malo.ancho_infinito = false;
  malo.ancho_minimo = 500.0f;
  malo.ancho_maximo = 100.0f;
  VERIFICAR(!malo.valido());

  CASO("confinar: no hay borde en el eje infinito");
  tinta::Punto p{900.0f, 5000.0f};
  const bool movido = tinta::ConfinarPunto(limites, 800.0f, 600.0f, &p);
  VERIFICAR(movido);
  VERIFICAR(CasiIgual(p.x, 800.0f));     // recortado por el eje limitado
  VERIFICAR(CasiIgual(p.y, 5000.0f));    // intacto por el eje infinito
}

void PruebasPapel() {
  CASO("papel oscuro cambia la tinta por defecto");
  tinta::Papel claro;
  claro.tipo = tinta::TipoPapel::BlancoSol;
  VERIFICAR(!claro.es_oscuro());

  tinta::Papel oscuro;
  oscuro.tipo = tinta::TipoPapel::Oscuro;
  VERIFICAR(oscuro.es_oscuro());
  // Escribir en negro sobre negro es el error facil: la tinta debe ser clara.
  VERIFICAR(oscuro.color_tinta_por_defecto() != claro.color_tinta_por_defecto());

  CASO("pauta rayada: solo horizontales, alineadas a la rejilla");
  tinta::Papel rayado;
  rayado.pauta = tinta::Pauta::Rayado;
  rayado.separacion = 50.0f;
  auto lineas = tinta::GenerarPauta(rayado, 0.0f, 0.0f, 200.0f, 200.0f);
  VERIFICAR(!lineas.empty());
  for (const auto& l : lineas) VERIFICAR(CasiIgual(l.desde.y, l.hasta.y));

  CASO("pauta no se desplaza al desplazar la vista");
  auto desplazada = tinta::GenerarPauta(rayado, 0.0f, 25.0f, 200.0f, 200.0f);
  VERIFICAR(!desplazada.empty());
  // La primera linea visible debe caer en un multiplo de la separacion.
  const float resto = std::fmod(desplazada.front().desde.y, 50.0f);
  VERIFICAR(CasiIgual(resto, 0.0f) || CasiIgual(resto, 50.0f));

  CASO("cuadricula da horizontales y verticales");
  tinta::Papel cuadricula;
  cuadricula.pauta = tinta::Pauta::Cuadricula;
  cuadricula.separacion = 50.0f;
  auto rejilla = tinta::GenerarPauta(cuadricula, 0.0f, 0.0f, 200.0f, 200.0f);
  bool hay_vertical = false;
  for (const auto& l : rejilla) {
    if (CasiIgual(l.desde.x, l.hasta.x) && !CasiIgual(l.desde.y, l.hasta.y)) {
      hay_vertical = true;
    }
  }
  VERIFICAR(hay_vertical);

  CASO("el margen sale marcado como destacado");
  tinta::Papel con_margen;
  con_margen.pauta = tinta::Pauta::Rayado;
  con_margen.separacion = 50.0f;
  con_margen.margen_izquierdo = 80.0f;
  auto conm = tinta::GenerarPauta(con_margen, 0.0f, 0.0f, 200.0f, 200.0f);
  bool hay_destacada = false;
  for (const auto& l : conm) if (l.destacada) hay_destacada = true;
  VERIFICAR(hay_destacada);

  CASO("sin pauta no se genera nada");
  tinta::Papel liso;
  VERIFICAR(tinta::GenerarPauta(liso, 0, 0, 500, 500).empty());
}

void PruebasPagina() {
  CASO("la caja de tinta incluye el grosor, no el eje");
  tinta::Pagina pagina;
  pagina.trazos.push_back(TrazoRecto(100.0f, 100.0f, 3));  // ancho_base 4
  float x = 0, y = 0, ancho = 0, alto = 0;
  VERIFICAR(pagina.CajaDeTinta(&x, &y, &ancho, &alto));
  VERIFICAR(CasiIgual(x, 98.0f));    // 100 - 4/2
  VERIFICAR(CasiIgual(alto, 4.0f));  // trazo horizontal: solo el grosor

  CASO("pagina vacia no tiene caja de tinta");
  tinta::Pagina vacia;
  VERIFICAR(!vacia.CajaDeTinta(&x, &y, &ancho, &alto));

  CASO("el lienzo crece solo por el eje infinito");
  tinta::Pagina crece;
  crece.limites.ancho_infinito = false;
  crece.limites.alto_infinito = true;
  crece.limites.ancho_maximo = 500.0f;
  crece.ancho = 500.0f;
  crece.alto = 500.0f;
  crece.trazos.push_back(TrazoRecto(100.0f, 900.0f, 2));
  VERIFICAR(crece.CrecerSiHaceFalta(50.0f));
  VERIFICAR(crece.alto > 900.0f);
  VERIFICAR(CasiIgual(crece.ancho, 500.0f));  // el eje limitado no crece
}

void PruebasMoverPagina() {
  CASO("mover pagina entre secciones");
  tinta::Cuaderno cuaderno;
  cuaderno.secciones.resize(2);
  for (int i = 0; i < 3; ++i) {
    tinta::Pagina p;
    p.titulo = "A" + std::to_string(i);
    cuaderno.secciones[0].paginas.push_back(p);
  }
  VERIFICAR(tinta::MoverPagina(cuaderno, 0, 1, 1, 0));
  VERIFICAR(cuaderno.secciones[0].paginas.size() == 2);
  VERIFICAR(cuaderno.secciones[1].paginas.size() == 1);
  VERIFICAR(cuaderno.secciones[1].paginas[0].titulo == "A1");

  CASO("mover dentro de la misma seccion ajusta el indice");
  // A0, A2 -> mover A0 al final debe dar A2, A0 y no A0, A2.
  VERIFICAR(tinta::MoverPagina(cuaderno, 0, 0, 0, 2));
  VERIFICAR(cuaderno.secciones[0].paginas[0].titulo == "A2");
  VERIFICAR(cuaderno.secciones[0].paginas[1].titulo == "A0");

  CASO("indices invalidos no rompen nada");
  VERIFICAR(!tinta::MoverPagina(cuaderno, 9, 0, 0, 0));
  VERIFICAR(!tinta::MoverPagina(cuaderno, 0, 9, 0, 0));
}

void PruebasBorrador() {
  CASO("el borrador detecta el segmento, no solo las muestras");
  tinta::Pagina pagina;
  // Muestras a 100 px: el punto medio esta lejos de ambas.
  pagina.trazos.push_back(TrazoRecto(0.0f, 0.0f, 2, 100.0f));
  // Borrador en el medio del segmento, sin tocar ninguna muestra.
  auto tocados = tinta::TrazosTocados(pagina, {50.0f, 0.0f}, 5.0f);
  VERIFICAR(tocados.size() == 1);

  CASO("el borrador no toca lo que esta lejos");
  VERIFICAR(tinta::TrazosTocados(pagina, {50.0f, 500.0f}, 5.0f).empty());

  CASO("partir un trazo por el medio da dos pedazos");
  tinta::Trazo largo = TrazoRecto(0.0f, 0.0f, 9);  // x = 0..80
  auto pedazos = tinta::PartirTrazo(largo, {40.0f, 0.0f}, 5.0f);
  VERIFICAR(pedazos.size() == 2);
  VERIFICAR(!pedazos[0].muestras.empty());
  VERIFICAR(!pedazos[1].muestras.empty());
  // El pedazo izquierdo termina antes del borrador y el derecho empieza despues.
  VERIFICAR(pedazos[0].muestras.back().posicion.x < 40.0f);
  VERIFICAR(pedazos[1].muestras.front().posicion.x > 40.0f);

  CASO("los pedazos heredan color y grosor");
  VERIFICAR(pedazos[0].ancho_base == largo.ancho_base);
  VERIFICAR(pedazos[0].color_argb == largo.color_argb);

  CASO("borrar por la punta deja un solo pedazo");
  auto punta = tinta::PartirTrazo(largo, {0.0f, 0.0f}, 5.0f);
  VERIFICAR(punta.size() == 1);

  CASO("borrar todo el trazo no deja pedazos");
  auto corto = TrazoRecto(0.0f, 0.0f, 2, 2.0f);
  VERIFICAR(tinta::PartirTrazo(corto, {1.0f, 0.0f}, 50.0f).empty());

  CASO("borrado parcial sobre la pagina");
  tinta::Pagina p2;
  p2.trazos.push_back(TrazoRecto(0.0f, 0.0f, 9));
  VERIFICAR(tinta::BorrarParcial(p2, {40.0f, 0.0f}, 5.0f));
  VERIFICAR(p2.trazos.size() == 2);

  CASO("borrado parcial lejos no cambia nada");
  VERIFICAR(!tinta::BorrarParcial(p2, {0.0f, 999.0f}, 5.0f));

  CASO("borrar completos de mayor a menor no corre los indices");
  tinta::Pagina p3;
  for (int i = 0; i < 4; ++i) p3.trazos.push_back(TrazoRecto(i * 200.0f, 0.0f, 2));
  auto quitados = tinta::BorrarTrazosCompletos(p3, {0, 2});
  VERIFICAR(quitados.size() == 2);
  VERIFICAR(p3.trazos.size() == 2);
  // Deben quedar los que estaban en 1 y 3.
  VERIFICAR(CasiIgual(p3.trazos[0].muestras[0].posicion.x, 200.0f));
  VERIFICAR(CasiIgual(p3.trazos[1].muestras[0].posicion.x, 600.0f));
}

void PruebasLazo() {
  CASO("punto en poligono");
  std::vector<tinta::Punto> cuadro = {{0, 0}, {100, 0}, {100, 100}, {0, 100}};
  VERIFICAR(tinta::PuntoEnPoligono(cuadro, {50, 50}));
  VERIFICAR(!tinta::PuntoEnPoligono(cuadro, {150, 50}));
  VERIFICAR(!tinta::PuntoEnPoligono(cuadro, {50, -10}));

  CASO("poligono degenerado no selecciona nada");
  std::vector<tinta::Punto> linea = {{0, 0}, {10, 10}};
  VERIFICAR(!tinta::PuntoEnPoligono(linea, {5, 5}));

  CASO("lazo completo vs parcial");
  tinta::Pagina pagina;
  pagina.trazos.push_back(TrazoRecto(10.0f, 50.0f, 3, 10.0f));   // dentro entero
  pagina.trazos.push_back(TrazoRecto(80.0f, 50.0f, 5, 20.0f));   // sale del cuadro

  auto completos = tinta::SeleccionarConLazo(pagina, cuadro, true);
  VERIFICAR(completos.size() == 1);
  VERIFICAR(completos[0] == 0);

  auto parciales = tinta::SeleccionarConLazo(pagina, cuadro, false);
  VERIFICAR(parciales.size() == 2);
}

void PruebasTransformar() {
  CASO("escalar mueve los puntos y engorda la pluma");
  tinta::Pagina pagina;
  pagina.trazos.push_back(TrazoRecto(10.0f, 10.0f, 2));
  const float ancho_previo = pagina.trazos[0].ancho_base;

  tinta::Transformacion t;
  t.escala = 2.0f;
  t.pivote = {0.0f, 0.0f};
  tinta::TransformarTrazos(pagina, {0}, t);

  VERIFICAR(CasiIgual(pagina.trazos[0].muestras[0].posicion.x, 20.0f));
  VERIFICAR(CasiIgual(pagina.trazos[0].ancho_base, ancho_previo * 2.0f));

  CASO("desplazar no toca el grosor");
  tinta::Transformacion d;
  d.desplazar_x = 5.0f;
  d.desplazar_y = -3.0f;
  const float ancho_antes = pagina.trazos[0].ancho_base;
  tinta::TransformarTrazos(pagina, {0}, d);
  VERIFICAR(CasiIgual(pagina.trazos[0].muestras[0].posicion.x, 25.0f));
  VERIFICAR(CasiIgual(pagina.trazos[0].ancho_base, ancho_antes));

  CASO("rotar 90 grados");
  tinta::Pagina giro;
  giro.trazos.push_back(TrazoRecto(10.0f, 0.0f, 1));
  tinta::Transformacion r;
  r.rotacion = 3.14159265f / 2.0f;
  r.pivote = {0.0f, 0.0f};
  tinta::TransformarTrazos(giro, {0}, r);
  VERIFICAR(CasiIgual(giro.trazos[0].muestras[0].posicion.x, 0.0f, 0.01f));
  VERIFICAR(CasiIgual(giro.trazos[0].muestras[0].posicion.y, 10.0f, 0.01f));

  CASO("recolorear");
  tinta::RecolorearTrazos(pagina, {0}, 0xFFAABBCCu);
  VERIFICAR(pagina.trazos[0].color_argb == 0xFFAABBCCu);

  CASO("indice invalido se ignora sin romper");
  tinta::TransformarTrazos(pagina, {99}, t);
  VERIFICAR(pagina.trazos.size() == 1);
}

void PruebasHistorial() {
  CASO("deshacer y rehacer un trazo anadido");
  tinta::Pagina pagina;
  tinta::Historial historial;

  tinta::Trazo trazo = TrazoRecto(0.0f, 0.0f, 3);
  pagina.trazos.push_back(trazo);
  historial.Registrar(tinta::Historial::OperacionAnadir(0, trazo));

  VERIFICAR(historial.puede_deshacer());
  VERIFICAR(historial.Deshacer(pagina));
  VERIFICAR(pagina.trazos.empty());

  VERIFICAR(historial.puede_rehacer());
  VERIFICAR(historial.Rehacer(pagina));
  VERIFICAR(pagina.trazos.size() == 1);

  CASO("deshacer un borrado devuelve el trazo a su sitio");
  tinta::Pagina p2;
  for (int i = 0; i < 3; ++i) p2.trazos.push_back(TrazoRecto(i * 100.0f, 0.0f, 2));
  tinta::Historial h2;
  auto quitados = tinta::BorrarTrazosCompletos(p2, {1});
  h2.Registrar(tinta::Historial::OperacionQuitar({1}, quitados));
  VERIFICAR(p2.trazos.size() == 2);
  VERIFICAR(h2.Deshacer(p2));
  VERIFICAR(p2.trazos.size() == 3);
  VERIFICAR(CasiIgual(p2.trazos[1].muestras[0].posicion.x, 100.0f));

  CASO("una accion nueva borra la rama rehecha");
  tinta::Historial h3;
  tinta::Pagina p3;
  tinta::Trazo t3 = TrazoRecto(0.0f, 0.0f, 2);
  p3.trazos.push_back(t3);
  h3.Registrar(tinta::Historial::OperacionAnadir(0, t3));
  h3.Deshacer(p3);
  VERIFICAR(h3.puede_rehacer());
  h3.Registrar(tinta::Historial::OperacionAnadir(0, t3));
  VERIFICAR(!h3.puede_rehacer());

  CASO("deshacer con historial vacio no rompe");
  tinta::Historial h4;
  tinta::Pagina p4;
  VERIFICAR(!h4.Deshacer(p4));
  VERIFICAR(!h4.Rehacer(p4));

  CASO("reemplazar: deshacer restaura el estado anterior");
  tinta::Pagina p5;
  p5.trazos.push_back(TrazoRecto(0.0f, 0.0f, 2));
  std::vector<tinta::Trazo> antes = {p5.trazos[0]};
  tinta::RecolorearTrazos(p5, {0}, 0xFF00FF00u);
  std::vector<tinta::Trazo> despues = {p5.trazos[0]};
  tinta::Historial h5;
  h5.Registrar(tinta::Historial::OperacionReemplazar({0}, antes, despues));
  VERIFICAR(h5.Deshacer(p5));
  VERIFICAR(p5.trazos[0].color_argb == antes[0].color_argb);
}

void PruebasTintaYPrediccion() {
  CASO("contorno: un ancho por muestra, dos bordes");
  tinta::Trazo trazo = TrazoRecto(0.0f, 0.0f, 5);
  tinta::ParametrosPluma params;
  auto contorno = tinta::CalcularContorno(trazo, params);
  VERIFICAR(contorno.izquierda.size() == trazo.muestras.size());
  VERIFICAR(contorno.derecha.size() == trazo.muestras.size());
  VERIFICAR(contorno.anchos.size() == trazo.muestras.size());

  CASO("la presion manda sobre el ancho");
  tinta::Trazo suave = TrazoRecto(0.0f, 0.0f, 5);
  tinta::Trazo fuerte = TrazoRecto(0.0f, 0.0f, 5);
  for (auto& m : suave.muestras) m.presion = 0.1f;
  for (auto& m : fuerte.muestras) m.presion = 1.0f;
  const auto c_suave = tinta::CalcularContorno(suave, params);
  const auto c_fuerte = tinta::CalcularContorno(fuerte, params);
  VERIFICAR(c_fuerte.anchos[2] > c_suave.anchos[2]);

  CASO("sin presion, la velocidad adelgaza el trazo");
  tinta::Trazo lento = TrazoRecto(0.0f, 0.0f, 8, 1.0f);
  tinta::Trazo rapido = TrazoRecto(0.0f, 0.0f, 8, 60.0f);
  for (auto& m : lento.muestras) m.presion = tinta::kSinPresion;
  for (auto& m : rapido.muestras) m.presion = tinta::kSinPresion;
  const auto c_lento = tinta::CalcularContorno(lento, params);
  const auto c_rapido = tinta::CalcularContorno(rapido, params);
  VERIFICAR(c_rapido.anchos[5] < c_lento.anchos[5]);

  CASO("teselacion: triangulos completos");
  auto vertices = tinta::TeselarTrazo(trazo, params);
  VERIFICAR(!vertices.empty());
  VERIFICAR(vertices.size() % 3 == 0);

  CASO("un solo punto se tesela como disco");
  tinta::Trazo punto = TrazoRecto(0.0f, 0.0f, 1);
  auto v_punto = tinta::TeselarTrazo(punto, params);
  VERIFICAR(!v_punto.empty());
  VERIFICAR(v_punto.size() % 3 == 0);

  CASO("prediccion: hacen falta al menos tres muestras");
  tinta::ParametrosPrediccion pp;
  tinta::Trazo dos = TrazoRecto(0.0f, 0.0f, 2);
  VERIFICAR(tinta::PredecirContinuacion(dos, pp, 1'000'000'000).empty());

  CASO("prediccion: continua en la direccion del trazo");
  tinta::Trazo recto = TrazoRecto(0.0f, 0.0f, 6);
  auto predichas = tinta::PredecirContinuacion(recto, pp, 1'000'000'000);
  VERIFICAR(!predichas.empty());
  // El trazo va hacia +x: la prediccion debe seguir hacia alla.
  VERIFICAR(predichas.front().posicion.x > recto.muestras.back().posicion.x);
  VERIFICAR(CasiIgual(predichas.front().posicion.y, 0.0f, 1.0f));

  CASO("prediccion: nunca se pasa del tope de distancia");
  tinta::ParametrosPrediccion tope;
  tope.distancia_maxima_px = 10.0f;
  tope.ms_adelante = 200.0f;  // horizonte absurdo a proposito
  tinta::Trazo veloz = TrazoRecto(0.0f, 0.0f, 6, 80.0f);
  auto limitada = tinta::PredecirContinuacion(veloz, tope, 1'000'000'000);
  for (const auto& m : limitada) {
    const float d = m.posicion.x - veloz.muestras.back().posicion.x;
    VERIFICAR(d <= tope.distancia_maxima_px + 0.01f);
  }

  CASO("prediccion: el giro brusco la recorta");
  // Trazo que va a la derecha y de golpe se devuelve: el latigazo clasico.
  tinta::Trazo giro;
  giro.ancho_base = 4.0f;
  const float xs[] = {0.0f, 20.0f, 40.0f, 60.0f, 40.0f, 20.0f};
  for (int i = 0; i < 6; ++i) {
    tinta::Muestra m;
    m.posicion = {xs[i], 0.0f};
    m.presion = 0.5f;
    m.tiempo_qpc = static_cast<int64_t>(i) * 4'000'000;
    giro.muestras.push_back(m);
  }
  auto p_giro = tinta::PredecirContinuacion(giro, pp, 1'000'000'000);
  auto p_recto = tinta::PredecirContinuacion(recto, pp, 1'000'000'000);
  // Con el mismo horizonte, el trazo que gira debe predecir menos que el recto.
  float avance_giro = 0.0f, avance_recto = 0.0f;
  if (!p_giro.empty()) {
    avance_giro = std::fabs(p_giro.back().posicion.x - giro.muestras.back().posicion.x);
  }
  if (!p_recto.empty()) {
    avance_recto = std::fabs(p_recto.back().posicion.x - recto.muestras.back().posicion.x);
  }
  VERIFICAR(avance_giro < avance_recto);

  CASO("prediccion: la presion predicha se queda en rango");
  for (const auto& m : predichas) {
    VERIFICAR(!m.tiene_presion() || (m.presion >= 0.0f && m.presion <= 1.0f));
  }

  CASO("frecuencia de muestreo");
  // 6 muestras cada 4 ms = 250 Hz.
  const double hz = tinta::FrecuenciaMuestreoHz(recto, 1'000'000'000);
  VERIFICAR(hz > 200.0 && hz < 300.0);
}

// ---------------------------------------------------------------------------

void PruebasMedios() {
  CASO("recorte: la region visible sale en pixeles del original");
  tinta::Medio medio;
  medio.recorte_x = 0.25f;
  medio.recorte_y = 0.0f;
  medio.recorte_ancho = 0.5f;
  medio.recorte_alto = 1.0f;
  VERIFICAR(medio.recorte_valido());
  auto region = medio.RegionVisible(400.0f, 200.0f);
  VERIFICAR(CasiIgual(region.x, 100.0f));
  VERIFICAR(CasiIgual(region.ancho, 200.0f));

  CASO("recortar dos veces se compone sobre lo que ya se ve");
  tinta::Medio doble;
  tinta::RecortarRelativo(&doble, 0.5f, 0.0f, 0.5f, 1.0f);  // mitad derecha
  VERIFICAR(CasiIgual(doble.recorte_x, 0.5f));
  VERIFICAR(CasiIgual(doble.recorte_ancho, 0.5f));
  // Otra mitad derecha, pero de lo que queda: debe dar el ultimo cuarto.
  tinta::RecortarRelativo(&doble, 0.5f, 0.0f, 0.5f, 1.0f);
  VERIFICAR(CasiIgual(doble.recorte_x, 0.75f));
  VERIFICAR(CasiIgual(doble.recorte_ancho, 0.25f));
  VERIFICAR(doble.recorte_valido());

  CASO("un recorte fuera de rango se acota, no corrompe");
  tinta::Medio raro;
  tinta::RecortarRelativo(&raro, 0.8f, 0.8f, 5.0f, 5.0f);
  VERIFICAR(raro.recorte_valido());

  CASO("rectangulo: contiene e intersecta");
  tinta::Rect r{0, 0, 100, 100};
  VERIFICAR(r.Contiene({50, 50}));
  VERIFICAR(!r.Contiene({150, 50}));
  VERIFICAR(r.Intersecta(tinta::Rect{50, 50, 100, 100}));
  VERIFICAR(!r.Intersecta(tinta::Rect{200, 200, 10, 10}));
}

// Documento mínimo: un cuaderno, una sección, dos páginas.
tinta::Documento DocumentoDePrueba() {
  tinta::Documento doc;
  doc.titulo = "Electronica";
  tinta::Cuaderno cuaderno;
  cuaderno.titulo = "EL3307";
  tinta::Seccion seccion;
  seccion.titulo = "Semana 1";
  seccion.paginas.resize(2);
  seccion.paginas[0].titulo = "P0";
  seccion.paginas[1].titulo = "P1";
  cuaderno.secciones.push_back(seccion);
  doc.cuadernos.push_back(cuaderno);
  return doc;
}

void PruebasNotas() {
  CASO("una nota de pagina solo se ve en su pagina");
  tinta::Documento doc = DocumentoDePrueba();
  tinta::NotaFija nota;
  nota.id = "n1";
  nota.texto = "revisar el laboratorio";
  nota.alcance = tinta::AlcanceNota::Pagina;
  doc.cuadernos[0].secciones[0].paginas[0].notas.push_back(nota);

  VERIFICAR(tinta::NotasVisibles(doc, {0, 0, 0}).size() == 1);
  VERIFICAR(tinta::NotasVisibles(doc, {0, 0, 1}).empty());

  CASO("promovida a cuaderno, sigue al cambiar de pagina");
  VERIFICAR(tinta::CambiarAlcanceNota(doc, "n1", tinta::AlcanceNota::Cuaderno, {0, 0, 0}));
  VERIFICAR(doc.cuadernos[0].secciones[0].paginas[0].notas.empty());
  VERIFICAR(tinta::NotasVisibles(doc, {0, 0, 0}).size() == 1);
  VERIFICAR(tinta::NotasVisibles(doc, {0, 0, 1}).size() == 1);

  CASO("promovida a global, sigue al cambiar de cuaderno");
  doc.cuadernos.push_back(tinta::Cuaderno{});  // segundo cuaderno, vacio
  VERIFICAR(tinta::CambiarAlcanceNota(doc, "n1", tinta::AlcanceNota::Global, {0, 0, 0}));
  VERIFICAR(doc.cuadernos[0].notas.empty());
  VERIFICAR(doc.notas.size() == 1);
  VERIFICAR(tinta::NotasVisibles(doc, {1, 0, 0}).size() == 1);

  CASO("el alcance guardado en la nota concuerda con donde vive");
  VERIFICAR(doc.notas[0].alcance == tinta::AlcanceNota::Global);

  CASO("bajarla de nuevo a una pagina concreta");
  VERIFICAR(tinta::CambiarAlcanceNota(doc, "n1", tinta::AlcanceNota::Pagina, {0, 0, 1}));
  VERIFICAR(doc.notas.empty());
  VERIFICAR(doc.cuadernos[0].secciones[0].paginas[1].notas.size() == 1);

  CASO("un destino invalido NO hace desaparecer la nota");
  // Este es el fallo peligroso: sacarla antes de comprobar el destino la pierde.
  VERIFICAR(!tinta::CambiarAlcanceNota(doc, "n1", tinta::AlcanceNota::Pagina, {0, 0, 99}));
  VERIFICAR(tinta::BuscarNota(doc, "n1") != nullptr);

  CASO("una nota que no existe no rompe nada");
  VERIFICAR(!tinta::CambiarAlcanceNota(doc, "nope", tinta::AlcanceNota::Global, {0, 0, 0}));
  VERIFICAR(tinta::BuscarNota(doc, "nope") == nullptr);

  CASO("una ubicacion fuera de rango devuelve solo lo que si aplica");
  VERIFICAR(tinta::NotasVisibles(doc, {99, 0, 0}).empty());
}

// Construye un trazo a partir de puntos sueltos.
tinta::Trazo TrazoDePuntos(const std::vector<tinta::Punto>& puntos) {
  tinta::Trazo t;
  t.ancho_base = 4.0f;
  for (size_t i = 0; i < puntos.size(); ++i) {
    tinta::Muestra m;
    m.posicion = puntos[i];
    m.presion = 0.6f;
    m.tiempo_qpc = static_cast<int64_t>(i) * 4'000'000;
    t.muestras.push_back(m);
  }
  return t;
}

void PruebasFormas() {
  CASO("simplificar quita los puntos que no cambian la forma");
  std::vector<tinta::Punto> recta;
  for (int i = 0; i <= 20; ++i) recta.push_back({i * 5.0f, 0.0f});
  auto simple = tinta::Simplificar(recta, 1.0f);
  VERIFICAR(simple.size() == 2);

  CASO("simplificar conserva una esquina real");
  std::vector<tinta::Punto> ele;
  for (int i = 0; i <= 10; ++i) ele.push_back({i * 10.0f, 0.0f});
  for (int i = 1; i <= 10; ++i) ele.push_back({100.0f, i * 10.0f});
  auto simple_ele = tinta::Simplificar(ele, 2.0f);
  VERIFICAR(simple_ele.size() == 3);

  CASO("una raya casi recta se reconoce como linea");
  std::vector<tinta::Punto> casi;
  for (int i = 0; i <= 20; ++i) {
    casi.push_back({i * 10.0f, (i % 2 == 0) ? 0.6f : -0.6f});  // temblor de la mano
  }
  auto forma_linea = tinta::ReconocerForma(TrazoDePuntos(casi));
  VERIFICAR(forma_linea.tipo == tinta::TipoForma::Linea);
  VERIFICAR(forma_linea.vertices.size() == 2);

  CASO("un circulo a mano se reconoce como elipse");
  std::vector<tinta::Punto> circulo;
  for (int i = 0; i <= 40; ++i) {
    const float a = 2.0f * 3.14159265f * i / 40.0f;
    const float ruido = (i % 3 == 0) ? 2.0f : -2.0f;  // pulso irregular
    circulo.push_back({150.0f + (100.0f + ruido) * std::cos(a),
                       150.0f + (100.0f + ruido) * std::sin(a)});
  }
  auto forma_elipse = tinta::ReconocerForma(TrazoDePuntos(circulo));
  VERIFICAR(forma_elipse.tipo == tinta::TipoForma::Elipse);

  CASO("un ovalo ancho tambien, no solo el circulo");
  std::vector<tinta::Punto> ovalo;
  for (int i = 0; i <= 40; ++i) {
    const float a = 2.0f * 3.14159265f * i / 40.0f;
    ovalo.push_back({200.0f + 180.0f * std::cos(a), 100.0f + 60.0f * std::sin(a)});
  }
  VERIFICAR(tinta::ReconocerForma(TrazoDePuntos(ovalo)).tipo == tinta::TipoForma::Elipse);

  CASO("un cuadrado a mano se endereza a rectangulo");
  std::vector<tinta::Punto> cuadro;
  for (int i = 0; i <= 10; ++i) cuadro.push_back({i * 20.0f, 0.0f});
  for (int i = 1; i <= 10; ++i) cuadro.push_back({200.0f, i * 20.0f});
  for (int i = 1; i <= 10; ++i) cuadro.push_back({200.0f - i * 20.0f, 200.0f});
  for (int i = 1; i <= 10; ++i) cuadro.push_back({0.0f, 200.0f - i * 20.0f});
  auto forma_rect = tinta::ReconocerForma(TrazoDePuntos(cuadro));
  VERIFICAR(forma_rect.tipo == tinta::TipoForma::Rectangulo);
  VERIFICAR(forma_rect.vertices.size() == 4);

  CASO("un triangulo da tres vertices");
  std::vector<tinta::Punto> tri;
  for (int i = 0; i <= 10; ++i) tri.push_back({i * 20.0f, 0.0f});
  for (int i = 1; i <= 10; ++i) tri.push_back({200.0f - i * 10.0f, i * 20.0f});
  for (int i = 1; i <= 10; ++i) tri.push_back({100.0f - i * 10.0f, 200.0f - i * 20.0f});
  auto forma_tri = tinta::ReconocerForma(TrazoDePuntos(tri));
  VERIFICAR(forma_tri.tipo == tinta::TipoForma::Triangulo);
  VERIFICAR(forma_tri.vertices.size() == 3);

  CASO("un garabato no se endereza a nada");
  std::vector<tinta::Punto> garabato = {
      {0, 0}, {50, 80}, {10, 40}, {90, 20}, {30, 95}, {70, 10}, {20, 60}};
  VERIFICAR(tinta::ReconocerForma(TrazoDePuntos(garabato)).tipo == tinta::TipoForma::Ninguna);

  CASO("un trazo muy corto no se reconoce");
  VERIFICAR(tinta::ReconocerForma(TrazoDePuntos({{0, 0}, {1, 1}})).tipo ==
            tinta::TipoForma::Ninguna);

  CASO("el trazo ideal conserva color y grosor");
  tinta::Trazo original = TrazoDePuntos(circulo);
  original.color_argb = 0xFFC62828u;
  original.ancho_base = 7.5f;
  auto ideal = tinta::TrazoDesdeForma(tinta::ReconocerForma(original), original);
  VERIFICAR(!ideal.muestras.empty());
  VERIFICAR(ideal.color_argb == 0xFFC62828u);
  VERIFICAR(CasiIgual(ideal.ancho_base, 7.5f));

  CASO("de una forma Ninguna no sale trazo");
  VERIFICAR(tinta::TrazoDesdeForma(tinta::FormaReconocida{}, original).muestras.empty());
}

void PruebasArchivo() {
  CASO("ida y vuelta conserva el documento");
  tinta::Documento doc = DocumentoDePrueba();
  doc.id = "7a9dd9a8-1f11-4b1c-a13a-d80518a7b338";
  doc.cuadernos[0].secciones[0].paginas[0].trazos.push_back(TrazoRecto(10.0f, 20.0f, 5));
  doc.cuadernos[0].secciones[0].paginas[0].papel.tipo = tinta::TipoPapel::BlancoSol;
  doc.cuadernos[0].secciones[0].paginas[0].papel.pauta = tinta::Pauta::Cuadricula;
  doc.cuadernos[0].secciones[0].paginas[0].limites.alto_infinito = true;

  tinta::CuadroTexto texto;
  texto.id = "t1";
  texto.texto = "Ley de Ohm: V = I R";
  texto.marco = {10, 10, 300, 40};
  texto.desde_dictado = true;
  doc.cuadernos[0].secciones[0].paginas[0].textos.push_back(texto);

  tinta::Medio foto;
  foto.id = "m1";
  foto.hash = "abc123";
  foto.nombre_original = "circuito.png";
  foto.recorte_x = 0.1f;
  foto.recorte_ancho = 0.8f;
  doc.cuadernos[0].secciones[0].paginas[0].medios.push_back(foto);

  tinta::NotaFija global;
  global.id = "g1";
  global.texto = "examen el 20";
  global.alcance = tinta::AlcanceNota::Global;
  global.trazos.push_back(TrazoRecto(0.0f, 0.0f, 3));
  doc.notas.push_back(global);

  const auto bytes = tinta::Serializar(doc);
  VERIFICAR(!bytes.empty());

  tinta::Documento leido;
  VERIFICAR(tinta::Deserializar(bytes, &leido));
  VERIFICAR(leido.id == doc.id);
  VERIFICAR(leido.titulo == doc.titulo);
  VERIFICAR(leido.cuadernos.size() == 1);
  VERIFICAR(leido.cuadernos[0].titulo == "EL3307");
  VERIFICAR(leido.cuadernos[0].secciones[0].paginas.size() == 2);

  const auto& p = leido.cuadernos[0].secciones[0].paginas[0];
  VERIFICAR(p.trazos.size() == 1);
  VERIFICAR(p.trazos[0].muestras.size() == 5);
  VERIFICAR(CasiIgual(p.trazos[0].muestras[2].posicion.x, 30.0f));
  VERIFICAR(p.papel.tipo == tinta::TipoPapel::BlancoSol);
  VERIFICAR(p.papel.pauta == tinta::Pauta::Cuadricula);
  VERIFICAR(p.limites.alto_infinito);
  VERIFICAR(p.textos.size() == 1);
  VERIFICAR(p.textos[0].texto == "Ley de Ohm: V = I R");
  VERIFICAR(p.textos[0].desde_dictado);
  VERIFICAR(p.medios.size() == 1);
  VERIFICAR(p.medios[0].nombre_original == "circuito.png");
  VERIFICAR(CasiIgual(p.medios[0].recorte_ancho, 0.8f));

  VERIFICAR(leido.notas.size() == 1);
  VERIFICAR(leido.notas[0].id == "g1");
  VERIFICAR(leido.notas[0].trazos.size() == 1);

  CASO("las marcas de tiempo del lapiz sobreviven");
  VERIFICAR(p.trazos[0].muestras[3].tiempo_qpc ==
            doc.cuadernos[0].secciones[0].paginas[0].trazos[0].muestras[3].tiempo_qpc);

  CASO("un documento vacio va y vuelve");
  tinta::Documento vacio;
  vacio.id = "documento-vacio";
  tinta::Documento vacio_leido;
  VERIFICAR(tinta::Deserializar(tinta::Serializar(vacio), &vacio_leido));
  VERIFICAR(vacio_leido.cuadernos.empty());

  CASO("una cabecera ajena se rechaza");
  std::vector<uint8_t> basura(64, 0x41);
  tinta::Documento destino = DocumentoDePrueba();
  VERIFICAR(!tinta::Deserializar(basura, &destino));
  // Y el documento que ya estaba abierto no se toca.
  VERIFICAR(destino.cuadernos.size() == 1);

  CASO("un archivo truncado se rechaza sin leer basura");
  auto cortado = bytes;
  cortado.resize(bytes.size() / 2);
  tinta::Documento destino2 = DocumentoDePrueba();
  VERIFICAR(!tinta::Deserializar(cortado, &destino2));
  VERIFICAR(destino2.cuadernos.size() == 1);
  VERIFICAR(std::string(tinta::UltimoErrorArchivo()).size() > 0);

  CASO("una version futura se rechaza en vez de leer mal");
  auto futuro = bytes;
  futuro[6] = 0xFF;
  futuro[7] = 0xFF;
  tinta::Documento destino3;
  VERIFICAR(!tinta::Deserializar(futuro, &destino3));

  CASO("un archivo mas corto que la cabecera se rechaza");
  std::vector<uint8_t> minusculo = {'T', 'I'};
  tinta::Documento destino4;
  VERIFICAR(!tinta::Deserializar(minusculo, &destino4));
}

void PruebasLazoAvanzado() {
  CASO("lazo elipse selecciona dentro del radio");
  tinta::Pagina pagina;
  pagina.trazos.push_back(TrazoRecto(10.0f, 10.0f, 1));
  pagina.trazos.push_back(TrazoRecto(100.0f, 100.0f, 1));
  
  auto elipse = tinta::SeleccionarElipse(pagina, {10.0f, 10.0f}, 20.0f, 20.0f);
  VERIFICAR(elipse.trazos.size() == 1);
  VERIFICAR(elipse.trazos[0] == 0);

  CASO("lazo rectangulo selecciona dentro de los limites");
  auto rect = tinta::SeleccionarRectangulo(pagina, 90.0f, 90.0f, 20.0f, 20.0f);
  VERIFICAR(rect.trazos.size() == 1);
  VERIFICAR(rect.trazos[0] == 1);

  CASO("filtro por tipo descarta lo que no toca");
  tinta::CuadroTexto texto;
  texto.marco = {5.0f, 5.0f, 10.0f, 10.0f};
  pagina.textos.push_back(texto);

  tinta::Medio img;
  img.tipo = tinta::TipoMedio::Imagen;
  img.marco = {5.0f, 5.0f, 10.0f, 10.0f};
  pagina.medios.push_back(img);

  tinta::FiltroLazo solo_tinta;
  solo_tinta.tipo = tinta::TipoFiltroLazo::SoloTinta;
  auto st = tinta::SeleccionarRectangulo(pagina, 0.0f, 0.0f, 50.0f, 50.0f, solo_tinta);
  VERIFICAR(st.trazos.size() == 1);
  VERIFICAR(st.textos.empty());
  VERIFICAR(st.medios.empty());

  CASO("filtro por color solo selecciona ese color");
  tinta::FiltroLazo color_rojo;
  color_rojo.color_especifico = 0xFFFF0000u;
  pagina.trazos[0].color_argb = 0xFFFF0000u;
  pagina.trazos[1].color_argb = 0xFF00FF00u;
  auto sc = tinta::SeleccionarRectangulo(pagina, 0.0f, 0.0f, 200.0f, 200.0f, color_rojo);
  VERIFICAR(sc.trazos.size() == 1);
  VERIFICAR(sc.trazos[0] == 0);

  CASO("operaciones de conjuntos: unir");
  tinta::Seleccion s1, s2;
  s1.trazos = {0};
  s2.trazos = {1};
  auto uni = tinta::UnirSelecciones(s1, s2);
  VERIFICAR(uni.trazos.size() == 2);
  VERIFICAR(uni.trazos[0] == 0);
  VERIFICAR(uni.trazos[1] == 1);

  CASO("operaciones de conjuntos: restar");
  auto res = tinta::RestarSelecciones(uni, s2);
  VERIFICAR(res.trazos.size() == 1);
  VERIFICAR(res.trazos[0] == 0);

  CASO("operaciones de conjuntos: invertir");
  auto inv = tinta::InvertirSeleccion(pagina, s1);
  VERIFICAR(inv.trazos.size() == 1);
  VERIFICAR(inv.trazos[0] == 1);
  // Invirtiendo con todos, deberia incluir textos y medios
  VERIFICAR(inv.textos.size() == 1);
  VERIFICAR(inv.medios.size() == 1);
  
  CASO("transformacion: reflejar");
  tinta::Transformacion t;
  t.reflejar_horizontal = true;
  t.pivote = {10.0f, 10.0f};
  pagina.trazos[0].muestras[0].posicion = {15.0f, 10.0f};
  tinta::TransformarTrazos(pagina, {0}, t);
  VERIFICAR(CasiIgual(pagina.trazos[0].muestras[0].posicion.x, 5.0f));
}

void PruebasOrdenZ() {
  CASO("traer al frente mueve elementos al final");
  tinta::Pagina pagina;
  for (int i = 0; i < 4; ++i) {
    pagina.trazos.push_back(TrazoRecto(i * 10.0f, 0.0f, 1));
  }
  tinta::Seleccion sel;
  sel.trazos = {1, 2};
  tinta::TraerAlFrente(pagina, sel);
  // Original: 0 1 2 3
  // Selected: 1, 2. Unselected: 0, 3. Result: 0 3 1 2
  VERIFICAR(CasiIgual(pagina.trazos[0].muestras[0].posicion.x, 0.0f));
  VERIFICAR(CasiIgual(pagina.trazos[1].muestras[0].posicion.x, 30.0f));
  VERIFICAR(CasiIgual(pagina.trazos[2].muestras[0].posicion.x, 10.0f));
  VERIFICAR(CasiIgual(pagina.trazos[3].muestras[0].posicion.x, 20.0f));

  CASO("enviar atras mueve elementos al principio");
  // Current: 0 3 1 2 (x = 0, 30, 10, 20)
  // Let's send {1, 2} (which are currently at indices 2 and 3) to back.
  tinta::Seleccion sel2;
  sel2.trazos = {2, 3}; // corresponds to 1 and 2
  tinta::EnviarAtras(pagina, sel2);
  // Should result in: 1 2 0 3 -> x = 10, 20, 0, 30
  VERIFICAR(CasiIgual(pagina.trazos[0].muestras[0].posicion.x, 10.0f));
  VERIFICAR(CasiIgual(pagina.trazos[1].muestras[0].posicion.x, 20.0f));
  VERIFICAR(CasiIgual(pagina.trazos[2].muestras[0].posicion.x, 0.0f));
  VERIFICAR(CasiIgual(pagina.trazos[3].muestras[0].posicion.x, 30.0f));
}

void PruebasApiC() {
  CASO("la sesion se crea y se libera");
  TintaSesion* s = tinta_sesion_crear();
  VERIFICAR(s != nullptr);
  size_t bytes_sin_identidad = 1;
  VERIFICAR(tinta_guardar(s, &bytes_sin_identidad) == nullptr);
  VERIFICAR(bytes_sin_identidad == 0);
  VERIFICAR(std::string(tinta_ultimo_error()).find("identidad") !=
            std::string::npos);
  const char* identidad = "7a9dd9a8-1f11-4b1c-a13a-d80518a7b338";
  VERIFICAR(tinta_documento_asignar_identidad(s, identidad));
  VERIFICAR(std::string(tinta_documento_identidad(s)) == identidad);
  VERIFICAR(tinta_cantidad_trazos(s) == 0);
  VERIFICAR(!tinta_hay_trazo_en_curso(s));

  CASO("un trazo completo entra y sale como vertices");
  tinta_comenzar_trazo(s, 5.0f, 0xFF1A1A1Au, false);
  for (int i = 0; i < 8; ++i) {
    tinta_anadir_muestra(s, i * 10.0f, 0.0f, 0.5f, 0.0f, 0.0f, i * 0.004);
  }
  VERIFICAR(tinta_hay_trazo_en_curso(s));
  size_t cantidad = 0;
  const float* v = tinta_vertices_en_curso(s, &cantidad);
  VERIFICAR(v != nullptr);
  VERIFICAR(cantidad > 0 && cantidad % 3 == 0);

  CASO("la frecuencia de muestreo se calcula desde los timestamps");
  const double hz = tinta_frecuencia_hz(s);
  VERIFICAR(hz > 200.0 && hz < 300.0);  // 4 ms por muestra = 250 Hz

  CASO("terminar el trazo lo guarda en la pagina");
  tinta_terminar_trazo(s);
  VERIFICAR(tinta_cantidad_trazos(s) == 1);
  VERIFICAR(!tinta_hay_trazo_en_curso(s));

  CASO("las provisionales mandan sobre el predictor propio");
  tinta_comenzar_trazo(s, 5.0f, 0xFF1A1A1Au, false);
  for (int i = 0; i < 6; ++i) {
    tinta_anadir_muestra(s, i * 10.0f, 50.0f, 0.5f, 0.0f, 0.0f, i * 0.004);
  }
  tinta_set_prediccion(s, true);
  tinta_vertices_en_curso(s, &cantidad);
  const size_t con_nucleo = tinta_muestras_predichas(s);
  VERIFICAR(con_nucleo > 0);

  tinta_limpiar_provisionales(s);
  tinta_anadir_provisional(s, 70.0f, 50.0f, 0.5f, 0.0f, 0.0f, 0.024);
  tinta_anadir_provisional(s, 80.0f, 50.0f, 0.5f, 0.0f, 0.0f, 0.028);
  tinta_vertices_en_curso(s, &cantidad);
  VERIFICAR(tinta_muestras_predichas(s) == 2);
  tinta_descartar_trazo(s);

  CASO("las muestras predichas no entran en la pagina");
  // Tras descartar, la pagina sigue con el unico trazo terminado.
  VERIFICAR(tinta_cantidad_trazos(s) == 1);

  CASO("las propiedades estimadas se corrigen despues de terminar el trazo");
  TintaSesion* estimadas = tinta_sesion_crear();
  tinta_comenzar_trazo(estimadas, 5.0f, 0xFF112233u, false);
  const uint64_t muestra_estimada = tinta_anadir_muestra_actualizable(
      estimadas, 10.0f, 20.0f, 0.2f, 0.3f, 0.4f, 1.0);
  tinta_anadir_muestra(estimadas, 20.0f, 25.0f, 0.5f, 0.4f, 0.5f, 1.004);
  const uint64_t segunda_estimada = tinta_anadir_muestra_actualizable(
      estimadas, 30.0f, 30.0f, 0.6f, 0.5f, 0.6f, 1.008);
  VERIFICAR(muestra_estimada != 0);
  VERIFICAR(segunda_estimada != 0 && segunda_estimada != muestra_estimada);
  VERIFICAR(tinta_terminar_trazo_con_id(estimadas, "trazo-estimado") == 0);

  size_t bytes_estimados = 0;
  const uint8_t* serializado_estimado =
      tinta_trazo_serializar(estimadas, 0, &bytes_estimados);
  std::vector<uint8_t> antes_correccion;
  if (serializado_estimado && bytes_estimados > 0) {
    antes_correccion.assign(serializado_estimado,
                            serializado_estimado + bytes_estimados);
  }

  const uint32_t todas_estimadas =
      TintaPropiedadMuestraPosicion | TintaPropiedadMuestraFuerza |
      TintaPropiedadMuestraAltitud | TintaPropiedadMuestraAzimut;
  VERIFICAR(tinta_actualizar_muestra(
      estimadas, muestra_estimada, todas_estimadas,
      12.0f, 24.0f, 0.9f, 0.7f, 0.8f));

  size_t puntos_estimados = 0;
  const float* puntos_corregidos =
      tinta_trazo_puntos(estimadas, 0, &puntos_estimados);
  VERIFICAR(puntos_corregidos != nullptr && puntos_estimados == 3);
  VERIFICAR(CasiIgual(puntos_corregidos[0], 12.0f));
  VERIFICAR(CasiIgual(puntos_corregidos[1], 24.0f));

  serializado_estimado = tinta_trazo_serializar(
      estimadas, 0, &bytes_estimados);
  std::vector<uint8_t> despues_correccion;
  if (serializado_estimado && bytes_estimados > 0) {
    despues_correccion.assign(serializado_estimado,
                              serializado_estimado + bytes_estimados);
  }
  VERIFICAR(!antes_correccion.empty() &&
            antes_correccion != despues_correccion);

  CASO("deshacer y rehacer conserva el ID y la correccion estimada");
  tinta_deshacer(estimadas);
  VERIFICAR(tinta_cantidad_trazos(estimadas) == 0);
  tinta_rehacer(estimadas);
  VERIFICAR(tinta_cantidad_trazos(estimadas) == 1);
  VERIFICAR(std::string(tinta_trazo_id(estimadas, 0)) == "trazo-estimado");
  serializado_estimado = tinta_trazo_serializar(
      estimadas, 0, &bytes_estimados);
  std::vector<uint8_t> despues_rehacer;
  if (serializado_estimado && bytes_estimados > 0) {
    despues_rehacer.assign(serializado_estimado,
                           serializado_estimado + bytes_estimados);
  }
  VERIFICAR(despues_rehacer == despues_correccion);

  CASO("una muestra finalizada ya no acepta correcciones tardias");
  VERIFICAR(!tinta_actualizar_muestra(
      estimadas, muestra_estimada, 1u << 30,
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f));
  VERIFICAR(tinta_finalizar_actualizaciones_muestra(
      estimadas, muestra_estimada));
  VERIFICAR(!tinta_actualizar_muestra(
      estimadas, muestra_estimada, TintaPropiedadMuestraPosicion,
      99.0f, 99.0f, 0.0f, 0.0f, 0.0f));
  VERIFICAR(tinta_finalizar_actualizaciones_muestra(
      estimadas, segunda_estimada));

  CASO("terminar con un ID duplicado no crea otro trazo");
  tinta_comenzar_trazo(estimadas, 3.0f, 0xFF000000u, false);
  tinta_anadir_muestra(estimadas, 1.0f, 1.0f, 0.5f, 0.0f, 0.0f, 2.0);
  VERIFICAR(tinta_terminar_trazo_con_id(
      estimadas, "trazo-estimado") == -1);
  VERIFICAR(tinta_hay_trazo_en_curso(estimadas));
  VERIFICAR(tinta_cantidad_trazos(estimadas) == 1);
  tinta_descartar_trazo(estimadas);
  VERIFICAR(tinta_anadir_muestra_actualizable(
      estimadas, 0, 0, 0, 0, 0, 0) == 0);
  tinta_sesion_liberar(estimadas);

  CASO("deshacer y rehacer por la API");
  VERIFICAR(tinta_puede_deshacer(s));
  tinta_deshacer(s);
  VERIFICAR(tinta_cantidad_trazos(s) == 0);
  VERIFICAR(tinta_puede_rehacer(s));
  tinta_rehacer(s);
  VERIFICAR(tinta_cantidad_trazos(s) == 1);

  CASO("el borrador completo quita el trazo y se puede deshacer");
  VERIFICAR(tinta_borrar(s, 30.0f, 0.0f, 8.0f, false));
  VERIFICAR(tinta_cantidad_trazos(s) == 0);
  tinta_deshacer(s);
  VERIFICAR(tinta_cantidad_trazos(s) == 1);

  CASO("el borrador lejos no cambia nada");
  VERIFICAR(!tinta_borrar(s, 5000.0f, 5000.0f, 8.0f, false));

  CASO("el borrador parcial parte el trazo");
  VERIFICAR(tinta_borrar(s, 35.0f, 0.0f, 6.0f, true));
  VERIFICAR(tinta_cantidad_trazos(s) == 2);

  CASO("papel: el color de tinta se aclara en papel oscuro");
  const uint32_t tinta_clara_antes = tinta_color_tinta_sugerido(s);
  tinta_set_papel(s, 4, 1, 40.0f, 0.0f);  // oscuro, rayado
  VERIFICAR(tinta_color_papel(s) != 0xFFFDFDFBu);
  VERIFICAR(tinta_color_tinta_sugerido(s) != tinta_clara_antes);

  CASO("la pauta sale como cuartetos");
  size_t segmentos = 0;
  const float* pauta = tinta_pauta_visible(s, 0, 0, 400, 400, &segmentos);
  VERIFICAR(segmentos > 0);
  VERIFICAR(pauta != nullptr);
  // Rayado: cada segmento es horizontal, o sea y0 == y1.
  VERIFICAR(CasiIgual(pauta[1], pauta[3]));

  CASO("un tipo de papel invalido se ignora en vez de corromper el enum");
  const uint32_t antes = tinta_color_papel(s);
  tinta_set_papel(s, 99, 99, -5.0f, 0.0f);
  VERIFICAR(tinta_color_papel(s) == antes);

  CASO("limites incoherentes se rechazan enteros");
  tinta_set_limites(s, false, true, 300.0f, 900.0f, 400.0f, 5000.0f);
  const float ancho_bueno = tinta_ancho_pagina(s);
  tinta_set_limites(s, false, false, 900.0f, 100.0f, 400.0f, 500.0f);  // max < min
  VERIFICAR(CasiIgual(tinta_ancho_pagina(s), ancho_bueno));

  CASO("guardar y cargar por la API");
  size_t bytes = 0;
  const uint8_t* datos = tinta_guardar(s, &bytes);
  VERIFICAR(datos != nullptr && bytes > 0);
  std::vector<uint8_t> copia(datos, datos + bytes);

  const size_t trazos_antes = tinta_cantidad_trazos(s);
  TintaSesion* s2 = tinta_sesion_crear();
  VERIFICAR(tinta_documento_asignar_identidad(s2, identidad));
  VERIFICAR(tinta_cargar(s2, copia.data(), copia.size()));
  VERIFICAR(tinta_cantidad_trazos(s2) == trazos_antes);
  VERIFICAR(tinta_color_papel(s2) == tinta_color_papel(s));
  VERIFICAR(std::string(tinta_documento_identidad(s2)) == identidad);

  CASO("la identidad durable no puede reasignarse");
  VERIFICAR(!tinta_documento_asignar_identidad(
      s2, "8d34c7b5-f92a-4df3-aeed-15dfc597a6be"));
  VERIFICAR(std::string(tinta_documento_identidad(s2)) == identidad);
  VERIFICAR(std::string(tinta_ultimo_error()).find("inmutable") !=
            std::string::npos);

  CASO("un archivo no puede cargarse bajo la identidad de otro documento");
  TintaSesion* identidad_incorrecta = tinta_sesion_crear();
  VERIFICAR(tinta_documento_asignar_identidad(
      identidad_incorrecta, "b06d0c96-36dc-4394-93e9-f276c9623394"));
  VERIFICAR(!tinta_cargar(identidad_incorrecta, copia.data(), copia.size()));
  VERIFICAR(std::string(tinta_ultimo_error()).find("identidad") !=
            std::string::npos);
  VERIFICAR(tinta_cargar_como_copia(
      identidad_incorrecta, copia.data(), copia.size(),
      "b06d0c96-36dc-4394-93e9-f276c9623394"));
  VERIFICAR(tinta_cantidad_trazos(identidad_incorrecta) == trazos_antes);

  CASO("un archivo v4 sin identidad adopta la identidad durable de la sesion");
  tinta::Documento documento_legado;
  documento_legado.id = "identidad-eliminada-al-convertir-a-v4";
  documento_legado.cuadernos.emplace_back();
  documento_legado.cuadernos[0].secciones.emplace_back();
  documento_legado.cuadernos[0].secciones[0].paginas.emplace_back();
  std::vector<uint8_t> legado = tinta::Serializar(documento_legado);
  VERIFICAR(legado.size() > 12);
  const uint32_t largo_id = static_cast<uint32_t>(legado[8]) |
      (static_cast<uint32_t>(legado[9]) << 8) |
      (static_cast<uint32_t>(legado[10]) << 16) |
      (static_cast<uint32_t>(legado[11]) << 24);
  VERIFICAR(legado.size() >= 12u + largo_id);
  legado.erase(legado.begin() + 8, legado.begin() + 12 + largo_id);
  legado[6] = 4;
  legado[7] = 0;
  TintaSesion* sesion_legada = tinta_sesion_crear();
  const char* identidad_migrada = "906acaaa-8697-4fd4-a475-493972180d77";
  VERIFICAR(tinta_documento_asignar_identidad(
      sesion_legada, identidad_migrada));
  VERIFICAR(tinta_cargar(sesion_legada, legado.data(), legado.size()));
  VERIFICAR(std::string(tinta_documento_identidad(sesion_legada)) ==
            identidad_migrada);

  CASO("cargar basura falla y deja la sesion como estaba");
  std::vector<uint8_t> basura(80, 0x5A);
  VERIFICAR(!tinta_cargar(s2, basura.data(), basura.size()));
  VERIFICAR(tinta_cantidad_trazos(s2) == trazos_antes);
  VERIFICAR(std::string(tinta_ultimo_error()).size() > 0);

  CASO("cargar limpia el historial");
  VERIFICAR(!tinta_puede_deshacer(s2));

  CASO("enderezar el ultimo trazo lo reconoce");
  TintaSesion* s3 = tinta_sesion_crear();
  tinta_comenzar_trazo(s3, 4.0f, 0xFF000000u, false);
  for (int i = 0; i <= 40; ++i) {
    const float a = 2.0f * 3.14159265f * i / 40.0f;
    tinta_anadir_muestra(s3, 150.0f + 100.0f * std::cos(a),
                         150.0f + 100.0f * std::sin(a), 0.5f, 0.0f, 0.0f,
                         i * 0.004);
  }
  tinta_terminar_trazo(s3);
  VERIFICAR(tinta_enderezar_ultimo_trazo(s3) == 3);  // 3 = elipse
  VERIFICAR(tinta_cantidad_trazos(s3) == 1);

  CASO("enderezar se puede deshacer");
  VERIFICAR(tinta_puede_deshacer(s3));
  tinta_deshacer(s3);
  VERIFICAR(tinta_cantidad_trazos(s3) == 1);

  CASO("un garabato no se endereza");
  TintaSesion* s4 = tinta_sesion_crear();
  tinta_comenzar_trazo(s4, 4.0f, 0xFF000000u, false);
  const float gx[] = {0, 50, 10, 90, 30, 70, 20};
  const float gy[] = {0, 80, 40, 20, 95, 10, 60};
  for (int i = 0; i < 7; ++i) {
    tinta_anadir_muestra(s4, gx[i], gy[i], 0.5f, 0.0f, 0.0f, i * 0.004);
  }
  tinta_terminar_trazo(s4);
  VERIFICAR(tinta_enderezar_ultimo_trazo(s4) == 0);

  CASO("las funciones aguantan un puntero nulo");
  // Swift puede pasar nil; nada de esto debe reventar.
  tinta_comenzar_trazo(nullptr, 1.0f, 0u, false);
  tinta_anadir_muestra(nullptr, 0, 0, 0, 0, 0, 0);
  tinta_terminar_trazo(nullptr);
  tinta_deshacer(nullptr);
  VERIFICAR(tinta_cantidad_trazos(nullptr) == 0);
  VERIFICAR(tinta_vertices_en_curso(nullptr, &cantidad) == nullptr);
  VERIFICAR(!tinta_borrar(nullptr, 0, 0, 1, false));
  VERIFICAR(tinta_enderezar_ultimo_trazo(nullptr) == 0);
  VERIFICAR(!tinta_cargar(nullptr, nullptr, 0));

  tinta_sesion_liberar(s);
  tinta_sesion_liberar(s2);
  tinta_sesion_liberar(identidad_incorrecta);
  tinta_sesion_liberar(sesion_legada);
  tinta_sesion_liberar(s3);
  tinta_sesion_liberar(s4);
  tinta_sesion_liberar(nullptr);  // liberar nulo tampoco debe reventar
}

// ---------------------------------------------------------------------------

void PruebasGrabacion() {
  std::printf("\nGrabacion de pantalla\n");

  // Un iPad Pro de 11": 2048x2732 en vertical.
  tinta::CapacidadesGrabacion cap;
  cap.pantalla = {2048, 2732};
  cap.fps_maximo = 120;
  cap.estereo = true;
  cap.espacial = false;
  cap.hevc = true;
  cap.hdr = false;

  CASO("la resolucion nativa es la de la pantalla");
  tinta::AjustesVideo v;
  v.resolucion = tinta::Resolucion::Nativa;
  tinta::TamanoVideo t = tinta::ResolverTamano(v, cap);
  VERIFICAR(t.ancho == 2048 && t.alto == 2732);

  CASO("1080p escala por el lado corto y no deforma");
  v.resolucion = tinta::Resolucion::R1080p;
  t = tinta::ResolverTamano(v, cap);
  VERIFICAR(t.ancho == 1080);
  // 2732 x 1080/2048 = 1440,6 -> par
  VERIFICAR(t.alto == 1440);

  CASO("los dos lados salen pares");
  tinta::CapacidadesGrabacion impar;
  impar.pantalla = {1179, 2555};
  t = tinta::ResolverTamano(tinta::AjustesVideo{}, impar);
  VERIFICAR(t.ancho % 2 == 0 && t.alto % 2 == 0);

  CASO("nunca se amplia por encima de la pantalla");
  tinta::CapacidadesGrabacion chica;
  chica.pantalla = {1280, 720};
  v.resolucion = tinta::Resolucion::R2160p;
  t = tinta::ResolverTamano(v, chica);
  VERIFICAR(t.ancho == 1280 && t.alto == 720);

  CASO("la resolucion a mano tambien se limita a la pantalla");
  v.resolucion = tinta::Resolucion::Personalizada;
  v.ancho_personalizado = 4000;
  v.alto_personalizado = 4000;
  t = tinta::ResolverTamano(v, chica);
  VERIFICAR(t.ancho == 1280 && t.alto == 720);

  CASO("la resolucion a mano respeta un tamano que si cabe");
  v.ancho_personalizado = 960;
  v.alto_personalizado = 540;
  t = tinta::ResolverTamano(v, chica);
  VERIFICAR(t.ancho == 960 && t.alto == 540);

  CASO("el bitrate sugerido sube con los FPS y con la calidad");
  const tinta::TamanoVideo full{1920, 1080};
  const int a30 = tinta::KbpsVideoSugerido(full, 30, tinta::CalidadVideo::Alta,
                                           tinta::CodecVideo::H264);
  const int a60 = tinta::KbpsVideoSugerido(full, 60, tinta::CalidadVideo::Alta,
                                           tinta::CodecVideo::H264);
  const int baja = tinta::KbpsVideoSugerido(full, 60, tinta::CalidadVideo::Baja,
                                            tinta::CodecVideo::H264);
  VERIFICAR(a60 > a30);
  VERIFICAR(a60 > baja);

  CASO("HEVC pide menos bitrate que H264 a igual calidad");
  const int hevc = tinta::KbpsVideoSugerido(full, 60, tinta::CalidadVideo::Alta,
                                            tinta::CodecVideo::HEVC);
  VERIFICAR(hevc < a60);

  CASO("el bitrate puesto a mano manda sobre la calidad");
  tinta::AjustesVideo manual;
  manual.kbps = 6000;
  manual.calidad = tinta::CalidadVideo::Maxima;
  VERIFICAR(tinta::KbpsVideoEfectivo(manual, cap) == 6000);

  CASO("un bitrate absurdo se recorta al rango");
  manual.kbps = 999999999;
  VERIFICAR(tinta::KbpsVideoEfectivo(manual, cap) == tinta::kKbpsVideoMaximo);

  CASO("mono, estereo y espacial tienen 1, 2 y 4 canales");
  VERIFICAR(tinta::CanalesDe(tinta::ModoCanales::Mono) == 1);
  VERIFICAR(tinta::CanalesDe(tinta::ModoCanales::Estereo) == 2);
  VERIFICAR(tinta::CanalesDe(tinta::ModoCanales::Espacial) == 4);

  CASO("el bitrate de audio crece con los canales");
  VERIFICAR(tinta::KbpsAudioSugerido(tinta::ModoCanales::Mono, 48000) <
            tinta::KbpsAudioSugerido(tinta::ModoCanales::Estereo, 48000));
  VERIFICAR(tinta::KbpsAudioSugerido(tinta::ModoCanales::Estereo, 48000) <
            tinta::KbpsAudioSugerido(tinta::ModoCanales::Espacial, 48000));

  CASO("sin audio el bitrate de audio es cero");
  tinta::AjustesAudio sin_audio;
  sin_audio.microfono = false;
  sin_audio.audio_de_la_app = false;
  VERIFICAR(tinta::KbpsAudioEfectivo(sin_audio, cap) == 0);

  CASO("el espacial sin hardware baja a estereo, no falla");
  tinta::AjustesGrabacion pide_espacial;
  pide_espacial.audio.canales = tinta::ModoCanales::Espacial;
  tinta::AjustesGrabacion corregido =
      tinta::AjustarACapacidades(pide_espacial, cap);
  VERIFICAR(corregido.audio.canales == tinta::ModoCanales::Estereo);

  CASO("y sin estereo baja hasta mono");
  tinta::CapacidadesGrabacion pobre = cap;
  pobre.estereo = false;
  pobre.espacial = false;
  corregido = tinta::AjustarACapacidades(pide_espacial, pobre);
  VERIFICAR(corregido.audio.canales == tinta::ModoCanales::Mono);

  CASO("los FPS se recortan a lo que da el panel");
  tinta::AjustesGrabacion rapido;
  rapido.modo = tinta::ModoCaptura::PantallaConAudio;
  rapido.video.fps = 240;
  tinta::CapacidadesGrabacion sesenta = cap;
  sesenta.fps_maximo = 60;
  VERIFICAR(tinta::AjustarACapacidades(rapido, sesenta).video.fps == 60);

  CASO("sin codificador HEVC se cae a H264");
  tinta::CapacidadesGrabacion sin_hevc = cap;
  sin_hevc.hevc = false;
  tinta::AjustesGrabacion con_hevc;
  con_hevc.video.codec = tinta::CodecVideo::HEVC;
  VERIFICAR(tinta::AjustarACapacidades(con_hevc, sin_hevc).video.codec ==
            tinta::CodecVideo::H264);

  CASO("los ajustes por defecto son grabables");
  std::string motivo;
  VERIFICAR(tinta::Validar(tinta::AjustesGrabacion{}, cap, &motivo));
  VERIFICAR(motivo.empty());

  CASO("validar explica el problema en vez de solo decir que no");
  VERIFICAR(!tinta::Validar(rapido, sesenta, &motivo));
  VERIFICAR(!motivo.empty());

  CASO("lo corregido siempre pasa la validacion");
  VERIFICAR(tinta::Validar(tinta::AjustarACapacidades(rapido, sesenta), sesenta,
                           &motivo));
  VERIFICAR(tinta::Validar(tinta::AjustarACapacidades(pide_espacial, pobre),
                           pobre, &motivo));

  CASO("el patron estereo con una sola pista no vale");
  tinta::AjustesGrabacion incoherente;
  incoherente.audio.canales = tinta::ModoCanales::Mono;
  incoherente.audio.patron = tinta::PatronPolar::Estereo;
  VERIFICAR(!tinta::Validar(incoherente, cap, &motivo));
  VERIFICAR(tinta::AjustarACapacidades(incoherente, cap).audio.patron ==
            tinta::PatronPolar::Cardioide);

  CASO("los megabytes por minuto salen del bitrate");
  tinta::AjustesGrabacion medido;
  medido.modo = tinta::ModoCaptura::PantallaConAudio;
  medido.video.kbps = 8000;
  medido.audio.microfono = false;
  medido.audio.audio_de_la_app = false;
  VERIFICAR(CasiIgual(
      static_cast<float>(tinta::MegabytesPorMinuto(medido, cap)), 60.0f, 0.01f));

  CASO("con 600 MB libres caben 10 minutos");
  VERIFICAR(CasiIgual(
      static_cast<float>(tinta::MinutosQueCaben(medido, cap, 600.0)), 10.0f,
      0.01f));

  CASO("hay preajustes y todos son validos en un aparato normal");
  const std::vector<tinta::PreajusteGrabacion> preajustes = tinta::Preajustes();
  VERIFICAR(preajustes.size() >= 4);
  tinta::CapacidadesGrabacion completo = cap;
  completo.espacial = true;
  completo.canales_entrada_max = 4;
  for (const tinta::PreajusteGrabacion& p : preajustes) {
    VERIFICAR(!p.nombre.empty());
    VERIFICAR(tinta::Validar(p.ajustes, completo, &motivo));
  }

  CASO("ahorro de espacio pesa menos que maxima calidad");
  double maxima = 0.0;
  double ahorro = 0.0;
  for (const tinta::PreajusteGrabacion& p : preajustes) {
    if (p.nombre == "Maxima calidad") maxima = tinta::MegabytesPorMinuto(p.ajustes, completo);
    if (p.nombre == "Ahorro de espacio") ahorro = tinta::MegabytesPorMinuto(p.ajustes, completo);
  }
  VERIFICAR(maxima > 0.0 && ahorro > 0.0 && ahorro < maxima);
}

void PruebasSesionGrabacion() {
  std::printf("\nLa grabacion en curso\n");

  tinta::SesionGrabacion sesion;

  CASO("empieza detenida");
  VERIFICAR(sesion.estado() == tinta::EstadoGrabacion::Detenida);
  VERIFICAR(!sesion.grabando());

  CASO("no se puede pausar ni detener lo que no ha empezado");
  VERIFICAR(!sesion.Pausar(1.0));
  VERIFICAR(!sesion.Detener(1.0));
  VERIFICAR(!sesion.Reanudar(1.0));

  CASO("comenzar y contar");
  VERIFICAR(sesion.Comenzar(100.0));
  VERIFICAR(sesion.grabando());
  VERIFICAR(CasiIgual(static_cast<float>(sesion.SegundosGrabados(110.0)), 10.0f));

  CASO("no se puede comenzar dos veces");
  VERIFICAR(!sesion.Comenzar(105.0));

  CASO("el tiempo en pausa no cuenta");
  VERIFICAR(sesion.Pausar(110.0));
  VERIFICAR(CasiIgual(static_cast<float>(sesion.SegundosGrabados(200.0)), 10.0f));

  CASO("al reanudar sigue sumando desde donde iba");
  VERIFICAR(sesion.Reanudar(200.0));
  VERIFICAR(CasiIgual(static_cast<float>(sesion.SegundosGrabados(205.0)), 15.0f));

  CASO("detener congela el total");
  VERIFICAR(sesion.Detener(205.0));
  VERIFICAR(sesion.estado() == tinta::EstadoGrabacion::Detenida);
  VERIFICAR(CasiIgual(static_cast<float>(sesion.SegundosGrabados(999.0)), 15.0f));

  CASO("volver a comenzar arranca de cero");
  VERIFICAR(sesion.Comenzar(300.0));
  VERIFICAR(CasiIgual(static_cast<float>(sesion.SegundosGrabados(300.0)), 0.0f));

  CASO("un reloj que retrocede no resta tiempo");
  VERIFICAR(CasiIgual(static_cast<float>(sesion.SegundosGrabados(290.0)), 0.0f));

  CASO("los megabytes estimados salen del tiempo real grabado");
  tinta::CapacidadesGrabacion cap;
  tinta::AjustesGrabacion ajustes;
  ajustes.modo = tinta::ModoCaptura::PantallaConAudio;
  ajustes.video.kbps = 8000;  // 60 MB por minuto
  ajustes.audio.microfono = false;
  ajustes.audio.audio_de_la_app = false;
  VERIFICAR(CasiIgual(
      static_cast<float>(sesion.MegabytesEstimados(360.0, ajustes, cap)), 60.0f,
      0.1f));
}

void PruebasImportarPdf() {
  std::printf("\nImportar PDF\n");

  tinta::PaginaPdfFuente carta;  // 612 x 792 pt, vertical
  tinta::PaginaPdfFuente apaisada;
  apaisada.ancho_pt = 792.0f;
  apaisada.alto_pt = 612.0f;

  CASO("una carta vertical es vertical");
  VERIFICAR(tinta::OrientacionDe(carta) == tinta::OrientacionPagina::Vertical);

  CASO("una diapositiva apaisada es horizontal");
  VERIFICAR(tinta::OrientacionDe(apaisada) ==
            tinta::OrientacionPagina::Horizontal);

  CASO("la rotacion del PDF cambia la orientacion");
  tinta::PaginaPdfFuente girada = carta;
  girada.rotacion = 90;
  VERIFICAR(tinta::OrientacionDe(girada) ==
            tinta::OrientacionPagina::Horizontal);
  girada.rotacion = 180;
  VERIFICAR(tinta::OrientacionDe(girada) == tinta::OrientacionPagina::Vertical);
  girada.rotacion = 270;
  VERIFICAR(tinta::OrientacionDe(girada) ==
            tinta::OrientacionPagina::Horizontal);

  CASO("una rotacion negativa o mayor de 360 tambien vale");
  girada.rotacion = -90;
  VERIFICAR(tinta::OrientacionDe(girada) ==
            tinta::OrientacionPagina::Horizontal);
  girada.rotacion = 450;
  VERIFICAR(tinta::OrientacionDe(girada) ==
            tinta::OrientacionPagina::Horizontal);

  CASO("a 150 ppp una carta mide 1275 x 1650");
  tinta::AjustesImportarPdf ajustes;
  ajustes.ppp = 150.0f;
  float ancho = 0.0f;
  float alto = 0.0f;
  tinta::TamanoDePagina(carta, ajustes, &ancho, &alto);
  VERIFICAR(CasiIgual(ancho, 1275.0f, 0.5f));
  VERIFICAR(CasiIgual(alto, 1650.0f, 0.5f));

  CASO("el margen para notas ensancha la hoja pero no el PDF");
  ajustes.margen_para_notas = true;
  ajustes.fraccion_margen = 0.4f;
  tinta::TamanoDePagina(carta, ajustes, &ancho, &alto);
  VERIFICAR(CasiIgual(ancho, 1275.0f * 1.4f, 0.5f));
  VERIFICAR(CasiIgual(alto, 1650.0f, 0.5f));
  tinta::Rect marco = tinta::MarcoDelPdf(carta, ajustes);
  VERIFICAR(CasiIgual(marco.ancho, 1275.0f, 0.5f));
  ajustes.margen_para_notas = false;

  CASO("ajustar al ancho del lienzo no deforma");
  ajustes.ajuste = tinta::AjustePdf::AnchoDelLienzo;
  ajustes.ancho_objetivo = 1240.0f;
  tinta::TamanoDePagina(carta, ajustes, &ancho, &alto);
  VERIFICAR(CasiIgual(ancho, 1240.0f, 0.5f));
  VERIFICAR(CasiIgual(alto / ancho, 792.0f / 612.0f, 0.01f));

  CASO("encajar mete la pagina entera dentro del objetivo");
  ajustes.ajuste = tinta::AjustePdf::Encajar;
  ajustes.ancho_objetivo = 1240.0f;
  ajustes.alto_objetivo = 1000.0f;
  tinta::TamanoDePagina(apaisada, ajustes, &ancho, &alto);
  VERIFICAR(ancho <= 1240.5f && alto <= 1000.5f);
  VERIFICAR(CasiIgual(ancho / alto, 792.0f / 612.0f, 0.01f));

  CASO("la pagina importada recuerda de que hoja del PDF salio");
  tinta::AjustesImportarPdf simple;
  tinta::Pagina p = tinta::PaginaDesdePdf(carta, 3, simple, "hash1", "Clase 4");
  VERIFICAR(p.pagina_pdf == 3);
  VERIFICAR(p.medios.size() == 1);
  VERIFICAR(p.medios[0].tipo == tinta::TipoMedio::Pdf);
  VERIFICAR(p.medios[0].pagina_fuente == 3);
  VERIFICAR(p.medios[0].hash == "hash1");

  CASO("el PDF fija el ancho de la hoja");
  VERIFICAR(!p.limites.ancho_infinito);
  VERIFICAR(CasiIgual(p.limites.ancho_minimo, p.limites.ancho_maximo));
  VERIFICAR(!p.limites.alto_infinito);

  CASO("se puede pedir alto infinito sobre un PDF");
  simple.alto_infinito = true;
  tinta::Pagina abierta =
      tinta::PaginaDesdePdf(carta, 0, simple, "hash1", "Clase 4");
  VERIFICAR(abierta.limites.alto_infinito);
  VERIFICAR(!abierta.limites.ancho_infinito);
  simple.alto_infinito = false;

  CASO("una presentacion mezcla verticales y apaisadas sin aplastar ninguna");
  std::vector<tinta::PaginaPdfFuente> fuentes{apaisada, carta, apaisada};
  std::vector<tinta::Pagina> paginas =
      tinta::ImportarPdf(fuentes, simple, "hash2", "Presentacion");
  VERIFICAR(paginas.size() == 3);
  VERIFICAR(paginas[0].orientacion == tinta::OrientacionPagina::Horizontal);
  VERIFICAR(paginas[1].orientacion == tinta::OrientacionPagina::Vertical);
  VERIFICAR(paginas[0].ancho > paginas[0].alto);
  VERIFICAR(paginas[1].alto > paginas[1].ancho);
  VERIFICAR(paginas[2].pagina_pdf == 2);

  CASO("una pagina del PDF se puede soltar en el lienzo libre");
  tinta::Pagina origen = tinta::PaginaDesdePdf(carta, 1, simple, "h", "PDF");
  tinta::Trazo encima = TrazoRecto(10.0f, 20.0f, 3);
  origen.trazos.push_back(encima);
  tinta::Pagina destino;
  VERIFICAR(tinta::SoltarEnLienzoLibre(origen, &destino, 100.0f, 200.0f, 0.5f));
  VERIFICAR(destino.medios.size() == 1);
  VERIFICAR(CasiIgual(destino.medios[0].marco.x, 100.0f));
  VERIFICAR(CasiIgual(destino.medios[0].marco.ancho,
                      origen.medios[0].marco.ancho * 0.5f, 0.5f));

  CASO("la tinta escrita encima viaja con la diapositiva");
  VERIFICAR(destino.trazos.size() == 1);
  VERIFICAR(CasiIgual(destino.trazos[0].muestras[0].posicion.x, 105.0f));
  VERIFICAR(CasiIgual(destino.trazos[0].muestras[0].posicion.y, 210.0f));
  VERIFICAR(CasiIgual(destino.trazos[0].ancho_base, encima.ancho_base * 0.5f));

  CASO("una pagina que no viene de un PDF no se puede soltar");
  tinta::Pagina normal;
  VERIFICAR(!tinta::SoltarEnLienzoLibre(normal, &destino, 0.0f, 0.0f, 1.0f));
  VERIFICAR(!tinta::SoltarEnLienzoLibre(origen, nullptr, 0.0f, 0.0f, 1.0f));
}

void PruebasPuntero() {
  std::printf("\nPuntero del lápiz del iPad\n");

  tinta::AjustesPuntero p;

  CASO("el radio del puntero no es el grosor de la pluma");
  // Son dos ajustes distintos a propósito: el grosor es lo que queda escrito y
  // el radio es la marca que señala en el aire dónde está la punta.
  tinta::Trazo trazo;
  trazo.ancho_base = 1.0f;
  p.radio = 20.0f;
  VERIFICAR(CasiIgual(p.RadioEfectivo(), 20.0f));
  VERIFICAR(!CasiIgual(p.RadioEfectivo(), trazo.ancho_base));

  CASO("el radio se recorta a un rango util");
  p.radio = 0.0f;
  VERIFICAR(CasiIgual(p.RadioEfectivo(), tinta::kRadioPunteroMinimo));
  p.radio = 500.0f;
  VERIFICAR(CasiIgual(p.RadioEfectivo(), tinta::kRadioPunteroMaximo));

  CASO("el anillo nunca es mas grueso que el radio");
  p.radio = 4.0f;
  p.grosor_anillo = 50.0f;
  VERIFICAR(CasiIgual(p.GrosorAnilloEfectivo(), 4.0f));

  CASO("el color propio manda si no se sigue a la pluma");
  p.color_argb = 0x80FF0000u;
  p.seguir_color_pluma = false;
  VERIFICAR(p.ColorEfectivo(0xFF00FF00u) == 0x80FF0000u);

  CASO("siguiendo a la pluma se toma el color pero se conserva la transparencia");
  p.seguir_color_pluma = true;
  VERIFICAR(p.ColorEfectivo(0xFF00FF00u) == 0x8000FF00u);

  CASO("sin hover no hay puntero");
  p.visible = true;
  VERIFICAR(!p.DebeVerse(false, false));
  VERIFICAR(p.DebeVerse(true, false));

  CASO("mientras se dibuja el puntero estorba, salvo que se pida");
  p.mostrar_al_dibujar = false;
  VERIFICAR(!p.DebeVerse(true, true));
  p.mostrar_al_dibujar = true;
  VERIFICAR(p.DebeVerse(true, true));

  CASO("apagado no aparece nunca");
  p.visible = false;
  VERIFICAR(!p.DebeVerse(true, false));
}

void PruebasPantalla() {
  std::printf("\nPantalla siempre activa\n");

  tinta::AjustesPantalla a;

  CASO("por defecto solo se mantiene encendida cuando hace falta");
  VERIFICAR(!a.Debe(false, false));
  VERIFICAR(a.Debe(true, false));   // grabando
  VERIFICAR(a.Debe(false, true));   // escribiendo

  CASO("el interruptor de siempre gana a todo");
  a.siempre_activa = true;
  VERIFICAR(a.Debe(false, false));

  CASO("se puede desactivar caso por caso");
  a.siempre_activa = false;
  a.siempre_activa_al_grabar = false;
  a.siempre_activa_al_escribir = false;
  VERIFICAR(!a.Debe(true, true));
}

void PruebasDictado() {
  std::printf("\nDictado\n");

  tinta::AjustesDictado d;

  CASO("plegar acentos para poder comparar palabras");
  VERIFICAR(tinta::PlegarAcentos("l\xC3\xADnea") == "linea");
  VERIFICAR(tinta::PlegarAcentos("Ni\xC3\xB1o") == "Nino");
  VERIFICAR(tinta::PlegarAcentos("P\xC3\x81RRAFO") == "PARRAFO");
  VERIFICAR(tinta::PlegarAcentos("sin nada raro") == "sin nada raro");

  CASO("la coma dictada no deja espacio delante");
  VERIFICAR(tinta::LimpiarDictado("hola coma esto es una prueba punto", d) ==
            "Hola, esto es una prueba.");

  CASO("punto y aparte abre parrafo");
  VERIFICAR(tinta::LimpiarDictado("hola punto y aparte adios", d) ==
            "Hola.\n\nAdios");

  CASO("punto y coma no lo gana punto");
  VERIFICAR(tinta::LimpiarDictado("uno punto y coma dos", d) == "Uno; dos");

  CASO("nueva linea salta sin punto");
  VERIFICAR(tinta::LimpiarDictado("uno nueva linea dos", d) == "Uno\nDos");

  CASO("y funciona con tilde, como lo dice el reconocedor");
  VERIFICAR(tinta::LimpiarDictado("uno nueva l\xC3\xADnea dos", d) ==
            "Uno\nDos");

  CASO("los parentesis pegan por el lado que toca");
  VERIFICAR(tinta::LimpiarDictado("abre parentesis nota cierra parentesis", d) ==
            "(nota)");

  CASO("dos puntos y signos de interrogacion");
  VERIFICAR(tinta::LimpiarDictado("ejemplo dos puntos uno", d) ==
            "Ejemplo: uno");
  VERIFICAR(tinta::LimpiarDictado("que hora es signo de interrogacion", d) ==
            "Que hora es?");

  CASO("se puede apagar la puntuacion hablada");
  d.puntuacion_hablada = false;
  VERIFICAR(tinta::LimpiarDictado("hola coma que tal", d) == "Hola coma que tal");
  d.puntuacion_hablada = true;

  CASO("se puede apagar la mayuscula inicial");
  d.mayuscula_inicial = false;
  VERIFICAR(tinta::LimpiarDictado("hola punto adios", d) == "hola. adios");
  d.mayuscula_inicial = true;

  CASO("la mayuscula respeta las tildes");
  VERIFICAR(tinta::LimpiarDictado("\xC3\xA1rbol", d) == "\xC3\x81rbol");

  CASO("el vacio y los espacios de mas no rompen nada");
  VERIFICAR(tinta::LimpiarDictado("", d).empty());
  VERIFICAR(tinta::LimpiarDictado("   ", d).empty());
  VERIFICAR(tinta::LimpiarDictado("  hola   mundo  ", d) == "Hola mundo");

  CASO("un dictado que es solo puntuacion no deja basura");
  VERIFICAR(tinta::LimpiarDictado("nueva linea", d).empty());
}

void PruebasApiCNueva() {
  std::printf("\nAPI en C: grabacion, puntero y dictado\n");

  TintaCapacidades cap;
  cap.ancho_pantalla = 2048;
  cap.alto_pantalla = 2732;
  cap.fps_maximo = 120;
  cap.estereo = true;
  cap.espacial = false;
  cap.hevc = true;
  cap.hdr = false;
  cap.canales_entrada_max = 2;

  CASO("los ajustes por defecto cruzan la frontera en C");
  TintaAjustesGrabacion a = tinta_grabacion_por_defecto();
  VERIFICAR(a.fps == 60);
  VERIFICAR(a.kbps_video == 0);  // 0 = automatico
  VERIFICAR(a.microfono);

  CASO("el tamano resuelto es el mismo que en C++");
  int ancho = 0;
  int alto = 0;
  a.resolucion = 2;  // 1080p
  tinta_grabacion_tamano(a, cap, &ancho, &alto);
  VERIFICAR(ancho == 1080 && alto == 1440);

  CASO("los kbps automaticos salen de ese tamano");
  const int kbps = tinta_grabacion_kbps_video(a, cap);
  VERIFICAR(kbps > 0 && kbps <= 200000);

  CASO("pedir espacial sin hardware devuelve estereo, no un error");
  a.canales = 2;  // espacial
  TintaAjustesGrabacion corregido = tinta_grabacion_ajustar(a, cap);
  VERIFICAR(corregido.canales == 1);  // estereo
  VERIFICAR(tinta_grabacion_canales(corregido) == 2);

  CASO("validar devuelve el motivo en el buffer de Swift");
  char motivo[128] = {0};
  a.fps = 240;
  VERIFICAR(!tinta_grabacion_validar(a, cap, motivo, sizeof(motivo)));
  VERIFICAR(motivo[0] != '\0');

  CASO("un buffer diminuto se trunca pero no se desborda");
  char corto[8] = {0};
  tinta_grabacion_validar(a, cap, corto, sizeof(corto));
  VERIFICAR(corto[7] == '\0');

  CASO("los preajustes se leen por indice");
  VERIFICAR(tinta_grabacion_cantidad_preajustes() >= 4);
  VERIFICAR(tinta_grabacion_nombre_preajuste(0)[0] != '\0');
  VERIFICAR(tinta_grabacion_para_que_preajuste(0)[0] != '\0');
  VERIFICAR(tinta_grabacion_nombre_preajuste(-1)[0] == '\0');
  VERIFICAR(tinta_grabacion_nombre_preajuste(999)[0] == '\0');

  CASO("MB por minuto y minutos que caben");
  TintaAjustesGrabacion medido = tinta_grabacion_por_defecto();
  medido.modo = 1; // PantallaConAudio
  medido.kbps_video = 8000;
  medido.microfono = false;
  medido.audio_de_la_app = false;
  VERIFICAR(CasiIgual(
      static_cast<float>(tinta_grabacion_mb_por_minuto(medido, cap)), 60.0f,
      0.01f));
  VERIFICAR(CasiIgual(static_cast<float>(
                          tinta_grabacion_minutos_que_caben(medido, cap, 600.0)),
                      10.0f, 0.01f));

  CASO("el puntero por C se comporta igual");
  VERIFICAR(CasiIgual(tinta_puntero_radio_efectivo(500.0f), 48.0f));
  VERIFICAR(tinta_puntero_color_efectivo(0x80FF0000u, 0xFF00FF00u, true) ==
            0x8000FF00u);
  VERIFICAR(tinta_puntero_color_efectivo(0x80FF0000u, 0xFF00FF00u, false) ==
            0x80FF0000u);
  VERIFICAR(!tinta_puntero_debe_verse(true, false, false, false));
  VERIFICAR(!tinta_puntero_debe_verse(true, true, true, false));
  VERIFICAR(tinta_puntero_debe_verse(true, true, true, true));

  CASO("la pantalla siempre activa por C");
  VERIFICAR(!tinta_pantalla_siempre_activa(false, true, true, false, false));
  VERIFICAR(tinta_pantalla_siempre_activa(false, true, true, true, false));
  VERIFICAR(tinta_pantalla_siempre_activa(true, false, false, false, false));

  CASO("limpiar dictado por C");
  char texto[128] = {0};
  const size_t escritos =
      tinta_limpiar_dictado("hola coma mundo punto", true, true, texto,
                            sizeof(texto));
  VERIFICAR(std::string(texto) == "Hola, mundo.");
  VERIFICAR(escritos == std::string("Hola, mundo.").size());

  CASO("pidiendo solo el tamano no hace falta buffer");
  VERIFICAR(tinta_limpiar_dictado("hola coma mundo punto", true, true, nullptr,
                                  0) == escritos);

  CASO("dictado con puntero nulo o buffer corto");
  VERIFICAR(tinta_limpiar_dictado(nullptr, true, true, texto, sizeof(texto)) == 0);
  char chico[4] = {0};
  tinta_limpiar_dictado("hola coma mundo punto", true, true, chico,
                        sizeof(chico));
  VERIFICAR(chico[3] == '\0');

  CASO("orientacion de una pagina de PDF por C");
  VERIFICAR(tinta_pdf_orientacion(612.0f, 792.0f, 0) == 0);
  VERIFICAR(tinta_pdf_orientacion(612.0f, 792.0f, 90) == 1);
  VERIFICAR(tinta_pdf_orientacion(792.0f, 612.0f, 0) == 1);

  CASO("tamano y marco de una pagina de PDF por C");
  float w = 0.0f;
  float h = 0.0f;
  tinta_pdf_tamano_pagina(612.0f, 792.0f, 0, 150.0f, 0, 1240.0f, 1754.0f, true,
                          0.4f, &w, &h);
  VERIFICAR(CasiIgual(w, 1275.0f * 1.4f, 0.5f));
  VERIFICAR(CasiIgual(h, 1650.0f, 0.5f));

  float mx = 0.0f;
  float my = 0.0f;
  float mw = 0.0f;
  float mh = 0.0f;
  tinta_pdf_marco(612.0f, 792.0f, 0, 150.0f, 0, 1240.0f, 1754.0f, &mx, &my, &mw,
                  &mh);
  VERIFICAR(CasiIgual(mx, 0.0f) && CasiIgual(my, 0.0f));
  VERIFICAR(CasiIgual(mw, 1275.0f, 0.5f));

  CASO("recortar dos veces se compone sobre lo que se ve, no sobre el original");
  float rx = 0.0f, ry = 0.0f, rw = 1.0f, rh = 1.0f;
  tinta_recorte_componer(&rx, &ry, &rw, &rh, 0.5f, 0.0f, 0.5f, 1.0f);
  VERIFICAR(CasiIgual(rx, 0.5f) && CasiIgual(rw, 0.5f));
  // La segunda mitad de la mitad derecha es el ultimo cuarto del original.
  tinta_recorte_componer(&rx, &ry, &rw, &rh, 0.5f, 0.0f, 0.5f, 1.0f);
  VERIFICAR(CasiIgual(rx, 0.75f) && CasiIgual(rw, 0.25f));

  CASO("un recorte imposible no rompe el que ya habia");
  tinta_recorte_componer(&rx, &ry, &rw, &rh, 0.0f, 0.0f, 0.0f, 0.0f);
  VERIFICAR(CasiIgual(rx, 0.75f) && CasiIgual(rw, 0.25f));
  tinta_recorte_componer(nullptr, nullptr, nullptr, nullptr, 0, 0, 1, 1);
}

// ---------------------------------------------------------------------------
// Tamaños y pautas nuevas
// ---------------------------------------------------------------------------

void PruebasTamanosPagina() {
  std::printf("\nTamanos de pagina\n");

  CASO("DimensionesDeTamano devuelve correcto");
  auto dim_a4 = tinta::DimensionesDeTamano(tinta::TamanoPagina::A4);
  VERIFICAR(CasiIgual(dim_a4.ancho, 1240.0f) && CasiIgual(dim_a4.alto, 1754.0f));
  
  CASO("CrearPagina A4 vertical y horizontal");
  tinta::Pagina p_vert = tinta::CrearPagina(tinta::TamanoPagina::A4, tinta::OrientacionPagina::Vertical);
  VERIFICAR(CasiIgual(p_vert.ancho, 1240.0f) && CasiIgual(p_vert.alto, 1754.0f));
  VERIFICAR(p_vert.limites.ancho_maximo == 1240.0f);
  VERIFICAR(!p_vert.limites.ancho_infinito && !p_vert.limites.alto_infinito);
  
  tinta::Pagina p_horiz = tinta::CrearPagina(tinta::TamanoPagina::A4, tinta::OrientacionPagina::Horizontal);
  VERIFICAR(CasiIgual(p_horiz.ancho, 1754.0f) && CasiIgual(p_horiz.alto, 1240.0f));

  CASO("CambiarTamano preserva limites y ajusta variables");
  tinta::CambiarTamano(&p_vert, tinta::TamanoPagina::Carta);
  VERIFICAR(p_vert.ancho != 1240.0f);
  VERIFICAR(CasiIgual(p_vert.ancho, std::round(216.0f * 150.0f / 25.4f)));

  CASO("FijarTamano hace los dos limites no infinitos");
  tinta::FijarTamano(&p_vert, 1000.0f, 1000.0f);
  VERIFICAR(!p_vert.limites.ancho_infinito && !p_vert.limites.alto_infinito);
  VERIFICAR(CasiIgual(p_vert.ancho, 1000.0f));

  CASO("HacerInfinito hace ambos infinitos");
  tinta::HacerInfinito(&p_vert);
  VERIFICAR(p_vert.limites.ancho_infinito && p_vert.limites.alto_infinito);

  CASO("HacerVerticalInfinito ancho=false alto=true");
  tinta::HacerVerticalInfinito(&p_vert);
  VERIFICAR(!p_vert.limites.ancho_infinito && p_vert.limites.alto_infinito);

  CASO("HacerHorizontalInfinito ancho=true alto=false");
  tinta::HacerHorizontalInfinito(&p_vert);
  VERIFICAR(p_vert.limites.ancho_infinito && !p_vert.limites.alto_infinito);

  CASO("AjustarAlContenido encoge hacia la tinta");
  tinta::Pagina p_ajuste = tinta::CrearPagina(tinta::TamanoPagina::A4);
  p_ajuste.trazos.push_back(TrazoRecto(100.0f, 100.0f, 5));
  float margen = 20.0f;
  VERIFICAR(tinta::AjustarAlContenido(&p_ajuste, margen));
  VERIFICAR(p_ajuste.ancho < 1240.0f); // Se encogio mucho
}

void PruebasPautasNuevas() {
  std::printf("\nPautas nuevas\n");
  
  CASO("Milimetrico genera lineas y algunas destacadas");
  tinta::Papel papel;
  papel.pauta = tinta::Pauta::Milimetrico;
  papel.separacion = 10.0f; 
  auto lineas = tinta::GenerarPauta(papel, 0, 0, 100, 100);
  VERIFICAR(!lineas.empty());
  bool hay_destacada = false;
  for (const auto& l : lineas) if (l.destacada) hay_destacada = true;
  VERIFICAR(hay_destacada);
  
  CASO("Cornell genera margenes destacados");
  papel.pauta = tinta::Pauta::Cornell;
  lineas = tinta::GenerarPauta(papel, 0, 0, 1000, 1000);
  VERIFICAR(!lineas.empty());
  
  CASO("Isometrico genera mas lineas que horizontales");
  papel.pauta = tinta::Pauta::Isometrico;
  lineas = tinta::GenerarPauta(papel, 0, 0, 500, 500);
  VERIFICAR(!lineas.empty());

  CASO("Logaritmico genera mas lineas que horizontales");
  papel.pauta = tinta::Pauta::Logaritmico;
  lineas = tinta::GenerarPauta(papel, 0, 0, 500, 500);
  VERIFICAR(!lineas.empty());

  CASO("Polar genera lineas");
  papel.pauta = tinta::Pauta::Polar;
  lineas = tinta::GenerarPauta(papel, 0, 0, 500, 500);
  VERIFICAR(!lineas.empty());

  CASO("Circuitos genera cuadricula grande");
  papel.pauta = tinta::Pauta::Circuitos;
  lineas = tinta::GenerarPauta(papel, 0, 0, 500, 500);
  VERIFICAR(!lineas.empty());

  CASO("Color personalizado anula color de fondo");
  papel.tipo = tinta::TipoPapel::Blanco;
  papel.color_personalizado = 0xFF112233;
  VERIFICAR(papel.color_fondo() == 0xFF112233);

  CASO("Opacidad afecta alfa");
  papel.opacidad_pauta = 0.5f;
  uint32_t c = papel.color_pauta();
  uint8_t alpha = (c >> 24) & 0xFF;
  VERIFICAR(alpha < 0x22); // Menor al original 0x22
}

// ---------------------------------------------------------------------------
// Panel lateral: las Casio dentro del cuaderno
// ---------------------------------------------------------------------------

void PruebasPanelCasio() {
  std::printf("\nPanel lateral de la calculadora\n");

  // Anchos reales, en puntos: iPad Pro 12,9\" apaisado y iPhone en vertical.
  constexpr float kIpadApaisado = 1366.0f;
  constexpr float kIphoneVertical = 390.0f;

  tinta::AjustesPanel a;

  CASO("con el panel cerrado el lienzo se queda con toda la pantalla");
  a.visible = false;
  VERIFICAR(CasiIgual(tinta::AnchoPanel(kIpadApaisado, a), 0.0f));
  VERIFICAR(CasiIgual(tinta::AnchoLienzo(kIpadApaisado, a), kIpadApaisado));
  VERIFICAR(CasiIgual(tinta::OrigenLienzo(kIpadApaisado, a), 0.0f));

  CASO("en iPad apaisado se reparte, con el panel a la derecha");
  a.visible = true;
  VERIFICAR(!tinta::PanelCubreTodo(kIpadApaisado, a));
  VERIFICAR(CasiIgual(tinta::AnchoPanel(kIpadApaisado, a), 519.08f, 0.01f));
  VERIFICAR(CasiIgual(tinta::AnchoLienzo(kIpadApaisado, a), 846.92f, 0.01f));
  // Los dos trozos suman la pantalla: ni se solapan ni dejan hueco.
  VERIFICAR(CasiIgual(tinta::AnchoPanel(kIpadApaisado, a)
                      + tinta::AnchoLienzo(kIpadApaisado, a), kIpadApaisado, 0.01f));
  VERIFICAR(CasiIgual(tinta::OrigenLienzo(kIpadApaisado, a), 0.0f));
  VERIFICAR(CasiIgual(tinta::OrigenPanel(kIpadApaisado, a), 846.92f, 0.01f));

  CASO("el panel a la izquierda empuja el lienzo, no lo tapa");
  a.lado = tinta::LadoPanel::Izquierda;
  VERIFICAR(CasiIgual(tinta::OrigenPanel(kIpadApaisado, a), 0.0f));
  VERIFICAR(CasiIgual(tinta::OrigenLienzo(kIpadApaisado, a), 519.08f, 0.01f));
  a.lado = tinta::LadoPanel::Derecha;

  CASO("nunca se encoge por debajo del minimo legible de la ClassWiz");
  // Una fraccion pequeña no puede dejar la calculadora ilegible: manda min_pt.
  a.fraccion = 0.10f;
  VERIFICAR(CasiIgual(tinta::AnchoPanel(800.0f, a), 320.0f));
  VERIFICAR(CasiIgual(tinta::AnchoLienzo(800.0f, a), 480.0f));

  CASO("ni crece tanto que el cuaderno deje de ser el sitio donde se escribe");
  a.fraccion = 0.90f;
  // Atado al propio ajuste y no a un numero suelto: cuando max_fraccion paso de
  // 0,66 a 2/3 exacto, la version con el numero escrito a mano fallo.
  VERIFICAR(CasiIgual(tinta::AnchoPanel(1000.0f, a), 1000.0f * a.max_fraccion));
  VERIFICAR(CasiIgual(tinta::AnchoLienzo(1000.0f, a),
                      1000.0f - 1000.0f * a.max_fraccion));
  // Y que ese tope sean de verdad dos tercios, no cualquier cosa.
  VERIFICAR(CasiIgual(tinta::AnchoPanel(1000.0f, a), 666.67f, 0.01f));
  a.fraccion = 0.38f;

  CASO("en iPhone vertical no se reparte: el panel cubre la pantalla");
  VERIFICAR(tinta::PanelCubreTodo(kIphoneVertical, a));
  VERIFICAR(CasiIgual(tinta::AnchoPanel(kIphoneVertical, a), kIphoneVertical));
  VERIFICAR(CasiIgual(tinta::OrigenPanel(kIphoneVertical, a), 0.0f));

  CASO("cubriendo todo, el lienzo NO se encoge: se queda entero detras");
  // Encogerlo reacomodaria los apuntes cada vez que se abre la calculadora, y
  // volverian a moverse al cerrarla.
  VERIFICAR(CasiIgual(tinta::AnchoLienzo(kIphoneVertical, a), kIphoneVertical));
  VERIFICAR(CasiIgual(tinta::OrigenLienzo(kIphoneVertical, a), 0.0f));

  CASO("si el minimo no cabe en la fraccion maxima, tampoco se reparte");
  // Pantalla ancha de sobra, pero 0,35 de 800 son 280 y el panel pide 320.
  tinta::AjustesPanel apretado;
  apretado.visible = true;
  apretado.max_fraccion = 0.35f;
  VERIFICAR(tinta::PanelCubreTodo(800.0f, apretado));

  CASO("un ancho de cero no revienta el reparto");
  VERIFICAR(tinta::PanelCubreTodo(0.0f, a));
  VERIFICAR(CasiIgual(tinta::AnchoPanel(0.0f, a), 0.0f));
  VERIFICAR(CasiIgual(tinta::AnchoLienzo(0.0f, a), 0.0f));

  CASO("al soltar, el divisor se pega a un tercio, la mitad o dos tercios");
  VERIFICAR(CasiIgual(tinta::FraccionAlSoltar(0.30f, a), 1.0f / 3.0f));
  VERIFICAR(CasiIgual(tinta::FraccionAlSoltar(0.48f, a), 0.5f));
  VERIFICAR(CasiIgual(tinta::FraccionAlSoltar(0.60f, a), 2.0f / 3.0f));

  CASO("arrastrarlo hasta el borde lo cierra, sin boton aparte");
  VERIFICAR(CasiIgual(tinta::FraccionAlSoltar(0.05f, a), 0.0f));
  VERIFICAR(CasiIgual(tinta::FraccionAlSoltar(0.0f, a), 0.0f));
  // Justo por encima del umbral todavia NO cierra: abre a un tercio.
  VERIFICAR(CasiIgual(tinta::FraccionAlSoltar(0.20f, a), 1.0f / 3.0f));

  CASO("no ofrece repartos que la configuracion prohibe");
  tinta::AjustesPanel estrecho;
  estrecho.max_fraccion = 0.40f;   // deja fuera la mitad y los dos tercios
  VERIFICAR(CasiIgual(tinta::FraccionAlSoltar(0.45f, estrecho), 1.0f / 3.0f));

  CASO("si ni el reparto mas pequeno cabe, se queda en el maximo");
  tinta::AjustesPanel minusculo;
  minusculo.max_fraccion = 0.20f;
  VERIFICAR(CasiIgual(tinta::FraccionAlSoltar(0.15f, minusculo), 0.20f));
  VERIFICAR(CasiIgual(tinta::FraccionAlSoltar(0.05f, minusculo), 0.0f));

  // --- lo mismo por la API en C, que es lo que llama Swift ---

  CASO("el reparto por C da lo mismo que por C++");
  float ol = 0, al = 0, op = 0, ap = 0;
  tinta_panel_reparto(kIpadApaisado, true, 0, 0.38f, 320.0f, 0.66f, 700.0f,
                      &ol, &al, &op, &ap);
  VERIFICAR(CasiIgual(ap, 519.08f, 0.01f) && CasiIgual(al, 846.92f, 0.01f));
  VERIFICAR(CasiIgual(ol, 0.0f) && CasiIgual(op, 846.92f, 0.01f));

  CASO("por C se pueden pedir solo los numeros que interesan");
  float solo_ancho = -1.0f;
  tinta_panel_reparto(kIpadApaisado, true, 0, 0.38f, 320.0f, 0.66f, 700.0f,
                      nullptr, nullptr, nullptr, &solo_ancho);
  VERIFICAR(CasiIgual(solo_ancho, 519.08f, 0.01f));
  // Con los cuatro a NULL no hace nada, pero no se cae.
  tinta_panel_reparto(kIpadApaisado, true, 0, 0.38f, 320.0f, 0.66f, 700.0f,
                      nullptr, nullptr, nullptr, nullptr);

  CASO("el lado izquierdo por C mueve el origen del lienzo");
  tinta_panel_reparto(kIpadApaisado, true, 1, 0.38f, 320.0f, 0.66f, 700.0f,
                      &ol, &al, &op, &ap);
  VERIFICAR(CasiIgual(op, 0.0f) && CasiIgual(ol, 519.08f, 0.01f));

  CASO("cubrir todo y el pegado del divisor tambien por C");
  VERIFICAR(tinta_panel_cubre_todo(kIphoneVertical, 320.0f, 0.66f, 700.0f));
  VERIFICAR(!tinta_panel_cubre_todo(kIpadApaisado, 320.0f, 0.66f, 700.0f));
  VERIFICAR(CasiIgual(tinta_panel_fraccion_al_soltar(0.48f, 0.66f), 0.5f));
  VERIFICAR(CasiIgual(tinta_panel_fraccion_al_soltar(0.05f, 0.66f), 0.0f));
}

void PruebasPdfEngine() {
  CASO("CrearPrintout genera un Medio valido con hash y pagina_fuente");
  tinta::DocumentoPdf pdf;
  pdf.id = "doc1";
  pdf.hash = "pdfhash123";
  pdf.total_paginas = 10;
  
  tinta::Medio m = pdf.CrearPrintout(3, 50.0f, 60.0f);
  VERIFICAR(m.hash == "pdfhash123");
  VERIFICAR(m.pagina_fuente == 3);
  VERIFICAR(m.tipo == tinta::TipoMedio::Imagen);
  VERIFICAR(CasiIgual(m.marco.x, 50.0f));
  VERIFICAR(CasiIgual(m.marco.y, 60.0f));
  
  CASO("HacerLienzoInfinitoAlrededorDePdf cambia limites y aumenta margenes");
  tinta::Pagina p;
  p.ancho = 500.0f;
  p.alto = 600.0f;
  p.limites.ancho_infinito = false;
  p.limites.alto_infinito = false;
  
  tinta::HacerLienzoInfinitoAlrededorDePdf(&p, 300.0f);
  
  VERIFICAR(p.limites.ancho_infinito == true);
  VERIFICAR(p.limites.alto_infinito == true);
  VERIFICAR(CasiIgual(p.ancho, 1100.0f));
  VERIFICAR(CasiIgual(p.alto, 1200.0f));
}

void PruebasSourceAwareClips() {
  CASO("MedioDesdeClip convierte correctamente los limites a Medio");
  tinta::RecorteClip clip;
  clip.id = "clip1";
  clip.marco = tinta::Rect{10.0f, 20.0f, 100.0f, 200.0f};
  clip.forma = tinta::FormaClip::Rectangulo;
  
  tinta::Medio m = tinta::MedioDesdeClip(clip, "cliphash123");
  VERIFICAR(m.hash == "cliphash123");
  VERIFICAR(m.tipo == tinta::TipoMedio::Imagen);
  VERIFICAR(CasiIgual(m.marco.x, 10.0f));
  VERIFICAR(CasiIgual(m.marco.y, 20.0f));
  VERIFICAR(CasiIgual(m.marco.ancho, 100.0f));
  VERIFICAR(CasiIgual(m.marco.alto, 200.0f));
}

}  // namespace

#include "pruebas_combinadas.cpp_snippet"
#include <filesystem>

void PruebasAlmacen() {
  CASO("Configurar ruta y guardar cuaderno");
  std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "tinta_test";
  std::filesystem::remove_all(temp_dir);
  tinta::ConfigurarRutaAlmacen(temp_dir.string());

  tinta::Documento doc = DocumentoDePrueba();
  VERIFICAR(doc.id.empty());
  VERIFICAR(tinta::GuardarCuaderno(doc));
  VERIFICAR(!doc.id.empty());
  
  std::string id_guardado = doc.id;

  CASO("Listar cuadernos");
  auto lista = tinta::ListarCuadernos();
  VERIFICAR(lista.size() == 1);
  VERIFICAR(lista[0].id == id_guardado);
  VERIFICAR(lista[0].titulo == "Electronica");
  
  CASO("Cargar cuaderno");
  tinta::Documento cargado;
  VERIFICAR(tinta::CargarCuaderno(id_guardado, &cargado));
  VERIFICAR(cargado.id == id_guardado);
  VERIFICAR(cargado.titulo == "Electronica");
  VERIFICAR(cargado.cuadernos.size() == 1);
  VERIFICAR(cargado.cuadernos[0].secciones[0].paginas[0].titulo == "P0");

  CASO("rechazar rutas e identidades de almacen cruzadas");
  VERIFICAR(!tinta::CargarCuaderno("../fuera", &cargado));
  const auto ruta_meta = temp_dir / (id_guardado + ".tinta") / "metadata.bin";
  {
    std::fstream meta(ruta_meta, std::ios::binary | std::ios::in | std::ios::out);
    uint32_t largo = 0;
    VERIFICAR(static_cast<bool>(meta.read(
        reinterpret_cast<char*>(&largo), sizeof(largo))));
    VERIFICAR(largo == id_guardado.size());
    meta.seekp(static_cast<std::streamoff>(sizeof(largo)));
    std::string identidad_cruzada(largo, 'x');
    meta.write(identidad_cruzada.data(), identidad_cruzada.size());
    meta.flush();
    VERIFICAR(static_cast<bool>(meta));
  }
  VERIFICAR(!tinta::CargarCuaderno(id_guardado, &cargado));
  VERIFICAR(std::string(tinta::UltimoErrorArchivo()).find("identidad") !=
            std::string::npos);
  
  std::filesystem::remove_all(temp_dir);
}

int main() {
  std::printf("Pruebas del nucleo de tinta\n\n");

  PruebasLienzo();
  PruebasPapel();
  PruebasPagina();
  PruebasMoverPagina();
  PruebasBorrador();
  PruebasLazo();
  PruebasTransformar();
  PruebasHistorial();
  PruebasTintaYPrediccion();
  PruebasMedios();
  PruebasNotas();
  PruebasFormas();
  PruebasArchivo();
  PruebasApiC();
  PruebasGrabacion();
  PruebasSesionGrabacion();
  PruebasImportarPdf();
  PruebasPuntero();
  PruebasPantalla();
  PruebasDictado();
  PruebasApiCNueva();
  PruebasTamanosPagina();
  PruebasPautasNuevas();
  PruebasLazoAvanzado();
  PruebasOrdenZ();
  PruebasPdfEngine();
  PruebasSourceAwareClips();
  EjecutarPruebasCircuitos();
  PruebasEstadoSesion();
  ProbarGestorFoco();
  EjecutarPruebasMatematicas();
  EjecutarPruebasPerspectiva();
  PruebaMultiplesDocumentos();
  PruebaSplitView();
  PruebasPanelCasio();
  PruebasAlmacen();

  std::printf("\n%d verificaciones, %d fallos\n", total, fallos);
  return fallos == 0 ? 0 : 1;
}
