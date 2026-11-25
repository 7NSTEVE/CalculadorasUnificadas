#pragma once

// ============================================================================
// keymap_config.hpp
// Configurable keyboard shortcut & pen pressure/stabilization profile loader
// (F-257, F-268). Supports deterministic collision detection and guaranteed
// safe fallback on corrupt/missing keymap.json.
// ============================================================================

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace DocStudio {

struct KeymapProfile {
    std::map<std::string, std::string> actionToShortcut;
    double penStabilization = 0.65;
    double penPressureGamma = 1.0;
    double penMinWidthPx = 0.8;
    double penMaxWidthPx = 8.0;
    bool usedFallback = false;
    std::vector<std::string> warnings;

    static KeymapProfile Default() {
        KeymapProfile p;
        p.actionToShortcut = {
            {"file.open", "Ctrl+O"},
            {"file.save", "Ctrl+S"},
            {"file.export_pdf", "Ctrl+Shift+E"},
            {"file.print", "Ctrl+P"},
            {"edit.undo", "Ctrl+Z"},
            {"edit.redo", "Ctrl+Y"},
            {"edit.find", "Ctrl+F"},
            {"edit.delete", "Delete"},
            {"tool.hand", "V"},
            {"tool.pen", "P"},
            {"tool.highlighter", "H"},
            {"tool.eraser", "E"},
            {"tool.lasso", "L"},
            {"tool.laser", "K"},
            {"tool.textbox", "T"},
            {"tool.shape_rect", "R"},
            {"tool.shape_arrow", "A"},
            {"stem.math_block", "Ctrl+M"},
            {"stem.circuit_block", "Ctrl+Shift+C"},
            {"stem.plot_block", "Ctrl+Shift+P"},
            {"stem.check_my_work", "Ctrl+Shift+K"},
            {"study.flashcards", "Ctrl+Shift+F"},
            {"study.audio_record", "Ctrl+Shift+R"},
            {"view.zoom_in", "Ctrl+="},
            {"view.zoom_out", "Ctrl+-"},
            {"view.zoom_fit", "Ctrl+0"},
            {"view.focus_mode", "F11"},
            {"view.command_palette", "Ctrl+K"},
            {"academic.tec_digital", "Ctrl+Shift+T"},
            {"academic.horarios", "Ctrl+Shift+H"}
        };
        return p;
    }

    static KeymapProfile LoadWithFallback(const std::string& pathUtf8) {
        KeymapProfile eff = Default();
        if (pathUtf8.empty()) return eff;

        std::ifstream ifs(std::filesystem::u8path(pathUtf8), std::ios::binary);
        if (!ifs.is_open()) {
            eff.usedFallback = true;
            eff.warnings.push_back("keymap_missing_fallback_default");
            return eff;
        }
        std::ostringstream oss;
        oss << ifs.rdbuf();
        nlohmann::json j;
        try {
            j = nlohmann::json::parse(oss.str());
        } catch (const std::exception& ex) {
            eff.usedFallback = true;
            eff.warnings.push_back(std::string("corrupt_keymap_fallback_default: ") + ex.what());
            return eff;
        }
        return FromJson(j);
    }

    static KeymapProfile FromJson(const nlohmann::json& j) {
        KeymapProfile eff = Default();
        if (!j.is_object()) {
            eff.usedFallback = true;
            eff.warnings.push_back("keymap_root_not_object_fallback_default");
            return eff;
        }

        const nlohmann::json* bindings = nullptr;
        if (j.contains("bindings") && j["bindings"].is_object()) {
            bindings = &j["bindings"];
        } else if (j.contains("shortcuts") && j["shortcuts"].is_object()) {
            bindings = &j["shortcuts"];
        } else {
            bindings = &j;
        }

        for (auto it = bindings->begin(); it != bindings->end(); ++it) {
            if (!it.value().is_string()) continue;
            const std::string action = it.key();
            const std::string sc = it.value().get<std::string>();
            if (sc.empty()) continue;

            bool collides = false;
            for (const auto& existing : eff.actionToShortcut) {
                if (existing.first != action && existing.second == sc) {
                    collides = true;
                    break;
                }
            }
            if (collides) {
                eff.warnings.push_back("shortcut_collision_rejected:" + action + "=" + sc);
                continue;
            }
            eff.actionToShortcut[action] = sc;
        }

        if (j.contains("pen") && j["pen"].is_object()) {
            const auto& pen = j["pen"];
            if (pen.contains("stabilization") && pen["stabilization"].is_number()) {
                eff.penStabilization = std::clamp(pen["stabilization"].get<double>(), 0.0, 1.0);
            }
            if (pen.contains("pressure_gamma") && pen["pressure_gamma"].is_number()) {
                eff.penPressureGamma = std::clamp(pen["pressure_gamma"].get<double>(), 0.25, 4.0);
            }
            if (pen.contains("min_width_px") && pen["min_width_px"].is_number()) {
                eff.penMinWidthPx = std::clamp(pen["min_width_px"].get<double>(), 0.2, 16.0);
            }
            if (pen.contains("max_width_px") && pen["max_width_px"].is_number()) {
                eff.penMaxWidthPx = std::clamp(pen["max_width_px"].get<double>(), 0.5, 40.0);
            }
        }
        return eff;
    }

    nlohmann::json ToJson() const {
        nlohmann::json bindings = nlohmann::json::object();
        for (const auto& kv : actionToShortcut) {
            bindings[kv.first] = kv.second;
        }
        return nlohmann::json{
            {"schema_version", 1},
            {"used_fallback", usedFallback},
            {"warnings", warnings},
            {"bindings", bindings},
            {"pen", {
                {"stabilization", penStabilization},
                {"pressure_gamma", penPressureGamma},
                {"min_width_px", penMinWidthPx},
                {"max_width_px", penMaxWidthPx}
            }}
        };
    }

    bool ExportToFile(const std::string& pathUtf8) const {
        std::error_code ec;
        const auto p = std::filesystem::u8path(pathUtf8);
        if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path(), ec);
        std::ofstream ofs(p, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open()) return false;
        const std::string dump = ToJson().dump(2);
        ofs.write(dump.data(), static_cast<std::streamsize>(dump.size()));
        return ofs.good();
    }
};

} // namespace DocStudio
