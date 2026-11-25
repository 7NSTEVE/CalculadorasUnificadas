#include "academic/horarios_tec/horarios_tec.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <limits>
#include <locale>
#include <string>
#include <vector>

namespace {

int gPasses = 0;
int gFailures = 0;

void CheckImpl(bool condition, const char* expr, int line) {
    if (condition) {
        ++gPasses;
    } else {
        ++gFailures;
        std::cerr << "[FAIL] line " << line << ": " << expr << '\n';
    }
}

#define CHECK(expr) CheckImpl(static_cast<bool>(expr), #expr, __LINE__)

using namespace academic::horarios_tec;

const char* kHorariosTecFixtureJson = R"JSON([
    {
        "name": "Circuitos Eléctricos en C.C.",
        "code": "EL2113",
        "color": "linear-gradient(135deg, #10b981, #059669)",
        "groups": [
            {
                "id": "Gr 50",
                "modality": "Regular",
                "sessions": [
                    { "day": 2, "startH": 16, "startM": 10, "endH": 17, "endM": 55 },
                    { "day": 4, "startH": 7, "startM": 55, "endH": 9, "endM": 40 }
                ]
            }
        ]
    },
    {
        "name": "Física General II",
        "code": "FI1102",
        "color": "linear-gradient(135deg, #0ea5e9, #0284c7)",
        "groups": [
            {
                "id": "Gr 50",
                "modality": "Regular",
                "sessions": [
                    { "day": 1, "startH": 9, "startM": 45, "endH": 11, "endM": 30 },
                    { "day": 4, "startH": 9, "startM": 45, "endH": 11, "endM": 30 }
                ]
            },
            {
                "id": "Gr 51",
                "modality": "Regular",
                "sessions": [
                    { "day": 1, "startH": 18, "startM": 0, "endH": 19, "endM": 45 },
                    { "day": 3, "startH": 18, "startM": 0, "endH": 19, "endM": 45 }
                ]
            }
        ]
    },
    {
        "name": "Física General III",
        "code": "FI2103",
        "color": "linear-gradient(135deg, #f43f5e, #e11d48)",
        "groups": [
            {
                "id": "Gr 50",
                "modality": "Regular",
                "sessions": [
                    { "day": 1, "startH": 9, "startM": 45, "endH": 11, "endM": 30 },
                    { "day": 2, "startH": 9, "startM": 45, "endH": 11, "endM": 30 }
                ]
            },
            {
                "id": "Gr 51",
                "modality": "Semipresencial",
                "sessions": [
                    { "day": 2, "startH": 14, "startM": 20, "endH": 16, "endM": 5 },
                    { "day": 4, "startH": 14, "startM": 20, "endH": 16, "endM": 5 }
                ]
            }
        ]
    },
    {
        "name": "Cálculo y Álgebra Lineal",
        "code": "MA1103",
        "color": "linear-gradient(135deg, #f59e0b, #d97706)",
        "groups": [
            {
                "id": "Gr 50",
                "modality": "Semipresencial",
                "sessions": [
                    { "day": 2, "startH": 7, "startM": 55, "endH": 9, "endM": 40 },
                    { "day": 3, "startH": 14, "startM": 20, "endH": 16, "endM": 5 }
                ]
            },
            {
                "id": "Gr 51",
                "modality": "Semipresencial",
                "sessions": [
                    { "day": 1, "startH": 9, "startM": 45, "endH": 11, "endM": 30 },
                    { "day": 3, "startH": 9, "startM": 45, "endH": 11, "endM": 30 }
                ]
            },
            {
                "id": "Gr 52",
                "modality": "Semipresencial",
                "sessions": [
                    { "day": 2, "startH": 9, "startM": 45, "endH": 11, "endM": 30 },
                    { "day": 4, "startH": 9, "startM": 45, "endH": 11, "endM": 30 }
                ]
            }
        ]
    }
])JSON";

void TestHorariosTecFixtureLegacyParity() {
    SolverPreferences prefs;
    prefs.weights.legacySessionScoreOnly = true;

    const ScheduleResult res = HorariosSolver::SolveFromJson(kHorariosTecFixtureJson, "", prefs);
    CHECK(res.ok);
    CHECK(res.status == SolveStatus::Ok);
    CHECK(res.HasSolutions());
    CHECK(res.diagnostics.candidateCoursesCount == 4);
    CHECK(res.diagnostics.validSchedulesFound == 5);
    CHECK(res.schedules.size() == 5);

    // Exact descending scores from HorariosTEC/WebUI/app.js: +80, 0, -40, -40, -80
    CHECK(std::abs(res.schedules[0].metrics.totalScore - 80.0) < 1e-9);
    CHECK(std::abs(res.schedules[1].metrics.totalScore - 0.0) < 1e-9);
    CHECK(std::abs(res.schedules[2].metrics.totalScore - (-40.0)) < 1e-9);
    CHECK(std::abs(res.schedules[3].metrics.totalScore - (-40.0)) < 1e-9);
    CHECK(std::abs(res.schedules[4].metrics.totalScore - (-80.0)) < 1e-9);

    CHECK(res.schedules[0].rank == 1);
    CHECK(res.schedules[0].selections.size() == 4);
    CHECK(res.schedules[0].selections[0].courseCodeUtf8 == "EL2113");
    CHECK(res.schedules[0].selections[0].group.idUtf8 == "Gr 50");
    CHECK(res.schedules[0].selections[1].courseCodeUtf8 == "FI1102");
    CHECK(res.schedules[0].selections[1].group.idUtf8 == "Gr 50");
    CHECK(res.schedules[0].selections[2].courseCodeUtf8 == "FI2103");
    CHECK(res.schedules[0].selections[2].group.idUtf8 == "Gr 51");
    CHECK(res.schedules[0].selections[3].courseCodeUtf8 == "MA1103");
    CHECK(res.schedules[0].selections[3].group.idUtf8 == "Gr 50");

    // Verify deterministic tie-breaking between the two -40 schedules:
    // MA1103 Gr 52 has 350 total gap minutes vs MA1103 Gr 51's 1065 gap minutes (both have 4 active days).
    CHECK(res.schedules[2].selections[3].group.idUtf8 == "Gr 52");
    CHECK(res.schedules[2].metrics.totalGapMinutes == 350);
    CHECK(res.schedules[3].selections[3].group.idUtf8 == "Gr 51");
    CHECK(res.schedules[3].metrics.totalGapMinutes == 1065);
}

