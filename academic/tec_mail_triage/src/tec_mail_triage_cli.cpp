#include "academic/tec_mail_triage/triage_engine.hpp"

#include <nlohmann/json.hpp>

#include <iostream>
#include <string>

using namespace academic::tec_mail_triage;

int main(int argc, char** argv) {
    const std::string cmd = (argc >= 2) ? argv[1] : "info";
    if (cmd == "info") {
        nlohmann::json out = {
            { "module", "academic_tec_mail_triage" },
            { "standard", "ISO C23 + ISO C++23" },
            { "migrated_from", "E:/Proyectos/CuadernU/Componentes_DotNet/componentes/tec" },
            { "notebooklm_max_per_notebook", NotebookLmBundler::kMaxPerNotebook }
        };
        std::cout << out.dump(2) << "\n";
        return 0;
    }
    std::cerr << "Uso: tec_mail_triage_cli [info]\n";
    return 2;
}
