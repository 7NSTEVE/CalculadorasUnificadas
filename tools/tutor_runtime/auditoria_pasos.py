"""Auditoría de las lecciones del tutor: (1) que todo parta del formulario II-2026; (2) que no se salten pasos.

Uso:  python tools/tutor_runtime/auditoria_pasos.py [salida.json]
Imprime un resumen por lección, ordenado por gravedad. Es heurística: marca candidatos para revisar a ojo.
"""
import collections
import json
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from fisica3 import registro  # noqa: E402
from fisica3.formulario_ii_2026 import (  # noqa: E402
    _POR_ID, _celdas_en_texto, mapear_formulario_leccion, verificar_origen_formulario)

# Fórmulas que NO están en la hoja: si aparecen, la lección tiene que mostrar de dónde salen
FUERA = (
    ("ΔS sin partir de dS = dQ/T (la entropía no está en la hoja)",
     re.compile(r"\\Delta\s*S"), re.compile(r"dS\s*=\s*\\frac\{d\s*Q\}\{T\}|\\int[^\n]*\\frac\{d\s*Q\}\{T\}|\\frac\{dQ\}\{T\}")),
    ("K de Carnot sin derivarlo de K = |Q_C|/|W| y |Q_C|/|Q_H| = T_C/T_H",
     re.compile(r"\\frac\{T_[cC]\}\{T_H\s*[-−]\s*T_[cC]\}"), re.compile(r"K\s*=\s*\\frac\{\|Q_[cC]\|\}\{\|W|\\frac\{\|Q_[cC]\|\}\{\|?Q_H\|?\}\s*=\s*\\frac\{T_[cC]\}\{T_H\}")),
    ("e de Carnot sin derivar |Q_C|/|Q_H| = T_C/T_H",
     re.compile(r"1\s*[-−]\s*\\frac\{T_[cC]\}\{T_H\}"), re.compile(r"\\frac\{\|Q_[cC]\|\}\{\|?Q_H\|?\}\s*=\s*\\frac\{T_[cC]\}\{T_H\}|\\frac\{T_[cC]\s*\\ln|n\s*R\s*T_[cC]\s*\\ln")),
    ("Ley de Wien (no está en la hoja)", re.compile(r"2\{?,\}?898|\\lambda_\{?(?:\\text\{)?m(?:á|a)x"), None),
    ("v_mp o v_media de Maxwell (no están en la hoja)", re.compile(r"\\sqrt\{\\frac\{(?:2|8)\s*(?:R|k)"), None),
    ("Camino libre medio (no está en la hoja)", re.compile(r"\\lambda\s*=\s*\\frac\{V\}\{4\s*\\pi\s*\\sqrt|\\frac\{1\}\{4\\pi\\sqrt\{2\}"), None),
)

RESUMEN_TIT = re.compile(r"^resultado$|resumen|respuestas|en limpio|repaso|resultado final|conclusi|comparaci|qu[eé] opci", re.I)
SCI = re.compile(r"\\times\s*10\^\{?-?\d+\}?")


def complejidad(tex):
    t = SCI.sub("", tex)
    t = re.sub(r"\\text\{[^}]*\}", "", t)
    return (t.count(r"\frac") + t.count("^") + t.count(r"\ln") + t.count(r"\sqrt") + t.count(r"\cdot")
            + t.count(")(") + t.count(r"\times") + t.count("+") + len(re.findall(r"(?<![{^_])-", t)))


def clase(acc):
    rol = acc.get("rol") or ""
    tex = str(acc.get("tex") or "")
    if rol == "formula":
        return "F"
    if rol == "resultado":
        return "R"
    return "N" if re.search(r"\d", SCI.sub("", re.sub(r"_\{[^}]*\}|_[A-Za-z0-9]|\^\{[^}]*\}", "", tex))) else "S"


FUERA_PARCIAL = {"6.16", "6.17"}
SOLO_CARNOT = {"Q118", "Q125"}


