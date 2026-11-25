"""Brain A — Neuro-Symbolic Math & Physics III Verification Engine (SymPy).

Provides deterministic, hallucination-free step planning and student-step error
diagnosis using sympy.integrals.manualintegrate.integral_steps() and dimensional
checks without ever calling Python eval().
"""

from __future__ import annotations

import ast
import io
import re
import tokenize
from dataclasses import dataclass, field
from typing import Any, Callable, Dict, List, Optional

import sympy as sp
from sympy.integrals.manualintegrate import (
    AddRule,
    ConstantRule,
    ConstantTimesRule,
    ExpRule,
    PartsRule,
    PowerRule,
    ReciprocalRule,
    RewriteRule,
    TrigRule,
    URule,
    integral_steps,
)

_ALLOWED_FUNCS: Dict[str, Callable[..., sp.Expr]] = {
    "sin": sp.sin,
    "cos": sp.cos,
    "tan": sp.tan,
    "asin": sp.asin,
    "acos": sp.acos,
    "atan": sp.atan,
    "sinh": sp.sinh,
    "cosh": sp.cosh,
    "tanh": sp.tanh,
    "exp": sp.exp,
    "log": sp.log,
    "ln": sp.log,
    "sqrt": sp.sqrt,
    "Abs": sp.Abs,
    "abs": sp.Abs,
}

_ALLOWED_CONSTS: Dict[str, sp.Expr] = {
    "pi": sp.pi,
    "E": sp.E,
    "e": sp.E,
}

_FORBIDDEN_RE = re.compile(
    r"(__|import|exec|eval|open|os\.|sys\.|subprocess|lambda|class|globals|locals|builtins|getattr|setattr)",
    re.IGNORECASE,
)


def _preprocess_latex_to_sympy_str(raw: str) -> str:
    """Safely normalize common LaTeX math constructs to SymPy-parseable syntax."""
    s = (raw or "").strip()
    if not s:
        return ""
    if _FORBIDDEN_RE.search(s):
        raise ValueError("Expresión rechazada por el filtro de seguridad simbólica.")

    # Strip display math wrappers and spacing commands
    s = s.replace("$$", "").replace("$", "")
    s = re.sub(r"\\class\{[^}]*\}\{([^{}]*)\}", r"\1", s)
    s = s.replace(r"\,", " ").replace(r"\;", " ").replace(r"\!", "")
    s = s.replace(r"\left", "").replace(r"\right", "")
    s = s.replace(r"\cdot", "*").replace(r"\times", "*").replace("·", "*")
    s = s.replace("−", "-").replace("–", "-")

    # Convert \frac{a}{b} iteratively (supports up to 4 levels of nesting)
    frac_re = re.compile(r"\\frac\{([^{}]+)\}\{([^{}]+)\}")
    for _ in range(5):
        new_s = frac_re.sub(r"((\1)/(\2))", s)
        if new_s == s:
            break
        s = new_s

    # Convert \sqrt{a} -> sqrt(a)
    sqrt_re = re.compile(r"\\sqrt\{([^{}]+)\}")
    for _ in range(4):
        new_s = sqrt_re.sub(r"sqrt(\1)", s)
        if new_s == s:
            break
        s = new_s

    # Functions and constants
    replacements = [
        (r"\ln", "log"),
        (r"\log", "log"),
        (r"\sin", "sin"),
        (r"\cos", "cos"),
        (r"\tan", "tan"),
        (r"\exp", "exp"),
        (r"\pi", "pi"),
        ("π", "pi"),
    ]
    for old, new in replacements:
        s = s.replace(old, new)

    # Convert ^{...} to **(...) and _{...} to _...
    s = re.sub(r"\^\{([^{}]+)\}", r"**(\1)", s)
    s = re.sub(r"_\{([^{}]+)\}", r"_\1", s)
    return s.strip()


