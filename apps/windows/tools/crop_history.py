from PIL import Image

full_path = r"D:\OneDrive\Desktop\Calculadoras (blanca y gris)\Calculadora Unificada - Core Oficial\verification\extreme_math_gallery\03_03_extreme_deep_parentheses_65_full.png"
img = Image.open(full_path)
w, h = img.size
print("Width:", w, "Height:", h)

# Right sidebar is roughly the last 300 pixels
right_sidebar = img.crop((w - 280, 50, w, h - 50))
right_sidebar.save(r"D:\OneDrive\Desktop\Calculadoras (blanca y gris)\Calculadora Unificada - Core Oficial\verification\extreme_math_gallery\step3_history.png")
print("Saved step3_history.png")