void TestCatalogParsingAndHelpers() {
    DayOfWeek day = DayOfWeek::Monday;
    CHECK(TryParseDay("L", day) && day == DayOfWeek::Monday);
    CHECK(TryParseDay("K", day) && day == DayOfWeek::Tuesday);
    CHECK(TryParseDay("M", day) && day == DayOfWeek::Wednesday);
    CHECK(TryParseDay("Miércoles", day) && day == DayOfWeek::Wednesday);
    CHECK(TryParseDay("jueves", day) && day == DayOfWeek::Thursday);
    CHECK(TryParseDay("V", day) && day == DayOfWeek::Friday);
    CHECK(TryParseDay("Sábado", day) && day == DayOfWeek::Saturday);
    CHECK(TryParseDay("D", day) && day == DayOfWeek::Sunday);
    CHECK(!TryParseDay("X", day));

    CHECK(std::string(DayToShortEs(DayOfWeek::Tuesday)) == "K");
    CHECK(std::string(DayToSpanishName(DayOfWeek::Wednesday)) == "Miércoles");

    int mins = -1;
    CHECK(TryParseTimeHHMM("07:55", mins) && mins == 475);
    CHECK(TryParseTimeHHMM("7:55", mins) && mins == 475);
    CHECK(TryParseTimeHHMM("24:00", mins) && mins == 1440);
    CHECK(!TryParseTimeHHMM("24:01", mins));
    CHECK(!TryParseTimeHHMM("12:60", mins));
    CHECK(!TryParseTimeHHMM("bad", mins));
    CHECK(FormatTimeHHMM(475) == "07:55");
    CHECK(FormatTimeHHMM(1440) == "24:00");

    // TEC Digital curriculum aliases + {courses} + UTF-8 BOM + HH:MM + TEC day codes
    const std::string tecDigitalJson = "\xEF\xBB\xBF" R"JSON({
        "dsc_curriculum": "Licenciatura en Electrónica",
        "term": "2026-S2",
        "campus": "Cartago",
        "levels": [
            {
                "id": "Semestre 1",
                "courses": [
                    {
                        "id_course": "CI0200",
                        "name": "Introducción a la Computación",
                        "credits": 4,
                        "hours": 4,
                        "best_status": "materiaPendiente",
                        "requirements": [{"id": "MA0101"}],
                        "co_requirements": ["FI1101"],
                        "groups": [
                            {
                                "group": 54,
                                "crn": "10234",
                                "modality": "Regular",
                                "professor": "Ing. Rojas",
                                "campus": "Cartago",
                                "seatsAvailable": 8,
                                "seatsCapacity": 30,
                                "schedule": [
                                    {
                                        "dia": "K",
                                        "start": "07:30",
                                        "end": "09:20",
                                        "aula": "B3-04",
                                        "building": "B3"
                                    },
                                    {
                                        "day": "J",
                                        "start": "07:30",
                                        "end": "09:20"
                                    }
                                ]
                            }
                        ]
                    }
                ]
            }
        ]
    })JSON";

    const CatalogParseResult parsed = ParseCatalogJson(tecDigitalJson);
    CHECK(parsed.ok);
    CHECK(parsed.catalog.titleUtf8 == "Licenciatura en Electrónica");
    CHECK(parsed.catalog.termUtf8 == "2026-S2");
    CHECK(parsed.catalog.campusUtf8 == "Cartago");
    CHECK(parsed.catalog.courses.size() == 1);
    const Course& c = parsed.catalog.courses[0];
    CHECK(c.codeUtf8 == "CI0200");
    CHECK(c.credits == 4);
    CHECK(c.hours == 4);
    CHECK(c.curriculumStatusUtf8 == "materiaPendiente");
    CHECK(c.prerequisitesUtf8.size() == 1 && c.prerequisitesUtf8[0] == "MA0101");
    CHECK(c.corequisitesUtf8.size() == 1 && c.corequisitesUtf8[0] == "FI1101");
    CHECK(c.groups.size() == 1);
    CHECK(c.groups[0].idUtf8 == "54");
    CHECK(c.groups[0].crnUtf8 == "10234");
    CHECK(c.groups[0].professorUtf8 == "Ing. Rojas");
    CHECK(c.groups[0].seatsAvailable == 8);
    CHECK(c.groups[0].seatsCapacity == 30);
    CHECK(c.groups[0].sessions.size() == 2);
    CHECK(c.groups[0].sessions[0].day == DayOfWeek::Tuesday);
    CHECK(c.groups[0].sessions[0].startMinute == 450);
    CHECK(c.groups[0].sessions[0].endMinute == 560);
    CHECK(c.groups[0].sessions[0].roomUtf8 == "B3-04");
    CHECK(c.groups[0].sessions[1].day == DayOfWeek::Thursday);

    // Malformed JSON diagnostics
    CHECK(!ParseCatalogJson("").ok);
    CHECK(!ParseCatalogJson("{invalid json").ok);
    CHECK(!ParseCatalogJson(R"({"other": []})").ok);
    CHECK(!ParseCatalogJson(R"([{"code":"X","groups":[{"id":"1","sessions":[{"day":"Z","start":"08:00","end":"09:00"}]}]}])").ok);
}

void TestConflictDetectionAndBoundaries() {
    // Exact touching boundary: 09:40 == 09:40 -> NO conflict with buffer 0
    CHECK(!TimeSlotsConflict(DayOfWeek::Monday, 475, 580, DayOfWeek::Monday, 580, 690, 0));
    CHECK(!TimeSlotsConflict(DayOfWeek::Monday, 580, 690, DayOfWeek::Monday, 475, 580, 0));

    // Overlap by 1 minute -> conflict
    CHECK(TimeSlotsConflict(DayOfWeek::Monday, 475, 581, DayOfWeek::Monday, 580, 690, 0));

    // Different days -> NO conflict
    CHECK(!TimeSlotsConflict(DayOfWeek::Monday, 475, 580, DayOfWeek::Tuesday, 475, 580, 15));

    // Break buffer: 09:40 to 09:45 is 5 minutes
    CHECK(!TimeSlotsConflict(DayOfWeek::Friday, 475, 580, DayOfWeek::Friday, 585, 690, 5));
    CHECK(TimeSlotsConflict(DayOfWeek::Friday, 475, 580, DayOfWeek::Friday, 585, 690, 10));

    // Verify minBreakMinutesBetweenClasses = 5 preserves all 5 schedules, while = 10 eliminates 5-min TEC transitions
    SolverPreferences prefs5;
    prefs5.minBreakMinutesBetweenClasses = 5;
    const ScheduleResult res5 = HorariosSolver::SolveFromJson(kHorariosTecFixtureJson, "", prefs5);
    CHECK(res5.ok);
    CHECK(res5.diagnostics.validSchedulesFound == 5);

    SolverPreferences prefs10;
    prefs10.minBreakMinutesBetweenClasses = 10;
    const ScheduleResult res10 = HorariosSolver::SolveFromJson(kHorariosTecFixtureJson, "", prefs10);
    CHECK(!res10.ok);
    CHECK(res10.status == SolveStatus::NoConflictFreeSchedule);
    CHECK(res10.diagnostics.validSchedulesFound == 0);
}

void TestBusySlotsAndDiagnostics() {
    const std::string busyJson = R"JSON({
        "busySlots": [
            { "day": "K", "start": "09:30", "end": "11:00", "label": "Trabajo", "source": "calendar" }
        ]
    })JSON";

    const BusySlotsParseResult parsedBusy = ParseBusySlotsJson(busyJson);
    CHECK(parsedBusy.ok);
    CHECK(parsedBusy.busySlots.size() == 1);
    CHECK(parsedBusy.busySlots[0].day == DayOfWeek::Tuesday);
    CHECK(parsedBusy.busySlots[0].labelUtf8 == "Trabajo");

    // Blocking Tuesday 09:30-11:00 eliminates FI1102 Gr 50, FI2103 Gr 50, MA1103 Gr 51
    // Remaining: EL2113 Gr50 + FI1102 Gr51 + FI2103 Gr51 + MA1103 {Gr50, Gr52} -> 2 valid schedules
    const ScheduleResult res = HorariosSolver::SolveFromJson(kHorariosTecFixtureJson, busyJson);
    CHECK(res.ok);
    CHECK(res.diagnostics.validSchedulesFound == 2);

    // Block both Tuesday morning AND Tuesday night -> FI1102 has 0 surviving groups
    const std::string blockAllFi1102 = R"JSON([
        { "day": "K", "start": "10:00", "end": "10:30", "label": "Reunión AM", "source": "work" },
        { "day": "K", "start": "18:30", "end": "19:00", "label": "Tutoría PM", "source": "work" }
    ])JSON";
    const ScheduleResult blocked = HorariosSolver::SolveFromJson(kHorariosTecFixtureJson, blockAllFi1102);
    CHECK(!blocked.ok);
    CHECK(blocked.status == SolveStatus::NoConflictFreeSchedule);
    CHECK(blocked.schedules.empty());
    CHECK(!blocked.diagnostics.blockingConflicts.empty());
    bool sawFi1102Diag = false;
    for (const auto& cd : blocked.diagnostics.courseDiagnostics) {
        if (cd.courseCodeUtf8 == "FI1102") {
            sawFi1102Diag = true;
            CHECK(cd.initialGroupCount == 2);
            CHECK(cd.afterFilterGroupCount == 2);
            CHECK(cd.afterBusySlotGroupCount == 0);
        }
    }
    CHECK(sawFi1102Diag);
    CHECK(!ParseBusySlotsJson("{bad").ok);
}