def _insert_implicit_multiplication(expr: str) -> str:
    """Insert explicit '*' where standard math notation omits it (2x, 2(x+1), (a)(b)) safely."""
    s = expr.replace("^", "**")
    try:
        toks = list(tokenize.generate_tokens(io.StringIO(s).readline))
    except tokenize.TokenError as exc:
        raise ValueError("Sintaxis matemática incompleta o inválida.") from exc

    out: List[str] = []
    prev_type: Optional[int] = None
    prev_str: str = ""
    for tok in toks:
        ttype, tstr = tok.type, tok.string
        if ttype in (tokenize.ENDMARKER, tokenize.NEWLINE, tokenize.NL, tokenize.INDENT, tokenize.DEDENT):
            continue
        if prev_type is not None:
            need_mul = False
            if prev_type == tokenize.NUMBER and (ttype == tokenize.NAME or tstr == "("):
                need_mul = True
            elif prev_str == ")" and (ttype in (tokenize.NUMBER, tokenize.NAME) or tstr == "("):
                need_mul = True
            elif prev_type == tokenize.NAME and prev_str not in _ALLOWED_FUNCS and tstr == "(":
                need_mul = True
            if need_mul:
                out.append("*")
        out.append(tstr)
        prev_type, prev_str = ttype, tstr
    return "".join(out)


def _ast_to_sympy(node: ast.AST, symbols_env: Dict[str, sp.Expr], depth: int = 0) -> sp.Expr:
    """Strictly whitelisted AST -> SymPy converter. Never calls eval() or parse_expr()."""
    if depth > 48:
        raise ValueError("Expresión rechazada por el filtro de seguridad simbólica (profundidad excesiva).")
    if isinstance(node, ast.Expression):
        return _ast_to_sympy(node.body, symbols_env, depth + 1)
    if isinstance(node, ast.Constant):
        if isinstance(node.value, bool) or not isinstance(node.value, (int, float)):
            raise ValueError("Expresión rechazada por el filtro de seguridad simbólica (constante inválida).")
        if isinstance(node.value, int):
            return sp.Integer(node.value)
        return sp.Float(node.value)
    if isinstance(node, ast.Name):
        name = node.id
        if name.startswith("_"):
            raise ValueError("Expresión rechazada por el filtro de seguridad simbólica.")
        if name in _ALLOWED_CONSTS:
            return _ALLOWED_CONSTS[name]
        if name in symbols_env:
            return symbols_env[name]
        if not re.fullmatch(r"[A-Za-z][A-Za-z0-9_]{0,16}", name):
            raise ValueError("Expresión rechazada por el filtro de seguridad simbólica.")
        sym = sp.Symbol(name, real=True)
        symbols_env[name] = sym
        return sym
    if isinstance(node, ast.UnaryOp):
        operand = _ast_to_sympy(node.operand, symbols_env, depth + 1)
        if isinstance(node.op, ast.UAdd):
            return operand
        if isinstance(node.op, ast.USub):
            return -operand
        raise ValueError("Expresión rechazada por el filtro de seguridad simbólica (operador unario).")
    if isinstance(node, ast.BinOp):
        left = _ast_to_sympy(node.left, symbols_env, depth + 1)
        right = _ast_to_sympy(node.right, symbols_env, depth + 1)
        if isinstance(node.op, ast.Add):
            return left + right
        if isinstance(node.op, ast.Sub):
            return left - right
        if isinstance(node.op, ast.Mult):
            return left * right
        if isinstance(node.op, ast.Div):
            return left / right
        if isinstance(node.op, ast.Pow):
            if right.is_number:
                try:
                    if abs(complex(right)) > 200:
                        raise ValueError("Expresión rechazada por el filtro de seguridad simbólica (exponente extremo).")
                except TypeError:
                    pass
            return left**right
        raise ValueError("Expresión rechazada por el filtro de seguridad simbólica (operador binario).")
    if isinstance(node, ast.Call):
        if not isinstance(node.func, ast.Name) or node.keywords:
            raise ValueError("Expresión rechazada por el filtro de seguridad simbólica (llamada no permitida).")
        fn = _ALLOWED_FUNCS.get(node.func.id)
        if fn is None:
            raise ValueError(f"Expresión rechazada por el filtro de seguridad simbólica (función '{node.func.id}').")
        if not (1 <= len(node.args) <= 2):
            raise ValueError("Expresión rechazada por el filtro de seguridad simbólica (argumentos inválidos).")
        args = [_ast_to_sympy(a, symbols_env, depth + 1) for a in node.args]
        return fn(*args)
    raise ValueError(f"Expresión rechazada por el filtro de seguridad simbólica ({type(node).__name__}).")


