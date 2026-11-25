#include "PPTXZipBridge.h"

#include <zlib.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {
constexpr std::uint32_t kFirmaEOCD = 0x06054b50;
constexpr std::uint32_t kFirmaCentral = 0x02014b50;
constexpr std::uint16_t kMetodoGuardado = 0;
constexpr std::uint16_t kMetodoDeflate = 8;
// El importador todavía rasteriza diapositivas en memoria. Estos presupuestos
// evitan combinar un ZIP de cientos de MB con otra entrada de igual tamaño.
// Los PPTX que los excedan deben convertirse a PDF o procesarse mediante el
// importador paginado futuro, no arriesgar un jetsam durante una clase.
constexpr std::uint64_t kMaximoArchivo = 128ull * 1024ull * 1024ull;
constexpr std::uint64_t kMaximoExpandido = 256ull * 1024ull * 1024ull;
constexpr std::uint64_t kMaximoEntrada = 64ull * 1024ull * 1024ull;
constexpr std::uint32_t kMaximoEntradas = 20'000;

std::uint16_t u16(const std::vector<std::uint8_t>& d, std::size_t p) {
    return static_cast<std::uint16_t>(d[p]) |
           (static_cast<std::uint16_t>(d[p + 1]) << 8);
}
std::uint32_t u32(const std::vector<std::uint8_t>& d, std::size_t p) {
    return static_cast<std::uint32_t>(d[p]) |
           (static_cast<std::uint32_t>(d[p + 1]) << 8) |
           (static_cast<std::uint32_t>(d[p + 2]) << 16) |
           (static_cast<std::uint32_t>(d[p + 3]) << 24);
}

void error(char* salida, std::size_t capacidad, const std::string& texto) {
    if (!salida || capacidad == 0) return;
    const auto cantidad = std::min(capacidad - 1, texto.size());
    std::memcpy(salida, texto.data(), cantidad);
    salida[cantidad] = '\0';
}

bool rutaSegura(const std::string& nombre) {
    if (nombre.empty() || nombre.front() == '/' || nombre.front() == '\\') return false;
    if (nombre.find('\0') != std::string::npos || nombre.find(':') != std::string::npos)
        return false;
    std::filesystem::path ruta(nombre);
    for (const auto& parte : ruta) {
        if (parte == "..") return false;
    }
    return true;
}

bool inflar(const std::uint8_t* origen, std::size_t comprimido,
            std::vector<std::uint8_t>& salida) {
    z_stream flujo{};
    flujo.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(origen));
    flujo.avail_in = static_cast<uInt>(comprimido);
    flujo.next_out = reinterpret_cast<Bytef*>(salida.data());
    flujo.avail_out = static_cast<uInt>(salida.size());
    if (inflateInit2(&flujo, -MAX_WBITS) != Z_OK) return false;
    const int estado = inflate(&flujo, Z_FINISH);
    inflateEnd(&flujo);
    return estado == Z_STREAM_END && flujo.total_out == salida.size();
}
}