void TestFilters() {
    CourseCatalog cat;
    cat.campusUtf8 = "Cartago";

    Course c1;
    c1.codeUtf8 = "EL2113";
    c1.nameUtf8 = "Circuitos";
    c1.credits = 4;
    c1.groups.push_back({"G1", "1", "Regular", "Prof A", "Cartago", 0, 20, {{DayOfWeek::Monday, 480, 580, "", "", ""}}});
    c1.groups.push_back({"G2", "2", "Virtual", "Prof B", "San José", 5, 20, {{DayOfWeek::Monday, 600, 700, "", "", ""}}});
    c1.groups.push_back({"G3", "3", "Regular", "Prof C", "Cartago", -1, -1, {{DayOfWeek::Monday, 1080, 1180, "", "", ""}}});
    c1.groups.push_back({"G4", "4", "Regular", "Prof D", "Cartago", 10, 20, {{DayOfWeek::Tuesday, 540, 640, "", "", ""}}});

    Course c2;
    c2.codeUtf8 = "MA1101";
    c2.nameUtf8 = "Cálculo";
    c2.credits = 4;
    c2.groups.push_back({"M1", "5", "Regular", "Prof E", "Cartago", 3, 25, {{DayOfWeek::Wednesday, 540, 640, "", "", ""}}});

    cat.courses = {c1, c2};

    SolverPreferences prefs;
    prefs.filters.requiredCourseCodes = {"EL2113"};
    prefs.filters.allowedModalities = {"regular"};
    prefs.filters.allowedCampuses = {"cartago"};
    prefs.filters.excludedProfessors = {"prof a"};
    prefs.filters.requireAvailableSeats = true;
    prefs.filters.allowUnknownSeats = false;
    prefs.filters.excludeNightClasses = true;

    const ScheduleResult r1 = SolveSchedules(cat, {}, prefs);
    CHECK(r1.ok);
    CHECK(r1.schedules.size() == 1);
    CHECK(r1.schedules[0].selections.size() == 1);
    CHECK(r1.schedules[0].selections[0].group.idUtf8 == "G4");

    // allowedGroupIdsByCourse & excludedGroupIdsByCourse
    SolverPreferences prefs2;
    prefs2.filters.requiredCourseCodes = {"EL2113"};
    prefs2.filters.allowedGroupIdsByCourse["EL2113"] = {"G2", "G4"};
    prefs2.filters.excludedGroupIdsByCourse["EL2113"] = {"G4"};
    const ScheduleResult r2 = SolveSchedules(cat, {}, prefs2);
    CHECK(r2.ok && r2.schedules.size() == 1);
    CHECK(r2.schedules[0].selections[0].group.idUtf8 == "G2");

    // Unknown required course code -> InvalidInput
    SolverPreferences badPrefs;
    badPrefs.filters.requiredCourseCodes = {"NOPE9999"};
    const ScheduleResult badRes = SolveSchedules(cat, {}, badPrefs);
    CHECK(!badRes.ok);
    CHECK(badRes.status == SolveStatus::InvalidInput);
}

void TestMultiFactorScoringAndInvalidInputs() {
    // Compare two groups for Course X + Course Y:
    // G_compact: same day back-to-back (08:00-09:30 and 09:30-11:00 -> 1 active day, 0 gap, 180 span)
    // G_gapped:  same day with 2h gap  (08:00-09:30 and 11:30-13:00 -> 1 active day, 120 gap, 300 span)
    // G_spread:  different days         (08:00-09:30 Mon, 08:00-09:30 Tue -> 2 active days, 0 gap, 180 span)
    CourseCatalog cat;
    Course cFixed;
    cFixed.codeUtf8 = "C1";
    cFixed.nameUtf8 = "Base";
    cFixed.groups.push_back({"F1", "", "Regular", "", "", -1, -1, {{DayOfWeek::Monday, 480, 570, "", "", ""}}});

    Course cChoice;
    cChoice.codeUtf8 = "C2";
    cChoice.nameUtf8 = "Opciones";
    cChoice.groups.push_back({"Compact", "", "Regular", "", "", -1, -1, {{DayOfWeek::Monday, 570, 660, "", "", ""}}});
    cChoice.groups.push_back({"Gapped", "", "Regular", "", "", -1, -1, {{DayOfWeek::Monday, 690, 780, "", "", ""}}});
    cChoice.groups.push_back({"Spread", "", "Regular", "", "", -1, -1, {{DayOfWeek::Tuesday, 480, 570, "", "", ""}}});

    cat.courses = {cFixed, cChoice};

    const ScheduleResult res = SolveSchedules(cat);
    CHECK(res.ok);
    CHECK(res.schedules.size() == 3);
    CHECK(res.schedules[0].selections[1].group.idUtf8 == "Compact");
    CHECK(res.schedules[0].metrics.activeDaysCount == 1);
    CHECK(res.schedules[0].metrics.totalGapMinutes == 0);
    CHECK(res.schedules[0].metrics.totalSpanMinutes == 180);

    // Spread has 2 active days (+30+30 - 0.25*180 - 0 - 20*2 = 60 - 45 - 40 = -25)
    // Gapped  has 1 active day (+30+30 - 0.25*300 - 0.25*120 - 20*1 = 60 - 75 - 30 - 20 = -65)
    CHECK(res.schedules[1].selections[1].group.idUtf8 == "Spread");
    CHECK(res.schedules[2].selections[1].group.idUtf8 == "Gapped");
    CHECK(res.schedules[0].metrics.totalScore > res.schedules[1].metrics.totalScore);
    CHECK(res.schedules[1].metrics.totalScore > res.schedules[2].metrics.totalScore);

    // Invalid empty catalog
    CourseCatalog emptyCat;
    CHECK(SolveSchedules(emptyCat).status == SolveStatus::InvalidInput);

    // Invalid time range inside session
    CourseCatalog badTimeCat;
    Course badCourse;
    badCourse.codeUtf8 = "BAD1";
    badCourse.groups.push_back({"G1", "", "Regular", "", "", -1, -1, {{DayOfWeek::Monday, 600, 500, "", "", ""}}});
    badTimeCat.courses.push_back(badCourse);
    CHECK(SolveSchedules(badTimeCat).status == SolveStatus::InvalidInput);

    // Invalid BusySlot
    BusySlot badBusy{DayOfWeek::Monday, 700, 700, "Zero", "test"};
    CHECK(SolveSchedules(cat, {badBusy}).status == SolveStatus::InvalidInput);
}

void TestExporters() {
    CourseCatalog cat;
    Course c1;
    c1.codeUtf8 = "EL2113";
    c1.nameUtf8 = "Circuitos \"CC\", Parte 1\nBásica";
    c1.credits = 4;
    c1.groups.push_back({"Gr 50", "901", "Regular", "Dr. \"Ana\", Solís", "Cartago", 12, 30,
                         {{DayOfWeek::Wednesday, 970, 1075, "E1-02", "E1", "Cartago"}}});
    cat.courses.push_back(c1);

    const ScheduleResult res = SolveSchedules(cat);
    CHECK(res.ok && res.schedules.size() == 1);

    // JSON export (full + single, indented + compact)
    const std::string fullJson = ExportJson(res);
    CHECK(fullJson.find("\"status\": \"Ok\"") != std::string::npos);
    CHECK(fullJson.find("\"scheduleCount\": 1") != std::string::npos);
    CHECK(fullJson.find("\"courseCode\": \"EL2113\"") != std::string::npos);
    CHECK(fullJson.find("\"dayName\": \"Miércoles\"") != std::string::npos);
    CHECK(fullJson.find("\"diagnostics\"") != std::string::npos);
    CHECK(fullJson.find("\"metrics\"") != std::string::npos);

    JsonExportOptions compactOpts;
    compactOpts.indentSpaces = -1;
    compactOpts.includeMetrics = false;
    compactOpts.includeDiagnostics = false;
    const std::string compactJson = ExportJson(res.schedules[0], compactOpts);
    CHECK(compactJson.find('\n') == std::string::npos);
    CHECK(compactJson.find("\"metrics\"") == std::string::npos);
    CHECK(compactJson.find("\"rank\":1") != std::string::npos);

    // CSV export with BOM + RFC-4180 quoting
    const std::string csvBom = ExportCsv(res);
    CHECK(csvBom.size() > 3);
    CHECK(static_cast<unsigned char>(csvBom[0]) == 0xEF &&
          static_cast<unsigned char>(csvBom[1]) == 0xBB &&
          static_cast<unsigned char>(csvBom[2]) == 0xBF);
    CHECK(csvBom.find("Horario,Puntaje,Días Activos,Huecos (min),Código,Curso") != std::string::npos);
    // Double quotes doubled and embedded comma/newline wrapped in quotes
    CHECK(csvBom.find("\"Circuitos \"\"CC\"\", Parte 1\nBásica\"") != std::string::npos);
    CHECK(csvBom.find("\"Dr. \"\"Ana\"\", Solís\"") != std::string::npos);
    CHECK(csvBom.find("Miércoles,16:10,17:55,E1-02") != std::string::npos);

    // CSV without BOM or rank column, semicolon delimiter
    CsvExportOptions noBomOpts;
    noBomOpts.includeUtf8Bom = false;
    noBomOpts.includeRankColumn = false;
    noBomOpts.delimiter = ';';
    const std::string csvNoBom = ExportCsv(res.schedules[0], noBomOpts);
    CHECK(static_cast<unsigned char>(csvNoBom[0]) != 0xEF);
    CHECK(csvNoBom.rfind("Puntaje;Días Activos;", 0) == 0);
}

