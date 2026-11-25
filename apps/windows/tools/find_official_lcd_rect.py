from PIL import Image

im = Image.open(r"D:\OneDrive\Desktop\Calculadoras (blanca y gris)\Calculadora Unificada - Core Oficial\verification\official_manager_gallery\01_01_oficial_main_menu_inicial_full.png")
w, h = im.size

# Look for the black inner bezel or screen boundary
# Find min_x, max_x, min_y, max_y of the display
# The display has white background at y=200, colors at 240..460
screen_pts = []
for y in range(150, 500):
    for x in range(20, 400):
        p = im.getpixel((x, y))
        # inside the LCD, there are white background pixels (255, 255, 255), colored pixels, etc.
        # but outside is the gray calculator body (e.g. 77, 77, 77)
        if (p[0] > 200 and p[1] > 200 and p[2] > 200) or (p[0] != p[1] or p[1] != p[2]):
            screen_pts.append((x, y))

if screen_pts:
    min_x = min(p[0] for p in screen_pts)
    max_x = max(p[0] for p in screen_pts)
    min_y = min(p[1] for p in screen_pts)
    max_y = max(p[1] for p in screen_pts)
    print(f"LCD BOX: ({min_x}, {min_y}, {max_x}, {max_y}) - Size: {max_x - min_x} x {max_y - min_y}")
    crop = im.crop((min_x, min_y, max_x, max_y))
    crop.save(r"D:\OneDrive\Desktop\Calculadoras (blanca y gris)\Calculadora Unificada - Core Oficial\verification\official_manager_gallery\detected_official_lcd.png")
