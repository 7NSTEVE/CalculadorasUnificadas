'use strict';
// f14_trace_script.js — Traza integral del manager oficial CG50 (F1.4 + F2.4).
// Hooks:
//  - Todas las exports _GCAI_* de GCalAppIF.dll (args crudos + retval + payload dwords)
//  - El callback de CreateEmulator y el despachador 0x40D4D0 + slot2 0x417BB0 del exe
//    (peticiones OS->host, con payload)
//  - DLSim32E_orig.dll: rutas de carga/guardado de memoria del modelo (buffers preview)
//  - CreateFileW/ReadFile de contenedores .mem (correlación de I/O)
// Regla: no se interpreta ninguna firma desconocida; se capturan valores crudos.
const hooked = new Set();
const counters = {};
const MAX_PER_SYMBOL = 4000;

function emit(event, fields, data) {
    send(Object.assign({ event: event }, fields || {}), data || null);
}

function bump(symbol) {
    counters[symbol] = (counters[symbol] || 0) + 1;
    return counters[symbol] > MAX_PER_SYMBOL;
}

function hexPreview(ptr, len) {
    try {
        const bytes = ptr.readByteArray(len);
        const arr = new Uint8Array(bytes);
        let s = '';
        for (let i = 0; i < arr.length; ++i) {
            s += ('0' + arr[i].toString(16)).slice(-2);
        }
        return s;
    } catch (_) {
        return null;
    }
}

function dwords(ptr, count) {
    try {
        const out = [];
        for (let i = 0; i < count; ++i) out.push(ptr.add(i * 4).readU32());
        return out;
    } catch (_) {
        return null;
    }
}

function ptrs(args, count) {
    const out = [];
    for (let i = 0; i < count; ++i) out.push(args[i].toString());
    return out;
}

function safeUtf8(pointer) {
    if (pointer.isNull()) return null;
    try { return pointer.readUtf8String(256); } catch (_) { return null; }
}

function safeUtf16(pointer) {
    if (pointer.isNull()) return null;
    try { return pointer.readUtf16String(256); } catch (_) { return null; }
}

function moduleFor(address) {
    const module = Process.findModuleByAddress(address);
    if (module === null) return address.toString();
    return module.name + '+0x' + address.sub(module.base).toString(16);
}

function attachOnce(moduleName, exportName, handlers) {
    const key = moduleName.toLowerCase() + '!' + exportName;
    if (hooked.has(key)) return false;
    const module = Process.findModuleByName(moduleName);
    if (module === null) return false;
    const address = module.findExportByName(exportName);
    if (address === null) return false;
    try {
        Interceptor.attach(address, handlers);
    } catch (e) {
        return false;
    }
    hooked.add(key);
    emit('hooked', { module: moduleName, symbol: exportName, address: address.toString() });
    return true;
}

function hookGcai() {
    const module = Process.findModuleByName('GCalAppIF.dll');
    if (module === null) return;
    const exports = module.enumerateExports();
    for (const exp of exports) {
        if (exp.name.indexOf('_GCAI_') === -1) continue;
        const name = exp.name;
        const m = name.match(/@(\d+)$/);
        const argCount = m ? parseInt(m[1], 10) / 4 : 2;
        attachOnce('GCalAppIF.dll', name, {
            onEnter(args) {
                if (bump(name)) return;
                const rec = { args: ptrs(args, Math.min(argCount, 7)) };
                // Payloads: RequestResidentTask/RequestTask/CreateResidentTask
                // llevan un puntero a buffer de dwords en args[1] (resident) o
                // args[1]/args[3] (task). Capturar crudo donde sea legible.
                if (name.indexOf('Request') !== -1 || name.indexOf('Create') !== -1) {
                    for (let a = 0; a < Math.min(argCount, 5); ++a) {
                        const dw = dwords(args[a], 12);
                        if (dw !== null) rec['d' + a] = dw;
                    }
                }
                if (name.indexOf('SetSDCardPath') !== -1) {
                    rec.text = safeUtf8(args[0]);
                }
                this.rec = rec;
            },
            onLeave(retval) {
                if (this.rec === undefined) return;
                this.rec.result = retval.toInt32();
                emit('GCAI.' + name, this.rec);
            }
        });
    }
}

function hookExeDispatchers() {
    const exe = Process.enumerateModules().filter(m =>
        m.name.indexOf('fx-CG_Manager_PLUS') === 0)[0];
    if (exe === null) return;
    const dispatch = exe.base.add(0xD4D0);     // despachador de tareas CGCAI
    const slot2 = exe.base.add(0x417BB0);      // callback UI del slot 2
    try {
        Interceptor.attach(dispatch, {
            onEnter(args) {
                if (bump('dispatch_0x40d4d0')) return;
                emit('EXE.dispatch_0x40d4d0', {
                    a0: args[0].toString(),
                    a1: args[1].toString(),
                    obj: args[2].toString(),
                    obj_dwords: dwords(args[2], 8),
                    caller: moduleFor(this.returnAddress)
                });
            }
        });
        emit('hooked', { module: exe.name, symbol: 'dispatch_0x40d4d0', address: dispatch.toString() });
        Interceptor.attach(slot2, {
            onEnter(args) {
                if (bump('slot2_0x417bb0')) return;
                emit('EXE.slot2_0x417bb0', {
                    a0: args[0].toString(),
                    a1: args[1].toString(),
                    a2: args[2].toString(),
                    caller: moduleFor(this.returnAddress)
                });
            }
        });
        emit('hooked', { module: exe.name, symbol: 'slot2_0x417bb0', address: slot2.toString() });
    } catch (e) {
        emit('hook_error', { where: 'exe_dispatchers', error: e.toString() });
    }
}

