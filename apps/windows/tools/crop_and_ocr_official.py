from PIL import Image
import os
import subprocess

p = r"D:\OneDrive\Desktop\Calculadoras (blanca y gris)\Calculadora Unificada - Core Oficial\verification\official_manager_gallery\03_03_oficial_runmat_aritmetica_compleja_full.png"
out_crop = r"D:\OneDrive\Desktop\Calculadoras (blanca y gris)\Calculadora Unificada - Core Oficial\verification\official_manager_gallery\step3_calc_crop.png"

im = Image.open(p)
# crop LCD region
crop = im.crop((20, 150, 400, 500))
crop.save(out_crop)

ocr_ps1 = r"D:\OneDrive\Desktop\Calculadoras (blanca y gris)\Calculadora Unificada - Core Oficial\tools\ocr_png.ps1"
res = subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", ocr_ps1, out_crop],
                     capture_output=True, text=True)
print("=== OCR OUTPUT ===")
print(res.stdout)