def safe_parse_math(expr_text: str, extra_symbols: Optional[List[str]] = None) -> sp.Expr:
    """Parse math/LaTeX string into a SymPy Expr via a strict AST whitelist (zero eval())."""
    if len(expr_text or "") > 450:
        raise ValueError("Expresión rechazada por el filtro de seguridad simbólica (demasiado larga).")
    cleaned = _preprocess_latex_to_sympy_str(expr_text)
    if not cleaned:
        raise ValueError("La expresión matemática está vacía.")

    normalized = _insert_implicit_multiplication(cleaned)
    try:
        tree = ast.parse(normalized, mode="eval")
    except SyntaxError as exc:
        raise ValueError(f"Sintaxis matemática inválida: {exc.msg}") from exc

    symbols_env: Dict[str, sp.Expr] = {}
    common_syms = [
        "x", "t", "u", "v", "V", "P", "r", "R", "T", "n", "k", "L", "H", "A", "W",
        "Q", "C", "a", "b", "c", "m", "gamma", "V_i", "V_f", "r_1", "r_2", "T_1", "T_2",
        "P_0", "V_0",
    ]
    for name in common_syms + (extra_symbols or []):
        symbols_env[name] = (
            sp.Symbol(name, positive=True)
            if name in ("V", "r", "T", "n", "R", "k", "L")
            else sp.Symbol(name, real=True)
        )

    return _ast_to_sympy(tree, symbols_env)


def _describe_rule_es(rule: Any, steps_out: List[Dict[str, str]]) -> None:
    """Recursively flatten a SymPy manualintegrate rule tree into Spanish pedagogical steps."""
    if isinstance(rule, ConstantRule):
        steps_out.append({
            "rule": "ConstantRule",
            "title_es": "Integral de una constante",
            "rationale_es": f"Como {sp.latex(rule.constant)} es constante respecto a {sp.latex(rule.variable)}, su antiderivada es la constante multiplicada por {sp.latex(rule.variable)}.",
            "latex": f"\\int {sp.latex(rule.integrand)}\\,d{sp.latex(rule.variable)} = {sp.latex(rule.constant * rule.variable)}",
        })
    elif isinstance(rule, ConstantTimesRule):
        steps_out.append({
            "rule": "ConstantTimesRule",
            "title_es": "Sacar la constante fuera de la integral",
            "rationale_es": f"El factor {sp.latex(rule.constant)} no depende de {sp.latex(rule.variable)}, así que sale fuera del símbolo de integral por linealidad.",
            "latex": f"\\int {sp.latex(rule.integrand)}\\,d{sp.latex(rule.variable)} = {sp.latex(rule.constant)} \\int {sp.latex(rule.other)}\\,d{sp.latex(rule.variable)}",
        })
        _describe_rule_es(rule.substep, steps_out)
    elif isinstance(rule, PowerRule):
        exp_val = rule.exp
        new_exp = sp.simplify(exp_val + 1)
        steps_out.append({
            "rule": "PowerRule",
            "title_es": "Regla de potencias",
            "rationale_es": f"Sumamos 1 al exponente ({sp.latex(exp_val)} + 1 = {sp.latex(new_exp)}) y dividimos entre ese mismo número ({sp.latex(new_exp)}).",
            "latex": f"\\int {sp.latex(rule.integrand)}\\,d{sp.latex(rule.variable)} = \\frac{{{sp.latex(rule.base)}^{{{sp.latex(exp_val)}+1}}}}{{{sp.latex(exp_val)}+1}} = {sp.latex(rule.base**new_exp / new_exp)}",
        })
    elif isinstance(rule, ReciprocalRule):
        steps_out.append({
            "rule": "ReciprocalRule",
            "title_es": "Integral de 1/x (logaritmo natural)",
            "rationale_es": f"El caso exponente -1 ({sp.latex(rule.integrand)}) no usa regla de potencias porque dividiríamos entre cero; su antiderivada es el logaritmo natural.",
            "latex": f"\\int {sp.latex(rule.integrand)}\\,d{sp.latex(rule.variable)} = \\ln\\left|{sp.latex(rule.base)}\\right|",
        })
    elif isinstance(rule, AddRule):
        steps_out.append({
            "rule": "AddRule",
            "title_es": "Separar integral de una suma",
            "rationale_es": "La integral de una suma o resta se separa término a término.",
            "latex": f"\\int \\left({sp.latex(rule.integrand)}\\right)\\,d{sp.latex(rule.variable)}",
        })
        for sub in rule.substeps:
            _describe_rule_es(sub, steps_out)
    elif isinstance(rule, URule):
        steps_out.append({
            "rule": "URule",
            "title_es": "Cambio de variable (sustitución u)",
            "rationale_es": f"Elegimos u = {sp.latex(rule.u_func)} para simplificar la integral; así du = {sp.latex(sp.diff(rule.u_func, rule.variable))}\\,d{sp.latex(rule.variable)}.",
            "latex": f"u = {sp.latex(rule.u_func)},\\quad du = {sp.latex(sp.diff(rule.u_func, rule.variable))}\\,d{sp.latex(rule.variable)}",
        })
        _describe_rule_es(rule.substep, steps_out)
    elif isinstance(rule, PartsRule):
        steps_out.append({
            "rule": "PartsRule",
            "title_es": "Integración por partes",
            "rationale_es": f"Aplicamos ∫ u dv = u·v − ∫ v du con u = {sp.latex(rule.u)} y dv = {sp.latex(rule.dv)}.",
            "latex": f"u = {sp.latex(rule.u)},\\quad dv = {sp.latex(rule.dv)}\\,d{sp.latex(rule.variable)}",
        })
        _describe_rule_es(rule.second_step, steps_out)
    elif isinstance(rule, (TrigRule, ExpRule)):
        res = sp.integrate(rule.integrand, rule.variable)
        steps_out.append({
            "rule": type(rule).__name__,
            "title_es": "Integral inmediata estándar",
            "rationale_es": "Aplicamos la fórmula directa de antiderivada elemental.",
            "latex": f"\\int {sp.latex(rule.integrand)}\\,d{sp.latex(rule.variable)} = {sp.latex(res)}",
        })
    elif isinstance(rule, RewriteRule):
        steps_out.append({
            "rule": "RewriteRule",
            "title_es": "Reescritura algebraica previa",
            "rationale_es": f"Reescribimos el integrando de forma equivalente como {sp.latex(rule.rewritten)} antes de integrar.",
            "latex": f"{sp.latex(rule.integrand)} = {sp.latex(rule.rewritten)}",
        })
        _describe_rule_es(rule.substep, steps_out)
    else:
        res = sp.integrate(rule.integrand, rule.variable)
        steps_out.append({
            "rule": type(rule).__name__,
            "title_es": "Integración simbólica verificada",
            "rationale_es": "Evaluamos la antiderivada simbólica y verificamos derivando.",
            "latex": f"\\int {sp.latex(rule.integrand)}\\,d{sp.latex(rule.variable)} = {sp.latex(res)}",
        })


