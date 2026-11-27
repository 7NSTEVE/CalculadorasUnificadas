#pragma once

// Funciones experimentales de CuadernU (revisión de código del 27-nov-2026,
// docs/INVESTIGACION_COMPETENCIA_2026.md).
//
// Son comandos cuyo resultado es simulado o que pueden cambiar lo escrito sin
// pedirlo. Con «Mostrar funciones experimentales» apagado (valor por defecto)
// no aparecen en los menús, la barra ni Explorar (Ctrl+K), y si llegan por un
// atajo solo explican por qué están escondidos.

#include "studio_command_ids.hpp"
#include "../core/knowledge_graph.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace DocStudio::Experimental {

struct ExperimentalCommand {
    int commandId;
    const char* reasonUtf8; // una línea: qué hace mal
};

inline constexpr ExperimentalCommand kCommands[] = {
    { CmdIds::CMD_HANDWRITING_SPELLING,   "borra tu tinta y pone una letra sint\xC3\xA9tica con la primera sugerencia, sin preguntar" },
    { CmdIds::CMD_FLEXCIL_GESTURE_MODE,   "convierte cada trazo en una marca y no se puede deshacer" },
    { CmdIds::CMD_AUDIO_SPEED_CYCLE,      "muestra la velocidad, pero el audio no la aplica" },
    { CmdIds::CMD_AUDIO_DIARIZE,          "separa como m\xC3\xA1ximo 2 \xC2\xABhablantes\xC2\xBB por la altura de la voz" },
    { CmdIds::CMD_MEETING_MINUTES_ICS,    "saca tareas por palabras clave y, si no encuentra, convierte la \xC3\xBAltima frase en tarea" },
    { CmdIds::CMD_AUTO_EXTRACT_TASKS_ICS, "usa la misma b\xC3\xBAsqueda por palabras clave que las actas" },
    { CmdIds::CMD_LAN_QR,                 "el QR apunta a 127.0.0.1, donde no escucha nada" },
    { CmdIds::CMD_SHARE_LINKS_MGR,        "crea enlaces que no tienen ning\xC3\xBAn servidor detr\xC3\xA1s" },
    { CmdIds::CMD_TRANSLATE_SELECTION,    "sin un proveedor de IA solo usa un glosario de 20 frases" },
    { CmdIds::CMD_OPEN_VAULT_LIBRARY,     "la \xC2\xAB""b\xC3\xBAsqueda global\xC2\xBB solo ve lo indexado en esta sesi\xC3\xB3n y no guarda el \xC3\xADndice" },
    { CmdIds::CMD_NOTE_COVER_PAPER,       "el color de portada se guarda pero no se dibuja" },
    { CmdIds::CMD_REPLACE_TEXT,           "no reemplaza el texto del PDF: pone una caja blanca en una posici\xC3\xB3n fija" },
    { CmdIds::CMD_SCAN_RECTIFY_TO_PDF,    "usa esquinas fijas: no detecta el documento ni deja mover las esquinas" },
    { CmdIds::CMD_PDF_SCROLL_DIRECTION,   "la opci\xC3\xB3n se guarda pero nada la usa" },
};

constexpr const ExperimentalCommand* Find(int commandId) noexcept {
    for (const auto& c : kCommands) {
        if (c.commandId == commandId) return &c;
    }
    return nullptr;
}

constexpr bool IsExperimentalCommand(int commandId) noexcept { return Find(commandId) != nullptr; }

constexpr std::size_t Count() noexcept { return std::size(kCommands); }

// Vistas de la base de datos que muestran los datos mal (índices de CMD_CYCLE_DB_VIEW):
// 3 Kanban (un clic mueve siempre el primer registro), 4 Calendario (rejilla fija del
// 1 al 31) y 5 Cronograma (no escala por fecha).
constexpr bool IsExperimentalDbView(int viewIndex) noexcept {
    return viewIndex == 3 || viewIndex == 4 || viewIndex == 5;
}

// Siguiente vista de la base de datos que se puede mostrar (salta las experimentales
// cuando están escondidas). Recorre las 9 vistas en orden circular.
constexpr int NextDbView(int current, bool showExperimental) noexcept {
    int v = current;
    for (int step = 0; step < 9; ++step) {
        v = (v + 1) % 9;
        if (showExperimental || !IsExperimentalDbView(v)) return v;
    }
    return 0;
}

// Quita de Explorar (Ctrl+K) los comandos experimentales.
inline void RemoveFromPalette(CommandPaletteRegistry& palette) {
    std::vector<CommandPaletteEntry> kept;
    kept.reserve(palette.All().size());
    for (const auto& e : palette.All()) {
        if (!IsExperimentalCommand(e.commandId)) kept.push_back(e);
    }
    palette.Clear();
    for (auto& e : kept) palette.Register(std::move(e));
}

#ifdef _WIN32
// Quita de un menú (y de sus submenús) los comandos experimentales. Borra también
// los submenús que quedan vacíos y los separadores sobrantes.
inline void PruneMenu(HMENU menu) {
    if (!menu) return;
    for (int i = GetMenuItemCount(menu) - 1; i >= 0; --i) {
        MENUITEMINFOW mii{};
        mii.cbSize = sizeof(mii);
        mii.fMask = MIIM_ID | MIIM_SUBMENU | MIIM_FTYPE;
        if (!GetMenuItemInfoW(menu, static_cast<UINT>(i), TRUE, &mii)) continue;
        if (mii.hSubMenu) {
            PruneMenu(mii.hSubMenu);
            if (GetMenuItemCount(mii.hSubMenu) == 0) DeleteMenu(menu, static_cast<UINT>(i), MF_BYPOSITION);
        } else if (!(mii.fType & MFT_SEPARATOR) && IsExperimentalCommand(static_cast<int>(mii.wID))) {
            DeleteMenu(menu, static_cast<UINT>(i), MF_BYPOSITION);
        }
    }
    // Separadores al principio, al final o repetidos
    bool prevSep = true;
    for (int i = 0; i < GetMenuItemCount(menu);) {
        MENUITEMINFOW mii{};
        mii.cbSize = sizeof(mii);
        mii.fMask = MIIM_FTYPE;
        const bool isSep = GetMenuItemInfoW(menu, static_cast<UINT>(i), TRUE, &mii) && (mii.fType & MFT_SEPARATOR);
        if (isSep && prevSep) {
            DeleteMenu(menu, static_cast<UINT>(i), MF_BYPOSITION);
            continue;
        }
        prevSep = isSep;
        ++i;
    }
    const int n = GetMenuItemCount(menu);
    if (n > 0) {
        MENUITEMINFOW mii{};
        mii.cbSize = sizeof(mii);
        mii.fMask = MIIM_FTYPE;
        if (GetMenuItemInfoW(menu, static_cast<UINT>(n - 1), TRUE, &mii) && (mii.fType & MFT_SEPARATOR)) {
            DeleteMenu(menu, static_cast<UINT>(n - 1), MF_BYPOSITION);
        }
    }
}
#endif

} // namespace DocStudio::Experimental
