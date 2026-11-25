#include "matematicas_avanzadas.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstring>
#include <limits>

namespace tinta {
namespace {

using C = std::complex<double>;

class Parser {
 public:
  Parser(const std::string& texto, const std::map<std::string, C>& variables)
      : texto_(texto), variables_(variables) {}

  ResultadoExpresion Evaluar() {
    ResultadoExpresion r;
    C valor = Expresion();
    Espacios();
    if (error_.empty() && pos_ != texto_.size())
      Error("carácter inesperado en posición " + std::to_string(pos_ + 1));
    r.correcto = error_.empty() && std::isfinite(valor.real()) &&
                 std::isfinite(valor.imag());
    r.valor = valor;
    r.error = error_.empty() ? (r.correcto ? "" : "resultado no finito") : error_;
    return r;
  }

 private:
  C Expresion() {
    C valor = Termino();
    while (error_.empty()) {
      Espacios();
      if (Tomar('+')) valor += Termino();
      else if (Tomar('-')) valor -= Termino();
      else if (TomarCadena("||")) {
        const C otro = Termino();
        const C suma = valor + otro;
        if (std::abs(suma) < 1e-300) {
          Error("paralelo eléctrico indefinido: la suma es cero");
          return {};
        }
        valor = valor * otro / suma;
      }
      else break;
    }
    return valor;
  }

  C Termino() {
    C valor = Potencia();
    while (error_.empty()) {
      Espacios();
      if (Tomar('*')) valor *= Potencia();
      else if (Tomar('/')) {
        const C divisor = Potencia();
        if (std::abs(divisor) < 1e-300) { Error("división por cero"); return {}; }
        valor /= divisor;
      } else if (SiguePrimario()) {
        // Multiplicación implícita: 2pi, 3j, 5(x+1).
        valor *= Potencia();
      } else break;
    }
    return valor;
  }

  C Potencia() {
    C base = Unario();
    Espacios();
    if (Tomar('^')) base = std::pow(base, Potencia());  // asociativa a la derecha
    return base;
  }

  C Unario() {
    Espacios();
    if (Tomar('+')) return Unario();
    if (Tomar('-')) return -Unario();
    return Primario();
  }

  C Primario() {
    Espacios();
    if (Tomar('(')) {
      C valor = Expresion();
      if (!Tomar(')')) Error("falta ')' ");
      return valor;
    }
    if (pos_ < texto_.size() &&
        (std::isdigit(static_cast<unsigned char>(texto_[pos_])) || texto_[pos_] == '.'))
      return Numero();
    if (pos_ < texto_.size() &&
        (std::isalpha(static_cast<unsigned char>(texto_[pos_])) || texto_[pos_] == '_')) {
      const std::string nombre = Identificador();
      Espacios();
      if (Tomar('(')) {
        std::vector<C> args;
        if (!Mirar(')')) {
          do { args.push_back(Expresion()); } while (Tomar(','));
        }
        if (!Tomar(')')) { Error("falta ')' en función " + nombre); return {}; }
        return Funcion(nombre, args);
      }
      if (nombre == "pi") return std::acos(-1.0);
      if (nombre == "e") return std::exp(1.0);
      if (nombre == "i" || nombre == "j") return C(0, 1);
      auto it = variables_.find(nombre);
      if (it != variables_.end()) return it->second;
      Error("variable desconocida: " + nombre);
      return {};
    }
    Error("se esperaba un número, variable o paréntesis");
    return {};
  }