void TestMutualConflictDiagnostics() {
    CourseCatalog cat;
    Course cA;
    cA.codeUtf8 = "CS101";
    cA.groups.push_back({"G1", "", "Regular", "", "", -1, -1, {{DayOfWeek::Monday, 480, 600, "", "", ""}}});
    Course cB;
    cB.codeUtf8 = "CS102";
    cB.groups.push_back({"G2", "", "Regular", "", "", -1, -1, {{DayOfWeek::Monday, 540, 660, "", "", ""}}});
    cat.courses = {cA, cB};

    const ScheduleResult res = SolveSchedules(cat);
    CHECK(!res.ok);
    CHECK(res.status == SolveStatus::NoConflictFreeSchedule);
    CHECK(!res.diagnostics.blockingConflicts.empty());
    const ConflictDetail& cd = res.diagnostics.blockingConflicts[0];
    CHECK(cd.day == DayOfWeek::Monday);
    CHECK(cd.overlapStartMinute == 540);
    CHECK(cd.overlapEndMinute == 600);
}

void TestStressAndBudgets() {
    // 10 courses x 6 groups = 60,466,176 Cartesian combinations; pruning reduces search by >99.9%
    auto makeSlot = [](int slotIdx) -> TimeSlot {
        const auto day = static_cast<DayOfWeek>(slotIdx % 5);
        const int band = slotIdx / 5;
        const int startMin = 480 + band * 110;
        return TimeSlot{day, startMin, startMin + 95, "", "", ""};
    };

    CourseCatalog cat;
    for (int c = 0; c < 10; ++c) {
        Course course;
        course.codeUtf8 = "CUR" + std::to_string(c);
        course.nameUtf8 = "Curso " + std::to_string(c);
        course.credits = 3;
        for (int g = 0; g < 6; ++g) {
            Group grp;
            grp.idUtf8 = "G" + std::to_string(g);
            if (g == 0) {
                grp.sessions.push_back(makeSlot(2 * c));
                grp.sessions.push_back(makeSlot(2 * c + 1));
            } else if (g == 1) {
                grp.sessions.push_back(makeSlot(20 + (c % 5)));
                grp.sessions.push_back(makeSlot(2 * c + 1));
            } else {
                grp.sessions.push_back(makeSlot(g - 2));
                grp.sessions.push_back(makeSlot(5 + (c % 2)));
            }
            course.groups.push_back(std::move(grp));
        }
        cat.courses.push_back(std::move(course));
    }

    SolverPreferences prefs;
    prefs.maxResults = 15;
    prefs.maxSearchStates = 250000;

    const auto t0 = std::chrono::steady_clock::now();
    const ScheduleResult res = SolveSchedules(cat, {}, prefs);
    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0).count();

    CHECK(res.ok);
    CHECK(res.status == SolveStatus::Ok);
    CHECK(res.schedules.size() == 15);
    CHECK(res.diagnostics.prunedBranchesCount > 0);
    CHECK(!res.diagnostics.searchBudgetExhausted);
    CHECK(res.diagnostics.combinationsExplored < 250000);
    CHECK(elapsedMs < 3000);
    for (std::size_t i = 1; i < res.schedules.size(); ++i) {
        CHECK(res.schedules[i - 1].metrics.totalScore >= res.schedules[i].metrics.totalScore - 1e-9);
        CHECK(res.schedules[i].rank == i + 1);
    }

    // Tight budget cap test
    SolverPreferences tight;
    tight.maxResults = 10;
    tight.maxSearchStates = 5;
    const ScheduleResult capped = SolveSchedules(cat, {}, tight);
    CHECK(capped.status == SolveStatus::SearchBudgetExceeded);
    CHECK(capped.diagnostics.searchBudgetExhausted);
}

void TestStrictWeakOrderingAndWeightsValidation() {
    // 3 groups with scores {10.0, 10.0 + 0.6e-9, 10.0 + 1.2e-9} and {1, 2, 3} active days.
    // Test all 6 input permutations to verify strict weak ordering in std::sort.
    Group g1{"G1", "", "Regular", "", "", -1, -1, {{DayOfWeek::Monday, 480, 540, "", "", ""}}};
    Group g2{"G2", "", "Regular", "", "", -1, -1,
             {{DayOfWeek::Monday, 780, 840, "", "", ""}, {DayOfWeek::Tuesday, 780, 840, "", "", ""}}};
    Group g3{"G3", "", "Regular", "", "", -1, -1,
             {{DayOfWeek::Monday, 780, 840, "", "", ""},
              {DayOfWeek::Tuesday, 780, 840, "", "", ""},
              {DayOfWeek::Wednesday, 1020, 1080, "", "", ""}}};

    SolverPreferences prefs;
    prefs.weights.morningWeight = 10.0;
    prefs.weights.afternoonWeight = 5.0 + 0.3e-9;
    prefs.weights.nightWeight = 0.6e-9;
    prefs.weights.compactnessWeight = 0.0;
    prefs.weights.gapPenaltyPerMinute = 0.0;
    prefs.weights.activeDayPenalty = 0.0;

    std::vector<int> perm = {0, 1, 2};
    const std::vector<Group> baseGroups = {g1, g2, g3};
    int permCount = 0;
    do {
        CourseCatalog cat;
        Course c;
        c.codeUtf8 = "SWO101";
        c.nameUtf8 = "Orden Estricto";
        c.groups = {baseGroups[static_cast<std::size_t>(perm[0])],
                    baseGroups[static_cast<std::size_t>(perm[1])],
                    baseGroups[static_cast<std::size_t>(perm[2])]};
        cat.courses.push_back(c);

        const ScheduleResult res = SolveSchedules(cat, {}, prefs);
        CHECK(res.ok);
        CHECK(res.schedules.size() == 3);
        if (res.schedules.size() == 3) {
            CHECK(res.schedules[0].selections[0].group.idUtf8 == "G3");
            CHECK(res.schedules[1].selections[0].group.idUtf8 == "G2");
            CHECK(res.schedules[2].selections[0].group.idUtf8 == "G1");
        }
        ++permCount;
    } while (std::next_permutation(perm.begin(), perm.end()));
    CHECK(permCount == 6);

    // Non-finite ScoringWeights and negative minBreak -> InvalidInput
    CourseCatalog simpleCat;
    Course simpleCourse;
    simpleCourse.codeUtf8 = "FIN101";
    simpleCourse.groups.push_back(g1);
    simpleCat.courses.push_back(simpleCourse);

    const double badValues[] = {
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()
    };
    for (double badVal : badValues) {
        for (int field = 0; field < 6; ++field) {
            SolverPreferences badPrefs;
            switch (field) {
                case 0: badPrefs.weights.morningWeight = badVal; break;
                case 1: badPrefs.weights.afternoonWeight = badVal; break;
                case 2: badPrefs.weights.nightWeight = badVal; break;
                case 3: badPrefs.weights.compactnessWeight = badVal; break;
                case 4: badPrefs.weights.gapPenaltyPerMinute = badVal; break;
                case 5: badPrefs.weights.activeDayPenalty = badVal; break;
            }
            const ScheduleResult badRes = SolveSchedules(simpleCat, {}, badPrefs);
            CHECK(!badRes.ok);
            CHECK(badRes.status == SolveStatus::InvalidInput);
        }
    }

    SolverPreferences negBreak;
    negBreak.minBreakMinutesBetweenClasses = -1;
    CHECK(SolveSchedules(simpleCat, {}, negBreak).status == SolveStatus::InvalidInput);
}

