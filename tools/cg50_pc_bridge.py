#!/usr/bin/env python3
"""
cg50_pc_bridge.py - Direct PC <-> Casio fx-CG50 Bridge CLI (zero PIN / zero password).

Connects directly over Wi-Fi LAN or Android USB cable (--adb -> adb forward tcp:8750 tcp:8750)
to exchange individual SMEM files (.py, .g3a, .g3m, .g3e, .g3p, .csv, .bmp), capture the live
384x216 LCD screen, decode on-screen QR codes, or synchronize a local Windows/macOS/Linux folder
as a live virtual USB drive.

Examples:
  python tools/cg50_pc_bridge.py --adb ls /
  python tools/cg50_pc_bridge.py --adb put mi_script.py /mi_script.py
  python tools/cg50_pc_bridge.py --adb get /Geometry.g3a Geometry.g3a
  python tools/cg50_pc_bridge.py --adb screen captura.png
  python tools/cg50_pc_bridge.py --adb sync ./carpeta_cg50 --watch
  python tools/cg50_pc_bridge.py --host 192.168.1.42 web
"""

import argparse
import json
import os
import pathlib
import shutil
import subprocess
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
import webbrowser


DEFAULT_PORT = 8750


def find_adb() -> str:
    env_adb = os.environ.get("ADB_PATH")
    if env_adb and os.path.isfile(env_adb):
        return env_adb
    win_devtools = r"D:\DevTools\AndroidSDK\platform-tools\adb.exe"
    if os.path.isfile(win_devtools):
        return win_devtools
    found = shutil.which("adb")
    if found:
        return found
    raise RuntimeError("adb not found in PATH or D:\\DevTools\\AndroidSDK\\platform-tools\\adb.exe")


def setup_adb_forward(port: int, serial: str | None) -> None:
    adb = find_adb()
    cmd = [adb]
    if serial:
        cmd += ["-s", serial]
    cmd += ["forward", f"tcp:{port}", f"tcp:{port}"]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        raise RuntimeError(f"adb forward failed: {proc.stderr.strip() or proc.stdout.strip()}")


def http_request(base_url: str, method: str, path_and_query: str, body: bytes | None = None) -> tuple[int, bytes]:
    url = base_url.rstrip("/") + path_and_query
    req = urllib.request.Request(url, data=body, method=method)
    if body is not None:
        req.add_header("Content-Type", "application/octet-stream")
    try:
        with urllib.request.urlopen(req, timeout=20) as resp:
            return resp.status, resp.read()
    except urllib.error.HTTPError as e:
        return e.code, e.read()
    except urllib.error.URLError as e:
        raise RuntimeError(
            f"No se pudo conectar con {base_url} ({e.reason}). "
            "Activa 'Puente con PC' dentro de Conectar y archivos en la calculadora."
        ) from e


def fmt_size(n: int) -> str:
    if n < 1024:
        return f"{n} B"
    if n < 1024 * 1024:
        return f"{n / 1024:.1f} KB"
    return f"{n / (1024 * 1024):.2f} MB"


def normalize_remote(p: str) -> str:
    p = p.replace("\\", "/").strip()
    if not p.startswith("/"):
        p = "/" + p
    return p


def cmd_info(base_url: str, _args) -> int:
    code, raw = http_request(base_url, "GET", "/api/info")
    if code != 200:
        print(raw.decode("utf-8", errors="replace"), file=sys.stderr)
        return 1
    print(json.dumps(json.loads(raw), indent=2, ensure_ascii=False))
    return 0


def cmd_ls(base_url: str, args) -> int:
    remote = normalize_remote(args.path)
    code, raw = http_request(base_url, "GET", "/api/files?path=" + urllib.parse.quote(remote))
    if code != 200:
        print(raw.decode("utf-8", errors="replace"), file=sys.stderr)
        return 1
    data = json.loads(raw)
    print(f"SMEM {data['path']}  (Libre: {fmt_size(data['freeBytes'])} / Total: {fmt_size(data['totalBytes'])}, FAT OK: {data['fatEqual']})")
    for e in data.get("entries", []):
        kind = "<DIR> " if e["isDir"] else f"{fmt_size(e['size']):>9}"
        print(f"  {kind}  {e['name']}")
    return 0