  C Numero() {
    const size_t inicio = pos_;
    bool exponente = false;
    while (pos_ < texto_.size()) {
      const char c = texto_[pos_];
      if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') ++pos_;
      else if ((c == 'e' || c == 'E') && !exponente) {
        exponente = true; ++pos_;
        if (pos_ < texto_.size() && (texto_[pos_] == '+' || texto_[pos_] == '-')) ++pos_;
      } else break;
    }
    double valor = 0;
    try { valor = std::stod(texto_.substr(inicio, pos_ - inicio)); }
    catch (...) { Error("número inválido"); return {}; }
    // Prefijos de ingeniería pegados al número: 10k, 4.7u, 22n.
    if (pos_ < texto_.size()) {
      const char p = texto_[pos_];
      const std::map<char, int> prefijos = {
          {'p', -12}, {'n', -9}, {'u', -6}, {'m', -3},
          {'k', 3}, {'K', 3}, {'M', 6}, {'G', 9}, {'T', 12}};
      auto it = prefijos.find(p);
      if (it != prefijos.end()) { valor *= std::pow(10.0, it->second); ++pos_; }
    }
    return valor;
  }

  C Funcion(const std::string& n, const std::vector<C>& a) {
    if (n == "sin" && a.size() == 1) return std::sin(a[0]);
    if (n == "cos" && a.size() == 1) return std::cos(a[0]);
    if (n == "tan" && a.size() == 1) return std::tan(a[0]);
    if (n == "asin" && a.size() == 1) return std::asin(a[0]);
    if (n == "acos" && a.size() == 1) return std::acos(a[0]);
    if (n == "atan" && a.size() == 1) return std::atan(a[0]);
    if (n == "atan2" && a.size() == 2 && a[0].imag() == 0 && a[1].imag() == 0)
      return std::atan2(a[0].real(), a[1].real());
    if (n == "sinh" && a.size() == 1) return std::sinh(a[0]);
    if (n == "cosh" && a.size() == 1) return std::cosh(a[0]);
    if (n == "sqrt" && a.size() == 1) return std::sqrt(a[0]);
    if ((n == "ln" || n == "log") && a.size() == 1) return std::log(a[0]);
    if (n == "log10" && a.size() == 1) return std::log10(a[0]);
    if (n == "exp" && a.size() == 1) return std::exp(a[0]);
    if (n == "floor" && a.size() == 1 && a[0].imag() == 0) return std::floor(a[0].real());
    if (n == "ceil" && a.size() == 1 && a[0].imag() == 0) return std::ceil(a[0].real());
    if (n == "round" && a.size() == 1 && a[0].imag() == 0) return std::round(a[0].real());
    if (n == "deg" && a.size() == 1) return a[0] * 180.0 / std::acos(-1.0);
    if (n == "rad" && a.size() == 1) return a[0] * std::acos(-1.0) / 180.0;
    if (n == "abs" && a.size() == 1) return std::abs(a[0]);
    if (n == "arg" && a.size() == 1) return std::arg(a[0]);
    if (n == "conj" && a.size() == 1) return std::conj(a[0]);
    if (n == "real" && a.size() == 1) return a[0].real();
    if (n == "imag" && a.size() == 1) return a[0].imag();
    if (n == "min" && a.size() == 2 && a[0].imag() == 0 && a[1].imag() == 0)
      return std::min(a[0].real(), a[1].real());
    if (n == "max" && a.size() == 2 && a[0].imag() == 0 && a[1].imag() == 0)
      return std::max(a[0].real(), a[1].real());
    Error("función desconocida o argumentos incorrectos: " + n);
    return {};
  }

  std::string Identificador() {
    const size_t inicio = pos_++;
    while (pos_ < texto_.size() &&
           (std::isalnum(static_cast<unsigned char>(texto_[pos_])) || texto_[pos_] == '_')) ++pos_;
    std::string n = texto_.substr(inicio, pos_ - inicio);
    std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return std::tolower(c); });
    return n;
  }
  bool Tomar(char c) { Espacios(); if (pos_ < texto_.size() && texto_[pos_] == c) { ++pos_; return true; } return false; }
  bool TomarCadena(const char* cadena) {
    Espacios();
    const size_t n = std::strlen(cadena);
    if (texto_.compare(pos_, n, cadena) != 0) return false;
    pos_ += n; return true;
  }
  bool Mirar(char c) { Espacios(); return pos_ < texto_.size() && texto_[pos_] == c; }
  bool SiguePrimario() {
    Espacios();
    if (pos_ >= texto_.size()) return false;
    const unsigned char c = static_cast<unsigned char>(texto_[pos_]);
    return texto_[pos_] == '(' || texto_[pos_] == '_' || std::isalpha(c) ||
           std::isdigit(c) || texto_[pos_] == '.';
  }
  void Espacios() { while (pos_ < texto_.size() && std::isspace(static_cast<unsigned char>(texto_[pos_]))) ++pos_; }
  void Error(const std::string& e) { if (error_.empty()) error_ = e; }