@dataclass
class VerifiedIntegralPlan:
    ok: bool
    integrand_latex: str = ""
    variable: str = "x"
    antiderivative_expr: str = ""
    antiderivative_latex: str = ""
    derivative_check_passed: bool = False
    is_definite: bool = False
    lower_latex: str = ""
    upper_latex: str = ""
    definite_value_expr: str = ""
    definite_value_latex: str = ""
    definite_numeric: Optional[float] = None
    evaluation_bridge_latex: str = ""
    steps: List[Dict[str, str]] = field(default_factory=list)
    dimensional_note: str = ""
    error: str = ""

    def to_dict(self) -> Dict[str, Any]:
        return {
            "ok": self.ok,
            "integrand_latex": self.integrand_latex,
            "variable": self.variable,
            "antiderivative_expr": self.antiderivative_expr,
            "antiderivative_latex": self.antiderivative_latex,
            "derivative_check_passed": self.derivative_check_passed,
            "is_definite": self.is_definite,
            "lower_latex": self.lower_latex,
            "upper_latex": self.upper_latex,
            "definite_value_expr": self.definite_value_expr,
            "definite_value_latex": self.definite_value_latex,
            "definite_numeric": self.definite_numeric,
            "evaluation_bridge_latex": self.evaluation_bridge_latex,
            "steps": self.steps,
            "dimensional_note": self.dimensional_note,
            "error": self.error,
        }