def cmd_put(base_url: str, args) -> int:
    src = pathlib.Path(args.local)
    if not src.is_file():
        print(f"Archivo local no encontrado: {src}", file=sys.stderr)
        return 1
    remote = normalize_remote(args.remote) if args.remote else "/" + src.name
    if remote.endswith("/"):
        remote += src.name
    remount_flag = "0" if args.no_remount else "1"
    data = src.read_bytes()
    q = urllib.parse.urlencode({"path": remote, "overwrite": "1", "remount": remount_flag})
    code, raw = http_request(base_url, "POST", "/api/file?" + q, body=data)
    if code != 200:
        print(raw.decode("utf-8", errors="replace"), file=sys.stderr)
        return 1
    print(f"OK subido {src.name} ({fmt_size(len(data))}) -> {remote}")
    return 0


def cmd_get(base_url: str, args) -> int:
    remote = normalize_remote(args.remote)
    code, raw = http_request(base_url, "GET", "/api/file?path=" + urllib.parse.quote(remote))
    if code != 200:
        print(raw.decode("utf-8", errors="replace"), file=sys.stderr)
        return 1
    dest = pathlib.Path(args.local) if args.local else pathlib.Path(remote.rsplit("/", 1)[-1])
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_bytes(raw)
    print(f"OK descargado {remote} ({fmt_size(len(raw))}) -> {dest}")
    return 0


def cmd_rm(base_url: str, args) -> int:
    remote = normalize_remote(args.remote)
    q = urllib.parse.urlencode({
        "path": remote,
        "recursive": "1" if args.recursive else "0",
        "remount": "0" if args.no_remount else "1",
    })
    code, raw = http_request(base_url, "DELETE", "/api/file?" + q)
    if code != 200:
        print(raw.decode("utf-8", errors="replace"), file=sys.stderr)
        return 1
    print(f"OK eliminado {remote}")
    return 0


def cmd_mkdir(base_url: str, args) -> int:
    remote = normalize_remote(args.remote)
    code, raw = http_request(base_url, "POST", "/api/mkdir?path=" + urllib.parse.quote(remote))
    if code != 200:
        print(raw.decode("utf-8", errors="replace"), file=sys.stderr)
        return 1
    print(f"OK carpeta creada {remote}")
    return 0


def cmd_remount(base_url: str, _args) -> int:
    code, raw = http_request(base_url, "POST", "/api/remount")
    if code != 200:
        print(raw.decode("utf-8", errors="replace"), file=sys.stderr)
        return 1
    print("OK memoria recargada en Casio OS")
    return 0


def cmd_screen(base_url: str, args) -> int:
    code, raw = http_request(base_url, "GET", "/api/screen.png")
    if code != 200:
        print(f"Error capturando pantalla (HTTP {code})", file=sys.stderr)
        return 1
    dest = pathlib.Path(args.out)
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_bytes(raw)
    print(f"OK captura LCD guardada ({fmt_size(len(raw))}) -> {dest}")
    return 0


def cmd_qr(base_url: str, _args) -> int:
    code, raw = http_request(base_url, "GET", "/api/qr")
    if code != 200:
        print(raw.decode("utf-8", errors="replace"), file=sys.stderr)
        return 1
    qr = json.loads(raw).get("qr")
    if not qr:
        print("No se detecto ningun codigo QR en la pantalla actual.")
        return 0
    print(qr)
    return 0


