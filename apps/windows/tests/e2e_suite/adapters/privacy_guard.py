"""Zero-credential and PII leak scanner & scrubber (pure Python 3 stdlib)."""
from __future__ import annotations

import os
import re
from pathlib import Path
from typing import Iterable


_ENV_SECRET_KEYS = (
    "TEC_EMAIL",
    "TEC_PASSWORD",
    "TEC_USER",
    "TEC_PASS",
    "TEC_TOKEN",
    "TEC_COOKIE",
)

_ALLOWED_EMAIL_DOMAINS = (
    "example.invalid",
)

_REDACTED_MARKERS = {
    "***",
    "redacted",
    "<redacted>",
    "[redacted]",
}

_NAME_PATTERN = re.compile(r"\bnel" + r"son\b", re.IGNORECASE)
_EMAIL_PATTERN = re.compile(r"\b([A-Za-z0-9._%+-]+@([A-Za-z0-9.-]+\.[A-Za-z]{2,}))\b")
_COOKIE_PATTERN = re.compile(
    r"\b(ad_session_id|ad_user_login|AlteonP)\s*=\s*([^\s;\"',}<>{]+)"
)
_ZOOM_PWD_PATTERN = re.compile(r"([?&]pwd=)([^&\s\"'<>]+)", re.IGNORECASE)
_AUTH_HEADER_PATTERN = re.compile(
    r"(Authorization\s*:\s*(?:Bearer|Basic)\s+)([^\s\"'\r\n]+)", re.IGNORECASE
)
_PASS_KV_PATTERN = re.compile(
    r"([\"']?(?:password|passwd)[\"']?\s*[:=]\s*[\"'])([^\"'\r\n]+)([\"'])",
    re.IGNORECASE,
)
_PASS_UNQUOTED_KV_PATTERN = re.compile(
    r"(\b(?:password|passwd)\s*(?<![=!<>])=(?!=)\s*)([^\s&\"'<>]+)",
    re.IGNORECASE,
)

_TEXT_EXTENSIONS = {
    ".py",
    ".json",
    ".md",
    ".txt",
    ".csv",
    ".log",
    ".html",
    ".htm",
    ".xml",
    ".svg",
    ".dxf",
    ".kicad_sch",
    ".cpp",
    ".hpp",
    ".h",
    ".c",
    ".cmake",
    ".har",
    ".sql",
    ".ini",
    ".sh",
}


def _collect_env_secrets(extra_secrets: Iterable[str] | None = None) -> list[tuple[str, str]]:
    secrets: list[tuple[str, str]] = []
    for k in _ENV_SECRET_KEYS:
        val = os.environ.get(k, "")
        if val and len(val) >= 3:
            secrets.append((f"env:{k}", val))
    if extra_secrets:
        for idx, s in enumerate(extra_secrets):
            if s and len(s) >= 3:
                secrets.append((f"extra:{idx}", s))
    return secrets


def _is_allowed_email_domain(domain: str) -> bool:
    dl = domain.lower().strip(".")
    return any(dl == d or dl.endswith("." + d) for d in _ALLOWED_EMAIL_DOMAINS)


def _is_synthetic_or_redacted_cookie(val: str) -> bool:
    vl = val.strip().lower()
    return (
        vl in _REDACTED_MARKERS
        or (vl.startswith("[redacted") and vl.endswith("]"))
        or vl.startswith("synth_")
    )


def _is_synthetic_or_redacted_pwd(val: str) -> bool:
    vl = val.strip().lower()
    return (
        vl in _REDACTED_MARKERS
        or (vl.startswith("[redacted") and vl.endswith("]"))
        or vl.startswith("synth_")
        or vl.startswith("synth-")
    )


