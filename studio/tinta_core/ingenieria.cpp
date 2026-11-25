#include "ingenieria.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>

namespace tinta {
namespace {

template <typename T>
bool Gauss(std::vector<std::vector<T>> a, std::vector<T> b,
           std::vector<T>* x) {
  const int n = static_cast<int>(a.size());
  if (!x || static_cast<int>(b.size()) != n) return false;
  for (int i = 0; i < n; ++i) {
    int pivote = i;
    double mayor = std::abs(a[i][i]);
    for (int f = i + 1; f < n; ++f) {
      if (std::abs(a[f][i]) > mayor) {
        mayor = std::abs(a[f][i]);
        pivote = f;
      }
    }
    if (mayor < 1e-15 || !std::isfinite(mayor)) return false;
    if (pivote != i) {
      std::swap(a[pivote], a[i]);
      std::swap(b[pivote], b[i]);
    }
    for (int f = i + 1; f < n; ++f) {
      const T factor = a[f][i] / a[i][i];
      a[f][i] = T{};
      for (int c = i + 1; c < n; ++c) a[f][c] -= factor * a[i][c];
      b[f] -= factor * b[i];
    }
  }
  x->assign(n, T{});
  for (int i = n - 1; i >= 0; --i) {
    T suma = b[i];
    for (int c = i + 1; c < n; ++c) suma -= a[i][c] * (*x)[c];
    (*x)[i] = suma / a[i][i];
  }
  return true;
}

int MaxNodo(const std::vector<ComponenteElectrico>& componentes) {
  int maximo = 0;
  for (const auto& c : componentes) {
    if (!c.activo) continue;
    maximo = std::max({maximo, c.nodo_a, c.nodo_b});
  }
  return maximo;
}

struct CircuitoNormalizado {
  std::vector<ComponenteElectrico> componentes;
  // Índice normalizado -> número que ve el usuario. La posición cero es GND.
  std::vector<int> nodo_original = {0};
  int maximo_original = 0;
};

CircuitoNormalizado NormalizarNodos(
    const std::vector<ComponenteElectrico>& componentes) {
  CircuitoNormalizado salida;
  salida.componentes = componentes;
  std::set<int> numeros;
  for (const auto& c : componentes) {
    if (!c.activo) continue;
    if (c.nodo_a > 0) numeros.insert(c.nodo_a);
    if (c.nodo_b > 0) numeros.insert(c.nodo_b);
  }
  std::map<int, int> a_compacto;
  for (const int original : numeros) {
    const int compacto = static_cast<int>(salida.nodo_original.size());
    salida.nodo_original.push_back(original);
    a_compacto[original] = compacto;
    salida.maximo_original = std::max(salida.maximo_original, original);
  }
  for (auto& c : salida.componentes) {
    if (!c.activo) {
      c.nodo_a = c.nodo_b = c.nodo_control_a = c.nodo_control_b = 0;
      continue;
    }
    c.nodo_a = c.nodo_a > 0 ? a_compacto[c.nodo_a] : 0;
    c.nodo_b = c.nodo_b > 0 ? a_compacto[c.nodo_b] : 0;
    // Ningún modelo disponible usa terminales de control todavía. Se
    // conservan normalizados para que la extensión futura no reintroduzca el
    // supuesto de numeración contigua.
    c.nodo_control_a = c.nodo_control_a > 0 && a_compacto.count(c.nodo_control_a)
                           ? a_compacto[c.nodo_control_a]
                           : 0;
    c.nodo_control_b = c.nodo_control_b > 0 && a_compacto.count(c.nodo_control_b)
                           ? a_compacto[c.nodo_control_b]
                           : 0;
  }
  return salida;
}

bool EsFuenteVoltajeDC(const ComponenteElectrico& c) {
  return c.activo &&
         (c.tipo == TipoComponenteElectrico::FuenteVoltaje ||
          c.tipo == TipoComponenteElectrico::Bateria);
}

bool EsFuenteVoltajeAC(const ComponenteElectrico& c) {
  return c.activo &&
         (c.tipo == TipoComponenteElectrico::FuenteVoltaje ||
          c.tipo == TipoComponenteElectrico::FuenteAC);
}

template <typename T>
void EstamparAdmitancia(std::vector<std::vector<T>>* a, int na, int nb, T y) {
  if (na > 0) (*a)[na - 1][na - 1] += y;
  if (nb > 0) (*a)[nb - 1][nb - 1] += y;
  if (na > 0 && nb > 0) {
    (*a)[na - 1][nb - 1] -= y;
    (*a)[nb - 1][na - 1] -= y;
  }
}

template <typename T>
void EstamparCorriente(std::vector<T>* z, int na, int nb, T corriente) {
  if (na > 0) (*z)[na - 1] -= corriente;
  if (nb > 0) (*z)[nb - 1] += corriente;
}

std::string TipoNombre(TipoComponenteElectrico tipo) {
  switch (tipo) {
    case TipoComponenteElectrico::Resistor: return "resistor";
    case TipoComponenteElectrico::Capacitor: return "capacitor";
    case TipoComponenteElectrico::Inductor: return "inductor";
    case TipoComponenteElectrico::FuenteVoltaje: return "fuente de voltaje";
    case TipoComponenteElectrico::FuenteCorriente: return "fuente de corriente";
    case TipoComponenteElectrico::Diodo: return "diodo";
    case TipoComponenteElectrico::Led: return "LED";
    case TipoComponenteElectrico::BjtNpn: return "BJT NPN";
    case TipoComponenteElectrico::BjtPnp: return "BJT PNP";
    case TipoComponenteElectrico::MosfetN: return "MOSFET N";
    case TipoComponenteElectrico::MosfetP: return "MOSFET P";
    case TipoComponenteElectrico::OpAmp: return "op-amp";
    case TipoComponenteElectrico::Transformador: return "transformador";
    case TipoComponenteElectrico::Interruptor: return "interruptor";
    case TipoComponenteElectrico::Cristal: return "cristal";
    case TipoComponenteElectrico::Rele: return "relé";
    case TipoComponenteElectrico::Tierra: return "tierra";
    case TipoComponenteElectrico::Cable: return "cable";
    case TipoComponenteElectrico::ProbeVoltaje: return "sonda de voltaje";
    case TipoComponenteElectrico::ProbeCorriente: return "sonda de corriente";
    case TipoComponenteElectrico::CompuertaAnd: return "compuerta AND";
    case TipoComponenteElectrico::CompuertaOr: return "compuerta OR";
    case TipoComponenteElectrico::CompuertaNot: return "compuerta NOT";
    case TipoComponenteElectrico::CompuertaNand: return "compuerta NAND";
    case TipoComponenteElectrico::CompuertaNor: return "compuerta NOR";
    case TipoComponenteElectrico::CompuertaXor: return "compuerta XOR";
    case TipoComponenteElectrico::Integrado: return "circuito integrado";
    case TipoComponenteElectrico::Conector: return "conector";
    case TipoComponenteElectrico::CapacitorPolarizado:
      return "capacitor polarizado";
    case TipoComponenteElectrico::DiodoZener: return "diodo Zener";
    case TipoComponenteElectrico::Fotodiodo: return "fotodiodo";
    case TipoComponenteElectrico::JfetN: return "JFET N";
    case TipoComponenteElectrico::JfetP: return "JFET P";
    case TipoComponenteElectrico::Scr: return "SCR";
    case TipoComponenteElectrico::Triac: return "TRIAC";
    case TipoComponenteElectrico::Comparador: return "comparador";
    case TipoComponenteElectrico::FuenteAC: return "fuente AC";
    case TipoComponenteElectrico::FuenteVoltajeDependiente:
      return "fuente de voltaje dependiente";
    case TipoComponenteElectrico::FuenteCorrienteDependiente:
      return "fuente de corriente dependiente";
    case TipoComponenteElectrico::Pulsador: return "pulsador";
    case TipoComponenteElectrico::Vcc: return "símbolo VCC";
    case TipoComponenteElectrico::Bateria: return "batería";
    case TipoComponenteElectrico::Fusible: return "fusible";
    case TipoComponenteElectrico::Motor: return "motor";
    case TipoComponenteElectrico::Antena: return "antena";
    case TipoComponenteElectrico::FlipFlopD: return "flip-flop D";
    case TipoComponenteElectrico::FlipFlopJK: return "flip-flop JK";
    case TipoComponenteElectrico::Multiplexor: return "multiplexor";
    case TipoComponenteElectrico::Demultiplexor: return "demultiplexor";
    case TipoComponenteElectrico::Potenciometro: return "potenciómetro";
    case TipoComponenteElectrico::Voltimetro: return "voltímetro";
    case TipoComponenteElectrico::Amperimetro: return "amperímetro";
    case TipoComponenteElectrico::Osciloscopio: return "osciloscopio";
    case TipoComponenteElectrico::Desconocido: return "tipo desconocido";
  }
  return "tipo desconocido";
}

std::string Identidad(const ComponenteElectrico& c) {
  const std::string tipo = TipoNombre(c.tipo);
  return c.id.empty() ? tipo : tipo + " " + c.id;
}

bool TieneModeloDC(TipoComponenteElectrico tipo) {
  switch (tipo) {
    case TipoComponenteElectrico::Resistor:
    case TipoComponenteElectrico::Capacitor:
    case TipoComponenteElectrico::CapacitorPolarizado:
    case TipoComponenteElectrico::Inductor:
    case TipoComponenteElectrico::FuenteVoltaje:
    case TipoComponenteElectrico::FuenteCorriente:
    case TipoComponenteElectrico::Bateria:
    case TipoComponenteElectrico::Interruptor:
    case TipoComponenteElectrico::Pulsador:
    case TipoComponenteElectrico::Tierra:
    case TipoComponenteElectrico::Cable:
    case TipoComponenteElectrico::ProbeVoltaje:
    case TipoComponenteElectrico::ProbeCorriente:
    case TipoComponenteElectrico::Voltimetro:
    case TipoComponenteElectrico::Osciloscopio:
      return true;
    default:
      return false;
  }
}

bool TieneModeloAC(TipoComponenteElectrico tipo) {
  switch (tipo) {
    case TipoComponenteElectrico::Resistor:
    case TipoComponenteElectrico::Capacitor:
    case TipoComponenteElectrico::CapacitorPolarizado:
    case TipoComponenteElectrico::Inductor:
    case TipoComponenteElectrico::FuenteVoltaje:
    case TipoComponenteElectrico::FuenteCorriente:
    case TipoComponenteElectrico::FuenteAC:
    case TipoComponenteElectrico::Interruptor:
    case TipoComponenteElectrico::Pulsador:
    case TipoComponenteElectrico::Tierra:
    case TipoComponenteElectrico::Cable:
    case TipoComponenteElectrico::ProbeVoltaje:
    case TipoComponenteElectrico::ProbeCorriente:
    case TipoComponenteElectrico::Voltimetro:
    case TipoComponenteElectrico::Osciloscopio:
      return true;
    default:
      return false;
  }
}

std::string ValidarComponente(const ComponenteElectrico& c, bool ac) {
  if (!c.activo) return {};
  if (!(ac ? TieneModeloAC(c.tipo) : TieneModeloDC(c.tipo))) {
    return Identidad(c) + " no tiene un modelo " + (ac ? "AC" : "DC") +
           " implementado; no se calculó un resultado parcial";
  }
  switch (c.tipo) {
    case TipoComponenteElectrico::Resistor:
      if (!(c.valor > 0.0) || !std::isfinite(c.valor))
        return "Falta una resistencia positiva confirmada en " + Identidad(c);
      break;
    case TipoComponenteElectrico::Capacitor:
    case TipoComponenteElectrico::CapacitorPolarizado:
      if (!(c.valor > 0.0) || !std::isfinite(c.valor))
        return "Falta una capacitancia positiva confirmada en " + Identidad(c);
      break;
    case TipoComponenteElectrico::Inductor:
      if (!(c.valor > 0.0) || !std::isfinite(c.valor))
        return "Falta una inductancia positiva confirmada en " + Identidad(c);
      break;
    case TipoComponenteElectrico::Bateria:
      if (!(c.valor > 0.0) || !std::isfinite(c.valor))
        return "Falta el voltaje confirmado de " + Identidad(c);
      break;
    case TipoComponenteElectrico::FuenteVoltaje:
    case TipoComponenteElectrico::FuenteCorriente:
    case TipoComponenteElectrico::FuenteAC:
      if (!std::isfinite(c.valor) || !std::isfinite(c.fase_grados))
        return "Valor o fase no finitos en " + Identidad(c);
      break;
    default:
      break;
  }
  return {};
}

}  // namespace

ResultadoDC ResolverDC(const std::vector<ComponenteElectrico>& componentes) {
  ResultadoDC r;
  for (const auto& c : componentes) {
    if (const std::string error = ValidarComponente(c, false); !error.empty()) {
      r.error = error;
      return r;
    }
  }
  const CircuitoNormalizado normalizado = NormalizarNodos(componentes);
  const auto& circuito = normalizado.componentes;
  const int nodos = MaxNodo(circuito);
  const int fuentes = static_cast<int>(std::count_if(
      circuito.begin(), circuito.end(), EsFuenteVoltajeDC));
  const int n = nodos + fuentes;
  if (n == 0) {
    r.correcto = true;
    r.voltajes = {0.0};
    return r;
  }
  std::vector<std::vector<double>> a(n, std::vector<double>(n, 0.0));
  std::vector<double> z(n, 0.0);
  int fuente = 0;
  for (const auto& c : circuito) {
    if (!c.activo) continue;
    switch (c.tipo) {
      case TipoComponenteElectrico::Resistor:
        EstamparAdmitancia(&a, c.nodo_a, c.nodo_b, 1.0 / c.valor);
        break;
      case TipoComponenteElectrico::Capacitor:
      case TipoComponenteElectrico::CapacitorPolarizado:
        break;  // circuito abierto en estado estacionario DC
      case TipoComponenteElectrico::Inductor:
      case TipoComponenteElectrico::Cable:
      case TipoComponenteElectrico::Tierra:
      case TipoComponenteElectrico::Interruptor:
      case TipoComponenteElectrico::Pulsador:
        if ((c.tipo != TipoComponenteElectrico::Interruptor &&
             c.tipo != TipoComponenteElectrico::Pulsador) ||
            c.auxiliar > 0.5)
          EstamparAdmitancia(&a, c.nodo_a, c.nodo_b, 1e9);
        break;
      case TipoComponenteElectrico::FuenteCorriente:
        EstamparCorriente(&z, c.nodo_a, c.nodo_b, c.valor);
        break;
      case TipoComponenteElectrico::FuenteVoltaje:
      case TipoComponenteElectrico::Bateria: {
        const int k = nodos + fuente++;
        if (c.nodo_a > 0) { a[c.nodo_a - 1][k] += 1; a[k][c.nodo_a - 1] += 1; }
        if (c.nodo_b > 0) { a[c.nodo_b - 1][k] -= 1; a[k][c.nodo_b - 1] -= 1; }
        z[k] = c.valor;
        break;
      }
      case TipoComponenteElectrico::ProbeVoltaje:
      case TipoComponenteElectrico::ProbeCorriente:
      case TipoComponenteElectrico::Voltimetro:
      case TipoComponenteElectrico::Osciloscopio:
        break;  // miden el circuito sin modificarlo
      default:
        // ValidarComponente impide llegar aquí con un tipo activo sin modelo.
        break;
    }
  }
  std::vector<double> x;
  if (!Gauss(a, z, &x)) {
    r.error = "Matriz singular: revise tierra, nodos flotantes y fuentes incompatibles";
    return r;
  }
  r.voltajes.assign(normalizado.maximo_original + 1, 0.0);
  for (int i = 0; i < nodos; ++i) {
    const int original = normalizado.nodo_original[i + 1];
    r.voltajes[original] = x[i];
  }
  r.corrientes_fuentes.assign(x.begin() + nodos, x.end());
  r.correcto = true;
  return r;
}

ResultadoAC ResolverAC(const std::vector<ComponenteElectrico>& componentes,
                       double frecuencia_hz) {
  ResultadoAC r;
  if (!(frecuencia_hz > 0.0) || !std::isfinite(frecuencia_hz)) {
    r.error = "Frecuencia AC inválida";
    return r;
  }
  for (const auto& c : componentes) {
    if (const std::string error = ValidarComponente(c, true); !error.empty()) {
      r.error = error;
      return r;
    }
  }
  using C = std::complex<double>;
  constexpr C j(0.0, 1.0);
  const double w = 2.0 * std::acos(-1.0) * frecuencia_hz;
  const CircuitoNormalizado normalizado = NormalizarNodos(componentes);
  const auto& circuito = normalizado.componentes;
  const int nodos = MaxNodo(circuito);
  const int fuentes = static_cast<int>(std::count_if(
      circuito.begin(), circuito.end(), EsFuenteVoltajeAC));
  const int n = nodos + fuentes;
  if (n == 0) { r.correcto = true; r.voltajes = {C{}}; return r; }
  std::vector<std::vector<C>> a(n, std::vector<C>(n, C{}));
  std::vector<C> z(n, C{});
  int fuente = 0;
  for (const auto& c : circuito) {
    if (!c.activo) continue;
    C y{};
    switch (c.tipo) {
      case TipoComponenteElectrico::Resistor:
        y = 1.0 / c.valor; EstamparAdmitancia(&a, c.nodo_a, c.nodo_b, y); break;
      case TipoComponenteElectrico::Capacitor:
      case TipoComponenteElectrico::CapacitorPolarizado:
        EstamparAdmitancia(&a, c.nodo_a, c.nodo_b, j * w * c.valor);
        break;
      case TipoComponenteElectrico::Inductor:
        EstamparAdmitancia(&a, c.nodo_a, c.nodo_b, 1.0 / (j * w * c.valor));
        break;
      case TipoComponenteElectrico::Cable:
      case TipoComponenteElectrico::Tierra:
        EstamparAdmitancia(&a, c.nodo_a, c.nodo_b, C(1e9, 0)); break;
      case TipoComponenteElectrico::Interruptor:
      case TipoComponenteElectrico::Pulsador:
        if (c.auxiliar > 0.5)
          EstamparAdmitancia(&a, c.nodo_a, c.nodo_b, C(1e9, 0));
        break;
      case TipoComponenteElectrico::FuenteCorriente: {
        const double fase = c.fase_grados * std::acos(-1.0) / 180.0;
        EstamparCorriente(&z, c.nodo_a, c.nodo_b, std::polar(c.valor, fase));
        break;
      }
      case TipoComponenteElectrico::FuenteVoltaje:
      case TipoComponenteElectrico::FuenteAC: {
        const int k = nodos + fuente++;
        if (c.nodo_a > 0) { a[c.nodo_a - 1][k] += 1.0; a[k][c.nodo_a - 1] += 1.0; }
        if (c.nodo_b > 0) { a[c.nodo_b - 1][k] -= 1.0; a[k][c.nodo_b - 1] -= 1.0; }
        z[k] = std::polar(c.valor, c.fase_grados * std::acos(-1.0) / 180.0);
        break;
      }
      case TipoComponenteElectrico::ProbeVoltaje:
      case TipoComponenteElectrico::ProbeCorriente:
      case TipoComponenteElectrico::Voltimetro:
      case TipoComponenteElectrico::Osciloscopio:
        break;  // miden el circuito sin modificarlo
      default:
        // ValidarComponente impide llegar aquí con un tipo activo sin modelo.
        break;
    }
  }
  std::vector<C> x;
  if (!Gauss(a, z, &x)) { r.error = "Matriz AC singular"; return r; }
  r.voltajes.assign(normalizado.maximo_original + 1, C{});
  for (int i = 0; i < nodos; ++i) {
    const int original = normalizado.nodo_original[i + 1];
    r.voltajes[original] = x[i];
  }
  r.corrientes_fuentes.assign(x.begin() + nodos, x.end());
  r.correcto = true;
  return r;
}

std::vector<PuntoBarridoAC> BarridoAC(
    const std::vector<ComponenteElectrico>& componentes, double inicio_hz,
    double fin_hz, int puntos_por_decada) {
  std::vector<PuntoBarridoAC> salida;
  if (inicio_hz <= 0 || fin_hz < inicio_hz || puntos_por_decada < 1) return salida;
  const double decadas = std::log10(fin_hz / inicio_hz);
  const int puntos = std::max(1, static_cast<int>(std::ceil(decadas * puntos_por_decada)));
  salida.reserve(puntos + 1);
  for (int i = 0; i <= puntos; ++i) {
    const double f = inicio_hz * std::pow(10.0, decadas * i / puntos);
    salida.push_back({f, ResolverAC(componentes, f)});
  }
  return salida;
}

std::vector<PuntoTransitorio> ResolverTransitorio(
    const std::vector<ComponenteElectrico>& componentes, double duracion_s,
    double paso_s, std::string* error) {
  std::vector<PuntoTransitorio> salida;
  if (duracion_s <= 0 || paso_s <= 0 || duracion_s / paso_s > 2'000'000) {
    if (error) *error = "Duración o paso transitorio inválido";
    return salida;
  }
  for (const auto& c : componentes) {
    if (const std::string motivo = ValidarComponente(c, false);
        !motivo.empty()) {
      if (error) *error = motivo;
      return salida;
    }
  }
  // Backward Euler para R/C/L con energía inicial nula. En t=0 un capacitor
  // descargado se comporta como un corto y un inductor sin corriente como un
  // circuito abierto. Las fuentes son constantes durante esta llamada.
  std::map<std::string, double> v_cap;
  std::map<std::string, double> i_ind;
  std::vector<ComponenteElectrico> iniciales;
  iniciales.reserve(componentes.size());
  for (const auto& c : componentes) {
    if (!c.activo) {
      iniciales.push_back(c);
    } else if (c.tipo == TipoComponenteElectrico::Capacitor ||
               c.tipo == TipoComponenteElectrico::CapacitorPolarizado) {
      auto corto = c;
      corto.tipo = TipoComponenteElectrico::Resistor;
      corto.valor = 1e-9;
      iniciales.push_back(std::move(corto));
    } else if (c.tipo == TipoComponenteElectrico::Inductor) {
      auto abierto = c;
      abierto.activo = false;
      iniciales.push_back(std::move(abierto));
    } else {
      iniciales.push_back(c);
    }
  }
  const ResultadoDC inicial = ResolverDC(iniciales);
  if (!inicial.correcto) {
    if (error) *error = "Condición inicial: " + inicial.error;
    return salida;
  }
  salida.push_back({0.0, inicial.voltajes});
  for (const auto& c : componentes) {
    const double va = c.nodo_a < static_cast<int>(inicial.voltajes.size())
                          ? inicial.voltajes[c.nodo_a]
                          : 0.0;
    const double vb = c.nodo_b < static_cast<int>(inicial.voltajes.size())
                          ? inicial.voltajes[c.nodo_b]
                          : 0.0;
    if (c.tipo == TipoComponenteElectrico::Capacitor ||
        c.tipo == TipoComponenteElectrico::CapacitorPolarizado)
      v_cap[c.id] = va - vb;
    if (c.tipo == TipoComponenteElectrico::Inductor) i_ind[c.id] = 0.0;
  }
  const int pasos = static_cast<int>(std::ceil(duracion_s / paso_s));
  double tiempo_anterior = 0.0;
  for (int paso = 1; paso <= pasos; ++paso) {
    const double tiempo = std::min(duracion_s, paso * paso_s);
    const double delta_t = tiempo - tiempo_anterior;
    if (!(delta_t > 0.0)) continue;
    std::vector<ComponenteElectrico> equivalentes;
    equivalentes.reserve(componentes.size() * 2);
    for (const auto& c : componentes) {
      if (!c.activo) {
        equivalentes.push_back(c);
      } else if ((c.tipo == TipoComponenteElectrico::Capacitor ||
                  c.tipo == TipoComponenteElectrico::CapacitorPolarizado) &&
                 c.valor > 0) {
        const double g = c.valor / delta_t;
        equivalentes.push_back({c.id + "_g", TipoComponenteElectrico::Resistor,
                                c.nodo_a, c.nodo_b, 0, 0, 1.0 / g});
        equivalentes.push_back({c.id + "_i", TipoComponenteElectrico::FuenteCorriente,
                                c.nodo_a, c.nodo_b, 0, 0, -g * v_cap[c.id]});
      } else if (c.tipo == TipoComponenteElectrico::Inductor && c.valor > 0) {
        const double g = delta_t / c.valor;
        equivalentes.push_back({c.id + "_g", TipoComponenteElectrico::Resistor,
                                c.nodo_a, c.nodo_b, 0, 0, 1.0 / g});
        equivalentes.push_back({c.id + "_i", TipoComponenteElectrico::FuenteCorriente,
                                c.nodo_a, c.nodo_b, 0, 0, i_ind[c.id]});
      } else {
        equivalentes.push_back(c);
      }
    }
    ResultadoDC dc = ResolverDC(equivalentes);
    if (!dc.correcto) { if (error) *error = dc.error; return {}; }
    salida.push_back({tiempo, dc.voltajes});
    for (const auto& c : componentes) {
      const double va = c.nodo_a < static_cast<int>(dc.voltajes.size()) ? dc.voltajes[c.nodo_a] : 0;
      const double vb = c.nodo_b < static_cast<int>(dc.voltajes.size()) ? dc.voltajes[c.nodo_b] : 0;
      if (c.tipo == TipoComponenteElectrico::Capacitor ||
          c.tipo == TipoComponenteElectrico::CapacitorPolarizado)
        v_cap[c.id] = va - vb;
      if (c.tipo == TipoComponenteElectrico::Inductor && c.valor > 0)
        i_ind[c.id] += delta_t / c.valor * (va - vb);
    }
    tiempo_anterior = tiempo;
  }
  return salida;
}

EquivalentePuerto CalcularEquivalente(
    const std::vector<ComponenteElectrico>& componentes, int nodo_a, int nodo_b) {
  EquivalentePuerto e;
  if (nodo_a == nodo_b || nodo_a < 0 || nodo_b < 0) {
    e.error = "Puerto inválido"; return e;
  }
  const ResultadoDC abierto = ResolverDC(componentes);
  if (!abierto.correcto || nodo_a >= static_cast<int>(abierto.voltajes.size()) ||
      nodo_b >= static_cast<int>(abierto.voltajes.size())) {
    e.error = abierto.error.empty() ? "Nodo fuera de rango" : abierto.error; return e;
  }
  e.voltaje_thevenin = abierto.voltajes[nodo_a] - abierto.voltajes[nodo_b];
  std::vector<ComponenteElectrico> prueba = componentes;
  for (auto& c : prueba) {
    if (c.tipo == TipoComponenteElectrico::FuenteVoltaje ||
        c.tipo == TipoComponenteElectrico::Bateria) {
      c.tipo = TipoComponenteElectrico::Resistor; c.valor = 1e-9;
    } else if (c.tipo == TipoComponenteElectrico::FuenteCorriente) c.activo = false;
  }
  prueba.push_back({"I_prueba", TipoComponenteElectrico::FuenteCorriente,
                    nodo_b, nodo_a, 0, 0, 1.0});
  const ResultadoDC excitado = ResolverDC(prueba);
  if (!excitado.correcto) { e.error = excitado.error; return e; }
  e.resistencia_thevenin = excitado.voltajes[nodo_a] - excitado.voltajes[nodo_b];
  if (std::abs(e.resistencia_thevenin) > 1e-15)
    e.corriente_norton = e.voltaje_thevenin / e.resistencia_thevenin;
  e.correcto = std::isfinite(e.resistencia_thevenin);
  return e;
}

double DivisorVoltaje(double entrada_v, double r1, double r2) {
  return (r1 >= 0 && r2 >= 0 && r1 + r2 > 0) ? entrada_v * r2 / (r1 + r2)
                                               : std::numeric_limits<double>::quiet_NaN();
}
double GananciaOpAmpNoInversor(double rf, double rg) {
  return rg > 0 ? 1.0 + rf / rg : std::numeric_limits<double>::quiet_NaN();
}
double GananciaOpAmpInversor(double ri, double rf) {
  return ri > 0 ? -rf / ri : std::numeric_limits<double>::quiet_NaN();
}
double FrecuenciaCorteRC(double r, double c) {
  return r > 0 && c > 0 ? 1.0 / (2 * std::acos(-1.0) * r * c)
                         : std::numeric_limits<double>::quiet_NaN();
}
double FrecuenciaResonanciaLC(double l, double c) {
  return l > 0 && c > 0 ? 1.0 / (2 * std::acos(-1.0) * std::sqrt(l * c))
                         : std::numeric_limits<double>::quiet_NaN();
}
double ConstanteTiempoRC(double r, double c) { return r * c; }
double ADecibelios(double relacion, bool potencia) {
  return relacion > 0 ? (potencia ? 10.0 : 20.0) * std::log10(relacion)
                       : -std::numeric_limits<double>::infinity();
}
double DesdeDecibelios(double db, bool potencia) {
  return std::pow(10.0, db / (potencia ? 10.0 : 20.0));
}

std::string NotacionIngenieria(double valor, const std::string& unidad, int cifras) {
  static const std::map<int, std::string> prefijos = {
      {-12, "p"}, {-9, "n"}, {-6, "u"}, {-3, "m"}, {0, ""},
      {3, "k"}, {6, "M"}, {9, "G"}, {12, "T"}};
  if (valor == 0) return "0 " + unidad;
  int exponente = static_cast<int>(std::floor(std::log10(std::abs(valor)) / 3.0)) * 3;
  exponente = std::clamp(exponente, -12, 12);
  std::ostringstream s;
  s << std::setprecision(std::clamp(cifras, 1, 12)) << valor / std::pow(10.0, exponente)
    << " " << prefijos.at(exponente) << unidad;
  return s.str();
}

CodigoResistor DecodificarResistorColores(const std::vector<int>& b) {
  CodigoResistor r;
  if (b.size() < 4 || b.size() > 6) { r.error = "Se requieren 4, 5 o 6 bandas"; return r; }
  const int digitos = b.size() >= 5 ? 3 : 2;
  int valor = 0;
  for (int i = 0; i < digitos; ++i) {
    if (b[i] < 0 || b[i] > 9) { r.error = "Color de dígito inválido"; return r; }
    valor = valor * 10 + b[i];
  }
  const int multiplicador = b[digitos];
  if (multiplicador < -2 || multiplicador > 9) { r.error = "Multiplicador inválido"; return r; }
  static const std::map<int, double> tolerancias = {
      {1, 1}, {2, 2}, {5, 0.5}, {6, 0.25}, {7, 0.1}, {8, 0.05}, {10, 5}, {11, 10}};
  const int t = b[digitos + 1];
  auto it = tolerancias.find(t);
  if (it == tolerancias.end()) { r.error = "Tolerancia inválida"; return r; }
  r.ohmios = valor * std::pow(10.0, multiplicador);
  r.tolerancia_porcentaje = it->second;
  r.valido = true;
  return r;
}

CodigoResistor DecodificarSMD(const std::string& codigo) {
  CodigoResistor r;
  if (codigo.size() == 3 && std::all_of(codigo.begin(), codigo.end(), ::isdigit)) {
    r.ohmios = std::stoi(codigo.substr(0, 2)) * std::pow(10.0, codigo[2] - '0');
    r.tolerancia_porcentaje = 5; r.valido = true; return r;
  }
  const auto pos = codigo.find_first_of("Rr");
  if (pos != std::string::npos) {
    std::string decimal = codigo; decimal[pos] = '.';
    try { r.ohmios = std::stod(decimal); r.tolerancia_porcentaje = 1; r.valido = true; }
    catch (...) { r.error = "Código SMD inválido"; }
    return r;
  }
  r.error = "Código SMD no reconocido";
  return r;
}

}  // namespace tinta