def _sync_once(base_url: str, local_root: pathlib.Path, remote_root: str, known_mtimes: dict[str, tuple[int, int]]) -> int:
    uploaded = 0
    remote_prefix = "" if remote_root == "/" else remote_root.rstrip("/")
    for item in sorted(local_root.rglob("*")):
        rel = item.relative_to(local_root).as_posix()
        if any(part.startswith(".") for part in item.relative_to(local_root).parts):
            continue
        remote_path = f"{remote_prefix}/{rel}"
        if item.is_dir():
            if rel not in known_mtimes:
                http_request(base_url, "POST", "/api/mkdir?path=" + urllib.parse.quote(remote_path))
                known_mtimes[rel] = (-1, 0)
            continue
        st = item.stat()
        sig = (st.st_mtime_ns, st.st_size)
        if known_mtimes.get(rel) == sig:
            continue
        q = urllib.parse.urlencode({"path": remote_path, "overwrite": "1", "remount": "0"})
        code, raw = http_request(base_url, "POST", "/api/file?" + q, body=item.read_bytes())
        if code == 200:
            known_mtimes[rel] = sig
            uploaded += 1
            print(f"  [sync] -> {remote_path} ({fmt_size(st.st_size)})")
        else:
            print(f"  [sync] ERROR {remote_path}: {raw.decode('utf-8', errors='replace')}", file=sys.stderr)
    if uploaded > 0:
        http_request(base_url, "POST", "/api/remount")
    return uploaded


def cmd_sync(base_url: str, args) -> int:
    local_root = pathlib.Path(args.folder).resolve()
    local_root.mkdir(parents=True, exist_ok=True)
    remote_root = normalize_remote(args.remote)
    known: dict[str, tuple[int, int]] = {}
    n = _sync_once(base_url, local_root, remote_root, known)
    print(f"Sincronizacion inicial completada ({n} archivo(s) enviados a {remote_root}).")
    if not args.watch:
        return 0
    print(f"Observando cambios en {local_root} (Ctrl+C para detener)...")
    try:
        while True:
            time.sleep(1.0)
            _sync_once(base_url, local_root, remote_root, known)
    except KeyboardInterrupt:
        print("\nObservacion detenida.")
    return 0


def cmd_web(base_url: str, _args) -> int:
    print(f"Abriendo {base_url}/")
    webbrowser.open(base_url + "/")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Puente directo PC <-> Casio fx-CG50 (sin PIN ni contrasenas)")
    parser.add_argument("--host", default="127.0.0.1", help="IP del telefono (por defecto 127.0.0.1 con --adb)")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help="Puerto del puente (por defecto 8750)")
    parser.add_argument("--adb", action="store_true", help="Conectar por cable USB del telefono usando adb forward")
    parser.add_argument("--serial", default=None, help="Serial ADB opcional si hay varios dispositivos")

    sub = parser.add_subparsers(dest="cmd", required=True)
    sub.add_parser("info").set_defaults(fn=cmd_info)

    p_ls = sub.add_parser("ls")
    p_ls.add_argument("path", nargs="?", default="/")
    p_ls.set_defaults(fn=cmd_ls)

    p_put = sub.add_parser("put")
    p_put.add_argument("local")
    p_put.add_argument("remote", nargs="?", default=None)
    p_put.add_argument("--no-remount", action="store_true")
    p_put.set_defaults(fn=cmd_put)

    p_get = sub.add_parser("get")
    p_get.add_argument("remote")
    p_get.add_argument("local", nargs="?", default=None)
    p_get.set_defaults(fn=cmd_get)

    p_rm = sub.add_parser("rm")
    p_rm.add_argument("remote")
    p_rm.add_argument("--recursive", action="store_true")
    p_rm.add_argument("--no-remount", action="store_true")
    p_rm.set_defaults(fn=cmd_rm)

    p_mk = sub.add_parser("mkdir")
    p_mk.add_argument("remote")
    p_mk.set_defaults(fn=cmd_mkdir)

    sub.add_parser("remount").set_defaults(fn=cmd_remount)

    p_scr = sub.add_parser("screen")
    p_scr.add_argument("out", nargs="?", default="cg50_screen.png")
    p_scr.set_defaults(fn=cmd_screen)

    sub.add_parser("qr").set_defaults(fn=cmd_qr)

    p_sync = sub.add_parser("sync")
    p_sync.add_argument("folder")
    p_sync.add_argument("--remote", default="/")
    p_sync.add_argument("--watch", action="store_true")
    p_sync.set_defaults(fn=cmd_sync)

    sub.add_parser("web").set_defaults(fn=cmd_web)

    args = parser.parse_args()
    if args.adb:
        setup_adb_forward(args.port, args.serial)
        args.host = "127.0.0.1"

    base_url = f"http://{args.host}:{args.port}"
    try:
        return args.fn(base_url, args)
    except RuntimeError as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
