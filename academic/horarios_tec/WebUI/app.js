// Data Models
const L = 0, K = 1, M = 2, J = 3, V = 4;

const courses = [
    {
        name: "Circuitos Eléctricos en C.C.",
        code: "EL2113",
        color: "linear-gradient(135deg, #10b981, #059669)", // Emerald
        groups: [
            {
                id: "Gr 50", modality: "Regular",
                sessions: [
                    { day: M, startH: 16, startM: 10, endH: 17, endM: 55 },
                    { day: V, startH: 7, startM: 55, endH: 9, endM: 40 }
                ]
            }
        ]
    },
    {
        name: "Física General II",
        code: "FI1102",
        color: "linear-gradient(135deg, #0ea5e9, #0284c7)", // Sky Blue
        groups: [
            {
                id: "Gr 50", modality: "Regular",
                sessions: [
                    { day: K, startH: 9, startM: 45, endH: 11, endM: 30 },
                    { day: V, startH: 9, startM: 45, endH: 11, endM: 30 }
                ]
            },
            {
                id: "Gr 51", modality: "Regular",
                sessions: [
                    { day: K, startH: 18, startM: 0, endH: 19, endM: 45 },
                    { day: J, startH: 18, startM: 0, endH: 19, endM: 45 }
                ]
            }
        ]
    },
    {
        name: "Física General III",
        code: "FI2103",
        color: "linear-gradient(135deg, #f43f5e, #e11d48)", // Rose
        groups: [
            {
                id: "Gr 50", modality: "Regular",
                sessions: [
                    { day: K, startH: 9, startM: 45, endH: 11, endM: 30 },
                    { day: M, startH: 9, startM: 45, endH: 11, endM: 30 }
                ]
            },
            {
                id: "Gr 51", modality: "Semipresencial",
                sessions: [
                    { day: M, startH: 14, startM: 20, endH: 16, endM: 5 },
                    { day: V, startH: 14, startM: 20, endH: 16, endM: 5 }
                ]
            }
        ]
    },
    {
        name: "Cálculo y Álgebra Lineal",
        code: "MA1103",
        color: "linear-gradient(135deg, #f59e0b, #d97706)", // Amber
        groups: [
            {
                id: "Gr 50", modality: "Semipresencial",
                sessions: [
                    { day: M, startH: 7, startM: 55, endH: 9, endM: 40 },
                    { day: J, startH: 14, startM: 20, endH: 16, endM: 5 }
                ]
            },
            {
                id: "Gr 51", modality: "Semipresencial",
                sessions: [
                    { day: K, startH: 9, startM: 45, endH: 11, endM: 30 },
                    { day: J, startH: 9, startM: 45, endH: 11, endM: 30 }
                ]
            },
            {
                id: "Gr 52", modality: "Semipresencial",
                sessions: [
                    { day: M, startH: 9, startM: 45, endH: 11, endM: 30 },
                    { day: V, startH: 9, startM: 45, endH: 11, endM: 30 }
                ]
            }
        ]
    }
];

function overlaps(s1, s2) {
    if (s1.day !== s2.day) return false;
    const t1Start = s1.startH * 60 + s1.startM;
    const t1End = s1.endH * 60 + s1.endM;
    const t2Start = s2.startH * 60 + s2.startM;
    const t2End = s2.endH * 60 + s2.endM;
    return (t1Start < t2End && t1End > t2Start);
}

function hasConflict(selection) {
    let allSessions = [];
    for (const sel of selection) {
        allSessions = allSessions.concat(sel.group.sessions);
    }
    for (let i = 0; i < allSessions.length; i++) {
        for (let j = i + 1; j < allSessions.length; j++) {
            if (overlaps(allSessions[i], allSessions[j])) return true;
        }
    }
    return false;
}

function calculateScore(selection) {
    let score = 0;
    for (const sel of selection) {
        for (const s of sel.group.sessions) {
            if (s.startH < 12) score += 30;       // Mañana
            else if (s.startH < 17) score -= 10;  // Tarde
            else score -= 50;                     // Noche
        }
    }
    return score;
}

let validCombos = [];

function generate(courseIdx, currentSel) {
    if (courseIdx === courses.length) {
        if (!hasConflict(currentSel)) {
            validCombos.push({
                selection: [...currentSel],
                score: calculateScore(currentSel)
            });
        }
        return;
    }
    for (const g of courses[courseIdx].groups) {
        currentSel.push({ course: courses[courseIdx], group: g });
        generate(courseIdx + 1, currentSel);
        currentSel.pop();
    }
}

generate(0, []);
validCombos.sort((a, b) => b.score - a.score);

let currentIdx = 0;