  const std::string& texto_;
  const std::map<std::string, C>& variables_;
  size_t pos_ = 0;
  std::string error_;
};

struct Unidad {
  std::array<int, 7> d{};  // L, M, T, I, Θ, N, J
  double escala = 1.0;
  double offset = 0.0;     // valor SI = (valor + offset) * escala
};

const std::map<std::string, Unidad>& Unidades() {
  static const std::map<std::string, Unidad> u = {
    {"m", {{1,0,0,0,0,0,0}, 1}}, {"cm", {{1,0,0,0,0,0,0}, 1e-2}},
    {"mm", {{1,0,0,0,0,0,0}, 1e-3}}, {"km", {{1,0,0,0,0,0,0}, 1e3}},
    {"in", {{1,0,0,0,0,0,0}, 0.0254}}, {"ft", {{1,0,0,0,0,0,0}, 0.3048}},
    {"yd", {{1,0,0,0,0,0,0}, 0.9144}}, {"mi", {{1,0,0,0,0,0,0}, 1609.344}},
    {"m2", {{2,0,0,0,0,0,0}, 1}}, {"cm2", {{2,0,0,0,0,0,0}, 1e-4}},
    {"mm2", {{2,0,0,0,0,0,0}, 1e-6}}, {"km2", {{2,0,0,0,0,0,0}, 1e6}},
    {"in2", {{2,0,0,0,0,0,0}, 0.00064516}}, {"ft2", {{2,0,0,0,0,0,0}, 0.09290304}},
    {"m3", {{3,0,0,0,0,0,0}, 1}}, {"cm3", {{3,0,0,0,0,0,0}, 1e-6}},
    {"l", {{3,0,0,0,0,0,0}, 1e-3}}, {"ml", {{3,0,0,0,0,0,0}, 1e-6}},
    {"gal_us", {{3,0,0,0,0,0,0}, 0.003785411784}},
    {"kg", {{0,1,0,0,0,0,0}, 1}}, {"g", {{0,1,0,0,0,0,0}, 1e-3}},
    {"mg", {{0,1,0,0,0,0,0}, 1e-6}}, {"lb", {{0,1,0,0,0,0,0}, 0.45359237}},
    {"s", {{0,0,1,0,0,0,0}, 1}}, {"ms", {{0,0,1,0,0,0,0}, 1e-3}},
    {"min", {{0,0,1,0,0,0,0}, 60}}, {"h", {{0,0,1,0,0,0,0}, 3600}},
    {"m/s", {{1,0,-1,0,0,0,0}, 1}}, {"km/h", {{1,0,-1,0,0,0,0}, 1.0/3.6}},
    {"mph", {{1,0,-1,0,0,0,0}, 0.44704}},
    {"a", {{0,0,0,1,0,0,0}, 1}}, {"ma", {{0,0,0,1,0,0,0}, 1e-3}},
    {"ua", {{0,0,0,1,0,0,0}, 1e-6}}, {"ka", {{0,0,0,1,0,0,0}, 1e3}},
    {"k", {{0,0,0,0,1,0,0}, 1, 0}},
    {"c", {{0,0,0,0,1,0,0}, 1, 273.15}},
    {"degc", {{0,0,0,0,1,0,0}, 1, 273.15}},
    {"celsius", {{0,0,0,0,1,0,0}, 1, 273.15}},
    {"degf", {{0,0,0,0,1,0,0}, 5.0/9.0, 459.67}},
    {"fahrenheit", {{0,0,0,0,1,0,0}, 5.0/9.0, 459.67}},
    {"hz", {{0,0,-1,0,0,0,0}, 1}}, {"khz", {{0,0,-1,0,0,0,0}, 1e3}},
    {"mhz", {{0,0,-1,0,0,0,0}, 1e6}},
    {"n", {{1,1,-2,0,0,0,0}, 1}}, {"kn", {{1,1,-2,0,0,0,0}, 1e3}},
    {"lbf", {{1,1,-2,0,0,0,0}, 4.4482216152605}},
    {"pa", {{-1,1,-2,0,0,0,0}, 1}}, {"kpa", {{-1,1,-2,0,0,0,0}, 1e3}},
    {"mpa", {{-1,1,-2,0,0,0,0}, 1e6}}, {"bar", {{-1,1,-2,0,0,0,0}, 1e5}},
    {"psi", {{-1,1,-2,0,0,0,0}, 6894.757293168}},
    {"j", {{2,1,-2,0,0,0,0}, 1}}, {"kj", {{2,1,-2,0,0,0,0}, 1e3}},
    {"wh", {{2,1,-2,0,0,0,0}, 3600}}, {"kwh", {{2,1,-2,0,0,0,0}, 3.6e6}},
    {"cal", {{2,1,-2,0,0,0,0}, 4.184}},
    {"w", {{2,1,-3,0,0,0,0}, 1}}, {"kw", {{2,1,-3,0,0,0,0}, 1e3}},
    {"mwatt", {{2,1,-3,0,0,0,0}, 1e6}}, {"hp", {{2,1,-3,0,0,0,0}, 745.699871582}},
    {"coulomb", {{0,0,1,1,0,0,0}, 1}}, {"ah", {{0,0,1,1,0,0,0}, 3600}},
    {"mah", {{0,0,1,1,0,0,0}, 3.6}},
    {"v", {{2,1,-3,-1,0,0,0}, 1}}, {"mv", {{2,1,-3,-1,0,0,0}, 1e-3}},
    {"uv", {{2,1,-3,-1,0,0,0}, 1e-6}}, {"kv", {{2,1,-3,-1,0,0,0}, 1e3}},
    {"ohm", {{2,1,-3,-2,0,0,0}, 1}}, {"kohm", {{2,1,-3,-2,0,0,0}, 1e3}},
    {"mohm", {{2,1,-3,-2,0,0,0}, 1e6}},
    {"f", {{-2,-1,4,2,0,0,0}, 1}},
    {"farad", {{-2,-1,4,2,0,0,0}, 1}}, {"uf", {{-2,-1,4,2,0,0,0}, 1e-6}},
    {"nf", {{-2,-1,4,2,0,0,0}, 1e-9}}, {"pf", {{-2,-1,4,2,0,0,0}, 1e-12}},
    {"hry", {{2,1,-2,-2,0,0,0}, 1}}, {"henry", {{2,1,-2,-2,0,0,0}, 1}},
    {"mh", {{2,1,-2,-2,0,0,0}, 1e-3}}, {"uh", {{2,1,-2,-2,0,0,0}, 1e-6}},
    {"rad", {{}, 1}}, {"deg", {{}, std::acos(-1.0)/180.0}},
  };
  return u;
}

