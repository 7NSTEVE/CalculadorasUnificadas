from PIL import Image
import os

img_path = r"D:\OneDrive\Desktop\Calculadoras (blanca y gris)\Calculadora Unificada - Core Oficial\verification\extreme_math_gallery\03_03_extreme_deep_parentheses_65_lcd.png"
if os.path.exists(img_path):
    img = Image.open(img_path)
    print("Dimensions:", img.size)
    print("Mode:", img.mode)
