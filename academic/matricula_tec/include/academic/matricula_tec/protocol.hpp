#pragma once

#include "academic/matricula_tec/models.hpp"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace academic::matricula_tec {

// Strict RFC 3986 percent-encoding (encodes +, /, =, $, &, etc.) for ASP.NET WebForms POSTs.
std::string StrictPercentEncode(std::string_view input);

// Builds an application/x-www-form-urlencoded payload with strict percent-encoding.
std::string BuildFormUrlEncoded(const std::vector<std::pair<std::string, std::string>>& fields);

// Normalizes TEC day tokens ("L","K","M","J","V","S","1".."6","Lunes".."Sábado") -> 0..5 (-1 if invalid).
int NormalizeTecDay(std::string_view raw_day);
std::string DayIndexToCode(int day_index);

// Parses "07:30", "7:30", "13:00", "07:30:00" -> minutes since 00:00 (-1 if invalid).
int ParseTimeToMinutes(std::string_view raw_time);

// Returns true if two schedule sessions overlap on the same day.
bool SessionsOverlap(const ScheduleSession& a, const ScheduleSession& b) noexcept;
bool SessionVectorsOverlap(const std::vector<ScheduleSession>& a,
                           const std::vector<ScheduleSession>& b) noexcept;

// Extracts __VIEWSTATE, __VIEWSTATEGENERATOR, __EVENTVALIDATION from ASP.NET HTML.
WebFormsTokens ExtractWebFormsTokens(std::string_view html);

// Extracts hidden bootstrap fields from frmMatricula.aspx HTML.
StudentBootstrap ParseFrmMatriculaBootstrap(std::string_view html);

// Unwraps ASP.NET ScriptService {"d": ...} JSON envelope (handles double-encoded JSON strings).
nlohmann::json UnwrapAspNetD(const nlohmann::json& envelope);

// Parsers for DATIC WebMethods JSON payloads.
std::vector<CourseOffer> ParseCursosPayload(const nlohmann::json& payload);
std::vector<CourseGroupLive> ParseHorariosPayload(const nlohmann::json& payload,
                                                  const std::string& course_code_hint = "");
CitaInfo ParseCitaPayload(const nlohmann::json& payload);
MorosidadInfo ParseMorosidadPayload(const nlohmann::json& payload);
std::vector<EnrolledScheduleItem> ParseHorarioMatriculadoPayload(const nlohmann::json& payload);
InformeSummary ParseGeneraInformePayload(const nlohmann::json& payload);
EnrollmentResult ParseMutationPayload(const nlohmann::json& payload,
                                      const std::string& action,
                                      const std::string& course_code,
                                      int grupo);

// Offline/Dry-Run preflight simulator (checks seats & schedule collisions without mutating TEC).
EnrollmentResult SimulateDryRunEnroll(const CourseGroupLive& candidate,
                                      const std::vector<EnrolledScheduleItem>& currently_enrolled);

// Sanitizes a string by removing any occurrence of sensitive secrets.
std::string RedactSecrets(std::string text, const std::vector<std::string>& secrets);

} // namespace academic::matricula_tec