function formatTime(h, m) {
    return `${h.toString().padStart(2, '0')}:${m.toString().padStart(2, '0')}`;
}

// 5-minute resolution grid positioning
// 07:00 is minute 420
// Each row represents 5 minutes.
// Row 1: Headers
// Row 2: 07:00 (min 420)
// Total hours: 07:00 to 20:00 (13 hours * 12 slots = 156 rows)
const BASE_MIN = 420; // 07:00 in minutes

function getGridRow(h, m) {
    const mins = h * 60 + m;
    const offset = Math.max(0, mins - BASE_MIN);
    return Math.floor(offset / 5) + 2; // +2 for header row
}

function buildGridStructure() {
    const grid = document.getElementById('calendar-grid');
    grid.innerHTML = '';

    // Day headers
    const days = ['Hora', 'Lunes', 'Martes', 'Miércoles', 'Jueves', 'Viernes'];
    days.forEach((d, colIdx) => {
        const el = document.createElement('div');
        el.className = 'cal-header';
        el.style.gridColumn = colIdx + 1;
        el.style.gridRow = 1;
        el.innerText = d;
        grid.appendChild(el);
    });

    // Time labels (every hour from 07:00 to 20:00)
    for (let h = 7; h <= 20; h++) {
        const timeEl = document.createElement('div');
        timeEl.className = 'cal-time-label';
        const startRow = getGridRow(h, 0);
        timeEl.style.gridColumn = 1;
        timeEl.style.gridRow = `${startRow} / span 12`; // spans 1 hour (12 * 5 mins)
        timeEl.innerText = formatTime(h, 0);
        grid.appendChild(timeEl);

        // Add horizontal background line for each hour
        const line = document.createElement('div');
        line.className = 'hour-line';
        line.style.gridColumn = '2 / span 5';
        line.style.gridRow = startRow;
        grid.appendChild(line);
    }
}

function renderSchedule(idx) {
    if (validCombos.length === 0) return;

    buildGridStructure();

    const combo = validCombos[idx];
    document.getElementById('current-idx').innerText = idx + 1;
    document.getElementById('total-combos').innerText = validCombos.length;

    const scoreEl = document.getElementById('score-display');
    scoreEl.innerText = `Puntaje: ${combo.score}`;
    scoreEl.style.color = combo.score > 0 ? '#059669' : '#dc2626';
    scoreEl.style.borderColor = combo.score > 0 ? '#34d399' : '#f87171';
    scoreEl.style.background = combo.score > 0 ? '#ecfdf5' : '#fef2f2';

    // Render legend
    const legend = document.getElementById('course-legend');
    legend.innerHTML = '';

    const grid = document.getElementById('calendar-grid');

    combo.selection.forEach(sel => {
        // Legend badge
        const badge = document.createElement('div');
        badge.className = 'course-badge';
        badge.style.background = sel.course.color;
        badge.innerHTML = `<strong>${sel.course.code}</strong> ${sel.course.name} <span class="badge-sub">(${sel.group.id})</span>`;
        legend.appendChild(badge);

        // Render continuous event blocks
        sel.group.sessions.forEach(session => {
            const startRow = getGridRow(session.startH, session.startM);
            const endRow = getGridRow(session.endH, session.endM);
            const col = session.day + 2; // +1 for time col, +1 for 1-based index

            const block = document.createElement('div');
            block.className = 'event-card';
            block.style.background = sel.course.color;
            block.style.gridColumn = col;
            block.style.gridRow = `${startRow} / ${endRow}`;

            const timeStr = `${formatTime(session.startH, session.startM)} - ${formatTime(session.endH, session.endM)}`;

            block.innerHTML = `
                <div class="event-code">${sel.course.code}</div>
                <div class="event-name">${sel.course.name}</div>
                <div class="event-details">${sel.group.id} • ${timeStr}</div>
            `;

            grid.appendChild(block);
        });
    });
}

// Initial setup
renderSchedule(currentIdx);

// Navigation
document.getElementById('prev-btn').addEventListener('click', () => {
    if (currentIdx > 0) {
        currentIdx--;
        renderSchedule(currentIdx);
    }
});

document.getElementById('next-btn').addEventListener('click', () => {
    if (currentIdx < validCombos.length - 1) {
        currentIdx++;
        renderSchedule(currentIdx);
    }
});

document.addEventListener('keydown', (e) => {
    if (e.key === 'ArrowLeft' && currentIdx > 0) {
        currentIdx--;
        renderSchedule(currentIdx);
    } else if (e.key === 'ArrowRight' && currentIdx < validCombos.length - 1) {
        currentIdx++;
        renderSchedule(currentIdx);
    }
});