std::string Minusculas(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
  return s;
}

}  // namespace

ResultadoExpresion EvaluarExpresion(
    const std::string& expresion, const std::map<std::string, C>& variables) {
  return Parser(expresion, variables).Evaluar();
}

std::vector<MuestraFuncion> MuestrearFuncion(
    const std::string& expresion, const std::string& variable,
    double inicio, double fin, size_t cantidad,
    const std::map<std::string, C>& variables) {
  std::vector<MuestraFuncion> salida;
  if (cantidad < 2 || !std::isfinite(inicio) || !std::isfinite(fin)) return salida;
  salida.reserve(cantidad);
  for (size_t i = 0; i < cantidad; ++i) {
    const double x = inicio + (fin - inicio) * i / static_cast<double>(cantidad - 1);
    auto vars = variables; vars[Minusculas(variable)] = x;
    const auto r = EvaluarExpresion(expresion, vars);
    salida.push_back({x, r.valor, r.correcto});
  }
  return salida;
}

ResultadoExpresion DerivadaNumerica(
    const std::string& expresion, const std::string& variable, double x,
    const std::map<std::string, C>& variables) {
  const double h = std::max(1e-7, std::abs(x) * 1e-6);
  auto a = variables, b = variables;
  a[Minusculas(variable)] = x + h; b[Minusculas(variable)] = x - h;
  const auto ra = EvaluarExpresion(expresion, a), rb = EvaluarExpresion(expresion, b);
  if (!ra.correcto) return ra;
  if (!rb.correcto) return rb;
  return {true, (ra.valor - rb.valor) / (2 * h), ""};
}