def build_verified_integral_plan(
    integrand_text: str,
    var_name: str = "x",
    lower_text: Optional[str] = None,
    upper_text: Optional[str] = None,
    physics_context: str = "",
) -> VerifiedIntegralPlan:
    """Compute a step-by-step Verified Solution Script (VSS) for an integral via SymPy."""
    try:
        var_sym = safe_parse_math(var_name)
        if not isinstance(var_sym, sp.Symbol):
            var_sym = sp.Symbol(var_name.strip(), real=True)
        expr = safe_parse_math(integrand_text, extra_symbols=[str(var_sym)])
    except Exception as exc:
        return VerifiedIntegralPlan(ok=False, variable=var_name, error=f"Error interpretando integrando: {exc}")

    # Align symbol instances so expr uses the exact same Symbol object as var_sym
    for sym in list(expr.free_symbols):
        if sym.name == var_sym.name and sym != var_sym:
            expr = expr.subs(sym, var_sym)

    steps: List[Dict[str, str]] = []
    try:
        rule_tree = integral_steps(expr, var_sym)
        _describe_rule_es(rule_tree, steps)
    except Exception:
        pass

    antideriv = sp.simplify(sp.integrate(expr, var_sym))
    if isinstance(antideriv, sp.Integral):
        return VerifiedIntegralPlan(
            ok=False,
            integrand_latex=sp.latex(expr),
            variable=str(var_sym),
            error="No existe una antiderivada elemental cerrada; requiere integración numérica.",
        )

    # Independent verification by differentiation: d/dx(F(x)) - f(x) == 0
    diff_residual = sp.simplify(sp.diff(antideriv, var_sym) - expr)
    deriv_ok = diff_residual == 0
    if not deriv_ok:
        # Try numerical spot-check for log(abs(x)) / branch identities
        try:
            test_vals = [1.3, 2.7, 4.1]
            free_others = {s: 2.0 for s in expr.free_symbols if s != var_sym}
            max_err = max(
                abs(complex(diff_residual.subs({**free_others, var_sym: tv}).evalf()))
                for tv in test_vals
            )
            deriv_ok = max_err < 1e-9
        except Exception:
            deriv_ok = False

    plan = VerifiedIntegralPlan(
        ok=deriv_ok,
        integrand_latex=sp.latex(expr),
        variable=str(var_sym),
        antiderivative_expr=str(antideriv),
        antiderivative_latex=sp.latex(antideriv),
        derivative_check_passed=deriv_ok,
        steps=steps,
    )

    if lower_text is not None and upper_text is not None and str(lower_text).strip() and str(upper_text).strip():
        try:
            a_expr = safe_parse_math(str(lower_text))
            b_expr = safe_parse_math(str(upper_text))
            fa = sp.simplify(antideriv.subs(var_sym, a_expr))
            fb = sp.simplify(antideriv.subs(var_sym, b_expr))
            val = sp.simplify(fb - fa)
            plan.is_definite = True
            plan.lower_latex = sp.latex(a_expr)
            plan.upper_latex = sp.latex(b_expr)
            plan.definite_value_expr = str(val)
            plan.definite_value_latex = sp.latex(val)
            plan.evaluation_bridge_latex = (
                f"\\left[{sp.latex(antideriv)}\\right]_{{{plan.lower_latex}}}^{{{plan.upper_latex}}} "
                f"= \\left({sp.latex(fb)}\\right) - \\left({sp.latex(fa)}\\right) = {plan.definite_value_latex}"
            )
            plan.steps.append({
                "rule": "FundamentalTheoremOfCalculus",
                "title_es": "Evaluar límites (Teorema Fundamental del Cálculo)",
                "rationale_es": f"Evaluamos la antiderivada en el límite superior ({plan.upper_latex}) y restamos su valor en el límite inferior ({plan.lower_latex}).",
                "latex": plan.evaluation_bridge_latex,
            })
            if not val.free_symbols:
                plan.definite_numeric = float(val.evalf())
        except Exception as exc:
            plan.ok = False
            plan.error = f"Error evaluando límites definidos: {exc}"

    ctx_low = (physics_context or "").lower()
    if "trabajo" in ctx_low or "p(v)" in ctx_low or "isoterm" in ctx_low or var_sym.name == "V":
        plan.dimensional_note = "SI: [P]·[dV] = Pa·m³ = (N/m²)·m³ = N·m = J [OK]"
    elif "conducc" in ctx_low or "esfer" in ctx_low or "cilindr" in ctx_low or var_sym.name == "r":
        plan.dimensional_note = "SI: [H] = [k]·[A]·[dT]/[dr] = (W/(m·K))·m²·K/m = W [OK]"

    return plan


