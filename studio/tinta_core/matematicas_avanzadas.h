#pragma once

#include <complex>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace tinta {

struct ResultadoExpresion {
  bool correcto = false;
  std::complex<double> valor{};
  std::string error;
};

ResultadoExpresion EvaluarExpresion(
    const std::string& expresion,
    const std::map<std::string, std::complex<double>>& variables = {});

struct MuestraFuncion {
  double x = 0.0;
  std::complex<double> y{};
  bool valida = false;
};

std::vector<MuestraFuncion> MuestrearFuncion(
    const std::string& expresion, const std::string& variable,
    double inicio, double fin, size_t cantidad,
    const std::map<std::string, std::complex<double>>& variables = {});

ResultadoExpresion DerivadaNumerica(
    const std::string& expresion, const std::string& variable, double x,
    const std::map<std::string, std::complex<double>>& variables = {});
ResultadoExpresion IntegralNumerica(
    const std::string& expresion, const std::string& variable,
    double inicio, double fin, int subintervalos,
    const std::map<std::string, std::complex<double>>& variables = {});

struct ResultadoConversion {
  bool correcto = false;
  double valor = 0.0;
  std::string error;
};
ResultadoConversion ConvertirUnidad(double valor, const std::string& origen,
                                    const std::string& destino);

struct ResultadoMatriz {
  bool correcto = false;
  size_t filas = 0;
  size_t columnas = 0;
  std::vector<std::complex<double>> valores;
  std::string error;
};

ResultadoExpresion DeterminanteMatriz(
    const std::vector<std::complex<double>>& matriz, size_t orden);
ResultadoMatriz InvertirMatriz(
    const std::vector<std::complex<double>>& matriz, size_t orden);
ResultadoMatriz ResolverSistemaLineal(
    const std::vector<std::complex<double>>& coeficientes,
    const std::vector<std::complex<double>>& terminos, size_t orden);

ResultadoExpresion ProductoPunto(
    const std::vector<std::complex<double>>& a,
    const std::vector<std::complex<double>>& b);
ResultadoMatriz ProductoCruz(
    const std::vector<std::complex<double>>& a,
    const std::vector<std::complex<double>>& b);

ResultadoExpresion BuscarRaiz(
    const std::string& expresion, const std::string& variable,
    double estimacion, const std::map<std::string, std::complex<double>>& variables = {});
ResultadoExpresion CalcularLimite(
    const std::string& expresion, const std::string& variable, double punto,
    int lado, const std::map<std::string, std::complex<double>>& variables = {});
ResultadoExpresion CalcularSumatoria(
    const std::string& expresion, const std::string& variable,
    int64_t inicio, int64_t fin,
    const std::map<std::string, std::complex<double>>& variables = {});

std::vector<std::complex<double>> TransformadaFourierDiscreta(
    const std::vector<std::complex<double>>& muestras, bool inversa);
ResultadoExpresion TransformadaLaplaceNumerica(
    const std::string& expresion, const std::string& variable,
    std::complex<double> s, double tiempo_maximo, int pasos,
    const std::map<std::string, std::complex<double>>& variables = {});

struct MuestraEDO {
  double t = 0.0;
  std::complex<double> y{};
};
struct ResultadoEDO {
  bool correcto = false;
  std::vector<MuestraEDO> muestras;
  std::string error;
};
ResultadoEDO IntegrarEDOPrimerOrden(
    const std::string& derivada, double t0, std::complex<double> y0,
    double t1, size_t pasos,
    const std::map<std::string, std::complex<double>>& variables = {});

}  // namespace tinta
