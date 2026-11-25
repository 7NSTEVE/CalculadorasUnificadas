#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <iomanip>
#include <windows.h>
#include <conio.h>

using namespace std;

enum Day { LUNES = 0, MARTES = 1, MIERCOLES = 2, JUEVES = 3, VIERNES = 4, SABADO = 5 };

string dayNames[] = {"L", "K", "M", "J", "V", "S"};

struct Time {
    int h, m;
    bool operator<(const Time& o) const {
        if(h != o.h) return h < o.h;
        return m < o.m;
    }
    bool operator<=(const Time& o) const {
        if(h != o.h) return h < o.h;
        return m <= o.m;
    }
    bool operator>=(const Time& o) const {
        if(h != o.h) return h > o.h;
        return m >= o.m;
    }
    bool operator>(const Time& o) const {
        return !(*this <= o);
    }
};

struct Session {
    Day day;
    Time start;
    Time end;
    bool overlaps(const Session& o) const {
        if (day != o.day) return false;
        if (end <= o.start || o.end <= start) return false;
        return true;
    }
};

struct Group {
    string id;
    string modality;
    vector<Session> sessions;
};

struct Course {
    string name;
    string code;
    int color;
    vector<Group> groups;
};

struct ScheduleCombination {
    vector<pair<Course, Group>> selection;
    int score;
    bool valid;
};

vector<Course> getCourses() {
    vector<Course> courses;

    Course el2113{"Circuitos Electricos en CC", "EL2113", 10}; // Verde brillante
    el2113.groups.push_back({"Gr 50", "Regular", {
        {MIERCOLES, {16, 10}, {17, 55}},
        {VIERNES, {7, 55}, {9, 40}}
    }});
    courses.push_back(el2113);

    Course fi1102{"Fisica General II", "FI1102", 11}; // Cyan claro
    fi1102.groups.push_back({"Gr 50", "Regular", {
        {MARTES, {9, 45}, {11, 30}},
        {VIERNES, {9, 45}, {11, 30}}
    }});
    fi1102.groups.push_back({"Gr 51", "Regular", {
        {MARTES, {18, 0}, {19, 45}},
        {JUEVES, {18, 0}, {19, 45}}
    }});
    courses.push_back(fi1102);

    Course fi2103{"Fisica General III", "FI2103", 12}; // Rojo claro
    fi2103.groups.push_back({"Gr 50", "Regular", {
        {MARTES, {9, 45}, {11, 30}},
        {MIERCOLES, {9, 45}, {11, 30}}
    }});
    fi2103.groups.push_back({"Gr 51", "Semipresencial", {
        {MIERCOLES, {14, 20}, {16, 5}},
        {VIERNES, {14, 20}, {16, 5}}
    }});
    courses.push_back(fi2103);

    Course ma1103{"Calculo y Algebra Lineal", "MA1103", 14}; // Amarillo claro
    ma1103.groups.push_back({"Gr 50", "Semipresencial", {
        {MIERCOLES, {7, 55}, {9, 40}},
        {JUEVES, {14, 20}, {16, 5}}
    }});
    ma1103.groups.push_back({"Gr 51", "Semipresencial", {
        {MARTES, {9, 45}, {11, 30}},
        {JUEVES, {9, 45}, {11, 30}}
    }});
    ma1103.groups.push_back({"Gr 52", "Semipresencial", {
        {MIERCOLES, {9, 45}, {11, 30}},
        {VIERNES, {9, 45}, {11, 30}}
    }});
    courses.push_back(ma1103);

    return courses;
}

int calculateScore(const vector<pair<Course, Group>>& selection) {
    int score = 0;
    for (auto& cg : selection) {
        for (auto& s : cg.second.sessions) {
            if (s.start.h < 12) score += 30;       // Excelente (manana)
            else if (s.start.h < 17) score -= 10;  // Regular (tarde)
            else score -= 50;                      // Terrible (noche)
        }
    }
    return score;
}

bool hasConflict(const vector<pair<Course, Group>>& selection) {
    vector<Session> allSessions;
    for (auto& cg : selection) {
        for (auto& s : cg.second.sessions) {
            allSessions.push_back(s);
        }
    }
    for (size_t i = 0; i < allSessions.size(); i++) {
        for (size_t j = i + 1; j < allSessions.size(); j++) {
            if (allSessions[i].overlaps(allSessions[j])) {
                return true;
            }
        }
    }
    return false;
}

vector<ScheduleCombination> validCombos;

