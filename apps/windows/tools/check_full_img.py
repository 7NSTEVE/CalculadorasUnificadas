from PIL import Image

full_path = r"D:\OneDrive\Desktop\Calculadoras (blanca y gris)\Calculadora Unificada - Core Oficial\verification\extreme_math_gallery\03_03_extreme_deep_parentheses_65_full.png"
img = Image.open(full_path)
print("Full size:", img.size)

# The window has width=2004, height=1330 (or similar)
# Let's crop the bottom or right where the key history is displayed
# In main.cpp, g_hHistoryWnd is located at bottom or right
