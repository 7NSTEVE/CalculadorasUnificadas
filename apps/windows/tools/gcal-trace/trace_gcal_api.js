'use strict';

const hooked = new Set();
let callbackHooked = false;
let callbackCount = 0;

function safeUtf8(pointer) {
    if (pointer.isNull()) return null;
    try {
        const value = pointer.readUtf8String(520);
        if (value === null) return null;
        // Escape control characters instead of reading beyond the first NUL.
        // Registry paths sometimes come from packed MFC string tables.
        return value.replace(/[\x00-\x1f\x7f]/g, ch =>
            '\\x' + ch.charCodeAt(0).toString(16).padStart(2, '0'));
    } catch (_) {
        return null;
    }
}

function safeUtf16(pointer) {
    if (pointer.isNull()) return null;
    try {
        const value = pointer.readUtf16String(520);
        if (value === null) return null;
        for (let i = 0; i < value.length; ++i) {
            const code = value.charCodeAt(i);
            if (code < 0x20 && code !== 0x09) return null;
        }
        return value;
    } catch (_) {
        return null;
    }
}

function ptrs(args, count) {
    const result = [];
    for (let i = 0; i < count; ++i) result.push(args[i].toString());
    return result;
}

function emit(event, fields) {
    console.log(JSON.stringify(Object.assign({ event }, fields || {})));
}

function moduleFor(address) {
    const module = Process.findModuleByAddress(address);
    if (module === null) return null;
    return module.name + '+0x' + address.sub(module.base).toString(16);
}

function attachReturnProbe(moduleName, exportName, argumentCount) {
    attachOnce(moduleName, exportName, {
        onEnter(args) {
            this.arguments = ptrs(args, argumentCount);
            this.caller = moduleFor(this.returnAddress);
        },
        onLeave(retval) {
            emit(exportName + '.leave', {
                args: this.arguments,
                caller: this.caller,
                result: retval.toInt32()
            });
        }
    });
}

function attachOnce(moduleName, exportName, handlers) {
    const key = moduleName.toLowerCase() + '!' + exportName;
    if (hooked.has(key)) return;

    const module = Process.findModuleByName(moduleName);
    if (module === null) return;
    const address = module.findExportByName(exportName);
    if (address === null) return;

    Interceptor.attach(address, handlers);
    hooked.add(key);
    emit('hooked', { module: moduleName, symbol: exportName, address: address.toString() });
}

function installHooks() {
    attachOnce('ADVAPI32.dll', 'RegOpenKeyExW', {
        onEnter(args) {
            this.root = args[0].toString();
            this.name = safeUtf16(args[1]);
        },
        onLeave(retval) {
            emit('RegOpenKeyExW', { root: this.root, name: this.name, result: retval.toInt32() });
        }
    });

    attachOnce('ADVAPI32.dll', 'RegOpenKeyExA', {
        onEnter(args) {
            this.root = args[0].toString();
            this.namePointer = args[1].toString();
            this.name = safeUtf8(args[1]);
            this.nameAsUtf16 = safeUtf16(args[1]);
            this.caller = this.returnAddress.toString();
            this.callerSymbol = moduleFor(this.returnAddress);
        },
        onLeave(retval) {
            emit('RegOpenKeyExA', {
                root: this.root,
                namePointer: this.namePointer,
                name: this.name,
                nameAsUtf16: this.nameAsUtf16,
                caller: this.caller,
                callerSymbol: this.callerSymbol,
                result: retval.toInt32()
            });
        }
    });

    attachOnce('ADVAPI32.dll', 'RegQueryValueExW', {
        onEnter(args) {
            this.name = safeUtf16(args[1]);
        },
        onLeave(retval) {
            emit('RegQueryValueExW', { name: this.name, result: retval.toInt32() });
        }
    });

    attachOnce('ADVAPI32.dll', 'RegQueryValueExA', {
        onEnter(args) {
            this.name = safeUtf8(args[1]);
        },
        onLeave(retval) {
            emit('RegQueryValueExA', { name: this.name, result: retval.toInt32() });
        }
    });

// attachReturnProbe('ActivationFxMb.dll', 'AcvFx_GetInfomation', 1);
// attachReturnProbe('ActivationFxMb.dll', 'AcvFx_GetInfomation2', 1);
// attachReturnProbe('ActivationFxMb.dll', 'AcvFx_IsActivationDialog', 1);
// attachReturnProbe('ActivationFxMb.dll', 'AcvFx_IsActivationDialog2', 1);
// attachReturnProbe('ActivationFxMb.dll', 'AcvFx_IsLicenseStatus', 1);

    attachOnce('USER32.dll', 'MessageBoxW', {
        onEnter(args) {
            emit('MessageBoxW', {
                caller: moduleFor(this.returnAddress),
                text: safeUtf16(args[1]),
                title: safeUtf16(args[2]),
                flags: args[3].toString()
            });
        }
    });

    attachOnce('USER32.dll', 'MessageBoxA', {
        onEnter(args) {
            emit('MessageBoxA', {
                caller: moduleFor(this.returnAddress),
                text: safeUtf8(args[1]),
                title: safeUtf8(args[2]),
                flags: args[3].toString()
            });
        }
    });

    attachOnce('GCalAppIF.dll', '_GCAI_CreateEmulator@28', {
        onEnter(args) {
            this.outputHandle = args[4];
            const arg0Text = safeUtf8(args[0]);
            const arg1Text = safeUtf8(args[1]);
            emit('GCAI_CreateEmulator.enter', {
                args: ptrs(args, 7),
                arg0Text,
                arg1Text
            });

            if (!callbackHooked && !args[2].isNull()) {
                callbackHooked = true;
                Interceptor.attach(args[2], {
                    onEnter(callbackArgs) {
                        if (callbackCount++ >= 100) return;
                        emit('GCAI.callback', { args: ptrs(callbackArgs, 6) });
                    }
                });
                emit('callback_hooked', { address: args[2].toString() });
            }
        },
        onLeave(retval) {
            let outputHandle = null;
            try {
                if (!this.outputHandle.isNull()) {
                    outputHandle = this.outputHandle.readPointer().toString();
                }
            } catch (_) {}
            emit('GCAI_CreateEmulator.leave', {
                result: retval.toInt32(),
                outputHandle
            });
        }
    });

    attachOnce('GCalAppIF.dll', '_GCAI_RequestTask@20', {
        onEnter(args) {
            emit('GCAI_RequestTask', { args: ptrs(args, 5) });
        }
    });

    attachOnce('CASIO GCSim.dll', '_GCSim_Init@16', {
        onEnter(args) {
            emit('GCSim_Init.enter', {
                args: ptrs(args, 4),
                arg0Text: safeUtf8(args[0]),
                arg1Text: safeUtf8(args[1]),
                arg2Text: safeUtf8(args[2])
            });
        },
        onLeave(retval) {
            emit('GCSim_Init.leave', { result: retval.toInt32() });
        }
    });

    attachOnce('DLSim32E.dll', '_DLSIM_GCAIKeyPress@8', {
        onEnter(args) {
            emit('DLSIM_GCAIKeyPress', { args: ptrs(args, 2) });
        }
    });

    attachOnce('DLSim32E.dll', '_DLSIM_GCAIUpdateDisplay@4', {
        onEnter(args) {
            emit('DLSIM_GCAIUpdateDisplay', { args: ptrs(args, 1) });
        }
    });
}

emit('trace_loaded', { pid: Process.id, arch: Process.arch });
installHooks();
setInterval(installHooks, 100);
