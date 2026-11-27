/* Reconstrucción Técnica Local de TEC Digital 2026 (100% sanitizada y sin login).
 * Representa jerarquía de navegación, 32 módulos, 6 namespaces de espacios, pestañas
 * dinámicas ?page_num=N, evaluaciones base 100 + %, matriz analítica de rúbrica con
 * comentarios por criterio, What-If, calendario Mes/Semana/Agenda, expediente con
 * gráfico ponderado + 8 estados, buscador global de horarios y trámites.
 */
(function () {
  "use strict";

  const SANITIZED_DATA = {
    sections: [
      { id: "inicio", label: "Inicio", route: "/dotlrn/index", cuadernu: "HubSection::Resumen" },
      { id: "espacios", label: "Cursos y Espacios", route: "/dotlrn/classes/{dept}/{course}/{group}/", cuadernu: "HubSection::Cursos" },
      { id: "calendario", label: "Calendario", route: "/dotlrn/calendar/view", cuadernu: "HubSection::Calendario" },
      { id: "expediente", label: "Expediente y Malla", route: "/tda-expediente-estudiantil/", cuadernu: "HubSection::ExpedienteYMalla" },
      { id: "horarios", label: "Horarios Todo el TEC", route: "https://tec-appsext.itcr.ac.cr/guiahorarios/", cuadernu: "HubSection::HorariosVivo" },
      { id: "documentos", label: "Documentos y Biblioteca", route: "/dotlrn/file-storage/", cuadernu: "HubSection::BibliotecaOffline" },
      { id: "tramites", label: "Trámites y Perfil", route: "/tda-control-plagio | /tda-um | /tda-feria-negocios", cuadernu: "HubSection::TramitesYPerfil" }
    ],
    modules32: [
      "Inicio (dotLRN)", "Cursos y Clases", "Cátedras", "Escuelas y Subgrupos", "DOP / Bienestar",
      "Programas (PAySE/MATEC)", "Espacios Estudiantiles", "Documentos (file-storage)", "Mis Documentos",
      "Evaluaciones (tda-ce-estudiante)", "Rúbricas (tda-rubrica)", "Expediente Estudiantil",
      "Malla Curricular (tds-curriculum-exp)", "Guía Pública Horarios", "Mi Calendario",
      "Notificaciones", "Suscripciones (/notifications/manage)", "Noticias y Archivo",
      "Foros de Discusión", "Mensajería SMS", "Integrantes", "Control de Plagio",
      "Gestor de Inclusiones (GraphQL)", "Prematrícula", "Gestor de Inscripción",
      "Modelo de Usuario (tda-um)", "Feria de Negocios", "Evaluación Docente",
      "Sincronización DAR", "Motor de Juegos / GAAP", "Consentimiento Informado", "Mi Cuenta y Ayuda"
    ],
    spaces: [
      {
        id: "sp-fi2103", type: "CLASE", code: "FI2103", name: "Física General III GR 51",
        period: "S-2-2026", glFolderId: "245726801",
        customTabs: [
          { pageNum: 0, label: "Portada" }, { pageNum: 1, label: "Mensaje inicial" },
          { pageNum: 4, label: "Cronograma" }, { pageNum: 6, label: "Documentos" }, { pageNum: 7, label: "Evaluaciones" }
        ],
        portlets: ["Bienvenida y Acuerdos", "Cronograma Semanal", "Noticias Recientes"],
        evaluations: [
          { id: "ev-1", name: "I Fase Proyecto", weight: 2.0, earned: 1.83, grade100: 91.7, graded: true, hasRubric: false, due: "15/09/2026 23:59" },
          { id: "ev-2", name: "II Fase Proyecto", weight: 8.0, earned: 5.20, grade100: 65.0, graded: true, hasRubric: true, due: "01/11/2026 23:59" },
          { id: "ev-3", name: "Examen Parcial 1", weight: 25.0, earned: 19.50, grade100: 78.0, graded: true, hasRubric: false, due: "10/10/2026 10:00" },
          { id: "ev-4", name: "Examen Final", weight: 30.0, earned: 0.0, grade100: -1, graded: false, hasRubric: false, due: "05/12/2026 08:00" }
        ]
      },
      {
        id: "sp-el2113", type: "CLASE", code: "EL2113", name: "Circuitos Eléctricos CC GR 50",
        period: "S-2-2026", glFolderId: "245720112",
        customTabs: [{ pageNum: 0, label: "Inicio" }, { pageNum: 2, label: "Documentos" }, { pageNum: 3, label: "Evaluaciones" }],
        portlets: ["Programa del Curso", "Laboratorio"],
        evaluations: [
          { id: "el-1", name: "Parcial 1 — Análisis Nodal", weight: 20.0, earned: 16.8, grade100: 84.0, graded: true, hasRubric: false, due: "22/09/2026" },
          { id: "el-2", name: "Laboratorio 2 — Thévenin", weight: 10.0, earned: 0.0, grade100: -1, graded: false, hasRubric: false, due: "02/12/2026" }
        ]
      },
      {
        id: "sp-cat-fi3", type: "CATEDRA", code: "CAT-FI3", name: "Cátedra de Física General III",
        period: "Permanente", glFolderId: "18940122",
        customTabs: [{ pageNum: 0, label: "Información general" }, { pageNum: 1, label: "Contenidos" }, { pageNum: 2, label: "Documentos" }, { pageNum: 25, label: "Repositorio de exámenes" }],
        portlets: ["Banners Oficiales (/repositorio/tecdigital/ESC_FISICA/)", "Repositorio de Exámenes"],
        evaluations: []
      },
      {
        id: "sp-esc-e", type: "ESCUELA", code: "ESC-E", name: "Escuela de Ingeniería en Electrónica",
        period: "Permanente", glFolderId: "10045210",
        customTabs: [{ pageNum: 0, label: "Noticias Escuela" }, { pageNum: 3, label: "Documentos" }],
        portlets: ["Avisos Administrativos", "Asociación Estudiantil (Subgrupo)"],
        evaluations: []
      },
      {
        id: "sp-dop-psed", type: "DOP", code: "PSED-NE", name: "DOP · PSED-NE Apoyos Educativos",
        period: "Permanente", glFolderId: "10982201",
        customTabs: [{ pageNum: 0, label: "PSED-NE" }, { pageNum: 2, label: "Escalas Psicoeducativas" }, { pageNum: 3, label: "Tips Académicos" }],
        portlets: ["Banners de Bienestar", "Orientación Estudiantil"],
        evaluations: []
      },
      {
        id: "sp-prog-matec", type: "PROGRAMA", code: "MATEC", name: "Programa PAySE · MATEC",
        period: "Permanente", glFolderId: "12310440",
        customTabs: [{ pageNum: 0, label: "Presentación" }, { pageNum: 7, label: "Evaluaciones" }, { pageNum: 8, label: "GAAP" }],
        portlets: ["Módulos de Nivelación Autodirigida"],
        evaluations: []
      }
    ],
    rubricDemo: {
      title: "2S-2026- Fase II Proyecto de Física III",
      target: "Grupo 5",
      totalScore: 65.0,
      maxScore: 100.0,
      levels: ["Excelente (3 pts)", "Satisfactorio (2 pts)", "En desarrollo (1 pt)", "Insuficiente (0 pts)"],
      criteria: [
        { name: "Fundamento físico y predicción", weight: 15, earnedWeight: 10.0, achieved: 1, cells: ["Explica fenómeno y justifica ecuaciones.", "Explicación y predicción correctas con pequeñas omisiones.", "Teoría parcial o débilmente conectada.", "Sin fundamento suficiente."], comment: "Precisar condiciones de aplicación del modelo físico." },
        { name: "Montaje y procedimiento", weight: 15, earnedWeight: 10.0, achieved: 1, cells: ["Funcional, seguro y reproducible.", "Funciona con limitaciones menores.", "Funciona parcialmente.", "No permite evidencia válida."], comment: "Documentar calibración previa." },
        { name: "Calidad de los datos", weight: 20, earnedWeight: 13.3, achieved: 1, cells: ["Suficientes, repetibles y con unidades.", "Adecuados con pequeñas limitaciones.", "Escasos o dispersos.", "No hay datos útiles."], comment: "Indicar incertidumbre instrumental." },
        { name: "Gráfico y análisis cuantitativo", weight: 15, earnedWeight: 10.0, achieved: 1, cells: ["Apropiado, rotulado y analizado.", "Correcto con análisis suficiente.", "Incompleto o descriptivo.", "Sin gráfico."], comment: "" },
        { name: "Comparación teoría-experimento", weight: 20, earnedWeight: 13.3, achieved: 1, cells: ["Cuantitativa con argumentos físicos.", "Compara correctamente y señala causas.", "Superficial.", "No compara."], comment: "" },
        { name: "Incertidumbre y conclusiones", weight: 10, earnedWeight: 6.7, achieved: 1, cells: ["Sustentadas directamente en datos.", "Identifica limitaciones y concluye.", "Parcialmente sustentadas.", "No analiza limitaciones."], comment: "" },
        { name: "Presentación y bitácora", weight: 5, earnedWeight: 1.7, achieved: 2, cells: ["Claro, conciso y bitácora completa.", "Adecuado con pequeñas omisiones.", "Informe o bitácora incompletos.", "Deficiente."], comment: "Adjuntar evidencias cronológicas completas de Etapas I y II." }
      ]
    },
    weightedHistory: [
      { period: "2024-S1", avg: 78.4 }, { period: "2024-S2", avg: 81.2 },
      { period: "2025-S1", avg: 79.6 }, { period: "2025-S2", avg: 83.5 },
      { period: "2026-S1", avg: 84.8 }, { period: "2026-S2", avg: 85.4 }
    ],
    eightStates: [
      { state: "Aprobadas", count: 22 }, { state: "En curso", count: 4 }, { state: "Reprobadas", count: 2 },
      { state: "Retiradas", count: 1 }, { state: "Reconocidas", count: 0 }, { state: "Suficiencia", count: 0 },
      { state: "Pendientes", count: 0 }, { state: "Congeladas", count: 0 }
    ],
    globalHorarios: [
      { code: "EL2113", name: "Circuitos Eléctricos CC", group: "50", campus: "Cartago", school: "Electrónica", schedule: "L 07:30-09:20 · J 07:30-09:20", room: "E-03", prof: "Docente A" },
      { code: "FI2103", name: "Física General III", group: "51", campus: "Cartago", school: "Física", schedule: "K 09:30-11:20 · V 09:30-11:20", room: "D3-04", prof: "Docente B" },
      { code: "IC3002", name: "Análisis de Algoritmos", group: "40", campus: "San José", school: "Computación", schedule: "M 18:00-21:50", room: "SJ-201", prof: "Docente C" },
      { code: "AE4208", name: "Desarrollo de Emprendedores", group: "10", campus: "Limón", school: "Administración", schedule: "J 14:00-16:50", room: "Virtual", prof: "Docente D" }
    ],
    notifications: [
      { id: "n1", title: "Nueva calificación con rúbrica disponible", origin: "FI2103", unread: true, date: "2026-11-25" },
      { id: "n2", title: "Cronograma de exámenes finales actualizado", origin: "Cátedra Física III", unread: true, date: "2026-11-24" },
      { id: "n3", title: "Cita de matrícula ordinaria publicada", origin: "Expediente", unread: false, date: "2026-11-20" }
    ],
    dataFlows: {
      inicio: "Usuario abre Inicio → GET /dotlrn/index + GET /tda-notifications/ajax/get_user_notifications + POST tabla_horario → TecAcademicStore → PaintSummaryCanvas (KPIs, Próxima Clase, Notificaciones).",
      espacios: "Usuario selecciona curso → GET one-community?page_num=N + GET tda-index (evaluaciones) → Auto-POST /tda-rubrica/tda_evaluacion_rubrica (accion=ver) → ParseRubricHtml (criterion_selected + tr[comment]) → Snapshot v2 (sin student_id) → PaintCourseDetailCanvas.",
      calendario: "SyncAll → GET /dotlrn/calendar/view?view=list + calendarios de curso + citas_matricula (HTML sin <tr>) + tabla_horario (4 partes '-----') → BuildUnifiedCalendarEvents → Vista Mes / Semana / Agenda + ICS.",
      expediente: "SyncAll → #carne_expediente (RAM) + detalle_notas (9 cols) + generar_tabla_promedio_ponderado + detalle_historial + GraphQL getStudentAcademicHistory (44+ filas) + getStudentGeneralInfo (rn[]) → PaintExpStatsCanvas.",
      horarios: "Indexar todo el TEC → POST tec-appsext.itcr.ac.cr/guiahorarios/escuela.aspx/* (90 escuelas, 5 sedes) → GlobalHorariosIndex (global_horarios_2026.json) → Búsqueda <1ms + HorariosTec Solver.",
      documentos: "GET <espacio>/file-storage/ → $rootScope.GL_FOLDER_ID → GET /dotlrn/file-storage/api/folder/{id} → Descarga SHA-256 → OfflineMaterialIndex (SQLite FTS5).",
      tramites: "SyncAll → cargaArbol + cargaMisDocs + cargaDocsCurso + generaReporte (Plagio) + 10 secciones HTML tda-um + #tferias + api/forms + /notifications/manage → PaintTramitesCanvas (5 sub-tabs)."
    }
  };

  let state = {
    section: "inicio",
    spaceId: "sp-fi2103",
    courseTab: "evaluaciones",
    selectedEvalId: "ev-2",
    whatIfScores: {},
    calView: "mes",
    expTab: "historial",
    tramiteTab: "plagio",
    horQuery: "",
    mobileSim: false
  };

  function el(id) { return document.getElementById(id); }

  function renderNav() {
    const buildLinks = (containerId) => {
      const c = el(containerId);
      c.innerHTML = "";
      SANITIZED_DATA.sections.forEach((s) => {
        const b = document.createElement("button");
        b.type = "button";
        b.className = "nav-item" + (state.section === s.id ? " active" : "");
        b.textContent = s.label;
        b.addEventListener("click", () => {
          state.section = s.id;
          el("menu-left").hidden = true;
          renderAll();
        });
        c.appendChild(b);
      });
    };
    buildLinks("desktop-nav-list");
    buildLinks("mobile-nav-list");

    const st = el("space-tree");
    st.innerHTML = "";
    SANITIZED_DATA.spaces.forEach((sp) => {
      const b = document.createElement("button");
      b.type = "button";
      b.className = "space-item" + (state.section === "espacios" && state.spaceId === sp.id ? " active" : "");
      b.innerHTML = `<span class="space-tag">${sp.type}</span>${sp.name}`;
      b.addEventListener("click", () => {
        state.section = "espacios";
        state.spaceId = sp.id;
        renderAll();
      });
      st.appendChild(b);
    });
  }

  function renderAppsAndNotifs() {
    const ag = el("apps-grid");
    ag.innerHTML = SANITIZED_DATA.modules32.map((m, i) =>
      `<button type="button" class="app-tile" data-mod="${i}"><strong>MOD-${String(i + 1).padStart(2, "0")}</strong><div>${m}</div></button>`
    ).join("");
    ag.querySelectorAll(".app-tile").forEach((btn) => {
      btn.addEventListener("click", () => {
        el("apps-popover").hidden = true;
        const idx = Number(btn.dataset.mod);
        if (idx === 0) state.section = "inicio";
        else if (idx <= 6 || idx === 9 || idx === 10) state.section = "espacios";
        else if (idx === 7 || idx === 8) state.section = "documentos";
        else if (idx === 11 || idx === 12) state.section = "expediente";
        else if (idx === 13) state.section = "horarios";
        else if (idx === 14) state.section = "calendario";
        else state.section = "tramites";
        renderAll();
      });
    });

    const unread = SANITIZED_DATA.notifications.filter((n) => n.unread).length;
    el("notif-badge").textContent = String(unread);
    el("notifs-list").innerHTML = SANITIZED_DATA.notifications.map((n) => `
      <div class="card" style="margin-bottom:8px;padding:10px;">
        <strong>${n.title}</strong>
        <div class="kpi-lbl">${n.origin} · ${n.date} · Estado: ${n.unread ? "No leída (U)" : "Leída (R)"}</div>
        ${n.unread ? `<button type="button" class="pill-btn" data-mark-read="${n.id}" style="margin-top:6px;">Simular marcar leída (en prod requiere confirmación)</button>` : ""}
      </div>`).join("");
    el("notifs-list").querySelectorAll("[data-mark-read]").forEach((b) => {
      b.addEventListener("click", () => {
        const n = SANITIZED_DATA.notifications.find((x) => x.id === b.dataset.markRead);
        if (n) n.unread = false;
        renderAppsAndNotifs();
      });
    });
  }

  function renderRubricMatrixHtml() {
    const r = SANITIZED_DATA.rubricDemo;
    return `
      <div class="card" style="margin-top:10px;">
        <div style="display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:8px;margin-bottom:10px;">
          <div>
            <strong>${r.title}</strong>
            <div class="kpi-lbl">Evaluar Grupo: ${r.target} · Sincronizada automáticamente vía POST /tda-rubrica/tda_evaluacion_rubrica (accion=ver)</div>
          </div>
          <span class="grade-pill grade-fail">Puntuación: ${r.totalScore.toFixed(1)} / ${r.maxScore}</span>
        </div>
        <table class="data-table">
          <thead>
            <tr>
              <th>Categoría / Peso</th>
              ${r.levels.map((l) => `<th>${l}</th>`).join("")}
            </tr>
          </thead>
          <tbody>
            ${r.criteria.map((c) => `
              <tr>
                <td><strong>${c.name}</strong><div class="kpi-lbl">Peso ${c.weight}% · Logrado ${c.earnedWeight.toFixed(1)}%</div></td>
                ${c.cells.map((txt, li) => `<td class="${li === c.achieved ? "rubric-cell-achieved" : ""}">${li === c.achieved ? "<div>[Logrado]</div>" : ""}${txt}</td>`).join("")}
              </tr>
              ${c.comment ? `<tr class="rubric-comment-row"><td colspan="5"><strong>Comentario docente (tr[comment]):</strong> ${c.comment}</td></tr>` : ""}
            `).join("")}
          </tbody>
        </table>
      </div>`;
  }

  function renderMain() {
    const main = el("main-content");
    const secMeta = SANITIZED_DATA.sections.find((s) => s.id === state.section);
    el("status-route").textContent = "Ruta equivalente: " + secMeta.route;
    el("status-cuadernu").textContent = "Trazabilidad CuadernU: " + secMeta.cuadernu;
    el("dataflow-body").innerHTML = `<div class="card"><strong>${secMeta.label}</strong><p>${SANITIZED_DATA.dataFlows[state.section]}</p><p class="kpi-lbl">Manejo de errores: HTTP 302 → /register/ detecta sesión expirada; offline restaura Snapshot JSON v2; 404 en foros/sms se maneja sin fallos.</p></div>`;

    if (state.section === "inicio") {
      main.innerHTML = `
        <h2 style="margin-top:0;">Inicio · Portal Académico (Referencia Local)</h2>
        <div class="kpi-grid">
          <div class="kpi-card"><div class="kpi-lbl">Promedio Ponderado (Sintético)</div><div class="kpi-val">85.40</div></div>
          <div class="kpi-card"><div class="kpi-lbl">Cursos Activos</div><div class="kpi-val">4</div></div>
          <div class="kpi-card"><div class="kpi-lbl">Rúbricas Auto-Sincronizadas</div><div class="kpi-val">14</div></div>
          <div class="kpi-card"><div class="kpi-lbl">Próxima Clase (tabla_horario)</div><div class="kpi-val" style="font-size:15px;">EL2113 · Aula E-03</div><div class="kpi-lbl">Docente A · 07:30-09:20</div></div>
        </div>
        <div class="card">
          <strong>Resumen de Evaluaciones Base 100 por Curso</strong>
          <table class="data-table" style="margin-top:8px;">
            <thead><tr><th>Curso</th><th>Evaluado</th><th>Nota Acumulada (Base 100)</th><th>Acción</th></tr></thead>
            <tbody>
              <tr><td>FI2103 · Física General III</td><td>35.0%</td><td><span class="grade-pill grade-pass">75.8 / 100</span></td><td><button type="button" class="pill-btn" data-jump-space="sp-fi2103">Abrir evaluaciones y rúbrica</button></td></tr>
              <tr><td>EL2113 · Circuitos CC</td><td>20.0%</td><td><span class="grade-pill grade-pass">84.0 / 100</span></td><td><button type="button" class="pill-btn" data-jump-space="sp-el2113">Abrir curso</button></td></tr>
            </tbody>
          </table>
        </div>`;
      main.querySelectorAll("[data-jump-space]").forEach((b) => {
        b.addEventListener("click", () => { state.section = "espacios"; state.spaceId = b.dataset.jumpSpace; state.courseTab = "evaluaciones"; renderAll(); });
      });
      return;
    }

    if (state.section === "espacios") {
      const sp = SANITIZED_DATA.spaces.find((x) => x.id === state.spaceId) || SANITIZED_DATA.spaces[0];
      let earnedSum = 0, weightSum = 0;
      sp.evaluations.forEach((ev) => {
        if (ev.graded) { earnedSum += ev.earned; weightSum += ev.weight; }
        else if (state.whatIfScores[ev.id] != null) { earnedSum += (state.whatIfScores[ev.id] / 100) * ev.weight; weightSum += ev.weight; }
      });
      const acc100 = weightSum > 0 ? (earnedSum / weightSum) * 100 : -1;

      main.innerHTML = `
        <h2 style="margin-top:0;">${sp.name} <span class="space-tag">${sp.type}</span> <span class="space-tag">${sp.period}</span></h2>
        <div class="tabs-bar">
          ${["resumen", "evaluaciones", "documentos", "actividades", "novedades"].map((t) =>
            `<button type="button" class="seg-btn ${state.courseTab === t ? "active" : ""}" data-ctab="${t}">${t[0].toUpperCase() + t.slice(1)}</button>`
          ).join("")}
        </div>
        <div class="card">
          <div class="kpi-lbl">Pestañas reales descubiertas (<code>?page_num=N</code> no consecutivos):</div>
          <div class="tabs-bar" style="margin:6px 0 0;">
            ${sp.customTabs.map((ct) => `<span class="space-tag">page_num=${ct.pageNum}: ${ct.label}</span>`).join("")}
          </div>
        </div>
        ${state.courseTab === "resumen" ? `
          <div class="card"><strong>Portlets y Banners del Espacio</strong><ul>${sp.portlets.map((p) => `<li>${p}</li>`).join("")}</ul></div>
        ` : state.courseTab === "evaluaciones" ? `
          <div class="card">
            <div style="display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:8px;">
              <div><strong>Nota Actual (Base 100):</strong> ${acc100 >= 0 ? `<span class="grade-pill ${acc100 >= 70 ? "grade-pass" : "grade-fail"}">${acc100.toFixed(1)} / 100</span>` : "Sin evaluaciones calificadas"}</div>
              <div class="kpi-lbl">Puntos ganados/simulados: ${earnedSum.toFixed(2)} / ${weightSum.toFixed(1)}%</div>
            </div>
            ${sp.evaluations.length ? `
              <table class="data-table" style="margin-top:10px;">
                <thead><tr><th>Evaluación (.assignNameText)</th><th>Aporte (%)</th><th>Nota (Base 100)</th><th>Entrega / What-If</th></tr></thead>
                <tbody>
                  ${sp.evaluations.map((ev) => {
                    const sim = state.whatIfScores[ev.id];
                    const pill = ev.graded
                      ? `<span class="grade-pill ${ev.grade100 >= 70 ? "grade-pass" : "grade-fail"}">${ev.grade100.toFixed(1)} / 100</span>`
                      : (sim != null ? `<span class="grade-pill grade-sim">Sim ${sim} / 100</span>` : `<span class="grade-pill grade-pend">Pendiente</span>`);
                    return `<tr>
                      <td><strong>${ev.name}</strong>${ev.hasRubric ? ` <span class="space-tag">Rúbrica auto-sincronizada</span>` : ""}</td>
                      <td>${ev.graded ? ev.earned.toFixed(2) : (sim != null ? ((sim / 100) * ev.weight).toFixed(2) : "--")} / ${ev.weight.toFixed(1)}%</td>
                      <td>${pill}</td>
                      <td>
                        ${ev.due}
                        ${!ev.graded ? `<div><button type="button" class="pill-btn" data-sim="${ev.id}:70">Sim 70</button> <button type="button" class="pill-btn" data-sim="${ev.id}:85">Sim 85</button> <button type="button" class="pill-btn" data-sim="${ev.id}:100">Sim 100</button></div>` : ""}
                        ${ev.hasRubric ? `<div><button type="button" class="pill-btn active" data-sel-ev="${ev.id}">Ver matriz de rúbrica</button></div>` : ""}
                      </td>
                    </tr>`;
                  }).join("")}
                </tbody>
              </table>
              ${sp.evaluations.some((e) => e.id === state.selectedEvalId && e.hasRubric) ? renderRubricMatrixHtml() : ""}
            ` : `<p class="kpi-lbl">Este espacio (${sp.type}) no monta el módulo de evaluaciones.</p>`}
          </div>
        ` : state.courseTab === "documentos" ? `
          <div class="card"><strong>Gestor Documental AngularJS (td-documents)</strong><p>Carpeta raíz descubierta vía <code>$rootScope.GL_FOLDER_ID = ${sp.glFolderId}</code> → <code>GET /dotlrn/file-storage/api/folder/${sp.glFolderId}</code>.</p></div>
        ` : state.courseTab === "actividades" ? `
          <div class="card"><strong>Calendario del Curso, GAAP, SMS y Foros</strong><p>Integra <code>&lt;curso&gt;/calendar/view?view=list</code>, histórico GAAP, <code>tda-sms</code> y <code>forums/</code> (tolerando HTTP 404 cuando el curso no los monta).</p></div>
        ` : `
          <div class="card"><strong>Novedades (RSS + Archivo Histórico) e Integrantes</strong><p>Combina <code>news/rss</code> + <code>news/?view=archive</code> + <code>POST ajax/load_members</code>.</p></div>
        `}`;

      main.querySelectorAll("[data-ctab]").forEach((b) => b.addEventListener("click", () => { state.courseTab = b.dataset.ctab; renderMain(); }));
      main.querySelectorAll("[data-sim]").forEach((b) => b.addEventListener("click", () => { const [id, val] = b.dataset.sim.split(":"); state.whatIfScores[id] = Number(val); renderMain(); }));
      main.querySelectorAll("[data-sel-ev]").forEach((b) => b.addEventListener("click", () => { state.selectedEvalId = b.dataset.selEv; renderMain(); }));
      return;
    }

    if (state.section === "calendario") {
      main.innerHTML = `
        <h2 style="margin-top:0;">Calendario Unificado (Mes · Semana · Agenda)</h2>
        <div class="tabs-bar">
          ${["mes", "semana", "agenda"].map((v) => `<button type="button" class="seg-btn ${state.calView === v ? "active" : ""}" data-calv="${v}">${v[0].toUpperCase() + v.slice(1)}</button>`).join("")}
        </div>
        ${state.calView === "mes" ? `
          <div class="cal-grid">
            ${["Lun", "Mar", "Mié", "Jue", "Vie", "Sáb", "Dom"].map((d) => `<div style="font-weight:600;padding:4px;">${d}</div>`).join("")}
            ${Array.from({ length: 28 }, (_, i) => {
              const d = i + 1;
              const ev = d === 5 ? `<span class="cal-dot">Examen Final FI2103</span>` : (d === 15 ? `<span class="cal-dot">Cita Matrícula</span>` : "");
              return `<div class="cal-cell"><strong>${d}</strong>${ev}</div>`;
            }).join("")}
          </div>` : `<div class="card"><strong>Vista ${state.calView}</strong><p>Unifica 4 fuentes: horario oficial (<code>tabla_horario</code>), calendario portal + curso, fechas de entrega de evaluaciones y citas de matrícula (<code>citas_matricula</code>).</p></div>`}`;
      main.querySelectorAll("[data-calv]").forEach((b) => b.addEventListener("click", () => { state.calView = b.dataset.calv; renderMain(); }));
      return;
    }

    if (state.section === "expediente") {
      const pts = SANITIZED_DATA.weightedHistory.map((p, i) => `${30 + i * 75},${120 - (p.avg - 65) * 3.5}`).join(" ");
      main.innerHTML = `
        <h2 style="margin-top:0;">Expediente Estudiantil y Malla Curricular</h2>
        <div class="card">
          <strong>Evolución del Promedio Ponderado por Semestre (generar_tabla_promedio_ponderado)</strong>
          <svg viewBox="0 0 460 140" style="width:100%;max-width:520px;display:block;margin-top:8px;">
            <line x1="20" y1="102" x2="440" y2="102" stroke="#c62828" stroke-dasharray="4 4"/>
            <polyline fill="none" stroke="#0066cc" stroke-width="2.5" points="${pts}"/>
            ${SANITIZED_DATA.weightedHistory.map((p, i) => `<circle cx="${30 + i * 75}" cy="${120 - (p.avg - 65) * 3.5}" r="4" fill="#0066cc"/><text x="${14 + i * 75}" y="135" font-size="10" fill="#525866">${p.period}</text>`).join("")}
          </svg>
        </div>
        <div class="card">
          <strong>Distribución de los 8 Estados Oficiales TEC (detalle_historial + GraphQL)</strong>
          <div class="kpi-grid" style="margin-top:8px;">
            ${SANITIZED_DATA.eightStates.map((s) => `<div class="kpi-card"><div class="kpi-lbl">${s.state}</div><div class="kpi-val">${s.count}</div></div>`).join("")}
          </div>
        </div>`;
      return;
    }

    if (state.section === "horarios") {
      const q = state.horQuery.toLowerCase();
      const rows = SANITIZED_DATA.globalHorarios.filter((r) => !q || (r.code + " " + r.name + " " + r.campus + " " + r.school).toLowerCase().includes(q));
      main.innerHTML = `
        <h2 style="margin-top:0;">Buscador Global de Horarios (Todo el TEC · 90 Escuelas · 5 Sedes)</h2>
        <div class="card">
          <input type="search" id="hor-search-input" value="${state.horQuery}" placeholder="Buscar por código, curso, sede o escuela..." style="width:100%;padding:8px 10px;border:1px solid var(--border);border-radius:8px;">
          <table class="data-table" style="margin-top:10px;">
            <thead><tr><th>Código</th><th>Curso</th><th>Grupo</th><th>Sede / Escuela</th><th>Horario y Aula</th></tr></thead>
            <tbody>${rows.map((r) => `<tr><td>${r.code}</td><td>${r.name}</td><td>${r.group}</td><td>${r.campus} · ${r.school}</td><td>${r.schedule} (${r.room})</td></tr>`).join("")}</tbody>
          </table>
        </div>`;
      el("hor-search-input").addEventListener("input", (e) => { state.horQuery = e.target.value; renderMain(); el("hor-search-input").focus(); });
      return;
    }

    if (state.section === "documentos") {
      main.innerHTML = `
        <h2 style="margin-top:0;">Mis Documentos y Biblioteca Offline (SQLite FTS5)</h2>
        <div class="card">
          <strong>Contratos Documentales Verificados</strong>
          <ul>
            <li>Descubrimiento raíz personal: enlace <code>Ir a Mis documentos</code> / <code>$rootScope.GL_FOLDER_ID</code>.</li>
            <li>Listado recursivo JSON: <code>GET /dotlrn/file-storage/api/folder/{folder_id}</code>.</li>
            <li>Historial de versiones: <code>GET /dotlrn/file-storage/api/file/{object_id}/versions</code>.</li>
            <li>Verificación de integridad offline: SHA-256 + índice de texto completo FTS5 en CuadernU.</li>
          </ul>
        </div>`;
      return;
    }

    main.innerHTML = `
      <h2 style="margin-top:0;">Trámites, Control de Plagio, Modelo de Usuario y Servicios</h2>
      <div class="tabs-bar">
        ${[["dar", "Resumen y DAR"], ["plagio", "Control de Plagio"], ["um", "Modelo de Usuario"], ["ferias", "Eval. Docente y Ferias"], ["cuenta", "Notificaciones y Cuenta"]].map(([k, l]) =>
          `<button type="button" class="seg-btn ${state.tramiteTab === k ? "active" : ""}" data-ttab="${k}">${l}</button>`
        ).join("")}
      </div>
      <div class="card">
        ${state.tramiteTab === "plagio" ? `
          <strong>Control de Plagio (Contratos Reales de arbolDocumentos.1.0.0.js)</strong>
          <ul>
            <li><code>GET /tda-control-plagio/ajax/cargaArbol</code></li>
            <li><code>GET /tda-control-plagio/ajax/cargaDocumentos/cargaMisDocs?folder_id={id}</code></li>
            <li><code>GET /tda-control-plagio/ajax/cargaDocumentos/cargaDocsCurso?community_id={id}</code></li>
            <li><code>GET /tda-control-plagio/ajax/generaReporte</code></li>
          </ul>
        ` : state.tramiteTab === "um" ? `
          <strong>Modelo de Usuario (/tda-um/ajax/get_info — 9 Dimensiones HTML)</strong>
          <p>Estilo de aprendizaje (Felder), Necesidades de aprendizaje, Perfil socioacadémico, Orientación al logro, Temperamento, Perfil emprendedor, Comunicación, Privacidad e Información básica.</p>
        ` : state.tramiteTab === "ferias" ? `
          <strong>Evaluación Docente y Feria de Negocios</strong>
          <p>Formularios <code>/tda-performance-evaluation/api/forms/</code> + tabla <code>#tferias</code> (<code>POST /tda-feria-negocios/ajax/ferias/get_ferias</code>).</p>
        ` : state.tramiteTab === "cuenta" ? `
          <strong>Notificaciones, Suscripciones (/notifications/manage) y Escuelas (100+)</strong>
          <p>Gestión de suscripciones de solo lectura y acciones mutantes protegidas por <code>ScopedUserMutationGuard</code>.</p>
        ` : `
          <strong>Inclusiones (GraphQL), Prematrícula, Inscripciones y DAR</strong>
          <p>Vista previa DAR en <code>POST /tda-expediente-estudiantil/ajax/actualizar_cursos</code> (solo lectura); sincronización mutante en <code>/dotlrn/ajax/actualizar_cursos</code> solo tras confirmación explícita del usuario.</p>
        `}
      </div>`;
    main.querySelectorAll("[data-ttab]").forEach((b) => b.addEventListener("click", () => { state.tramiteTab = b.dataset.ttab; renderMain(); }));
  }

  function renderAll() {
    renderNav();
    renderAppsAndNotifs();
    renderMain();
  }

  el("btn-toggle-viewport").addEventListener("click", () => {
    state.mobileSim = !state.mobileSim;
    el("workspace-shell").classList.toggle("mobile-sim", state.mobileSim);
    document.body.classList.toggle("mobile-mode", state.mobileSim);
    el("btn-toggle-viewport").textContent = state.mobileSim ? "Vista: Móvil (430px)" : "Vista: Escritorio";
  });
  el("btn-mobile-drawer").addEventListener("click", () => { el("menu-left").hidden = !el("menu-left").hidden; });
  el("btn-close-drawer").addEventListener("click", () => { el("menu-left").hidden = true; });
  el("btn-open-apps").addEventListener("click", () => { el("apps-popover").hidden = !el("apps-popover").hidden; el("notifs-popover").hidden = true; });
  el("btn-open-notifs").addEventListener("click", () => { el("notifs-popover").hidden = !el("notifs-popover").hidden; el("apps-popover").hidden = true; });
  el("btn-open-flow").addEventListener("click", () => {
    const p = el("dataflow-inspector");
    p.hidden = !p.hidden;
    el("workspace-shell").classList.toggle("with-inspector", !p.hidden && !state.mobileSim);
  });
  document.querySelectorAll("[data-close]").forEach((b) => {
    b.addEventListener("click", () => {
      el(b.dataset.close).hidden = true;
      el("workspace-shell").classList.remove("with-inspector");
    });
  });

  renderAll();
})();