void TestIntraGroupOverlapAndDuplicateCourseCodes() {
    // Probe A1: intra-group overlapping sessions -> InvalidInput
    CourseCatalog selfOverlapCat;
    Course selfCourse;
    selfCourse.codeUtf8 = "SELF101";
    selfCourse.groups.push_back({"G1", "", "Regular", "", "", -1, -1,
                                 {{DayOfWeek::Monday, 480, 600, "", "", ""},
                                  {DayOfWeek::Monday, 540, 660, "", "", ""}}});
    selfOverlapCat.courses.push_back(selfCourse);
    const ScheduleResult rSelf = SolveSchedules(selfOverlapCat);
    CHECK(!rSelf.ok);
    CHECK(rSelf.status == SolveStatus::InvalidInput);
    CHECK(!rSelf.diagnostics.errorsUtf8.empty());
    if (!rSelf.diagnostics.errorsUtf8.empty()) {
        CHECK(rSelf.diagnostics.errorsUtf8[0].find("SELF101") != std::string::npos);
        CHECK(rSelf.diagnostics.errorsUtf8[0].find("G1") != std::string::npos);
    }

    // Probe A2: intra-group duplicate identical sessions -> InvalidInput
    CourseCatalog dupSessCat;
    Course dupSessCourse;
    dupSessCourse.codeUtf8 = "DUP_SESS";
    dupSessCourse.groups.push_back({"G2", "", "Regular", "", "", -1, -1,
                                    {{DayOfWeek::Tuesday, 480, 570, "", "", ""},
                                     {DayOfWeek::Tuesday, 480, 570, "", "", ""}}});
    dupSessCat.courses.push_back(dupSessCourse);
    const ScheduleResult rDupSess = SolveSchedules(dupSessCat);
    CHECK(!rDupSess.ok);
    CHECK(rDupSess.status == SolveStatus::InvalidInput);

    // Touching sessions within the same group remain valid (minBreak applies between distinct courses & busySlots)
    CourseCatalog touchingIntraCat;
    Course touchingCourse;
    touchingCourse.codeUtf8 = "BRK_INTRA";
    touchingCourse.groups.push_back({"G3", "", "Regular", "", "", -1, -1,
                                     {{DayOfWeek::Wednesday, 480, 540, "", "", ""},
                                      {DayOfWeek::Wednesday, 540, 600, "", "", ""}}});
    touchingIntraCat.courses.push_back(touchingCourse);
    SolverPreferences breakPrefs;
    breakPrefs.minBreakMinutesBetweenClasses = 15;
    const ScheduleResult rTouch = SolveSchedules(touchingIntraCat, {}, breakPrefs);
    CHECK(rTouch.ok);
    CHECK(rTouch.status == SolveStatus::Ok);

    // Duplicate course codes (case/whitespace insensitive) -> InvalidInput
    CourseCatalog dupCourseCat;
    Course ma1;
    ma1.codeUtf8 = "MA1101";
    ma1.groups.push_back({"G1", "", "Regular", "", "", -1, -1, {{DayOfWeek::Monday, 480, 540, "", "", ""}}});
    Course ma2;
    ma2.codeUtf8 = " ma1101 ";
    ma2.groups.push_back({"G2", "", "Regular", "", "", -1, -1, {{DayOfWeek::Tuesday, 480, 540, "", "", ""}}});
    dupCourseCat.courses = {ma1, ma2};

    const ScheduleResult rDupAll = SolveSchedules(dupCourseCat);
    CHECK(!rDupAll.ok);
    CHECK(rDupAll.status == SolveStatus::InvalidInput);

    SolverPreferences reqMa;
    reqMa.filters.requiredCourseCodes = {"MA1101"};
    const ScheduleResult rDupReq = SolveSchedules(dupCourseCat, {}, reqMa);
    CHECK(!rDupReq.ok);
    CHECK(rDupReq.status == SolveStatus::InvalidInput);
}

