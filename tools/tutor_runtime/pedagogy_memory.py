"""Adaptive Pedagogical Memory Engine: BKT + FSRS-5 Sprint + Misconception Ledger (SQLite WAL).

Soporta tanto estudio continuo como el Modo Sprint de Examen (< 7 días para Reposición FI2103):
  * BKT (Bayesian Knowledge Tracing) actualiza probabilidad de dominio P(L_n);
  * FSRS-5 comprimido en sprint programa repasos cortos realistas (15 min -> 4 h -> 12 h -> 24 h -> 48 h);
  * Misconception Ledger registra categorías exactas de error (signo, unidades, fase, integral, fórmula)
    y los marca resueltos automáticamente tras 2 aciertos ciegos consecutivos.
"""

from __future__ import annotations

import math
import pathlib
import sqlite3
import time
from typing import Any, Dict, List, Optional


class PedagogyMemoryStore:
    """Tracks mastery probability (BKT), forgetting stability (FSRS-5), short-step study scaffolding level,
    consecutive blind passes, and recurring algebraic/physics misconceptions across study sessions."""

    def __init__(self, db_path: str | pathlib.Path = ":memory:") -> None:
        self._path = str(db_path)
        if self._path != ":memory:":
            pathlib.Path(self._path).parent.mkdir(parents=True, exist_ok=True)
        self._conn = sqlite3.connect(self._path, check_same_thread=False)
        self._conn.execute("PRAGMA journal_mode=WAL;")
        self._init_schema()

    def _init_schema(self) -> None:
        self._conn.executescript(
            """
            CREATE TABLE IF NOT EXISTS skill_mastery (
                skill_id TEXT PRIMARY KEY,
                course_code TEXT NOT NULL DEFAULT 'FI2103',
                title_es TEXT NOT NULL,
                p_know REAL NOT NULL DEFAULT 0.15,
                p_learn REAL NOT NULL DEFAULT 0.22,
                p_guess REAL NOT NULL DEFAULT 0.15,
                p_slip REAL NOT NULL DEFAULT 0.10,
                fsrs_stability REAL NOT NULL DEFAULT 1.0,
                fsrs_difficulty REAL NOT NULL DEFAULT 5.0,
                due_epoch_sec INTEGER NOT NULL DEFAULT 0,
                scaffold_level INTEGER NOT NULL DEFAULT 3,
                attempts INTEGER NOT NULL DEFAULT 0,
                consecutive_blind_passes INTEGER NOT NULL DEFAULT 0,
                updated_epoch_sec INTEGER NOT NULL
            );

            CREATE TABLE IF NOT EXISTS misconception_ledger (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                skill_id TEXT NOT NULL,
                error_category TEXT NOT NULL,
                example_wrong_expr TEXT NOT NULL,
                example_correct_expr TEXT NOT NULL,
                diagnostic_note_es TEXT NOT NULL,
                occurrences INTEGER NOT NULL DEFAULT 1,
                resolved INTEGER NOT NULL DEFAULT 0,
                last_seen_epoch_sec INTEGER NOT NULL
            );

            CREATE TABLE IF NOT EXISTS tutor_events (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                session_id TEXT NOT NULL,
                beat_id TEXT,
                event_kind TEXT NOT NULL,
                latency_ms INTEGER DEFAULT 0,
                payload_json TEXT DEFAULT '{}',
                created_epoch_sec INTEGER NOT NULL
            );
            """
        )
        # Migración segura para bases existentes creadas antes de `consecutive_blind_passes`
        cols = {r[1] for r in self._conn.execute("PRAGMA table_info(skill_mastery)").fetchall()}
        if "consecutive_blind_passes" not in cols:
            self._conn.execute(
                "ALTER TABLE skill_mastery ADD COLUMN consecutive_blind_passes INTEGER NOT NULL DEFAULT 0"
            )
        self._conn.commit()

    def close(self) -> None:
        self._conn.close()

    @staticmethod
    def _compute_scaffold_level(p_know: float) -> int:
        # Fading scaffolds for short-step study: maximum micro-chunking (3) when new -> autonomous (0) when mastered
        if p_know < 0.45:
            return 3
        if p_know < 0.68:
            return 2
        if p_know < 0.86:
            return 1
        return 0

    def record_practice_outcome(
        self,
        skill_id: str,
        title_es: str,
        correct: bool,
        course_code: str = "FI2103",
        now_sec: Optional[int] = None,
        *,
        sprint_mode: bool = False,
        blind: bool = False,
    ) -> Dict[str, Any]:
        """Update Bayesian Knowledge Tracing P(L_n) + FSRS-5 stability/difficulty and fading scaffold.

        When `sprint_mode=True` (exam in < 7 days), intervals compress to 15 min -> 4 h -> 12 h -> 24 h -> 48 h.
        When `blind=True` and `correct=True`, 2 consecutive passes resolve open misconceptions for `skill_id`.
        """
        now = int(now_sec if now_sec is not None else time.time())
        row = self._conn.execute(
            """
            SELECT p_know, p_learn, p_guess, p_slip, fsrs_stability, fsrs_difficulty, attempts,
                   consecutive_blind_passes
            FROM skill_mastery WHERE skill_id = ?
            """,
            (skill_id,),
        ).fetchone()

        if row is None:
            p_know, p_learn, p_guess, p_slip, stability, difficulty, attempts, blind_streak = (
                0.15, 0.22, 0.15, 0.10, 1.0, 5.0, 0, 0
            )
        else:
            p_know, p_learn, p_guess, p_slip, stability, difficulty, attempts, blind_streak = row

        # BKT posterior update
        if correct:
            num = p_know * (1.0 - p_slip)
            den = num + (1.0 - p_know) * p_guess
        else:
            num = p_know * p_slip
            den = num + (1.0 - p_know) * (1.0 - p_guess)
        p_given_obs = num / max(den, 1e-9)
        p_next = p_given_obs + (1.0 - p_given_obs) * p_learn
        p_next = max(0.02, min(0.99, p_next))

        # FSRS-5 stability/difficulty update (days)
        if correct:
            difficulty = max(1.0, difficulty - 0.35)
            gain = 1.0 + math.exp(0.65) * (11.0 - difficulty) * (stability ** -0.2) * 0.22
            stability = min(365.0, stability * max(1.25, gain))
            if blind:
                blind_streak = int(blind_streak) + 1
        else:
            difficulty = min(10.0, difficulty + 0.65)
            stability = max(0.5, stability * 0.55)
            blind_streak = 0

        if sprint_mode:
            # Compresión de intervalos para sprint < 7 días (15 min -> 4 h -> 12 h -> 24 h -> 48 h)
            if not correct:
                interval_sec = 900  # 15 minutos
            elif attempts == 0:
                interval_sec = 4 * 3600
            elif p_next < 0.72:
                interval_sec = 12 * 3600
            elif p_next < 0.88:
                interval_sec = 24 * 3600
            else:
                interval_sec = 48 * 3600
        else:
            interval_sec = int(max(1800, stability * 86400.0))

        due_sec = now + interval_sec
        scaffold = self._compute_scaffold_level(p_next)
        attempts += 1

        self._conn.execute(
            """
            INSERT INTO skill_mastery(
                skill_id, course_code, title_es, p_know, p_learn, p_guess, p_slip,
                fsrs_stability, fsrs_difficulty, due_epoch_sec, scaffold_level, attempts,
                consecutive_blind_passes, updated_epoch_sec
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(skill_id) DO UPDATE SET
                title_es=excluded.title_es,
                p_know=excluded.p_know,
                fsrs_stability=excluded.fsrs_stability,
                fsrs_difficulty=excluded.fsrs_difficulty,
                due_epoch_sec=excluded.due_epoch_sec,
                scaffold_level=excluded.scaffold_level,
                attempts=excluded.attempts,
                consecutive_blind_passes=excluded.consecutive_blind_passes,
                updated_epoch_sec=excluded.updated_epoch_sec
            """,
            (
                skill_id, course_code, title_es, p_next, p_learn, p_guess, p_slip,
                stability, difficulty, due_sec, scaffold, attempts, blind_streak, now,
            ),
        )

        resolved_count = 0
        if blind and correct and blind_streak >= 2:
            cur = self._conn.execute(
                "UPDATE misconception_ledger SET resolved = 1 WHERE skill_id = ? AND resolved = 0",
                (skill_id,),
            )
            resolved_count = int(cur.rowcount or 0)

        self._conn.commit()
        return {
            "skill_id": skill_id,
            "p_know": round(p_next, 4),
            "fsrs_stability_days": round(stability, 2),
            "fsrs_difficulty": round(difficulty, 2),
            "due_epoch_sec": due_sec,
            "interval_min": round(interval_sec / 60.0, 1),
            "scaffold_level": scaffold,
            "attempts": attempts,
            "consecutive_blind_passes": blind_streak,
            "misconceptions_resolved": resolved_count,
        }

    def log_misconception(
        self,
        skill_id: str,
        error_category: str,
        wrong_expr: str,
        correct_expr: str,
        note_es: str,
        now_sec: Optional[int] = None,
    ) -> Dict[str, Any]:
        now = int(now_sec if now_sec is not None else time.time())
        existing = self._conn.execute(
            """
            SELECT id, occurrences FROM misconception_ledger
            WHERE skill_id = ? AND error_category = ? AND resolved = 0
            ORDER BY id DESC LIMIT 1
            """,
            (skill_id, error_category),
        ).fetchone()

        if existing:
            mid, occ = existing[0], int(existing[1]) + 1
            self._conn.execute(
                """
                UPDATE misconception_ledger
                SET occurrences = ?, example_wrong_expr = ?, example_correct_expr = ?,
                    diagnostic_note_es = ?, last_seen_epoch_sec = ?
                WHERE id = ?
                """,
                (occ, wrong_expr, correct_expr, note_es, now, mid),
            )
        else:
            occ = 1
            cur = self._conn.execute(
                """
                INSERT INTO misconception_ledger(
                    skill_id, error_category, example_wrong_expr, example_correct_expr,
                    diagnostic_note_es, occurrences, resolved, last_seen_epoch_sec
                ) VALUES (?, ?, ?, ?, ?, 1, 0, ?)
                """,
                (skill_id, error_category, wrong_expr, correct_expr, note_es, now),
            )
            mid = int(cur.lastrowid)
        self._conn.commit()
        return {
            "id": mid,
            "skill_id": skill_id,
            "error_category": error_category,
            "occurrences": occ,
            "needs_remediation": occ >= 2,
        }

    def get_due_review_queue(self, limit: int = 6, now_sec: Optional[int] = None) -> Dict[str, Any]:
        """Devuelve la cola de rescate FSRS-5 ordenada por urgencia: errores sin resolver y temas vencidos."""
        now = int(now_sec if now_sec is not None else time.time())
        due_rows = self._conn.execute(
            """
            SELECT skill_id, title_es, p_know, due_epoch_sec, scaffold_level, attempts, consecutive_blind_passes
            FROM skill_mastery
            WHERE due_epoch_sec <= ? OR p_know < 0.65
            ORDER BY (CASE WHEN due_epoch_sec <= ? THEN 0 ELSE 1 END), p_know ASC, due_epoch_sec ASC
            LIMIT ?
            """,
            (now, now, int(limit)),
        ).fetchall()
        err_rows = self._conn.execute(
            """
            SELECT skill_id, error_category, diagnostic_note_es, occurrences
            FROM misconception_ledger
            WHERE resolved = 0
            ORDER BY occurrences DESC, last_seen_epoch_sec DESC
            LIMIT ?
            """,
            (int(limit),),
        ).fetchall()

        err_by_skill: Dict[str, Dict[str, Any]] = {}
        for sid, cat, note, occ in err_rows:
            err_by_skill.setdefault(sid, {"categoria": cat, "nota": note, "veces": int(occ)})

        items: List[Dict[str, Any]] = []
        vistos = set()
        for sid, tit, pk, due, scaf, att, streak in due_rows:
            vistos.add(sid)
            items.append({
                "skill_id": sid,
                "titulo": tit,
                "p_know": round(float(pk), 3),
                "vencido": int(due) <= now,
                "scaffold_level": int(scaf),
                "intentos": int(att),
                "racha_ciega": int(streak),
                "error_activo": err_by_skill.get(sid),
            })
        for sid, info in err_by_skill.items():
            if sid not in vistos and len(items) < limit:
                items.append({
                    "skill_id": sid,
                    "titulo": sid,
                    "p_know": 0.25,
                    "vencido": True,
                    "scaffold_level": 3,
                    "intentos": info["veces"],
                    "racha_ciega": 0,
                    "error_activo": info,
                })

        total_skills = self._conn.execute("SELECT COUNT(*) FROM skill_mastery").fetchone()[0]
        mastered = self._conn.execute("SELECT COUNT(*) FROM skill_mastery WHERE p_know >= 0.85").fetchone()[0]
        open_errors = self._conn.execute("SELECT COUNT(*) FROM misconception_ledger WHERE resolved = 0").fetchone()[0]
        return {
            "cola": items,
            "resumen": {
                "practicados": int(total_skills),
                "dominados": int(mastered),
                "errores_abiertos": int(open_errors),
                "pendientes_repaso": len(items),
            },
        }

    def log_event(self, session_id: str, event_kind: str, beat_id: str = "", latency_ms: int = 0, payload_json: str = "{}") -> None:
        self._conn.execute(
            """
            INSERT INTO tutor_events(session_id, beat_id, event_kind, latency_ms, payload_json, created_epoch_sec)
            VALUES (?, ?, ?, ?, ?, ?)
            """,
            (session_id, beat_id, event_kind, int(latency_ms), payload_json, int(time.time())),
        )
        self._conn.commit()

    def build_pedagogical_summary(self) -> Dict[str, Any]:
        skills = [
            {
                "skill_id": r[0],
                "course_code": r[1],
                "title_es": r[2],
                "p_know": round(float(r[3]), 3),
                "fsrs_stability_days": round(float(r[4]), 2),
                "scaffold_level": int(r[5]),
                "attempts": int(r[6]),
                "consecutive_blind_passes": int(r[7]),
            }
            for r in self._conn.execute(
                "SELECT skill_id, course_code, title_es, p_know, fsrs_stability, scaffold_level, attempts, consecutive_blind_passes FROM skill_mastery ORDER BY p_know ASC"
            ).fetchall()
        ]
        misconceptions = [
            {
                "skill_id": r[0],
                "error_category": r[1],
                "wrong": r[2],
                "correct": r[3],
                "note_es": r[4],
                "occurrences": int(r[5]),
            }
            for r in self._conn.execute(
                "SELECT skill_id, error_category, example_wrong_expr, example_correct_expr, diagnostic_note_es, occurrences FROM misconception_ledger WHERE resolved = 0 ORDER BY occurrences DESC LIMIT 8"
            ).fetchall()
        ]
        return {
            "skills": skills,
            "active_misconceptions": misconceptions,
        }