ResultadoExpresion IntegralNumerica(
    const std::string& expresion, const std::string& variable,
    double inicio, double fin, int n, const std::map<std::string, C>& variables) {
  if (n < 2) return {false, {}, "se requieren al menos dos subintervalos"};
  if (n % 2) ++n;
  const double h = (fin - inicio) / n;
  C suma{};
  for (int i = 0; i <= n; ++i) {
    auto vars = variables; vars[Minusculas(variable)] = inicio + i * h;
    const auto r = EvaluarExpresion(expresion, vars);
    if (!r.correcto) return r;
    suma += r.valor * (i == 0 || i == n ? 1.0 : (i % 2 ? 4.0 : 2.0));
  }
  return {true, suma * h / 3.0, ""};
}

ResultadoConversion ConvertirUnidad(double valor, const std::string& origen,
                                    const std::string& destino) {
  ResultadoConversion r;
  const auto& u = Unidades();
  const auto a = u.find(Minusculas(origen)), b = u.find(Minusculas(destino));
  if (a == u.end() || b == u.end()) { r.error = "unidad desconocida"; return r; }
  if (a->second.d != b->second.d) { r.error = "dimensiones incompatibles"; return r; }
  const double si = (valor + a->second.offset) * a->second.escala;
  r.valor = si / b->second.escala - b->second.offset;
  r.correcto = std::isfinite(r.valor);
  if (!r.correcto) r.error = "resultado no finito";
  return r;
}

ResultadoExpresion DeterminanteMatriz(const std::vector<C>& matriz,
                                      size_t orden) {
  if (orden == 0 || orden > 64 || matriz.size() != orden * orden)
    return {false, {}, "dimensiones de matriz inválidas"};
  std::vector<C> a = matriz;
  C determinante = 1.0;
  int signo = 1;
  for (size_t columna = 0; columna < orden; ++columna) {
    size_t pivote = columna;
    for (size_t fila = columna + 1; fila < orden; ++fila)
      if (std::abs(a[fila * orden + columna]) >
          std::abs(a[pivote * orden + columna])) pivote = fila;
    if (std::abs(a[pivote * orden + columna]) < 1e-14)
      return {true, C{}, ""};
    if (pivote != columna) {
      for (size_t j = 0; j < orden; ++j)
        std::swap(a[columna * orden + j], a[pivote * orden + j]);
      signo = -signo;
    }
    const C diagonal = a[columna * orden + columna];
    determinante *= diagonal;
    for (size_t fila = columna + 1; fila < orden; ++fila) {
      const C factor = a[fila * orden + columna] / diagonal;
      for (size_t j = columna + 1; j < orden; ++j)
        a[fila * orden + j] -= factor * a[columna * orden + j];
    }
  }
  return {true, determinante * static_cast<double>(signo), ""};
}