void TestConflictDetailNonInvertedAndMaxResultsSafety() {
    // ConflictDetail with minBreakMinutesBetweenClasses = 10 on 5-min gap (09:40 vs 09:45)
    CourseCatalog cat;
    Course cA;
    cA.codeUtf8 = "CS1";
    cA.groups.push_back({"G1", "", "Regular", "", "", -1, -1, {{DayOfWeek::Monday, 480, 580, "", "", ""}}});
    Course cB;
    cB.codeUtf8 = "CS2";
    cB.groups.push_back({"G2", "", "Regular", "", "", -1, -1, {{DayOfWeek::Monday, 585, 690, "", "", ""}}});
    cat.courses = {cA, cB};

    SolverPreferences breakPrefs;
    breakPrefs.minBreakMinutesBetweenClasses = 10;
    const ScheduleResult res = SolveSchedules(cat, {}, breakPrefs);
    CHECK(!res.ok);
    CHECK(res.status == SolveStatus::NoConflictFreeSchedule);
    CHECK(!res.diagnostics.blockingConflicts.empty());
    if (!res.diagnostics.blockingConflicts.empty()) {
        const ConflictDetail& cd = res.diagnostics.blockingConflicts[0];
        CHECK(cd.overlapStartMinute <= cd.overlapEndMinute);
        CHECK(cd.overlapStartMinute == 580);
        CHECK(cd.overlapEndMinute == 585);
    }

    // BusySlot ConflictDetail with minBreak = 10
    CourseCatalog singleCat;
    singleCat.courses.push_back(cA);
    BusySlot busy{DayOfWeek::Monday, 585, 660, "Almuerzo", "test"};
    const ScheduleResult resBusy = SolveSchedules(singleCat, {busy}, breakPrefs);
    CHECK(!resBusy.ok);
    CHECK(!resBusy.diagnostics.blockingConflicts.empty());
    if (!resBusy.diagnostics.blockingConflicts.empty()) {
        const ConflictDetail& cd = resBusy.diagnostics.blockingConflicts[0];
        CHECK(cd.overlapStartMinute <= cd.overlapEndMinute);
        CHECK(cd.overlapStartMinute == 580);
        CHECK(cd.overlapEndMinute == 585);
    }

    // maxResults = SIZE_MAX / 8 + 1 with 8^4 = 4096 valid schedules -> no std::length_error
    CourseCatalog largeCat;
    for (int c = 0; c < 4; ++c) {
        Course course;
        course.codeUtf8 = "BIG" + std::to_string(c);
        for (int g = 0; g < 8; ++g) {
            course.groups.push_back({"G" + std::to_string(g), "", "Regular", "", "", -1, -1,
                                     {{static_cast<DayOfWeek>(c), 480, 540, "", "", ""}}});
        }
        largeCat.courses.push_back(std::move(course));
    }
    SolverPreferences hugeMaxPrefs;
    hugeMaxPrefs.maxResults = (std::numeric_limits<std::size_t>::max() / 8) + 1;
    const ScheduleResult hugeRes = SolveSchedules(largeCat, {}, hugeMaxPrefs);
    CHECK(hugeRes.ok);
    CHECK(hugeRes.schedules.size() == 4096);

    // Global Top-K past trimThreshold = 4096: 5 courses x 6 groups = 7776 valid schedules, best G5 is last
    CourseCatalog topKCat;
    for (int c = 0; c < 5; ++c) {
        Course course;
        course.codeUtf8 = "TK" + std::to_string(c);
        for (int g = 0; g < 6; ++g) {
            const int startMin = (g == 5) ? 480 : 1080;
            course.groups.push_back({"G" + std::to_string(g), "", "Regular", "", "", -1, -1,
                                     {{static_cast<DayOfWeek>(c), startMin, startMin + 60, "", "", ""}}});
        }
        topKCat.courses.push_back(std::move(course));
    }
    SolverPreferences topKPrefs;
    topKPrefs.maxResults = 3;
    const ScheduleResult topKRes = SolveSchedules(topKCat, {}, topKPrefs);
    CHECK(topKRes.ok);
    CHECK(topKRes.diagnostics.validSchedulesFound == 7776);
    CHECK(topKRes.schedules.size() == 3);
    if (!topKRes.schedules.empty()) {
        for (const auto& sel : topKRes.schedules[0].selections) {
            CHECK(sel.group.idUtf8 == "G5");
        }
    }

    // MRV + pruning vs independent exhaustive enumeration on 4 courses x 3 groups
    CourseCatalog mrvCat;
    for (int c = 0; c < 4; ++c) {
        Course course;
        course.codeUtf8 = "MRV" + std::to_string(c);
        course.groups.push_back({"A", "", "Regular", "", "", -1, -1, {{DayOfWeek::Monday, 480 + c * 60, 540 + c * 60, "", "", ""}}});
        course.groups.push_back({"B", "", "Regular", "", "", -1, -1, {{DayOfWeek::Monday, 480, 540, "", "", ""}}});
        course.groups.push_back({"C", "", "Regular", "", "", -1, -1, {{DayOfWeek::Tuesday, 480 + (c % 2) * 30, 540 + (c % 2) * 30, "", "", ""}}});
        mrvCat.courses.push_back(std::move(course));
    }
    SolverPreferences mrvPrefs;
    mrvPrefs.maxResults = 0;
    mrvPrefs.minBreakMinutesBetweenClasses = 5;
    const ScheduleResult mrvRes = SolveSchedules(mrvCat, {}, mrvPrefs);

    std::size_t bruteValidCount = 0;
    for (const auto& g0 : mrvCat.courses[0].groups) {
        for (const auto& g1 : mrvCat.courses[1].groups) {
            if (TimeSlotsConflict(g0.sessions[0].day, g0.sessions[0].startMinute, g0.sessions[0].endMinute,
                                  g1.sessions[0].day, g1.sessions[0].startMinute, g1.sessions[0].endMinute, 5)) {
                continue;
            }
            for (const auto& g2 : mrvCat.courses[2].groups) {
                if (TimeSlotsConflict(g0.sessions[0].day, g0.sessions[0].startMinute, g0.sessions[0].endMinute,
                                      g2.sessions[0].day, g2.sessions[0].startMinute, g2.sessions[0].endMinute, 5) ||
                    TimeSlotsConflict(g1.sessions[0].day, g1.sessions[0].startMinute, g1.sessions[0].endMinute,
                                      g2.sessions[0].day, g2.sessions[0].startMinute, g2.sessions[0].endMinute, 5)) {
                    continue;
                }
                for (const auto& g3 : mrvCat.courses[3].groups) {
                    if (TimeSlotsConflict(g0.sessions[0].day, g0.sessions[0].startMinute, g0.sessions[0].endMinute,
                                          g3.sessions[0].day, g3.sessions[0].startMinute, g3.sessions[0].endMinute, 5) ||
                        TimeSlotsConflict(g1.sessions[0].day, g1.sessions[0].startMinute, g1.sessions[0].endMinute,
                                          g3.sessions[0].day, g3.sessions[0].startMinute, g3.sessions[0].endMinute, 5) ||
                        TimeSlotsConflict(g2.sessions[0].day, g2.sessions[0].startMinute, g2.sessions[0].endMinute,
                                          g3.sessions[0].day, g3.sessions[0].startMinute, g3.sessions[0].endMinute, 5)) {
                        continue;
                    }
                    ++bruteValidCount;
                }
            }
        }
    }
    CHECK(mrvRes.diagnostics.validSchedulesFound == bruteValidCount);
    CHECK(mrvRes.schedules.size() == bruteValidCount);
}

void TestParserOverflowSchemaAndExporterEdgeCases() {
    // Integer overflow strings & uint64 wraparound rejection
    CHECK(!ParseCatalogJson(R"([{"code":"O1","credits":"2147483648"}])").ok);
    CHECK(!ParseCatalogJson(R"([{"code":"O2","credits":"99999999999999999999"}])").ok);
    CHECK(!ParseCatalogJson(R"([{"code":"O3","credits":"-2147483649"}])").ok);
    CHECK(!ParseCatalogJson(R"([{"code":"O4","credits":18446744073709551615}])").ok);
    CHECK(!ParseCatalogJson(R"([{"code":"O5","groups":[{"id":"1","sessions":[{"day":"L","startMinute":"2147483648","endMinute":600}]}]}])").ok);

    // Unsigned 64-bit group ID preserved without wrapping to "-1"
    const CatalogParseResult u64Grp = ParseCatalogJson(R"([{"code":"O6","groups":[{"id":18446744073709551615}]}])");
    CHECK(u64Grp.ok);
    if (u64Grp.ok && !u64Grp.catalog.courses.empty() && !u64Grp.catalog.courses[0].groups.empty()) {
        CHECK(u64Grp.catalog.courses[0].groups[0].idUtf8 == "18446744073709551615");
    }

    // Schema consistency: non-string name/title & bad seats object fields rejected
    CHECK(!ParseCatalogJson(R"([{"code":"N1","name":false}])").ok);
    CHECK(!ParseCatalogJson(R"([{"code":"N2","title":123}])").ok);
    CHECK(!ParseCatalogJson(R"([{"code":"S1","groups":[{"id":"1","seats":{"available":"bad"}}]}])").ok);
    CHECK(!ParseCatalogJson(R"([{"code":"S2","groups":[{"id":"1","seats":{"available":5,"capacity":"bad"}}]}])").ok);

    // FormatTimeHHMM negative minutes
    CHECK(FormatTimeHHMM(-5) == "-00:05");
    CHECK(FormatTimeHHMM(-75) == "-01:15");

    // ExportJson with non-UTF-8 bytes does not throw and produces valid replacement U+FFFD
    CourseCatalog badUtf8Cat;
    Course badUtf8Course;
    badUtf8Course.codeUtf8 = "FI1101";
    badUtf8Course.nameUtf8 = "F\xEDsica\rConCR";
    badUtf8Course.groups.push_back({"G1", "", "Regular", "", "", -1, -1,
                                    {{DayOfWeek::Monday, 480, 540, "", "", ""}}});
    badUtf8Cat.courses.push_back(badUtf8Course);

    SolverPreferences fracPrefs;
    fracPrefs.weights.morningWeight = 12.5;
    fracPrefs.weights.compactnessWeight = 0.0;
    fracPrefs.weights.gapPenaltyPerMinute = 0.0;
    fracPrefs.weights.activeDayPenalty = 0.0;

    const ScheduleResult nonUtf8Res = SolveSchedules(badUtf8Cat, {}, fracPrefs);
    CHECK(nonUtf8Res.ok && nonUtf8Res.schedules.size() == 1);

    bool threwJson = false;
    std::string safeJsonFull;
    std::string safeJsonSingle;
    try {
        safeJsonFull = ExportJson(nonUtf8Res);
        safeJsonSingle = ExportJson(nonUtf8Res.schedules[0]);
    } catch (...) {
        threwJson = true;
    }
    CHECK(!threwJson);
    CHECK(safeJsonFull.find("\xEF\xBF\xBD") != std::string::npos);
    CHECK(safeJsonSingle.find("\xEF\xBF\xBD") != std::string::npos);

    // ExportCsv quotes fields containing '\r'
    const std::string csvCr = ExportCsv(nonUtf8Res);
    CHECK(csvCr.find("\"F\xEDsica\rConCR\"") != std::string::npos);

    // ExportCsv on empty ScheduleResult returns header only
    ScheduleResult emptyRes;
    CsvExportOptions noBom;
    noBom.includeUtf8Bom = false;
    const std::string emptyCsv = ExportCsv(emptyRes, noBom);
    CHECK(emptyCsv.rfind("Horario,Puntaje,", 0) == 0);
    CHECK(emptyCsv.find('\n') == emptyCsv.size() - 1);

    // ExportCsv score formatting is immune to global comma-decimal C++ locale
    struct CommaPunct : std::numpunct<char> {
        char do_decimal_point() const override { return ','; }
    };
    struct LocaleGuard {
        std::locale prev;
        LocaleGuard() : prev(std::locale::global(std::locale(std::locale::classic(), new CommaPunct()))) {}
        ~LocaleGuard() { std::locale::global(prev); }
    } localeGuard;

    const std::string localeSafeCsv = ExportCsv(nonUtf8Res, noBom);
    CHECK(localeSafeCsv.find("1,12.5,") != std::string::npos);
}