def auditar(lid):
    lec = registro.obtener(lid)
    d = lec.a_json()
    tipo = d.get("tipo")
    out = {"id": lid, "tipo": tipo, "titulo": d.get("titulo"), "tema": d.get("tema"), "faltas": [], "avisos": [],
           "fuera": (lec.tema in FUERA_PARCIAL) or lid in SOLO_CARNOT, "cotejo": (lec.cotejo() or {}).get("estado")}
    if tipo == "teoria":
        return out
    m = mapear_formulario_leccion(d)
    base = [c["id"] for c in m["celdas"] if not c.get("es_constante")]
    out["celdas"] = base
    pasos = d.get("pasos") or []
    total_mate = 0
    blob = []
    for p in pasos:
        lineas = []
        for tr in p.get("tramos") or []:
            for acc in tr.get("acc") or []:
                if isinstance(acc, dict) and acc.get("a") == "mate":
                    lineas.append(acc)
                    blob.append(str(acc.get("tex") or ""))
        total_mate += len(lineas)
        habla = len((p.get("habla") or "").split())
        tit = p.get("titulo") or ""
        n = p.get("n")
        es_resumen = bool(RESUMEN_TIT.search(tit))
        cls = [clase(a) for a in lineas]
        for i, (a, c) in enumerate(zip(lineas, cls)):
            tex = str(a.get("tex") or "")
            if c == "F":
                cids = [x for x in _celdas_en_texto(tex) if not _POR_ID[x][1]]
                if not cids:
                    out["avisos"].append(f"p{n}: fórmula que no es casilla de la hoja: {tex[:90]}")
            if c == "R" and not es_resumen:
                if i == 0 and tex.count("=") >= 2 and clase({"tex": tex.split("=", 1)[1]}) == "N":
                    pass  # el propio resultado trae la sustitución
                elif i == 0:
                    out["faltas"].append(f"p{n} «{tit[:40]}»: resultado sin desarrollo en el paso: {tex[:70]}")
                elif cls[i - 1] in ("F", "S") and re.search(r"\d", tex) and not (tex.count("=") >= 2 and clase({"tex": tex.split("=", 1)[1]}) == "N"):
                    out["faltas"].append(f"p{n} «{tit[:40]}»: de la fórmula al resultado sin sustituir: {lineas[i-1]['tex'][:50]} → {tex[:50]}")
                elif cls[i - 1] == "N":
                    prev = str(lineas[i - 1].get("tex") or "")
                    k = complejidad(prev)
                    if k >= 5 and prev.count("=") <= 1:
                        out["faltas"].append(f"p{n} «{tit[:40]}»: cuenta larga sin paso intermedio (complejidad {k}): {prev[:70]}")
        if lineas and not es_resumen and habla < 4 * len(lineas) and habla < 25:
            out["avisos"].append(f"p{n} «{tit[:40]}»: poca explicación hablada ({habla} palabras para {len(lineas)} renglones)")
    texto = "\n".join(blob)
    for desc, pat, base_pat in FUERA:
        if pat.search(texto) and (base_pat is None or not base_pat.search(texto)):
            out["faltas"].append("formulario: " + desc)
    for f in verificar_origen_formulario(d):
        out["faltas"].append("formulario: " + f)
    if tipo != "seleccion" and total_mate < 6:
        out["faltas"].append(f"muy resumida: solo {total_mate} renglones de matemática en {len(pasos)} pasos")
    if tipo == "seleccion" and total_mate < 3:
        out["faltas"].append(f"selección sin desarrollo: {total_mate} renglones")
    if not base:
        out["faltas"].append("no toma ninguna ecuación del formulario")
    out["renglones"] = total_mate
    out["pasos"] = len(pasos)
    return out


def main():
    res = [auditar(i) for i in registro.ids()]
    res.sort(key=lambda r: (-len(r["faltas"]), -len(r["avisos"]), r["id"]))
    tipos = collections.Counter(r["tipo"] for r in res)
    con = [r for r in res if r["faltas"]]
    print("lecciones:", len(res), dict(tipos), "| con faltas:", len(con), "| con avisos:", sum(1 for r in res if r["avisos"]))
    cuenta = collections.Counter()
    for r in res:
        for f in r["faltas"]:
            cuenta[re.sub(r"^p\d+ «[^»]*»: ", "", f).split(":")[0]] += 1
    for k, v in cuenta.most_common():
        print(f"  {v:4d}  {k}")
    for r in con:
        print(f"\n{r['id']} [{r['tipo']}] {r['titulo'][:60]} · {r.get('pasos')} pasos, {r.get('renglones')} renglones")
        for f in r["faltas"]:
            print("   X", f)
        for a in r["avisos"][:4]:
            print("   ·", a)
    if len(sys.argv) > 1:
        with open(sys.argv[1], "w", encoding="utf-8") as fh:
            json.dump(res, fh, ensure_ascii=False, indent=1)


if __name__ == "__main__":
    main()
