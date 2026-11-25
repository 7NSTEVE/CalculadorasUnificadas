#include "almacen.h"
#include "archivo.h"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <cstdlib>

namespace tinta {
namespace fs = std::filesystem;

static std::string g_ruta_base = "";

static bool IdSeguro(const std::string& id) {
  if (id.empty() || id.size() > 128 || id == "." || id == "..") return false;
  return std::all_of(id.begin(), id.end(), [](char c) {
    const unsigned char u = static_cast<unsigned char>(c);
    return std::isalnum(u) || c == '-' || c == '_';
  });
}

void ConfigurarRutaAlmacen(const std::string& ruta_base) {
  g_ruta_base = ruta_base;
  if (!g_ruta_base.empty()) {
    std::error_code ec;
    fs::create_directories(g_ruta_base, ec);
  }
}

static int64_t Ahora() {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

static void EscribirString(std::ofstream& ofs, const std::string& s) {
  uint32_t len = static_cast<uint32_t>(s.size());
  ofs.write(reinterpret_cast<const char*>(&len), sizeof(len));
  if (len > 0) {
    ofs.write(s.data(), len);
  }
}

static std::string LeerString(std::ifstream& ifs) {
  uint32_t len = 0;
  if (!ifs.read(reinterpret_cast<char*>(&len), sizeof(len))) return "";
  if (len == 0 || len > 1000000) return "";
  std::string s(len, '\0');
  ifs.read(s.data(), len);
  return s;
}

static bool GuardarMetadatos(const fs::path& ruta, const MetadatosCuaderno& meta) {
  std::ofstream ofs(ruta, std::ios::binary);
  if (!ofs.is_open()) return false;
  EscribirString(ofs, meta.id);
  EscribirString(ofs, meta.titulo);
  ofs.write(reinterpret_cast<const char*>(&meta.fecha_creacion), sizeof(meta.fecha_creacion));
  ofs.write(reinterpret_cast<const char*>(&meta.fecha_modificacion), sizeof(meta.fecha_modificacion));
  return true;
}

static bool LeerMetadatos(const fs::path& ruta, MetadatosCuaderno* meta) {
  std::ifstream ifs(ruta, std::ios::binary);
  if (!ifs.is_open()) return false;
  meta->id = LeerString(ifs);
  meta->titulo = LeerString(ifs);
  if (!ifs.read(reinterpret_cast<char*>(&meta->fecha_creacion), sizeof(meta->fecha_creacion))) return false;
  if (!ifs.read(reinterpret_cast<char*>(&meta->fecha_modificacion), sizeof(meta->fecha_modificacion))) return false;
  return true;
}

std::vector<MetadatosCuaderno> ListarCuadernos() {
  std::vector<MetadatosCuaderno> cuadernos;
  if (g_ruta_base.empty()) return cuadernos;

  std::error_code ec;
  if (!fs::exists(g_ruta_base, ec)) return cuadernos;

  for (const auto& entrada : fs::directory_iterator(g_ruta_base, ec)) {
    if (entrada.is_directory()) {
      auto nombre = entrada.path().filename().string();
      if (nombre.size() > 6 && nombre.substr(nombre.size() - 6) == ".tinta") {
        fs::path ruta_meta = entrada.path() / "metadata.bin";
        MetadatosCuaderno meta;
        if (LeerMetadatos(ruta_meta, &meta)) {
          cuadernos.push_back(meta);
        }
      }
    }
  }
  return cuadernos;
}

bool GuardarCuaderno(Documento& doc) {
  if (g_ruta_base.empty()) return false;

  bool nuevo = false;
  if (doc.id.empty()) {
    doc.id = std::to_string(Ahora()) + "_" + std::to_string(rand());
    nuevo = true;
  }
  if (!IdSeguro(doc.id)) return false;

  fs::path carpeta = fs::path(g_ruta_base) / (doc.id + ".tinta");
  std::error_code ec;
  fs::create_directories(carpeta, ec);
  if (ec) return false;

  fs::path ruta_trazos = carpeta / "trazos.bin";
  fs::path ruta_meta = carpeta / "metadata.bin";

  auto bytes = Serializar(doc);
  if (bytes.empty()) return false;
  std::ofstream ofs(ruta_trazos, std::ios::binary);
  if (!ofs.is_open()) return false;
  ofs.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
  ofs.close();

  MetadatosCuaderno meta;
  if (!nuevo && fs::exists(ruta_meta, ec)) {
    LeerMetadatos(ruta_meta, &meta);
  } else {
    meta.id = doc.id;
    meta.fecha_creacion = Ahora();
  }
  
  meta.titulo = doc.titulo;
  meta.fecha_modificacion = Ahora();

  return GuardarMetadatos(ruta_meta, meta);
}

bool CargarCuaderno(const std::string& id, Documento* doc) {
  if (g_ruta_base.empty() || !doc || !IdSeguro(id)) return false;

  fs::path carpeta = fs::path(g_ruta_base) / (id + ".tinta");
  fs::path ruta_trazos = carpeta / "trazos.bin";
  fs::path ruta_meta = carpeta / "metadata.bin";

  std::error_code ec;
  if (!fs::exists(ruta_trazos, ec)) return false;

  std::ifstream ifs(ruta_trazos, std::ios::binary | std::ios::ate);
  if (!ifs.is_open()) return false;
  std::streamsize size = ifs.tellg();
  ifs.seekg(0, std::ios::beg);
  std::vector<uint8_t> bytes(size);
  if (ifs.read(reinterpret_cast<char*>(bytes.data()), size)) {
    if (!Deserializar(bytes, doc)) return false;
  } else {
    return false;
  }

  MetadatosCuaderno meta;
  if (!LeerMetadatos(ruta_meta, &meta) || meta.id != id || doc->id != id) {
    EstablecerErrorArchivo(
        "la identidad embebida, la metadata y la ruta no coinciden");
    return false;
  }
  doc->titulo = meta.titulo;

  return true;
}

} // namespace tinta
