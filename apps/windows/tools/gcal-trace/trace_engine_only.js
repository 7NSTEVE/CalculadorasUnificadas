'use strict';

const hooked = new Set();

function textAt(pointer) {
    if (pointer.isNull()) return null;
    try {
        return pointer.readUtf8String(520);
    } catch (_) {
        return null;
    }
}

function emit(event, values) {
    console.log(JSON.stringify(Object.assign({ event }, values || {})));
}

function attach(moduleName, symbol, handlers) {
    const key = moduleName.toLowerCase() + '!' + symbol;
    if (hooked.has(key)) return;
    const module = Process.findModuleByName(moduleName);
    if (module === null) return;
    const address = module.findExportByName(symbol);
    if (address === null) return;
    try {
        Interceptor.attach(address, handlers);
        hooked.add(key);
        emit('hooked', { module: moduleName, symbol, address: address.toString() });
    } catch (error) {
        // Three-byte "return zero" patches are too short for a Frida trampoline.
        // Record that fact and continue installing the engine hooks.
        hooked.add(key);
        emit('hook_error', {
            module: moduleName,
            symbol,
            address: address.toString(),
            error: String(error)
        });
    }
}

function pointerArguments(args, count) {
    const values = [];
    for (let i = 0; i < count; ++i) values.push(args[i].toString());
    return values;
}

function install() {
    for (const name of [
        'AcvFx_GetInfomation',
        'AcvFx_GetInfomation2',
        'AcvFx_IsActivationDialog',
        'AcvFx_IsActivationDialog2',
        'AcvFx_IsLicenseStatus'
    ]) {
        attach('ActivationFxMb.dll', name, {
            onEnter(args) { this.args = pointerArguments(args, 2); },
            onLeave(retval) { emit(name + '.leave', { args: this.args, result: retval.toInt32() }); }
        });
    }

    attach('GCalAppIF.dll', '_GCAI_CreateEmulator@28', {
        onEnter(args) {
            this.output = args[4];
            emit('GCAI_CreateEmulator.enter', {
                args: pointerArguments(args, 7),
                arg0Text: textAt(args[0]),
                arg1Text: textAt(args[1])
            });
        },
        onLeave(retval) {
            let handle = null;
            try { handle = this.output.readPointer().toString(); } catch (_) {}
            emit('GCAI_CreateEmulator.leave', { result: retval.toInt32(), handle });
        }
    });

    attach('GCalAppIF.dll', '_GCAI_RequestTask@20', {
        onEnter(args) { emit('GCAI_RequestTask', { args: pointerArguments(args, 5) }); }
    });

    attach('CASIO GCSim.dll', '_GCSim_Init@16', {
        onEnter(args) {
            emit('GCSim_Init.enter', {
                args: pointerArguments(args, 4),
                arg0Text: textAt(args[0]),
                arg1Text: textAt(args[1]),
                arg2Text: textAt(args[2])
            });
        },
        onLeave(retval) { emit('GCSim_Init.leave', { result: retval.toInt32() }); }
    });

    attach('DLSim32E.dll', '_DLSIM_GCAIKeyPress@8', {
        onEnter(args) { emit('DLSIM_GCAIKeyPress', { args: pointerArguments(args, 2) }); }
    });
    attach('DLSim32E.dll', '_DLSIM_GCAIUpdateDisplay@4', {
        onEnter(args) { emit('DLSIM_GCAIUpdateDisplay', { args: pointerArguments(args, 1) }); }
    });
}

emit('trace_loaded', { pid: Process.id, arch: Process.arch });
install();
setInterval(install, 100);
