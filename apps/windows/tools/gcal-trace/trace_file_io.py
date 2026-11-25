"""Trace ALL file access APIs of GCalHostProbe to find how .mem files are loaded."""
import frida
import sys
import json
import time

SCRIPT = r"""
'use strict';

var counter = 0;

function interesting(p) {
    if (!p) return false;
    p = p.toLowerCase();
    return p.indexOf('.mem') !== -1 || p.indexOf('.dlm') !== -1 || p.indexOf('.dlp') !== -1 ||
           p.indexOf('casio') !== -1 || p.indexOf('emulator') !== -1 ||
           p.indexOf('rom') !== -1 || p.indexOf('ram') !== -1 || p.indexOf('.ini') !== -1 ||
           p.indexOf('model') !== -1;
}

// Hook ALL variants of file open APIs
var apis = [
    ['kernel32.dll', 'CreateFileW', 'utf16', 0],
    ['kernel32.dll', 'CreateFileA', 'utf8', 0],
    ['kernel32.dll', 'CreateFileMappingW', 'utf16', 5],
    ['kernel32.dll', 'CreateFileMappingA', 'utf8', 5],
];

apis.forEach(function(api) {
    var mod = api[0], fn = api[1], enc = api[2], nameIdx = api[3];
    var addr = Module.findExportByName(mod, fn);
    if (!addr) return;
    Interceptor.attach(addr, {
        onEnter: function(args) {
            try {
                if (enc === 'utf16') this.path = args[nameIdx].readUtf16String();
                else this.path = args[nameIdx].readUtf8String();
            } catch(e) { this.path = null; }
        },
        onLeave: function(retval) {
            var h = retval.toUInt32();
            var ok = (h !== 0 && h !== 0xFFFFFFFF);
            if (this.path && (interesting(this.path) || counter < 5)) {
                counter++;
                send({type: fn, path: this.path, ok: ok});
            }
        }
    });
    send({type:'HOOK', fn: fn});
});

// Also hook CRT fopen
var msvcrt_names = ['ucrtbase.dll', 'msvcrt.dll', 'msvcr100.dll', 'msvcr120.dll'];
msvcrt_names.forEach(function(dll) {
    var fopenAddr = Module.findExportByName(dll, 'fopen');
    if (fopenAddr) {
        Interceptor.attach(fopenAddr, {
            onEnter: function(args) {
                try { this.path = args[0].readUtf8String(); } catch(e) { this.path = null; }
                try { this.mode = args[1].readUtf8String(); } catch(e) { this.mode = '?'; }
            },
            onLeave: function(retval) {
                if (this.path) {
                    send({type:'fopen', dll: dll, path: this.path, mode: this.mode, ok: !retval.isNull()});
                }
            }
        });
        send({type:'HOOK', fn: 'fopen@' + dll});
    }
    var wfopenAddr = Module.findExportByName(dll, '_wfopen');
    if (wfopenAddr) {
        Interceptor.attach(wfopenAddr, {
            onEnter: function(args) {
                try { this.path = args[0].readUtf16String(); } catch(e) { this.path = null; }
                try { this.mode = args[1].readUtf16String(); } catch(e) { this.mode = '?'; }
            },
            onLeave: function(retval) {
                if (this.path) {
                    send({type:'_wfopen', dll: dll, path: this.path, mode: this.mode, ok: !retval.isNull()});
                }
            }
        });
        send({type:'HOOK', fn: '_wfopen@' + dll});
    }
});

send({type:'READY', pid: Process.id});
"""

def main():
    runtime = r"D:\OneDrive\Desktop\Calculadoras (blanca y gris)\Calculadora Unificada\build\Release\runtime"
    exe = r"D:\OneDrive\Desktop\Calculadoras (blanca y gris)\Calculadora Unificada\build\Release\GCalHostProbe.exe"
    
    pid = frida.spawn([exe, runtime, "3000", "0", "sw0a"])
    print(f"Spawned PID={pid}", flush=True)
    
    session = frida.attach(pid)
    script = session.create_script(SCRIPT)
    
    results = []
    def on_message(message, data):
        if message['type'] == 'send':
            p = message['payload']
            results.append(p)
            tp = p.get('type','')
            if tp == 'HOOK':
                print(f"  [HOOK] {p['fn']}", flush=True)
            elif tp == 'READY':
                print(f"  Hooks ready, resuming PID {p['pid']}...", flush=True)
            elif tp in ('CreateFileW','CreateFileA','CreateFileMappingW','CreateFileMappingA'):
                print(f"  [{tp}] ok={p['ok']} {p['path']}", flush=True)
            elif tp in ('fopen','_wfopen'):
                print(f"  [{tp}@{p.get('dll','')}] ok={p['ok']} mode={p.get('mode','?')} {p['path']}", flush=True)
            else:
                print(f"  [{tp}] {json.dumps(p)}", flush=True)
        elif message['type'] == 'error':
            print(f"  [ERROR] {message.get('description','')}", flush=True)
    
    script.on('message', on_message)
    script.load()
    frida.resume(pid)
    
    time.sleep(12)
    
    try: session.detach()
    except: pass
    
    print("\n=== SUMMARY ===", flush=True)
    file_events = [r for r in results if r.get('type') not in ('HOOK','READY')]
    print(f"Total file events: {len(file_events)}", flush=True)
    for f in file_events:
        print(f"  {f.get('type','')} ok={f.get('ok','')} {f.get('path','')}", flush=True)

if __name__ == '__main__':
    main()
