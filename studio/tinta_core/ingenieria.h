#pragma once

#include <complex>
#include <string>
#include <vector>

namespace tinta {

enum class TipoComponenteElectrico {
  Resistor, Capacitor, Inductor, FuenteVoltaje, FuenteCorriente,
  Diodo, Led, BjtNpn, BjtPnp, MosfetN, MosfetP, OpAmp,
  Transformador, Interruptor, Cristal, Rele, Tierra, Cable,
  ProbeVoltaje, ProbeCorriente, CompuertaAnd, CompuertaOr,
  CompuertaNot, CompuertaNand, CompuertaNor, CompuertaXor,
  Integrado, Conector,
  // Mantener este orden sincronizado con `TipoComponenteElectrico` en Swift.
  // Los casos se agregan al final para conservar los enteros persistidos.
  CapacitorPolarizado, DiodoZener, Fotodiodo, JfetN, JfetP, Scr, Triac,
  Comparador, FuenteAC, FuenteVoltajeDependiente,
  FuenteCorrienteDependiente, Pulsador, Vcc, Bateria, Fusible, Motor,
  Antena, FlipFlopD, FlipFlopJK, Multiplexor, Demultiplexor,
  Potenciometro, Voltimetro, Amperimetro, Osciloscopio,
  // Solo se usa al recibir un entero fuera del contrato del API C.
  Desconocido
};

struct ComponenteElectrico {
  std::string id;
  TipoComponenteElectrico tipo = TipoComponenteElectrico::Resistor;
  int nodo_a = 0;
  int nodo_b = 0;
  int nodo_control_a = 0;
  int nodo_control_b = 0;
  double valor = 0.0;
  double auxiliar = 0.0;
  double fase_grados = 0.0;
  bool activo = true;
};

struct ResultadoDC {
  bool correcto = false;
  std::string error;
  std::vector<double> voltajes;  // índice = nodo; el nodo 0 siempre vale 0
  std::vector<double> corrientes_fuentes;
  std::vector<std::string> advertencias;
};

struct ResultadoAC {
  bool correcto = false;
  std::string error;
  std::vector<std::complex<double>> voltajes;
  std::vector<std::complex<double>> corrientes_fuentes;
  std::vector<std::string> advertencias;
};

struct PuntoBarridoAC {
  double frecuencia_hz = 0.0;
  ResultadoAC resultado;
};

struct PuntoTransitorio {
  double segundo = 0.0;
  std::vector<double> voltajes;
};

ResultadoDC ResolverDC(const std::vector<ComponenteElectrico>& componentes);
ResultadoAC ResolverAC(const std::vector<ComponenteElectrico>& componentes,
                       double frecuencia_hz);
std::vector<PuntoBarridoAC> BarridoAC(
    const std::vector<ComponenteElectrico>& componentes, double inicio_hz,
    double fin_hz, int puntos_por_decada);
std::vector<PuntoTransitorio> ResolverTransitorio(
    const std::vector<ComponenteElectrico>& componentes, double duracion_s,
    double paso_s, std::string* error);

struct EquivalentePuerto {
  bool correcto = false;
  std::string error;
  double voltaje_thevenin = 0.0;
  double resistencia_thevenin = 0.0;
  double corriente_norton = 0.0;
};

EquivalentePuerto CalcularEquivalente(
    const std::vector<ComponenteElectrico>& componentes, int nodo_a, int nodo_b);

// Calculadoras del toolbox, todas en unidades SI.
double DivisorVoltaje(double entrada_v, double r_superior, double r_inferior);
double GananciaOpAmpNoInversor(double r_realimentacion, double r_tierra);
double GananciaOpAmpInversor(double r_entrada, double r_realimentacion);
double FrecuenciaCorteRC(double resistencia_ohm, double capacitancia_f);
double FrecuenciaResonanciaLC(double inductancia_h, double capacitancia_f);
double ConstanteTiempoRC(double resistencia_ohm, double capacitancia_f);
double ADecibelios(double relacion, bool potencia);
double DesdeDecibelios(double db, bool potencia);
std::string NotacionIngenieria(double valor, const std::string& unidad,
                               int cifras = 4);

struct CodigoResistor {
  bool valido = false;
  double ohmios = 0.0;
  double tolerancia_porcentaje = 0.0;
  std::string error;
};
CodigoResistor DecodificarResistorColores(const std::vector<int>& bandas);
CodigoResistor DecodificarSMD(const std::string& codigo);

}  // namespace tinta