ResultadoMatriz InvertirMatriz(const std::vector<C>& matriz, size_t orden) {
  ResultadoMatriz r;
  if (orden == 0 || orden > 64 || matriz.size() != orden * orden) {
    r.error = "dimensiones de matriz inválidas"; return r;
  }
  const size_t ancho = orden * 2;
  std::vector<C> a(orden * ancho);
  for (size_t i = 0; i < orden; ++i) {
    for (size_t j = 0; j < orden; ++j) a[i * ancho + j] = matriz[i * orden + j];
    a[i * ancho + orden + i] = 1.0;
  }
  for (size_t columna = 0; columna < orden; ++columna) {
    size_t pivote = columna;
    for (size_t fila = columna + 1; fila < orden; ++fila)
      if (std::abs(a[fila * ancho + columna]) >
          std::abs(a[pivote * ancho + columna])) pivote = fila;
    if (std::abs(a[pivote * ancho + columna]) < 1e-14) {
      r.error = "la matriz es singular"; return r;
    }
    if (pivote != columna)
      for (size_t j = 0; j < ancho; ++j)
        std::swap(a[columna * ancho + j], a[pivote * ancho + j]);
    const C divisor = a[columna * ancho + columna];
    for (size_t j = 0; j < ancho; ++j) a[columna * ancho + j] /= divisor;
    for (size_t fila = 0; fila < orden; ++fila) {
      if (fila == columna) continue;
      const C factor = a[fila * ancho + columna];
      for (size_t j = 0; j < ancho; ++j)
        a[fila * ancho + j] -= factor * a[columna * ancho + j];
    }
  }
  r.correcto = true; r.filas = orden; r.columnas = orden;
  r.valores.resize(orden * orden);
  for (size_t i = 0; i < orden; ++i)
    for (size_t j = 0; j < orden; ++j)
      r.valores[i * orden + j] = a[i * ancho + orden + j];
  return r;
}

ResultadoMatriz ResolverSistemaLineal(const std::vector<C>& coeficientes,
                                      const std::vector<C>& terminos,
                                      size_t orden) {
  ResultadoMatriz r;
  if (orden == 0 || orden > 64 || coeficientes.size() != orden * orden ||
      terminos.size() != orden) {
    r.error = "dimensiones del sistema inválidas"; return r;
  }
  const size_t ancho = orden + 1;
  std::vector<C> a(orden * ancho);
  for (size_t i = 0; i < orden; ++i) {
    for (size_t j = 0; j < orden; ++j)
      a[i * ancho + j] = coeficientes[i * orden + j];
    a[i * ancho + orden] = terminos[i];
  }
  for (size_t columna = 0; columna < orden; ++columna) {
    size_t pivote = columna;
    for (size_t fila = columna + 1; fila < orden; ++fila)
      if (std::abs(a[fila * ancho + columna]) >
          std::abs(a[pivote * ancho + columna])) pivote = fila;
    if (std::abs(a[pivote * ancho + columna]) < 1e-14) {
      r.error = "sistema singular o indeterminado"; return r;
    }
    if (pivote != columna)
      for (size_t j = columna; j < ancho; ++j)
        std::swap(a[columna * ancho + j], a[pivote * ancho + j]);
    const C divisor = a[columna * ancho + columna];
    for (size_t j = columna; j < ancho; ++j) a[columna * ancho + j] /= divisor;
    for (size_t fila = 0; fila < orden; ++fila) {
      if (fila == columna) continue;
      const C factor = a[fila * ancho + columna];
      for (size_t j = columna; j < ancho; ++j)
        a[fila * ancho + j] -= factor * a[columna * ancho + j];
    }
  }
  r.correcto = true; r.filas = orden; r.columnas = 1; r.valores.resize(orden);
  for (size_t i = 0; i < orden; ++i) r.valores[i] = a[i * ancho + orden];
  return r;
}

