"""Trace SH-4A CPU execution from reset vector."""
import frida, sys, json, time

SCRIPT = r"""
'use strict';

var cpuBase = null;

function hookCPU() {
    var mod = Process.findModuleByName('CPU73050.dll');
    if (!mod) return false;
    cpuBase = mod.base;
    send({type: 'CPU_FOUND', base: cpuBase.toString()});

    var pcPtr = cpuBase.add(0x675A18);
    var execFlag = cpuBase.add(0x675430);
    
    var count = 0;
    var timer = setInterval(function() {
        count++;
        try {
            var pc = pcPtr.readU32();
            var flag = execFlag.readU32();
            send({type: 'CPU_POLL', tick: count, pc: '0x' + pc.toString(16), flag: '0x' + flag.toString(16)});
        } catch(e) {}
        if (count >= 40) clearInterval(timer);
    }, 50);

    return true;
}

var interval = setInterval(function() {
    if (hookCPU()) clearInterval(interval);
}, 20);

send({type: 'READY'});
"""

exe = r'D:\OneDrive\Desktop\Calculadoras (blanca y gris)\Calculadora Unificada\build\Release\CalculadoraUnificada.exe'
pid = frida.spawn([exe], cwd=r'D:\OneDrive\Desktop\Calculadoras (blanca y gris)\Calculadora Unificada\build\Release')
session = frida.attach(pid)
script = session.create_script(SCRIPT)

def on_message(message, data):
    if message['type'] == 'send':
        p = message['payload']
        print(json.dumps(p), flush=True)

script.on('message', on_message)
script.load()
frida.resume(pid)

time.sleep(4)
try: session.detach()
except: pass
