#pragma once

#include "academic/tec_digital/academic_models.hpp"
#include "academic/tec_digital/http_transport.hpp"
#include "academic/tec_digital/sanitizer.hpp"
#include "academic/tec_digital/types.hpp"

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace academic::tec_digital {

struct PublicGuiaAllFetchStats {
    int total_schools = 0;
    int schools_succeeded = 0;
    int schools_with_rows = 0;
    int total_raw_rows = 0;
    int unique_courses = 0;
    bool cancelled = false;
};

class PublicGuiaHorariosClient {
public:
    using AllFetchProgressFn = std::function<void(int done, int total, const std::string& current_school)>;

    explicit PublicGuiaHorariosClient(
        IHttpTransport& transport,
        std::string base_url = "https://tec-appsext.itcr.ac.cr/guiahorarios",
        SanitizeOptions sanitize_opts = {});

    void SetSanitizeOptions(SanitizeOptions sanitize_opts);
    const std::string& BaseUrl() const noexcept { return base_url_; }

    Result<std::vector<PublicGuiaEscuelaInfo>> FetchEscuelas();
    Result<std::vector<PublicGuiaModalidadInfo>> FetchModalidades();
    Result<std::vector<PublicGuiaSessionRow>> FetchEscuelaByYear(std::string_view ide_depto, int year);
    Result<std::vector<PublicGuiaSessionRow>> FetchVeranoByYear(int year);
    Result<std::vector<PublicGuiaSessionRow>> FetchEstudianteByCarnet(std::string_view carnet, int year = 0);

    // Parallel crawl of ALL TEC schools/areas/programs (+ optional Verano) for a given year.
    // Strictly public & read-only (zero credentials or cookies).
    Result<std::vector<PublicGuiaSessionRow>> FetchAllEscuelasByYear(
        int year,
        AllFetchProgressFn on_progress = nullptr,
        std::atomic<bool>* cancel_flag = nullptr,
        int max_concurrency = 8,
        std::vector<PublicGuiaEscuelaInfo>* out_escuelas = nullptr,
        PublicGuiaAllFetchStats* out_stats = nullptr,
        bool include_verano = true);

private:
    Sanitizer SnapshotSanitizer() const;
    Result<std::string> PostWebMethodJson(std::string_view rel_path, const std::string& json_body);
    static void SanitizeRows(std::vector<PublicGuiaSessionRow>& rows, const Sanitizer& san);

    IHttpTransport& transport_;
    std::string base_url_;
    mutable std::mutex sanitizer_mutex_;
    Sanitizer sanitizer_;
};

}  // namespace academic::tec_digital
