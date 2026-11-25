#pragma once
#include <windows.h>

struct OfficialPdfManualInfo {
    const wchar_t* fileName;
    const wchar_t* shortButtonLabel;
    const wchar_t* fullTitle;
    const wchar_t* modelCategory; // Blanca, Gris, CG50, 991CW
    const wchar_t* manualKind;    // Hardware / Software
    int            catId;         // 1=Blanca/991CW, 2=Gris, 3=CG50, 4=Software
    int            calcMask;      // bitmask: 1=Blanca, 2=Gris, 4=CG50, 8=991CW
    RECT           btnRect{};
};

static OfficialPdfManualInfo g_officialManuals[] = {
    // Blanca / Negra ClassWiz & fx-991CW
    { L"fx-570CW_991CW_ES.pdf", L"991CW: Manual Completo (ES)", L"Gu\u00EDa del Usuario Oficial fx-570CW / fx-991CW (Espa\u00F1ol, 158 p\u00E1g.)", L"Negra (fx-991CW)", L"Usuario / Hardware & Software (ES)", 1, 8 },
    { L"fx-570CW_991CW_EN.pdf", L"991CW: User's Guide (EN)", L"Official User's Guide fx-570CW / fx-991CW (English, 151 pp.)", L"Negra (fx-991CW)", L"User's Guide / Hardware & Software (EN)", 1, 8 },
    { L"fx-570_991LA_X_ES.pdf", L"Blanca: Hardware LA X (ES)", L"Gu\u00EDa del Usuario Oficial fx-570LA X / fx-991LA X (Espa\u00F1ol, 49 p\u00E1g.)", L"Blanca (ClassWiz)", L"Hardware / Usuario (ES)", 1, 1 },
    { L"fx-570_991EX_ES.pdf", L"Blanca: Hardware EX (ES)", L"Gu\u00EDa del Usuario Oficial fx-570EX / fx-991EX (Espa\u00F1ol, 48 p\u00E1g.)", L"Blanca (ClassWiz)", L"Hardware / Usuario (ES)", 1, 1 },
    { L"fx-570_991EX_EN.pdf", L"Blanca: Hardware EX (EN)", L"Official User's Guide fx-570EX / fx-991EX (English, 46 pp.)", L"Blanca (ClassWiz)", L"Hardware / User (EN)", 1, 1 },
    { L"ClassWiz_Emulator_Software_Users_Guide_ES.pdf", L"ClassWiz: Software Emulador", L"Gu\u00EDa del Usuario Software ClassWiz Emulator Subscription v2.0 (88 p\u00E1g.)", L"Blanca/Negra (ClassWiz)", L"Software Emulador", 4, 1 | 8 },
    { L"CASIO_Activation_Guide_ES.pdf", L"ClassWiz: Gu\u00EDa Activaci\u00F3n", L"Gu\u00EDa Oficial de Activaci\u00F3n y Licencia Emulador CASIO (112 p\u00E1g.)", L"Blanca/Negra (ClassWiz)", L"Software Licencia", 4, 1 | 2 | 8 },

    // Gris Natural-V.P.A.M.
    { L"fx-570_991ES_PLUS_ES.pdf", L"Gris: Hardware ES+ (ES)", L"Gu\u00EDa del Usuario Oficial fx-570ES PLUS / fx-991ES PLUS (Espa\u00F1ol, 48 p\u00E1g.)", L"Gris (ES/VN PLUS)", L"Hardware / Usuario (ES)", 2, 2 },
    { L"fx-570_991ES_PLUS_EN.pdf", L"Gris: Hardware ES+ (EN)", L"Official User's Guide fx-570ES PLUS / fx-991ES PLUS (English, 46 pp.)", L"Gris (ES/VN PLUS)", L"Hardware / User (EN)", 2, 2 },
    { L"fx-570VN_PLUS_Hardware_Users_Guide.pdf", L"Gris: Hardware VN PLUS", L"Manual Oficial Completo fx-570VN PLUS Natural-V.P.A.M. (62 p\u00E1g.)", L"Gris (ES/VN PLUS)", L"Hardware / Usuario", 2, 2 },

    // Graficadora fx-CG50 PRIZM
    { L"fx-CG50_Soft_v370_ES.pdf", L"CG50: Manual Completo (ES)", L"Gu\u00EDa del Usuario Completa Software y Funciones fx-CG50 OS v3.70 (Espa\u00F1ol, 678 p\u00E1g.)", L"Graficadora (fx-CG50)", L"Software / Funciones (ES)", 3, 4 },
    { L"fx-CG50_Soft_v370_EN.pdf", L"CG50: Complete Guide (EN)", L"Complete Software & Operations User's Guide fx-CG50 OS v3.70 (English, 677 pp.)", L"Graficadora (fx-CG50)", L"Software / Functions (EN)", 3, 4 },
    { L"fx-CG50_Hard_ES.pdf", L"CG50: Gu\u00EDa Hardware (ES)", L"Gu\u00EDa del Usuario de Hardware fx-CG50 (Espa\u00F1ol, 17 p\u00E1g.)", L"Graficadora (fx-CG50)", L"Hardware F\u00EDsico (ES)", 3, 4 },
    { L"fx-CG50_Hard_EN.pdf", L"CG50: Hardware Guide (EN)", L"Hardware User's Guide fx-CG50 PRIZM (English, 17 pp.)", L"Graficadora (fx-CG50)", L"Physical Hardware (EN)", 3, 4 },
    { L"fx-CG_Manager_PLUS_Software_Users_Guide_EN.pdf", L"CG50: Emulador Manager+", L"Software User's Guide fx-CG Manager PLUS Subscription (45 pp.)", L"Graficadora (fx-CG50)", L"Software Emulador", 4, 4 },
    { L"fx-CG_Manager_PLUS_Activation_Guide.pdf", L"CG50: Activaci\u00F3n Manager+", L"Gu\u00EDa Oficial de Activaci\u00F3n fx-CG Manager PLUS (270 p\u00E1g.)", L"Graficadora (fx-CG50)", L"Software Licencia", 4, 4 },
};