void TestCampusFilterPerSessionAndGroupFilterDeterminism() {
    auto solveSingleGroupCampus = [](const std::string& catalogCampus,
                                     const std::string& groupCampus,
                                     const std::vector<std::string>& sessionCampuses,
                                     const std::vector<std::string>& allowedCampuses) {
        CourseCatalog cat;
        cat.campusUtf8 = catalogCampus;
        Course course;
        course.codeUtf8 = "CAMP101";
        course.nameUtf8 = "Campus";
        Group grp;
        grp.idUtf8 = "G1";
        grp.modalityUtf8 = "Regular";
        for (std::size_t i = 0; i < sessionCampuses.size(); ++i) {
            grp.sessions.push_back(TimeSlot{
                static_cast<DayOfWeek>(i % 5),
                480 + static_cast<int>(i) * 120,
                540 + static_cast<int>(i) * 120,
                "",
                "",
                sessionCampuses[i]});
        }
        grp.campusUtf8 = groupCampus;
        course.groups.push_back(std::move(grp));
        cat.courses.push_back(std::move(course));

        SolverPreferences prefs;
        prefs.filters.allowedCampuses = allowedCampuses;
        return SolveSchedules(cat, {}, prefs);
    };

    // BUG-6 Probes G1, G2, G3: explicit non-matching session campus must not fall back to catalog.campusUtf8
    const ScheduleResult probeG1 = solveSingleGroupCampus("Cartago", "", {"", "San José"}, {"Cartago"});
    CHECK(!probeG1.ok);
    CHECK(probeG1.status == SolveStatus::NoConflictFreeSchedule);

    const ScheduleResult probeG2 = solveSingleGroupCampus("Cartago", "", {"Alajuela", "San José"}, {"Cartago"});
    CHECK(!probeG2.ok);
    CHECK(probeG2.status == SolveStatus::NoConflictFreeSchedule);

    const ScheduleResult probeG3 = solveSingleGroupCampus("Cartago", "", {"Cartago", "San José"}, {"Cartago"});
    CHECK(!probeG3.ok);
    CHECK(probeG3.status == SolveStatus::NoConflictFreeSchedule);

    // Positive multi-campus match: Cartago + San José accepts G1 and G3, rejects G2
    CHECK(solveSingleGroupCampus("Cartago", "", {"", "San José"}, {"Cartago", "San José"}).ok);
    CHECK(solveSingleGroupCampus("Cartago", "", {"Cartago", "San José"}, {"Cartago", "San José"}).ok);
    CHECK(!solveSingleGroupCampus("Cartago", "", {"Alajuela", "San José"}, {"Cartago", "San José"}).ok);

    // group.campusUtf8 = "Cartago" with empty session campuses matches {"Cartago"};
    // session override to "San José" overrides group.campusUtf8 and fails {"Cartago"} while matching {"Cartago", "San José"}
    CHECK(solveSingleGroupCampus("Alajuela", "Cartago", {"", ""}, {"Cartago"}).ok);
    CHECK(!solveSingleGroupCampus("Cartago", "Cartago", {"", "San José"}, {"Cartago"}).ok);
    CHECK(solveSingleGroupCampus("Cartago", "Cartago", {"", "San José"}, {"Cartago", "San José"}).ok);

    // Empty sessions fallback to group.campusUtf8, else catalog.campusUtf8
    CHECK(solveSingleGroupCampus("Cartago", "", {}, {"Cartago"}).ok);
    CHECK(!solveSingleGroupCampus("Cartago", "Alajuela", {}, {"Cartago"}).ok);
    CHECK(!solveSingleGroupCampus("", "", {}, {"Cartago"}).ok);

    // Deterministic normalized map lookup for allowedGroupIdsByCourse & excludedGroupIdsByCourse
    CourseCatalog grpCat;
    Course ma;
    ma.codeUtf8 = "MA1101";
    ma.nameUtf8 = "Cálculo";
    ma.groups.push_back({"G1", "", "Regular", "", "", -1, -1, {{DayOfWeek::Monday, 480, 540, "", "", ""}}});
    ma.groups.push_back({"G2", "", "Regular", "", "", -1, -1, {{DayOfWeek::Tuesday, 480, 540, "", "", ""}}});
    ma.groups.push_back({"G3", "", "Regular", "", "", -1, -1, {{DayOfWeek::Wednesday, 480, 540, "", "", ""}}});
    ma.groups.push_back({"G4", "", "Regular", "", "", -1, -1, {{DayOfWeek::Thursday, 480, 540, "", "", ""}}});
    grpCat.courses.push_back(ma);

    SolverPreferences mixPrefs;
    mixPrefs.filters.allowedGroupIdsByCourse["MA1101"] = {"G1"};
    mixPrefs.filters.allowedGroupIdsByCourse["ma1101"] = {"G2"};
    mixPrefs.filters.allowedGroupIdsByCourse[" Ma1101 "] = {"G3"};
    mixPrefs.filters.excludedGroupIdsByCourse["MA1101"] = {"G1"};
    mixPrefs.filters.excludedGroupIdsByCourse[" ma1101 "] = {"G2"};
    mixPrefs.maxResults = 0;

    const ScheduleResult mixRes = SolveSchedules(grpCat, {}, mixPrefs);
    CHECK(mixRes.ok);
    CHECK(mixRes.schedules.size() == 1);
    if (mixRes.schedules.size() == 1) {
        CHECK(mixRes.schedules[0].selections[0].group.idUtf8 == "G3");
    }

    // Same mixed-case map works when course.codeUtf8 itself uses lowercase
    grpCat.courses[0].codeUtf8 = "ma1101";
    const ScheduleResult lowerCourseRes = SolveSchedules(grpCat, {}, mixPrefs);
    CHECK(lowerCourseRes.ok);
    CHECK(lowerCourseRes.schedules.size() == 1);
    if (lowerCourseRes.schedules.size() == 1) {
        CHECK(lowerCourseRes.schedules[0].selections[0].group.idUtf8 == "G3");
    }
}

