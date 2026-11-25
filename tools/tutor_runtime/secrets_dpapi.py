"""Windows DPAPI secret storage and log sanitization for CuadernU Tutor Runtime.

Stores API keys encrypted exclusively with the current Windows user's DPAPI
credentials (CryptProtectData / CryptUnprotectData) inside private_local/
(which is Git-ignored and verified by tools/check_workspace_hygiene.py).
Never writes plaintext secrets to disk or stdout.
"""

from __future__ import annotations

import ctypes
import ctypes.wintypes as wt
import json
import os
import pathlib
import re
from typing import Dict, Optional

DPAPI_ENTROPY = b"CuadernU.TutorVault.v1"

_SECRET_PATTERNS = [
    re.compile(r"AQ\.[A-Za-z0-9_\-]{12,}"),
    re.compile(r"AIza[0-9A-Za-z_\-]{20,}"),
    re.compile(r"sk-[A-Za-z0-9_\-]{16,}"),
    re.compile(r"Bearer\s+[A-Za-z0-9_\-\.]{16,}", re.IGNORECASE),
    re.compile(r"([?&](?:key|api_key|token)=)[^&\s\"']+", re.IGNORECASE),
]


class _DATA_BLOB(ctypes.Structure):
    _fields_ = [
        ("cbData", wt.DWORD),
        ("pbData", ctypes.POINTER(ctypes.c_char)),
    ]


def sanitize_text(text: str) -> str:
    """Redact any credential-like token before logging or returning errors."""
    if not text:
        return ""
    out = text
    for pat in _SECRET_PATTERNS:
        if pat.groups:
            out = pat.sub(r"\1[REDACTED]", out)
        else:
            out = pat.sub("[REDACTED]", out)
    return out


def mask_key(raw: str) -> str:
    if not raw or len(raw) < 10:
        return "[empty]"
    return f"{raw[:5]}...{raw[-4:]} (len={len(raw)})"


def dpapi_protect(plain: bytes, entropy: bytes = DPAPI_ENTROPY) -> bytes:
    if os.name != "nt":
        raise RuntimeError("Windows DPAPI is only available on Windows.")
    crypt32 = ctypes.windll.crypt32
    kernel32 = ctypes.windll.kernel32

    in_buf = ctypes.create_string_buffer(plain, len(plain))
    in_blob = _DATA_BLOB(len(plain), in_buf)
    ent_buf = ctypes.create_string_buffer(entropy, len(entropy))
    ent_blob = _DATA_BLOB(len(entropy), ent_buf)
    out_blob = _DATA_BLOB()

    # CRYPTPROTECT_UI_FORBIDDEN = 0x1
    ok = crypt32.CryptProtectData(
        ctypes.byref(in_blob),
        "CuadernU Tutor Secrets",
        ctypes.byref(ent_blob),
        None,
        None,
        0x1,
        ctypes.byref(out_blob),
    )
    if not ok:
        raise ctypes.WinError()
    try:
        return ctypes.string_at(out_blob.pbData, out_blob.cbData)
    finally:
        kernel32.LocalFree(out_blob.pbData)


def dpapi_unprotect(protected: bytes, entropy: bytes = DPAPI_ENTROPY) -> bytes:
    if os.name != "nt":
        raise RuntimeError("Windows DPAPI is only available on Windows.")
    crypt32 = ctypes.windll.crypt32
    kernel32 = ctypes.windll.kernel32

    in_buf = ctypes.create_string_buffer(protected, len(protected))
    in_blob = _DATA_BLOB(len(protected), in_buf)
    ent_buf = ctypes.create_string_buffer(entropy, len(entropy))
    ent_blob = _DATA_BLOB(len(entropy), ent_buf)
    out_blob = _DATA_BLOB()

    ok = crypt32.CryptUnprotectData(
        ctypes.byref(in_blob),
        None,
        ctypes.byref(ent_blob),
        None,
        None,
        0x1,
        ctypes.byref(out_blob),
    )
    if not ok:
        raise ctypes.WinError()
    try:
        return ctypes.string_at(out_blob.pbData, out_blob.cbData)
    finally:
        kernel32.LocalFree(out_blob.pbData)


def default_vault_path() -> pathlib.Path:
    repo_root = pathlib.Path(__file__).resolve().parent.parent.parent
    return repo_root / "private_local" / "tutor_secrets.dpapi"


def save_tutor_secrets(secrets: Dict[str, str], vault_path: Optional[pathlib.Path] = None) -> pathlib.Path:
    path = vault_path or default_vault_path()
    path.parent.mkdir(parents=True, exist_ok=True)
    clean = {k: v.strip() for k, v in secrets.items() if isinstance(v, str) and v.strip()}
    payload = json.dumps(clean, ensure_ascii=False).encode("utf-8")
    blob = dpapi_protect(payload)
    tmp = path.with_suffix(".dpapi.tmp")
    tmp.write_bytes(blob)
    os.replace(tmp, path)
    return path


def load_tutor_secrets(vault_path: Optional[pathlib.Path] = None) -> Dict[str, str]:
    path = vault_path or default_vault_path()
    if not path.is_file():
        return {}
    blob = path.read_bytes()
    if not blob:
        return {}
    plain = dpapi_unprotect(blob)
    data = json.loads(plain.decode("utf-8"))
    if not isinstance(data, dict):
        return {}
    return {str(k): str(v) for k, v in data.items() if isinstance(v, str)}