ResultadoExpresion ProductoPunto(const std::vector<C>& a,
                                 const std::vector<C>& b) {
  if (a.empty() || a.size() != b.size())
    return {false, {}, "los vectores deben tener la misma dimensión"};
  C suma{};
  for (size_t i = 0; i < a.size(); ++i) suma += std::conj(a[i]) * b[i];
  return {true, suma, ""};
}

ResultadoMatriz ProductoCruz(const std::vector<C>& a,
                             const std::vector<C>& b) {
  ResultadoMatriz r;
  if (a.size() != 3 || b.size() != 3) {
    r.error = "el producto cruz requiere vectores de tres componentes"; return r;
  }
  r.correcto = true; r.filas = 3; r.columnas = 1;
  r.valores = {a[1] * b[2] - a[2] * b[1],
               a[2] * b[0] - a[0] * b[2],
               a[0] * b[1] - a[1] * b[0]};
  return r;
}

ResultadoExpresion BuscarRaiz(const std::string& expresion,
                              const std::string& variable, double estimacion,
                              const std::map<std::string, C>& variables) {
  C x = estimacion;
  for (int iteracion = 0; iteracion < 80; ++iteracion) {
    auto vars = variables; vars[Minusculas(variable)] = x;
    const auto f = EvaluarExpresion(expresion, vars);
    if (!f.correcto) return f;
    if (std::abs(f.valor) < 1e-11) return {true, x, ""};
    const double h = std::max(1e-7, std::abs(x) * 1e-6);
    auto va = variables, vb = variables;
    va[Minusculas(variable)] = x + h; vb[Minusculas(variable)] = x - h;
    const auto fa = EvaluarExpresion(expresion, va);
    const auto fb = EvaluarExpresion(expresion, vb);
    if (!fa.correcto) return fa;
    if (!fb.correcto) return fb;
    const C derivada = (fa.valor - fb.valor) / (2.0 * h);
    if (std::abs(derivada) < 1e-14)
      return {false, {}, "derivada casi nula; pruebe otra estimación"};
    const C siguiente = x - f.valor / derivada;
    if (std::abs(siguiente - x) < 1e-12 * (1.0 + std::abs(x)))
      return {true, siguiente, ""};
    x = siguiente;
  }
  return {false, {}, "la búsqueda de raíz no convergió"};
}

ResultadoExpresion CalcularLimite(const std::string& expresion,
                                  const std::string& variable, double punto,
                                  int lado,
                                  const std::map<std::string, C>& variables) {
  auto aproximar = [&](double signo) -> ResultadoExpresion {
    C anterior{}; bool hay_anterior = false;
    for (int k = 1; k <= 12; ++k) {
      const double h = std::max(1.0, std::abs(punto)) * std::pow(10.0, -k);
      auto vars = variables; vars[Minusculas(variable)] = punto + signo * h;
      const auto r = EvaluarExpresion(expresion, vars);
      if (!r.correcto) continue;
      if (hay_anterior && std::abs(r.valor - anterior) < 1e-7 * (1 + std::abs(r.valor)))
        return r;
      anterior = r.valor; hay_anterior = true;
    }
    return hay_anterior ? ResultadoExpresion{true, anterior, ""}
                        : ResultadoExpresion{false, {}, "no hubo aproximaciones válidas"};
  };
  if (lado < 0) return aproximar(-1);
  if (lado > 0) return aproximar(1);
  const auto izquierda = aproximar(-1), derecha = aproximar(1);
  if (!izquierda.correcto) return izquierda;
  if (!derecha.correcto) return derecha;
  if (std::abs(izquierda.valor - derecha.valor) >
      1e-5 * (1 + std::max(std::abs(izquierda.valor), std::abs(derecha.valor))))
    return {false, {}, "los límites laterales no coinciden"};
  return {true, (izquierda.valor + derecha.valor) / 2.0, ""};
}

