#include "academic/tec_digital/public_guia_client.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <set>
#include <thread>
#include <unordered_set>

namespace academic::tec_digital {

namespace {

std::string JoinGuiaUrl(std::string_view base_url, std::string_view rel_path) {
    std::string b(base_url);
    while (!b.empty() && b.back() == '/') b.pop_back();
    std::string p(rel_path);
    if (p.empty() || p.front() != '/') p.insert(p.begin(), '/');
    return b + p;
}

}  // namespace

PublicGuiaHorariosClient::PublicGuiaHorariosClient(
    IHttpTransport& transport,
    std::string base_url,
    SanitizeOptions sanitize_opts)
    : transport_(transport),
      base_url_(base_url.empty() ? "https://tec-appsext.itcr.ac.cr/guiahorarios" : std::move(base_url)),
      sanitizer_(std::move(sanitize_opts)) {
    while (!base_url_.empty() && base_url_.back() == '/') {
        base_url_.pop_back();
    }
}

void PublicGuiaHorariosClient::SetSanitizeOptions(SanitizeOptions sanitize_opts) {
    std::lock_guard<std::mutex> lk(sanitizer_mutex_);
    sanitizer_ = Sanitizer(std::move(sanitize_opts));
}

Sanitizer PublicGuiaHorariosClient::SnapshotSanitizer() const {
    std::lock_guard<std::mutex> lk(sanitizer_mutex_);
    return sanitizer_;
}

Result<std::string> PublicGuiaHorariosClient::PostWebMethodJson(
    std::string_view rel_path,
    const std::string& json_body) {
    HttpRequest req;
    req.method = "POST";
    req.url = JoinGuiaUrl(base_url_, rel_path);
    req.follow_redirects = false;
    req.body = json_body.empty() ? "{}" : json_body;
    req.headers["Content-Type"] = "application/json; charset=utf-8";
    req.headers["Accept"] = "application/json, text/javascript, */*";
    req.headers["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) CuadernU/2.0";

    HttpResponse resp = transport_.Execute(req);
    const Sanitizer san = SnapshotSanitizer();
    if (!resp.transport_ok) {
        return Result<std::string>::Failure(
            resp.error_message.rfind("read-only-request-guard:", 0) == 0 ? "policy_blocked" : "transport_error",
            san.SanitizeText(resp.error_message));
    }
    if (resp.status_code < 200 || resp.status_code >= 300) {
        return Result<std::string>::Failure(
            "http_error",
            "Guía de Horarios Pública devolvió HTTP " + std::to_string(resp.status_code));
    }
    return Result<std::string>::Success(resp.BodyAsString());
}

void PublicGuiaHorariosClient::SanitizeRows(
    std::vector<PublicGuiaSessionRow>& rows,
    const Sanitizer& san) {
    for (auto& r : rows) {
        r.dsc_sede = san.SanitizeText(r.dsc_sede);
        r.ide_materia = san.SanitizeText(r.ide_materia);
        r.dsc_materia = san.SanitizeText(r.dsc_materia);
        r.ide_grupo = san.SanitizeText(r.ide_grupo);
        r.dsc_depto = san.SanitizeText(r.dsc_depto);
        r.nom_dia = san.SanitizeText(r.nom_dia);
        r.ide_modalidad = san.SanitizeText(r.ide_modalidad);
        r.ide_per_mod = san.SanitizeText(r.ide_per_mod);
        r.dsc_modalidad = san.SanitizeText(r.dsc_modalidad);
        r.tipo_curso = san.SanitizeText(r.tipo_curso);
        r.hinicio = san.SanitizeText(r.hinicio);
        r.hfin = san.SanitizeText(r.hfin);
        r.nom_profesor = san.SanitizeText(r.nom_profesor);
        r.reserva_sede = san.SanitizeText(r.reserva_sede);
        r.reserva_depto = san.SanitizeText(r.reserva_depto);
        r.reserva_plan = san.SanitizeText(r.reserva_plan);
        r.ide_est_est = san.SanitizeText(r.ide_est_est);
        r.dsc_plan = san.SanitizeText(r.dsc_plan);
        r.num_nivel = san.SanitizeText(r.num_nivel);
    }
}

Result<std::vector<PublicGuiaEscuelaInfo>> PublicGuiaHorariosClient::FetchEscuelas() {
    auto r = PostWebMethodJson("/escuela.aspx/cargaEscuelas", "{}");
    if (!r.ok) return Result<std::vector<PublicGuiaEscuelaInfo>>::Failure(r.error_code, r.error_message);
    auto parsed = ParsePublicGuiaEscuelasResponse(r.value);
    if (!parsed.ok) return parsed;
    const Sanitizer san = SnapshotSanitizer();
    for (auto& e : parsed.value) {
        e.ide_depto = san.SanitizeText(e.ide_depto);
        e.dsc_depto = san.SanitizeText(e.dsc_depto);
    }
    return parsed;
}

Result<std::vector<PublicGuiaModalidadInfo>> PublicGuiaHorariosClient::FetchModalidades() {
    auto r = PostWebMethodJson("/escuela.aspx/cargaModalidadPeriodos", "{}");
    if (!r.ok) return Result<std::vector<PublicGuiaModalidadInfo>>::Failure(r.error_code, r.error_message);
    auto parsed = ParsePublicGuiaModalidadesResponse(r.value);
    if (!parsed.ok) return parsed;
    const Sanitizer san = SnapshotSanitizer();
    for (auto& m : parsed.value) {
        m.ide_modalidad = san.SanitizeText(m.ide_modalidad);
        m.nombre = san.SanitizeText(m.nombre);
    }
    return parsed;
}

Result<std::vector<PublicGuiaSessionRow>> PublicGuiaHorariosClient::FetchEscuelaByYear(
    std::string_view ide_depto,
    int year) {
    const std::string esc = TrimAscii(ide_depto);
    if (esc.empty()) {
        return Result<std::vector<PublicGuiaSessionRow>>::Failure("invalid_arg", "Código de escuela vacío");
    }
    if (year < 1990 || year > 2100) {
        return Result<std::vector<PublicGuiaSessionRow>>::Failure("invalid_arg", "Año fuera de rango");
    }
    nlohmann::json payload = {
        {"escuela", esc},
        {"ano", std::to_string(year)}
    };
    auto r = PostWebMethodJson("/escuela.aspx/getdatosEscuelaAno", payload.dump());
    if (!r.ok) return Result<std::vector<PublicGuiaSessionRow>>::Failure(r.error_code, r.error_message);
    auto parsed = ParsePublicGuiaRowsResponse(r.value);
    if (!parsed.ok) return parsed;
    SanitizeRows(parsed.value, SnapshotSanitizer());
    return parsed;
}

Result<std::vector<PublicGuiaSessionRow>> PublicGuiaHorariosClient::FetchVeranoByYear(int year) {
    if (year < 1990 || year > 2100) {
        return Result<std::vector<PublicGuiaSessionRow>>::Failure("invalid_arg", "Año fuera de rango");
    }
    nlohmann::json payload = {
        {"ano", std::to_string(year)}
    };
    auto r = PostWebMethodJson("/verano.aspx/getdatosVerano", payload.dump());
    if (!r.ok) return Result<std::vector<PublicGuiaSessionRow>>::Failure(r.error_code, r.error_message);
    auto parsed = ParsePublicGuiaRowsResponse(r.value);
    if (!parsed.ok) return parsed;
    SanitizeRows(parsed.value, SnapshotSanitizer());
    return parsed;
}

Result<std::vector<PublicGuiaSessionRow>> PublicGuiaHorariosClient::FetchEstudianteByCarnet(
    std::string_view carnet,
    int year) {
    const std::string c = TrimAscii(carnet);
    if (c.empty()) {
        return Result<std::vector<PublicGuiaSessionRow>>::Failure("invalid_arg", "Número de carné requerido");
    }
    nlohmann::json payload;
    std::string rel_path;
    if (year >= 1990 && year <= 2100) {
        payload = {{"carnet", c}, {"ano", std::to_string(year)}};
        rel_path = "/estudiante.aspx/getdatosAno";
    } else {
        payload = {{"carnet", c}};
        rel_path = "/estudiante.aspx/getdatos";
    }
    auto r = PostWebMethodJson(rel_path, payload.dump());
    if (!r.ok) return Result<std::vector<PublicGuiaSessionRow>>::Failure(r.error_code, r.error_message);
    auto parsed = ParsePublicGuiaRowsResponse(r.value);
    if (!parsed.ok) return parsed;
    SanitizeRows(parsed.value, SnapshotSanitizer());
    return parsed;
}

Result<std::vector<PublicGuiaSessionRow>> PublicGuiaHorariosClient::FetchAllEscuelasByYear(
    int year,
    AllFetchProgressFn on_progress,
    std::atomic<bool>* cancel_flag,
    int max_concurrency,
    std::vector<PublicGuiaEscuelaInfo>* out_escuelas,
    PublicGuiaAllFetchStats* out_stats,
    bool include_verano) {
    if (year < 1990 || year > 2100) {
        return Result<std::vector<PublicGuiaSessionRow>>::Failure("invalid_arg", "Año fuera de rango");
    }

    auto esc_res = FetchEscuelas();
    if (!esc_res.ok) {
        return Result<std::vector<PublicGuiaSessionRow>>::Failure(esc_res.error_code, esc_res.error_message);
    }

    // Deduplicate schools by IDE_DEPTO while preserving clean order
    std::vector<PublicGuiaEscuelaInfo> unique_escuelas;
    std::unordered_set<std::string> seen_codes;
    for (const auto& e : esc_res.value) {
        std::string code = TrimAscii(e.ide_depto);
        if (code.empty()) continue;
        std::string key = AsciiLower(code);
        if (seen_codes.insert(key).second) {
            unique_escuelas.push_back({code, TrimAscii(e.dsc_depto)});
        }
    }
    if (out_escuelas != nullptr) {
        *out_escuelas = unique_escuelas;
    }
    if (unique_escuelas.empty()) {
        return Result<std::vector<PublicGuiaSessionRow>>::Failure("empty_schools", "No se encontraron escuelas en la Guía Pública TEC");
    }

    const size_t total = unique_escuelas.size();
    std::vector<std::vector<PublicGuiaSessionRow>> per_school(total);
    std::vector<bool> ok_flags(total, false);

    const int workers = std::clamp(max_concurrency, 1, 12);
    const size_t num_threads = std::min<size_t>(static_cast<size_t>(workers), total);
    std::atomic<size_t> next_idx{0};
    std::atomic<int> done_count{0};
    std::mutex progress_mtx;

    auto worker_fn = [&]() {
        while (true) {
            if (cancel_flag && cancel_flag->load(std::memory_order_acquire)) break;
            size_t idx = next_idx.fetch_add(1, std::memory_order_relaxed);
            if (idx >= total) break;

            const auto& esc = unique_escuelas[idx];
            auto r = FetchEscuelaByYear(esc.ide_depto, year);
            if (r.ok) {
                ok_flags[idx] = true;
                for (auto& row : r.value) {
                    if (row.dsc_depto.empty()) {
                        row.dsc_depto = esc.dsc_depto.empty() ? esc.ide_depto : esc.dsc_depto;
                    }
                    // Store school code in reserva_depto ONLY if empty? No — keep reserva fields untouched;
                    // dsc_depto already carries the school/program name cleanly.
                }
                per_school[idx] = std::move(r.value);
            }
            int done = done_count.fetch_add(1, std::memory_order_acq_rel) + 1;
            if (on_progress) {
                std::lock_guard<std::mutex> lk(progress_mtx);
                on_progress(done, static_cast<int>(total), esc.dsc_depto.empty() ? esc.ide_depto : esc.dsc_depto);
            }
        }
    };

    if (num_threads <= 1) {
        worker_fn();
    } else {
        std::vector<std::thread> pool;
        pool.reserve(num_threads);
        for (size_t t = 0; t < num_threads; ++t) {
            pool.emplace_back(worker_fn);
        }
        for (auto& th : pool) {
            if (th.joinable()) th.join();
        }
    }

    const bool was_cancelled = (cancel_flag && cancel_flag->load(std::memory_order_acquire));

    std::vector<PublicGuiaSessionRow> verano_rows;
    if (include_verano && !was_cancelled) {
        if (auto vr = FetchVeranoByYear(year); vr.ok) {
            verano_rows = std::move(vr.value);
        }
    }

    PublicGuiaAllFetchStats stats;
    stats.total_schools = static_cast<int>(total);
    stats.cancelled = was_cancelled;

    std::vector<PublicGuiaSessionRow> merged;
    std::unordered_set<std::string> seen_session_keys;
    std::unordered_set<std::string> unique_course_codes;

    auto append_rows = [&](std::vector<PublicGuiaSessionRow>& batch) {
        for (auto& row : batch) {
            std::string k = AsciiLower(TrimAscii(row.ide_materia)) + "|" +
                            AsciiLower(TrimAscii(row.ide_grupo)) + "|" +
                            AsciiLower(TrimAscii(row.dsc_sede)) + "|" +
                            AsciiLower(TrimAscii(row.ide_modalidad)) + "|" +
                            AsciiLower(TrimAscii(row.ide_per_mod)) + "|" +
                            std::to_string(row.num_ano) + "|" +
                            AsciiLower(TrimAscii(row.nom_dia)) + "|" +
                            TrimAscii(row.hinicio) + "|" +
                            TrimAscii(row.hfin);
            if (!seen_session_keys.insert(k).second) continue;
            if (!row.ide_materia.empty()) {
                unique_course_codes.insert(AsciiLower(TrimAscii(row.ide_materia)));
            }
            merged.push_back(std::move(row));
        }
    };

    for (size_t i = 0; i < total; ++i) {
        if (ok_flags[i]) ++stats.schools_succeeded;
        if (!per_school[i].empty()) {
            ++stats.schools_with_rows;
            append_rows(per_school[i]);
        }
    }
    if (!verano_rows.empty()) {
        append_rows(verano_rows);
    }

    stats.total_raw_rows = static_cast<int>(merged.size());
    stats.unique_courses = static_cast<int>(unique_course_codes.size());
    if (out_stats != nullptr) {
        *out_stats = stats;
    }

    if (was_cancelled) {
        return Result<std::vector<PublicGuiaSessionRow>>::Failure("cancelled", "Indexación cancelada por el usuario.");
    }
    if (stats.schools_succeeded == 0 && merged.empty()) {
        return Result<std::vector<PublicGuiaSessionRow>>::Failure(
            "transport_error", "No se pudo consultar ninguna escuela en la Guía de Horarios Pública TEC.");
    }
    return Result<std::vector<PublicGuiaSessionRow>>::Success(std::move(merged));
}

}  // namespace academic::tec_digital
