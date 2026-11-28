#include "academic/matricula_tec/client.hpp"
#include "academic/matricula_tec/protocol.hpp"
#include "academic/matricula_tec/scenario_engine.hpp"
#include "academic/matricula_tec/vault.hpp"

#include <iostream>
#include <string>

using namespace academic::matricula_tec;

int main(int argc, char** argv) {
    const std::string cmd = (argc >= 2) ? std::string(argv[1]) : "status";
    if (cmd == "status") {
        nlohmann::json out = {
            { "module", "academic_matricula_tec" },
            { "standard", "ISO C23 + ISO C++23" },
            { "default_base_url", kDefaultBaseUrl },
            { "vault_path", DefaultVaultPathUtf8() },
            { "vault_exists", ProtectedCredentialVault::DpapiFileExists(DefaultVaultPathUtf8()) },
            { "scenarios_path", DefaultScenariosPathUtf8() }
        };
        std::cout << out.dump(2) << "\n";
        return 0;
    }
    if (cmd == "probe-tls") {
        MatriculaClient client;
        int http_status = 0;
        bool tls_ok = false;
        std::string err;
        const bool ok = client.ProbePortalReachability(&http_status, &tls_ok, &err);
        nlohmann::json out = {
            { "ok", ok },
            { "http_status", http_status },
            { "tls_verified", tls_ok },
            { "server_offset_ms", client.ServerClockOffsetMs() },
            { "error", err }
        };
        std::cout << out.dump(2) << "\n";
        return ok ? 0 : 1;
    }
    std::cerr << "Uso: matricula_tec_cli [status|probe-tls]\n";
    return 2;
}