extern "C" int tinta_zip_extraer_seguro(const char* archivo_zip,
                                          const char* carpeta_destino,
                                          char* mensaje_error,
                                          size_t capacidad_error) {
    if (!archivo_zip || !carpeta_destino) {
        error(mensaje_error, capacidad_error, "Ruta de PPTX o destino vacía.");
        return 0;
    }
    try {
        std::ifstream archivo(archivo_zip, std::ios::binary);
        if (!archivo) {
            error(mensaje_error, capacidad_error, "No se pudo abrir el PPTX.");
            return 0;
        }
        archivo.seekg(0, std::ios::end);
        const auto tamano = archivo.tellg();
        if (tamano <= 21 || static_cast<std::uint64_t>(tamano) > kMaximoArchivo) {
            error(mensaje_error, capacidad_error, "El PPTX está vacío o excede el límite local.");
            return 0;
        }
        archivo.seekg(0, std::ios::beg);
        std::vector<std::uint8_t> datos(static_cast<std::size_t>(tamano));
        archivo.read(reinterpret_cast<char*>(datos.data()), tamano);
        if (!archivo) {
            error(mensaje_error, capacidad_error, "No se pudo leer el PPTX completo.");
            return 0;
        }

        const std::size_t minimo = datos.size() > 65'557 ? datos.size() - 65'557 : 0;
        std::size_t eocd = std::string::npos;
        for (std::size_t p = datos.size() - 22; ; --p) {
            if (u32(datos, p) == kFirmaEOCD) { eocd = p; break; }
            if (p == minimo) break;
        }
        if (eocd == std::string::npos) {
            error(mensaje_error, capacidad_error, "El contenedor PPTX no tiene directorio ZIP.");
            return 0;
        }
        const auto entradas = u16(datos, eocd + 10);
        const auto offsetCentral = u32(datos, eocd + 16);
        if (entradas > kMaximoEntradas || offsetCentral >= datos.size()) {
            error(mensaje_error, capacidad_error, "El directorio del PPTX excede límites seguros.");
            return 0;
        }

        const std::filesystem::path raiz =
            std::filesystem::weakly_canonical(std::filesystem::path(carpeta_destino));
        std::filesystem::create_directories(raiz);
        std::uint64_t totalExpandido = 0;
        std::size_t central = offsetCentral;
        for (std::uint32_t indice = 0; indice < entradas; ++indice) {
            if (central > datos.size() || datos.size() - central < 46 ||
                u32(datos, central) != kFirmaCentral) {
                error(mensaje_error, capacidad_error, "Entrada central PPTX inválida.");
                return 0;
            }
            const auto banderas = u16(datos, central + 8);
            const auto metodo = u16(datos, central + 10);
            const auto crcEsperado = u32(datos, central + 16);
            const auto comprimido = u32(datos, central + 20);
            const auto expandido = u32(datos, central + 24);
            const auto largoNombre = u16(datos, central + 28);
            const auto largoExtra = u16(datos, central + 30);
            const auto largoComentario = u16(datos, central + 32);
            const auto offsetLocal = u32(datos, central + 42);
            const std::size_t largoMetadatos = static_cast<std::size_t>(largoNombre) +
                                               largoExtra + largoComentario;
            if ((banderas & 0x1u) != 0 ||
                largoMetadatos > datos.size() - central - 46) {
                error(mensaje_error, capacidad_error, "El PPTX está cifrado o truncado.");
                return 0;
            }
            std::string nombre(reinterpret_cast<const char*>(datos.data() + central + 46),
                               largoNombre);
            std::replace(nombre.begin(), nombre.end(), '\\', '/');
            if (!rutaSegura(nombre)) {
                error(mensaje_error, capacidad_error, "El PPTX contiene una ruta no segura.");
                return 0;
            }
            if (comprimido > kMaximoEntrada || expandido > kMaximoEntrada) {
                error(mensaje_error, capacidad_error,
                      "Una entrada PPTX excede el límite de 64 MB.");
                return 0;
            }
            totalExpandido += expandido;
            if (totalExpandido > kMaximoExpandido) {
                error(mensaje_error, capacidad_error, "El PPTX se expande más de 256 MB.");
                return 0;
            }
            const auto destino = (raiz / std::filesystem::path(nombre)).lexically_normal();
            const auto relativo = destino.lexically_relative(raiz);
            const bool escapaRaiz = !relativo.empty() &&
                                    relativo.begin() != relativo.end() &&
                                    *relativo.begin() == std::filesystem::path("..");
            if (relativo.empty() || escapaRaiz) {
                error(mensaje_error, capacidad_error, "Una entrada intenta salir de la carpeta PPTX.");
                return 0;
            }
            if (!nombre.empty() && nombre.back() == '/') {
                std::filesystem::create_directories(destino);
            } else {
                const std::size_t local = offsetLocal;
                if (local > datos.size() || datos.size() - local < 30 ||
                    u32(datos, local) != 0x04034b50) {
                    error(mensaje_error, capacidad_error, "Cabecera local PPTX inválida.");
                    return 0;
                }
                const auto localNombre = u16(datos, local + 26);
                const auto localExtra = u16(datos, local + 28);
                const std::size_t largoCabecera = static_cast<std::size_t>(localNombre) +
                                                  localExtra;
                if (largoCabecera > datos.size() - local - 30) {
                    error(mensaje_error, capacidad_error, "Cabecera local PPTX truncada.");
                    return 0;
                }
                const std::size_t inicio = local + 30 + largoCabecera;
                if (comprimido > datos.size() - inicio) {
                    error(mensaje_error, capacidad_error, "Datos comprimidos PPTX truncados.");
                    return 0;
                }
                std::vector<std::uint8_t> contenido(expandido);
                if (metodo == kMetodoGuardado) {
                    if (comprimido != expandido) {
                        error(mensaje_error, capacidad_error, "Entrada PPTX almacenada con tamaño inválido.");
                        return 0;
                    }
                    std::copy_n(datos.data() + inicio, expandido, contenido.data());
                } else if (metodo == kMetodoDeflate) {
                    if (!inflar(datos.data() + inicio, comprimido, contenido)) {
                        error(mensaje_error, capacidad_error, "No se pudo descomprimir una entrada PPTX.");
                        return 0;
                    }
                } else {
                    error(mensaje_error, capacidad_error, "El PPTX usa una compresión ZIP no admitida.");
                    return 0;
                }
                const auto crc = crc32(0L, reinterpret_cast<const Bytef*>(contenido.data()),
                                       static_cast<uInt>(contenido.size()));
                if (crc != crcEsperado) {
                    error(mensaje_error, capacidad_error, "Una entrada PPTX falló su verificación CRC.");
                    return 0;
                }
                std::filesystem::create_directories(destino.parent_path());
                std::ofstream salida(destino, std::ios::binary | std::ios::trunc);
                salida.write(reinterpret_cast<const char*>(contenido.data()),
                             static_cast<std::streamsize>(contenido.size()));
                if (!salida) {
                    error(mensaje_error, capacidad_error, "No se pudo guardar una entrada PPTX.");
                    return 0;
                }
            }
            central += 46 + largoMetadatos;
        }
        error(mensaje_error, capacidad_error, "");
        return 1;
    } catch (const std::exception& e) {
        error(mensaje_error, capacidad_error, e.what());
        return 0;
    }
}

extern "C" int tinta_pptx_extraer(const char* archivo_zip,
                                    const char* carpeta_destino,
                                    char* mensaje_error,
                                    size_t capacidad_error) {
    return tinta_zip_extraer_seguro(archivo_zip, carpeta_destino,
                                    mensaje_error, capacidad_error);
}