void TestUtf8SpanishCaseFolding() {
    // 1. Campus filters: exact-accent UTF-8 case folding ("SAN JOSÉ" == "San José", "LIMÓN" == "Limón")
    //    while preserving accent distinction ("San Jose" != "San José").
    auto solveCampus = [](const std::string& sessionCampus, const std::vector<std::string>& allowedCampuses) {
        CourseCatalog cat;
        Course c;
        c.codeUtf8 = "CI1001";
        c.nameUtf8 = "Campus UTF-8";
        c.groups.push_back({"G1", "", "Regular", "Docente", "", -1, -1,
                            {{DayOfWeek::Monday, 480, 540, "", "", sessionCampus}}});
        cat.courses.push_back(std::move(c));

        SolverPreferences prefs;
        prefs.filters.allowedCampuses = allowedCampuses;
        return SolveSchedules(cat, {}, prefs);
    };

    CHECK(solveCampus("San José", {"SAN JOSÉ"}).ok);
    CHECK(solveCampus("SAN JOSÉ", {"San José"}).ok);
    CHECK(solveCampus("Limón", {"LIMÓN"}).ok);
    CHECK(solveCampus("LIMÓN", {"limón"}).ok);
    CHECK(!solveCampus("San José", {"San Jose"}).ok);
    CHECK(!solveCampus("San Jose", {"SAN JOSÉ"}).ok);

    // 2. Modality & excludedProfessors filters with Á, É, Í, Ó, Ú, Ü, Ñ
    CourseCatalog modProfCat;
    Course mp;
    mp.codeUtf8 = "EL3001";
    mp.nameUtf8 = "Modalidad y Profesores";
    mp.groups.push_back({"G1", "", "Híbrida", "Álvarez, María", "San José", -1, -1,
                         {{DayOfWeek::Monday, 480, 540, "", "", ""}}});
    mp.groups.push_back({"G2", "", "HÍBRIDA", "Muñoz, José", "San José", -1, -1,
                         {{DayOfWeek::Tuesday, 480, 540, "", "", ""}}});
    mp.groups.push_back({"G3", "", "híbrida", "Güell, Andrés", "San José", -1, -1,
                         {{DayOfWeek::Wednesday, 480, 540, "", "", ""}}});
    mp.groups.push_back({"G4", "", "Híbrida", "Pérez, Óscar", "San José", -1, -1,
                         {{DayOfWeek::Thursday, 480, 540, "", "", ""}}});
    mp.groups.push_back({"G5", "", "Regular", "Pérez, Óscar", "San José", -1, -1,
                         {{DayOfWeek::Friday, 480, 540, "", "", ""}}});
    modProfCat.courses.push_back(mp);

    SolverPreferences mpPrefs;
    mpPrefs.filters.allowedModalities = {"HÍBRIDA"};
    mpPrefs.filters.allowedCampuses = {"SAN JOSÉ"};
    mpPrefs.filters.excludedProfessors = {"ÁLVAREZ, MARÍA", "MUÑOZ, JOSÉ", "GÜELL, ANDRÉS"};
    const ScheduleResult mpRes = SolveSchedules(modProfCat, {}, mpPrefs);
    CHECK(mpRes.ok);
    CHECK(mpRes.schedules.size() == 1);
    if (mpRes.schedules.size() == 1) {
        CHECK(mpRes.schedules[0].selections[0].group.idUtf8 == "G4");
    }

    // Verify accent-preserving behavior for professor exclusion ("Alvarez, Maria" != "Álvarez, María")
    SolverPreferences noAccentPrefs;
    noAccentPrefs.filters.allowedModalities = {"HÍBRIDA"};
    noAccentPrefs.filters.excludedProfessors = {"Alvarez, Maria", "MUÑOZ, JOSÉ", "GÜELL, ANDRÉS", "PÉREZ, ÓSCAR"};
    const ScheduleResult noAccentRes = SolveSchedules(modProfCat, {}, noAccentPrefs);
    CHECK(noAccentRes.ok);
    CHECK(noAccentRes.schedules.size() == 1);
    if (noAccentRes.schedules.size() == 1) {
        CHECK(noAccentRes.schedules[0].selections[0].group.idUtf8 == "G1");
    }

    // 3. Course & group filter maps with accented Spanish letters ("ING-Ñ1" vs "ing-ñ1", "GR-Ñ1" vs "Gr-ñ1")
    CourseCatalog spanishMapCat;
    Course cN;
    cN.codeUtf8 = "ing-ñ1";
    cN.nameUtf8 = "Ingeniería";
    cN.groups.push_back({"Gr-ñ1", "", "Híbrida", "Úbeda, Íñigo", "Limón", -1, -1,
                         {{DayOfWeek::Monday, 480, 540, "", "", ""}}});
    cN.groups.push_back({"Gr-á2", "", "Híbrida", "Úbeda, Íñigo", "Limón", -1, -1,
                         {{DayOfWeek::Tuesday, 480, 540, "", "", ""}}});
    cN.groups.push_back({"Gr-ú3", "", "Híbrida", "Úbeda, Íñigo", "Limón", -1, -1,
                         {{DayOfWeek::Wednesday, 480, 540, "", "", ""}}});
    spanishMapCat.courses.push_back(cN);

    SolverPreferences mapPrefs;
    mapPrefs.filters.requiredCourseCodes = {"ING-Ñ1"};
    mapPrefs.filters.allowedGroupIdsByCourse["ING-Ñ1"] = {"GR-Ñ1", "GR-Á2"};
    mapPrefs.filters.excludedGroupIdsByCourse["ing-ñ1"] = {"gr-á2"};
    const ScheduleResult mapRes = SolveSchedules(spanishMapCat, {}, mapPrefs);
    CHECK(mapRes.ok);
    CHECK(mapRes.schedules.size() == 1);
    if (mapRes.schedules.size() == 1) {
        CHECK(mapRes.schedules[0].selections[0].group.idUtf8 == "Gr-ñ1");
    }

    // Duplicate course code detection with UTF-8 Spanish case difference -> InvalidInput
    CourseCatalog dupSpanishCat;
    Course dupA;
    dupA.codeUtf8 = "ING-Ñ1";
    dupA.groups.push_back({"G1", "", "Regular", "", "", -1, -1, {{DayOfWeek::Monday, 480, 540, "", "", ""}}});
    Course dupB;
    dupB.codeUtf8 = "ing-ñ1";
    dupB.groups.push_back({"G2", "", "Regular", "", "", -1, -1, {{DayOfWeek::Tuesday, 480, 540, "", "", ""}}});
    dupSpanishCat.courses = {dupA, dupB};
    CHECK(SolveSchedules(dupSpanishCat).status == SolveStatus::InvalidInput);

    // 4. Malformed / truncated trailing 0xC3 UTF-8 byte safety (no OOB read or crash)
    CourseCatalog malformedCat;
    Course badLead;
    badLead.codeUtf8 = "BAD\xC3";
    badLead.groups.push_back({"G1", "", "Á\xC3", "É\xC3", "San\xC3", -1, -1,
                              {{DayOfWeek::Monday, 480, 540, "", "", ""}}});
    malformedCat.courses.push_back(badLead);

    SolverPreferences malformedMatchPrefs;
    malformedMatchPrefs.filters.requiredCourseCodes = {"bad\xC3"};
    malformedMatchPrefs.filters.allowedModalities = {"á\xC3"};
    malformedMatchPrefs.filters.allowedCampuses = {"SAN\xC3"};
    const ScheduleResult malformedMatchRes = SolveSchedules(malformedCat, {}, malformedMatchPrefs);
    CHECK(malformedMatchRes.ok);
    CHECK(malformedMatchRes.schedules.size() == 1);

    SolverPreferences malformedRejectPrefs;
    malformedRejectPrefs.filters.allowedCampuses = {"San"};
    malformedRejectPrefs.filters.excludedProfessors = {"é\xC3"};
    CHECK(!SolveSchedules(malformedCat, {}, malformedRejectPrefs).ok);

    // Lone trailing 0xC3
    CHECK(!solveCampus("\xC3", {"San"}).ok);
    CHECK(solveCampus("\xC3", {"\xC3"}).ok);
}

} // namespace

int main() {
    TestHorariosTecFixtureLegacyParity();
    TestCatalogParsingAndHelpers();
    TestConflictDetectionAndBoundaries();
    TestBusySlotsAndDiagnostics();
    TestFilters();
    TestMultiFactorScoringAndInvalidInputs();
    TestExporters();
    TestMutualConflictDiagnostics();
    TestStressAndBudgets();
    TestStrictWeakOrderingAndWeightsValidation();
    TestIntraGroupOverlapAndDuplicateCourseCodes();
    TestConflictDetailNonInvertedAndMaxResultsSafety();
    TestParserOverflowSchemaAndExporterEdgeCases();
    TestCampusFilterPerSessionAndGroupFilterDeterminism();
    TestUtf8SpanishCaseFolding();

    std::cout << "academic_horarios_tec_tests: " << gPasses << " passed, " << gFailures << " failed.\n";
    return (gFailures == 0) ? 0 : 1;
}
