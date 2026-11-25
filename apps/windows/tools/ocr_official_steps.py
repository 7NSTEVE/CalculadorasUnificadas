from PIL import Image
import os
import subprocess

ocr_ps1 = r"D:\OneDrive\Desktop\Calculadoras (blanca y gris)\Calculadora Unificada - Core Oficial\tools\ocr_png.ps1"
gdir = r"D:\OneDrive\Desktop\Calculadoras (blanca y gris)\Calculadora Unificada - Core Oficial\verification\official_manager_gallery"

for step in ["04_04_oficial_runmat_potencia_2a10", "06_06_oficial_runmat_trigonometria_cos0", "07_07_oficial_runmat_logaritmo_log1000", "08_08_oficial_runmat_precision_1_sobre_7", "09_09_oficial_runmat_error_division_por_cero"]:
    p = os.path.join(gdir, f"{step}_full.png")
    if not os.path.exists(p):
        continue
    im = Image.open(p)
    crop = im.crop((20, 150, 400, 500))
    crop_path = os.path.join(gdir, f"{step}_crop.png")
    crop.save(crop_path)
    res = subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", ocr_ps1, crop_path],
                         capture_output=True, text=True)
    print(f"=== {step} ===")
    for line in res.stdout.splitlines():
        if "===" not in line and "OCR" not in line and line.strip():
            print("  ", line.strip())
