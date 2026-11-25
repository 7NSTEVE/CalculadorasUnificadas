import os

path = r"D:\OneDrive\Desktop\Calculadoras (blanca y gris)\Calculadora Unificada - Core Oficial\main.cpp"
with open(path, "r", encoding="utf-8") as f:
    text = f.read()

target = "        case 'X':        return 18;  // X_THETA\n        default: break;"
# try CRLF
target_crlf = "        case 'X':        return 18;  // X_THETA\r\n        default: break;"

replacement = """        case 'X':        return 18;  // X_THETA
        case 0x88: case VK_OEM_4: return 26;  // PAREN_A '('
        case 0x89: case VK_OEM_5: case VK_OEM_6: return 27;  // PAREN_C ')'
        case 0x8A:       return 16;  // POT '^'
        case 0x8B:       return 24;  // FRAC
        case 0x8C:       return 25;  // SD
        case 0x8D: case VK_OEM_COMMA: return 28;  // COMA ','
        case 0x8E:       return 47;  // EXP (x10^x)
        case 0x8F:       return 48;  // ANS
        case 0x92:       return 15;  // X^2
        default: break;"""

if target_crlf in text:
    rep = replacement.replace("\n", "\r\n")
    text = text.replace(target_crlf, rep, 1)
    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write(text)
    print("SUCCESS_CRLF")
elif target in text:
    text = text.replace(target, replacement, 1)
    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write(text)
    print("SUCCESS_LF")
else:
    print("FAILED TO MATCH")
