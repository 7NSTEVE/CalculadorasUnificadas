"""Trace DLSim32E path construction and file opening in detail."""
import frida
import sys
import json
import time

SCRIPT = r"""
'use strict';

var dlsimBase = null;

function hookDLSim() {
    var mod = Process.findModuleByName('DLSim32E.dll');
    if (!mod) return false;
    dlsimBase = mod.base;
    send({type: 'DLSIM_FOUND', base: dlsimBase.toString()});

    // Hook Path Builder at RVA 0x1A842
    var pathBuilder = dlsimBase.add(0x1A842);
    Interceptor.attach(pathBuilder, {
        onEnter: function(args) {
            try { this.arg0 = args[0].readUtf8String(); } catch(e) { this.arg0 = args[0].toString(); }
            try { this.arg1 = args[1].readUtf8String(); } catch(e) { this.arg1 = args[1].toString(); }
            try { this.arg2 = args[2].readUtf8String(); } catch(e) { this.arg2 = args[2].toString(); }
        },
        onLeave: function(retval) {
            var res = null;
            try { res = retval.readUtf8String(); } catch(e) { res = retval.toString(); }
            send({
                type: 'PATH_BUILDER',
                baseDir1: this.arg0,
                baseDir2: this.arg1,
                relPath: this.arg2,
                result: res
            });
        }
    });

    // Hook File Open at RVA 0x0FB3C
    var fileOpen = dlsimBase.add(0x0FB3C);
    Interceptor.attach(fileOpen, {
        onEnter: function(args) {
            try { this.path = args[0].readUtf8String(); } catch(e) { this.path = args[0].toString(); }
            this.mode = args[1].toUInt32();
        },
        onLeave: function(retval) {
            var handle = retval.toUInt32();
            send({
                type: 'DLSIM_FILE_OPEN',
                path: this.path,
                mode: '0x' + this.mode.toString(16),
                handle: handle ? '0x' + handle.toString(16) : 'NULL'
            });
        }
    });

    // Hook CheckMemHeader at RVA 0x16822
    var checkHeader = dlsimBase.add(0x16822);
    Interceptor.attach(checkHeader, {
        onEnter: function(args) {
            this.handle = args[0].toUInt32();
        },
        onLeave: function(retval) {
            send({
                type: 'CHECK_MEM_HEADER',
                handle: '0x' + this.handle.toString(16),
                format: retval.toInt32()
            });
        }
    });

    // Hook LoadMemFile at RVA 0x1688B
    var loadMem = dlsimBase.add(0x1688B);
    Interceptor.attach(loadMem, {
        onEnter: function(args) {
            this.handle = args[0].toUInt32();
        },
        onLeave: function(retval) {
            send({
                type: 'LOAD_MEM_FILE',
                handle: '0x' + this.handle.toString(16),
                result: retval.toInt32()
            });
        }
    });

    // Hook the Device Memory Loop at RVA 0x16D9E
    var memLoop = dlsimBase.add(0x16D9E);
    Interceptor.attach(memLoop, {
        onEnter: function(args) {
            send({type: 'ENTER_MEM_LOOP', arg0: args[0].toString()});
        },
        onLeave: function(retval) {
            send({type: 'LEAVE_MEM_LOOP', result: retval.toInt32()});
        }
    });

    return true;
}

var interval = setInterval(function() {
    if (hookDLSim()) {
        clearInterval(interval);
    }
}, 20);

send({type: 'READY', pid: Process.id});
"""

def main():
    runtime = r"D:\OneDrive\Desktop\Calculadoras (blanca y gris)\Calculadora Unificada\build\Release\runtime"
    exe = r"D:\OneDrive\Desktop\Calculadoras (blanca y gris)\Calculadora Unificada\build\Release\GCalHostProbe.exe"
    
    pid = frida.spawn([exe, runtime, "3000", "0", "sw0a"])
    session = frida.attach(pid)
    script = session.create_script(SCRIPT)
    
    def on_message(message, data):
        if message['type'] == 'send':
            p = message['payload']
            tp = p.get('type', '')
            if tp == 'PATH_BUILDER':
                print(f"[PATH_BUILDER] base1={p['baseDir1']} base2={p['baseDir2']} rel={p['relPath']} -> {p['result']}", flush=True)
            elif tp == 'DLSIM_FILE_OPEN':
                print(f"[FILE_OPEN] handle={p['handle']} mode={p['mode']} path={p['path']}", flush=True)
            elif tp == 'CHECK_MEM_HEADER':
                print(f"[CHECK_HEADER] handle={p['handle']} format={p['format']}", flush=True)
            elif tp == 'LOAD_MEM_FILE':
                print(f"[LOAD_MEM] handle={p['handle']} result={p['result']}", flush=True)
            elif tp in ('ENTER_MEM_LOOP', 'LEAVE_MEM_LOOP'):
                print(f"[{tp}] {json.dumps(p)}", flush=True)
            else:
                print(f"[{tp}] {json.dumps(p)}", flush=True)
        elif message['type'] == 'error':
            print(f"[ERROR] {message.get('description', '')}", flush=True)
            
    script.on('message', on_message)
    script.load()
    frida.resume(pid)
    
    time.sleep(10)
    try: session.detach()
    except: pass

if __name__ == '__main__':
    main()