void generate(const vector<Course>& courses, size_t courseIdx, vector<pair<Course, Group>>& currentSel) {
    if (courseIdx == courses.size()) {
        if (!hasConflict(currentSel)) {
            ScheduleCombination sc;
            sc.selection = currentSel;
            sc.score = calculateScore(currentSel);
            sc.valid = true;
            validCombos.push_back(sc);
        }
        return;
    }
    
    for (auto& g : courses[courseIdx].groups) {
        currentSel.push_back({courses[courseIdx], g});
        generate(courses, courseIdx + 1, currentSel);
        currentSel.pop_back();
    }
}

void setColor(int color) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

void hideCursor() {
    HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO info;
    info.dwSize = 100;
    info.bVisible = FALSE;
    SetConsoleCursorInfo(consoleHandle, &info);
}

void setCursorPosition(int x, int y) {
    HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD coord = { (short)x, (short)y };
    SetConsoleCursorPosition(consoleHandle, coord);
}

void renderSchedule(const ScheduleCombination& sc, int index, int total) {
    setCursorPosition(0, 0); // Avoid blinking by drawing over
    setColor(15);
    cout << "==========================================================================" << endl;
    cout << "                     VISUALIZADOR DE HORARIOS TEC                         " << endl;
    cout << "==========================================================================" << endl;
    cout << " Horario " << (index + 1) << " de " << total << "  |  Puntaje (Heuristica de Manana): ";
    if (sc.score > 0) setColor(10);
    else setColor(12);
    cout << left << setw(5) << sc.score;
    setColor(15);
    cout << "                  " << endl;
    cout << " [<-] Horario anterior | [->] Siguiente horario | [ESC] Salir             " << endl;
    cout << "--------------------------------------------------------------------------" << endl;

    // Render courses summary
    for (auto& cg : sc.selection) {
        setColor(cg.first.color);
        cout << " " << cg.first.code << " " << left << setw(28) << cg.first.name 
             << " - " << left << setw(6) << cg.second.id 
             << " (" << cg.second.modality << ")" << string(15, ' ') << endl;
    }
    setColor(15);
    cout << "--------------------------------------------------------------------------" << endl;
    cout << " Hora    | Lunes      | Martes     | Miercoles  | Jueves     | Viernes    |" << endl;
    cout << "--------------------------------------------------------------------------" << endl;

    for (int h = 7; h <= 20; h++) {
        for (int m = 0; m < 60; m += 30) {
            Time curTime = {h, m};
            Time nextTime = {h, m + 30};
            if(m == 30) { nextTime = {h + 1, 0}; }

            setColor(8);
            cout << " " << setfill('0') << setw(2) << h << ":" << setw(2) << m << "   |";
            
            for (int d = 0; d < 5; d++) {
                bool printed = false;
                for (auto& cg : sc.selection) {
                    for (auto& s : cg.second.sessions) {
                        // Check if current 30-min block falls inside the session
                        if (s.day == d && curTime >= s.start && curTime < s.end) {
                            setColor(cg.first.color);
                            cout << " " << left << setfill(' ') << setw(10) << cg.first.code << " |";
                            printed = true;
                            break;
                        }
                    }
                    if(printed) break;
                }
                if (!printed) {
                    setColor(15);
                    cout << "            |";
                }
            }
            cout << endl;
        }
    }
}

int main() {
    hideCursor();
    system("cls"); // Initial clear
    vector<Course> courses = getCourses();
    vector<pair<Course, Group>> sel;
    generate(courses, 0, sel);

    if (validCombos.empty()) {
        setColor(12);
        cout << "ERROR FATAL: No hay combinaciones validas posibles sin choques." << endl;
        setColor(15);
        return 0;
    }

    sort(validCombos.begin(), validCombos.end(), [](const ScheduleCombination& a, const ScheduleCombination& b) {
        return a.score > b.score; // Higher score first
    });

    int curIdx = 0;
    while (true) {
        renderSchedule(validCombos[curIdx], curIdx, validCombos.size());
        int ch = _getch();
        if (ch == 224) { // Special keys (arrows)
            ch = _getch();
            if (ch == 75 && curIdx > 0) curIdx--; // Left
            if (ch == 77 && curIdx < validCombos.size() - 1) curIdx++; // Right
        } else if (ch == 27) { // ESC
            break;
        }
    }
    
    setColor(15);
    system("cls");
    cout << "Saliendo del visualizador de horarios..." << endl;
    return 0;
}
