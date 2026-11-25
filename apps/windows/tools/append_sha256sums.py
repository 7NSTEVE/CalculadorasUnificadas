import hashlib
import os

files_to_hash = [
    "cg50_backend.cpp",
    "cg50_gcal_profile.h",
    "tests/cg50_backend_contract_test.cpp",
    "build-probe32/Release/GCalHostProbe.exe",
    "build-probe32/Release/Cg50BackendContractTest.exe",
    "build-verify/Release/runtime/SystemROM.raw",
    "build-verify/Release/runtime/UserROM.raw",
    "third_party/rom-native-reference/build-probe-x64/Release/Cg50RomProbe.exe",
    "verification/gui-verify-2026-10-17/gui-v3-canvas.png",
    "verification/gui-verify-2026-10-17/gui-lcd-crop.png",
    "verification/gui-verify-2026-10-17/gui-lcd-crop-3x.png",
    "verification/gui-verify-2026-10-17/ocr-lcd-crop-3x.txt",
    "verification/native-boot-tail.trace"
]

manifest_path = r"verification\2026-10-16\SHA256SUMS.txt"
lines_to_add = [
    "\n## Sesión Fase 2 (ROM RAW) + Fase 4 (Shutdown/Double-Init) + Fase 5 (verify_gui JSON) (2026-10-17)\n"
]

for rel_path in files_to_hash:
    norm_path = os.path.normpath(rel_path)
    if os.path.exists(norm_path):
        data = open(norm_path, "rb").read()
        sha = hashlib.sha256(data).hexdigest().lower()
        size = len(data)
        line = f"{sha}  {size:10d} B  {norm_path}\n"
        print(line.strip())
        lines_to_add.append(line)
    else:
        print(f"MISSING: {norm_path}")

with open(manifest_path, "a", encoding="utf-8") as f:
    f.writelines(lines_to_add)

print("\nAppended successfully to", manifest_path)
