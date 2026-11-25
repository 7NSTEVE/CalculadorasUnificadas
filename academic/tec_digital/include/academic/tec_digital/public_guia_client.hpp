#pragma once

#include "academic/tec_digital/academic_models.hpp"
#include "academic/tec_digital/http_transport.hpp"
#include "academic/tec_digital/sanitizer.hpp"
#include "academic/tec_digital/types.hpp"

#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace academic::tec_digital {

class PublicGuiaHorariosClient {
public:
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