function hookDlsim() {
    // Preferir DLSim32E_orig.dll (codigo real); si no existe, DLSim32E.dll.
    let moduleName = null;
    if (Process.findModuleByName('DLSim32E_orig.dll') !== null) moduleName = 'DLSim32E_orig.dll';
    else if (Process.findModuleByName('DLSim32E.dll') !== null) moduleName = 'DLSim32E.dll';
    if (moduleName === null) return;
    const module = Process.findModuleByName(moduleName);
    const wanted = [
        '_DLSIM_ModelLoadMemory@4', '_DLSIM_ModelSaveMemory@4',
        '_DLSIM_BinaryMemoryLoad@8', '_DLSIM_BinaryMemorySave@8',
        '_DLSIM_ModelLoad@4', '_DLSIM_ModelSave@4',
        '_DLSIM_ModelNew@0', '_DLSIM_ModelSetup@0', '_DLSIM_ModelUpdate@0',
        '_DLSIM_ImageLoad@12', '_DLSIM_ImageLoadEx@16',
        '_DLSIM_ProjectLoadProgramFile@12',
        '_DLSIM_CPUExecute@4', '_DLSIM_CPUInitMode@4',
        '_DLSIM_GCAIKeyPress@8', '_DLSIM_GCAIUpdateDisplay@4',
        '_DLSIM_GCAIGetRequest@8', '_DLSIM_GCAISendNotify@8'
    ];
    for (const name of wanted) {
        attachOnce(moduleName, name, {
            onEnter(args) {
                if (bump('DLSIM.' + name)) return;
                const rec = { args: ptrs(args, 4) };
                for (let a = 0; a < 3; ++a) {
                    const preview = hexPreview(args[a], 96);
                    if (preview !== null) rec['p' + a] = preview;
                }
                this.rec = rec;
                this.args = args;
            },
            onLeave(retval) {
                if (this.rec === undefined) return;
                this.rec.result = retval.toInt32();
                // Previews after: la carga materializa buffers visibles en onLeave.
                for (let a = 0; a < 3; ++a) {
                    const preview = hexPreview(this.args[a], 96);
                    if (preview !== null) this.rec['q' + a] = preview;
                }
                emit('DLSIM.' + name, this.rec);
            }
        });
    }
}

function hookFileIo() {
    attachOnce('kernel32.dll', 'CreateFileW', {
        onEnter(args) {
            const name = safeUtf16(args[0]);
            if (name === null) return;
            if (name.indexOf('.mem') === -1 && name.indexOf('.dlp') === -1 &&
                name.indexOf('.dlm') === -1 && name.indexOf('EmulatorData') === -1) {
                return;
            }
            emit('IO.CreateFileW', { name: name, caller: moduleFor(this.returnAddress) });
        }
    });
    attachOnce('kernel32.dll', 'CreateFileA', {
        onEnter(args) {
            const name = safeUtf8(args[0]);
            if (name === null) return;
            if (name.indexOf('.mem') === -1 && name.indexOf('.dlp') === -1 &&
                name.indexOf('.dlm') === -1) {
                return;
            }
            emit('IO.CreateFileA', { name: name, caller: moduleFor(this.returnAddress) });
        }
    });
}

let callbackHooked = false;
function hookCreateEmulatorCallback(callbackAddress) {
    if (callbackHooked) return;
    callbackHooked = true;
    try {
        Interceptor.attach(callbackAddress, {
            onEnter(args) {
                if (bump('gcai_callback')) return;
                emit('GCAI.callback', {
                    code: args[0].toInt32(),
                    payload: args[1].toString(),
                    payload_dwords: dwords(args[1], 10),
                    context: args[2].toString(),
                    caller: moduleFor(this.returnAddress)
                });
            }
        });
        emit('hooked', { symbol: 'gcai_callback', address: callbackAddress.toString() });
    } catch (e) {
        emit('hook_error', { where: 'gcai_callback', error: e.toString() });
    }
}

function installHooks() {
    hookGcai();
    hookExeDispatchers();
    hookDlsim();
    hookFileIo();
    const gcai = Process.findModuleByName('GCalAppIF.dll');
    if (gcai !== null) {
        const create = gcai.findExportByName('_GCAI_CreateEmulator@28');
        if (create !== null && !hooked.has('gcai!create_cb')) {
            hooked.add('gcai!create_cb');
            Interceptor.attach(create, {
                onEnter(args) {
                    if (!args[2].isNull()) hookCreateEmulatorCallback(args[2]);
                }
            });
        }
    }
}

emit('trace_loaded', { pid: Process.id, arch: Process.arch });
installHooks();
setInterval(installHooks, 200);