def scan_text_for_leaks(
    text: str,
    extra_secrets: list[str] | None = None,
) -> list[str]:
    """Return a list of leak descriptions found in `text` (empty list == clean)."""
    if not text:
        return []

    findings: list[str] = []

    for label, secret in _collect_env_secrets(extra_secrets):
        if secret in text:
            findings.append(f"LEAK_SECRET({label})")

    if _NAME_PATTERN.search(text):
        findings.append("LEAK_PERSONAL_NAME")

    for m in _EMAIL_PATTERN.finditer(text):
        full_email = m.group(1)
        domain = m.group(2)
        if not _is_allowed_email_domain(domain):
            findings.append(f"LEAK_EMAIL({full_email})")

    for m in _COOKIE_PATTERN.finditer(text):
        name, val = m.group(1), m.group(2)
        if not _is_synthetic_or_redacted_cookie(val):
            findings.append(f"LEAK_COOKIE({name})")

    for m in _ZOOM_PWD_PATTERN.finditer(text):
        val = m.group(2)
        if not _is_synthetic_or_redacted_pwd(val):
            findings.append("LEAK_ZOOM_PWD")

    for m in _AUTH_HEADER_PATTERN.finditer(text):
        val = m.group(2)
        if not _is_synthetic_or_redacted_pwd(val):
            findings.append("LEAK_AUTH_HEADER")

    for m in _PASS_KV_PATTERN.finditer(text):
        val = m.group(2)
        if not _is_synthetic_or_redacted_pwd(val):
            findings.append("LEAK_PASSWORD_LITERAL")

    for m in _PASS_UNQUOTED_KV_PATTERN.finditer(text):
        val = m.group(2)
        if not _is_synthetic_or_redacted_pwd(val):
            findings.append("LEAK_PASSWORD_KV")

    # Deduplicate while preserving order
    return list(dict.fromkeys(findings))


def scan_tree_for_leaks(
    root: Path,
    extra_secrets: list[str] | None = None,
) -> list[str]:
    """Recursively scan text files under `root` for credential or PII leaks."""
    root = Path(root)
    if not root.exists():
        return []

    all_findings: list[str] = []
    if root.is_file():
        files = [root]
    else:
        files = sorted(p for p in root.rglob("*") if p.is_file())

    for path in files:
        if any(part in ("__pycache__", ".git", "build-e2e-probe") for part in path.parts):
            continue
        ext = path.suffix.lower()
        name_lower = path.name.lower()
        if ext not in _TEXT_EXTENSIONS and name_lower != "cmakelists.txt":
            continue
        try:
            content = path.read_text(encoding="utf-8", errors="ignore")
        except OSError:
            continue

        for item in scan_text_for_leaks(content, extra_secrets=extra_secrets):
            rel = path.relative_to(root) if root.is_dir() else path.name
            all_findings.append(f"{rel}: {item}")

    return all_findings


def scrub_secrets(
    text: str,
    extra_secrets: list[str] | None = None,
) -> str:
    """Redact credentials, raw cookies, auth headers, ?pwd= tokens, emails, and personal names from `text`."""
    if not text:
        return text

    out = text
    for _, secret in _collect_env_secrets(extra_secrets):
        out = out.replace(secret, "[REDACTED]")

    out = _COOKIE_PATTERN.sub(
        lambda m: m.group(0) if _is_synthetic_or_redacted_cookie(m.group(2)) else f"{m.group(1)}=[REDACTED]",
        out,
    )
    out = _ZOOM_PWD_PATTERN.sub(
        lambda m: m.group(0) if _is_synthetic_or_redacted_pwd(m.group(2)) else f"{m.group(1)}[REDACTED]",
        out,
    )
    out = _AUTH_HEADER_PATTERN.sub(
        lambda m: m.group(0) if _is_synthetic_or_redacted_pwd(m.group(2)) else f"{m.group(1)}[REDACTED]",
        out,
    )
    out = _PASS_KV_PATTERN.sub(
        lambda m: m.group(0) if _is_synthetic_or_redacted_pwd(m.group(2)) else f"{m.group(1)}[REDACTED]{m.group(3)}",
        out,
    )
    out = _PASS_UNQUOTED_KV_PATTERN.sub(
        lambda m: m.group(0) if _is_synthetic_or_redacted_pwd(m.group(2)) else f"{m.group(1)}[REDACTED_PASSWORD]",
        out,
    )
    out = _EMAIL_PATTERN.sub(
        lambda m: m.group(1) if _is_allowed_email_domain(m.group(2)) else "[REDACTED_EMAIL]",
        out,
    )
    out = _NAME_PATTERN.sub("[REDACTED_NAME]", out)
    return out