ResultadoExpresion CalcularSumatoria(const std::string& expresion,
                                     const std::string& variable,
                                     int64_t inicio, int64_t fin,
                                     const std::map<std::string, C>& variables) {
  if (fin < inicio) return {false, {}, "el límite final es menor que el inicial"};
  if (fin - inicio > 1'000'000)
    return {false, {}, "la sumatoria excede un millón de términos"};
  C suma{};
  for (int64_t i = inicio; i <= fin; ++i) {
    auto vars = variables; vars[Minusculas(variable)] = static_cast<double>(i);
    const auto r = EvaluarExpresion(expresion, vars);
    if (!r.correcto) return r;
    suma += r.valor;
  }
  return {true, suma, ""};
}

std::vector<C> TransformadaFourierDiscreta(const std::vector<C>& muestras,
                                           bool inversa) {
  const size_t n = muestras.size();
  std::vector<C> salida(n);
  const double signo = inversa ? 1.0 : -1.0;
  const double dos_pi = 2.0 * std::acos(-1.0);
  for (size_t k = 0; k < n; ++k) {
    for (size_t t = 0; t < n; ++t) {
      const double angulo = signo * dos_pi * static_cast<double>(k * t) /
                            static_cast<double>(n);
      salida[k] += muestras[t] * std::exp(C(0.0, angulo));
    }
    if (inversa && n) salida[k] /= static_cast<double>(n);
  }
  return salida;
}

ResultadoExpresion TransformadaLaplaceNumerica(
    const std::string& expresion, const std::string& variable, C s,
    double tiempo_maximo, int pasos, const std::map<std::string, C>& variables) {
  if (!(tiempo_maximo > 0) || pasos < 2)
    return {false, {}, "intervalo o cantidad de pasos inválida"};
  if (pasos % 2) ++pasos;
  const double h = tiempo_maximo / pasos;
  C suma{};
  for (int i = 0; i <= pasos; ++i) {
    const double t = i * h;
    auto vars = variables; vars[Minusculas(variable)] = t;
    const auto r = EvaluarExpresion(expresion, vars);
    if (!r.correcto) return r;
    const double peso = i == 0 || i == pasos ? 1.0 : (i % 2 ? 4.0 : 2.0);
    suma += peso * r.valor * std::exp(-s * t);
  }
  return {true, suma * h / 3.0, ""};
}

ResultadoEDO IntegrarEDOPrimerOrden(
    const std::string& derivada, double t0, C y0, double t1, size_t pasos,
    const std::map<std::string, C>& variables) {
  ResultadoEDO salida;
  if (pasos == 0 || pasos > 1'000'000 || !std::isfinite(t0) || !std::isfinite(t1)) {
    salida.error = "intervalo o pasos inválidos"; return salida;
  }
  const double h = (t1 - t0) / static_cast<double>(pasos);
  auto f = [&](double t, C y) -> ResultadoExpresion {
    auto vars = variables; vars["t"] = t; vars["y"] = y;
    return EvaluarExpresion(derivada, vars);
  };
  double t = t0; C y = y0;
  salida.muestras.reserve(pasos + 1); salida.muestras.push_back({t, y});
  for (size_t i = 0; i < pasos; ++i) {
    const auto k1 = f(t, y); if (!k1.correcto) { salida.error = k1.error; return salida; }
    const auto k2 = f(t + h/2.0, y + h*k1.valor/2.0); if (!k2.correcto) { salida.error = k2.error; return salida; }
    const auto k3 = f(t + h/2.0, y + h*k2.valor/2.0); if (!k3.correcto) { salida.error = k3.error; return salida; }
    const auto k4 = f(t + h, y + h*k3.valor); if (!k4.correcto) { salida.error = k4.error; return salida; }
    y += h * (k1.valor + 2.0*k2.valor + 2.0*k3.valor + k4.valor) / 6.0;
    t = t0 + (i + 1) * h;
    salida.muestras.push_back({t, y});
  }
  salida.correcto = true;
  return salida;
}

}  // namespace tinta