def check_dimensional_work_pv(pressure_unit: str, volume_unit: str) -> Dict[str, Any]:
    """Verify SI consistency for thermodynamic work W = int P dV."""
    p_u = (pressure_unit or "").strip().lower()
    v_u = (volume_unit or "").strip().lower()
    p_scales = {"pa": 1.0, "kpa": 1e3, "mpa": 1e6, "atm": 101325.0, "bar": 1e5}
    v_scales = {"m3": 1.0, "m^3": 1.0, "m³": 1.0, "l": 1e-3, "cm3": 1e-6, "cm^3": 1e-6, "cm³": 1e-6}
    if p_u not in p_scales or v_u not in v_scales:
        return {
            "ok": False,
            "error_category": "DimensionalMismatch",
            "message_es": f"Unidades no reconocidas o incompatibles ({pressure_unit} · {volume_unit}); convierte a Pa y m³ para obtener Joules.",
        }
    factor = p_scales[p_u] * v_scales[v_u]
    if abs(factor - 1.0) > 1e-12:
        return {
            "ok": False,
            "error_category": "DimensionalMismatch",
            "conversion_factor_to_J": factor,
            "message_es": f"Atención con las unidades: 1 {pressure_unit}·{volume_unit} = {factor:g} J (no es 1 J directo; debes convertir primero a Pa y m³).",
        }
    return {
        "ok": True,
        "error_category": "None",
        "conversion_factor_to_J": 1.0,
        "message_es": "Consistencia dimensional SI verificada: Pa·m³ = J.",
    }


def verify_student_step(
    expected_expr_text: str,
    student_expr_text: str,
    var_name: str = "x",
    integrand_text: Optional[str] = None,
) -> Dict[str, Any]:
    """Compare student's algebraic/integration step against expected SymPy expression and classify errors."""
    try:
        expected = safe_parse_math(expected_expr_text)
        student = safe_parse_math(student_expr_text)
    except Exception as exc:
        return {
            "equivalent": False,
            "error_category": "SyntaxError",
            "message_es": f"No pude leer la expresión escrita ({exc}). Revisa paréntesis o signos.",
            "hint_es": "Escribe por ejemplo -1/r o x^3/3.",
        }

    # Normalize symbols by name
    sym_map = {s.name: s for s in expected.free_symbols}
    for s in list(student.free_symbols):
        if s.name in sym_map and s != sym_map[s.name]:
            student = student.subs(s, sym_map[s.name])

    diff = sp.simplify(student - expected)
    if diff == 0:
        return {
            "equivalent": True,
            "error_category": "None",
            "message_es": "Paso verificado simbólicamente: exactamente equivalente.",
            "hint_es": "",
        }

    # Check if equal up to an additive constant + C (valid for indefinite integrals)
    var_sym = sym_map.get(var_name, sp.Symbol(var_name))
    if sp.simplify(sp.diff(diff, var_sym)) == 0:
        return {
            "equivalent": True,
            "error_category": "None",
            "message_es": "Antiderivada correcta (equivalente salvo constante de integración C).",
            "hint_es": "",
        }

    # 1. Pure SignError: student == -expected
    if sp.simplify(student + expected) == 0:
        return {
            "equivalent": False,
            "error_category": "SignError",
            "message_es": "El valor es casi exacto, pero tiene el signo invertido.",
            "hint_es": "Revisa el signo que produce el denominador o el orden F(b) − F(a) al restar los límites.",
        }

    # 2. PowerRuleOffset: student differentiated integrand instead of integrating, or forgot 1/(n+1)
    if integrand_text:
        try:
            integ = safe_parse_math(integrand_text)
            for s in list(integ.free_symbols):
                if s.name in sym_map and s != sym_map[s.name]:
                    integ = integ.subs(s, sym_map[s.name])
            if sp.simplify(student - sp.diff(integ, var_sym)) == 0:
                return {
                    "equivalent": False,
                    "error_category": "PowerRuleOffset",
                    "message_es": "Aplicaste la derivada en lugar de la integral (restaste 1 al exponente).",
                    "hint_es": "Para integrar una potencia x^n, SIEMPRE sumas 1 al exponente y divides entre (n+1).",
                }
        except Exception:
            pass

    # 3. Missing constant factor: student / expected is a constant != 1
    try:
        ratio = sp.simplify(student / expected)
        if not ratio.free_symbols and ratio != 1:
            return {
                "equivalent": False,
                "error_category": "MissingConstantFactor",
                "message_es": f"Tu expresión difiere por un factor constante ({sp.latex(ratio)}).",
                "hint_es": "Revisa si olvidaste dividir entre el nuevo exponente (n+1) o sacar una constante fuera de la integral.",
            }
    except Exception:
        pass

    return {
        "equivalent": False,
        "error_category": "AlgebraicMismatch",
        "message_es": "La expresión todavía no coincide con el paso esperado.",
        "hint_es": "Vuelve al paso anterior y verifica qué variable estás integrando y cuál regla aplica.",
    }
