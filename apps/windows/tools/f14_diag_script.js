'use strict';
// f14_diag_script.js — Diagnostico del arranque del manager oficial:
// por que abre sin ventana y sale limpio a los ~30s. Hooks: registro,
// WINHTTP, ActivationFxMb.dll completo, ExitProcess, CreateWindowEx.
const hooked = new Set();

function emit(event, fields) {
    send(Object.assign({ event: event }, fields || {}));
}

function safeUtf8(pointer) {
    if (pointer.isNull()) return null;
    try { return pointer.readUtf8String(256); } catch (_) { return null; }
}

function rawHex(pointer, len) {
    try {
        const arr = new Uint8Array(pointer.readByteArray(len));
        let s = '';
        for (let i = 0; i < arr.length; ++i) {
            s += ('0' + arr[i].toString(16)).slice(-2);
        }
        return s;
    } catch (_) { return null; }
}

function safeUtf16(pointer) {
    if (pointer.isNull()) return null;
    try { return pointer.readUtf16String(256); } catch (_) { return null; }
}

function ptrs(args, count) {
    const out = [];
    for (let i = 0; i < count; ++i) out.push(args[i].toString());
    return out;
}

function moduleFor(address) {
    const module = Process.findModuleByAddress(address);
    if (module === null) return address.toString();
    return module.name + '+0x' + address.sub(module.base).toString(16);
}

function attachOnce(moduleName, exportName, handlers) {
    const key = moduleName.toLowerCase() + '!' + exportName;
    if (hooked.has(key)) return;
    const module = Process.findModuleByName(moduleName);
    if (module === null) return;
    const address = module.findExportByName(exportName);
    if (address === null) return;
    try {
        Interceptor.attach(address, handlers);
    } catch (e) {
        emit('hook_error', { module: moduleName, symbol: exportName, error: e.toString() });
        return;
    }
    hooked.add(key);
    emit('hooked', { module: moduleName, symbol: exportName });
}

function installHooks() {
    attachOnce('ADVAPI32.dll', 'RegOpenKeyExA', {
        onEnter(args) {
            this.name = safeUtf8(args[1]);
            this.raw = rawHex(args[1], 96);
        },
        onLeave(retval) {
            emit('reg', { op: 'OpenKeyExA', name: this.name, raw: this.raw, result: retval.toInt32() });
        }
    });
    attachOnce('ADVAPI32.dll', 'RegOpenKeyExW', {
        onEnter(args) {
            this.name = safeUtf16(args[1]);
            this.raw = rawHex(args[1], 128);
        },
        onLeave(retval) {
            emit('reg', { op: 'OpenKeyExW', name: this.name, raw: this.raw, result: retval.toInt32() });
        }
    });
    attachOnce('ADVAPI32.dll', 'RegQueryValueExA', {
        onEnter(args) { this.name = safeUtf8(args[1]); },
        onLeave(retval) { emit('reg', { op: 'QueryValueExA', name: this.name, result: retval.toInt32() }); }
    });
    attachOnce('USER32.dll', 'LoadStringW', {
        onEnter(args) {
            this.id = args[1].toUInt32();
            this.buffer = args[2];
        },
        onLeave(retval) {
            emit('loadstring', {
                id: this.id, result: retval.toInt32(),
                text: retval.toInt32() > 0 ? safeUtf16(this.buffer) : null
            });
        }
    });
    attachOnce('USER32.dll', 'LoadStringA', {
        onEnter(args) {
            this.id = args[1].toUInt32();
            this.buffer = args[2];
        },
        onLeave(retval) {
            emit('loadstring', {
                id: this.id, result: retval.toInt32(),
                text: retval.toInt32() > 0 ? safeUtf8(this.buffer) : null
            });
        }
    });
    attachOnce('ntdll.dll', 'RtlExitUserProcess', {
        onEnter(args) {
            emit('EXIT_PROCESS', { code: args[0].toInt32(), via: 'RtlExitUserProcess', caller: moduleFor(this.returnAddress) });
        }
    });
    attachOnce('USER32.dll', 'MessageBoxW', {
        onEnter(args) {
            const frames = [];
            try {
                const bt = Thread.backtrace(this.context, Backtracer.ACCURATE);
                for (const a of bt.slice(0, 12)) {
                    frames.push(moduleFor(a));
                }
            } catch (e) {
                frames.push('bt_error:' + e.toString());
            }
            // Volcado crudo de pila: resolucion manual, tolerante a fallos.
            const esp = this.context.esp;
            const stackWords = [];
            for (let i = 0; i < 48; ++i) {
                try {
                    const v = esp.add(i * 4).readPointer();
                    stackWords.push(moduleFor(v));
                } catch (_) {
                    stackWords.push('?');
                }
            }
            emit('msgbox', {
                caller: moduleFor(this.returnAddress),
                stack: frames,
                rawstack: stackWords,
                text: safeUtf16(args[1]),
                rawText: rawHex(args[1], 160),
                title: safeUtf16(args[2]),
                flags: args[3].toInt32()
            });
        }
    });
    attachOnce('USER32.dll', 'MessageBoxA', {
        onEnter(args) {
            const frames = [];
            try {
                const bt = Thread.backtrace(this.context, Backtracer.ACCURATE);
                for (const a of bt.slice(0, 12)) {
                    frames.push(moduleFor(a));
                }
            } catch (e) {
                frames.push('bt_error:' + e.toString());
            }
            const esp = this.context.esp;
            const stackWords = [];
            for (let i = 0; i < 48; ++i) {
                try {
                    const v = esp.add(i * 4).readPointer();
                    stackWords.push(moduleFor(v));
                } catch (_) {
                    stackWords.push('?');
                }
            }
            emit('msgbox', {
                caller: moduleFor(this.returnAddress),
                stack: frames,
                rawstack: stackWords,
                text: safeUtf8(args[1]),
                rawText: rawHex(args[1], 96),
                title: safeUtf8(args[2]),
                flags: args[3].toInt32()
            });
        }
    });
    attachOnce('USER32.dll', 'CreateWindowExW', {
        onEnter(args) {
            emit('window', {
                caller: moduleFor(this.returnAddress),
                class: safeUtf16(args[0]) !== null ? safeUtf16(args[0]) : ('atom:' + args[0]),
                title: safeUtf16(args[2])
            });
        }
    });
    attachOnce('kernel32.dll', 'ExitProcess', {
        onEnter(args) {
            emit('EXIT_PROCESS', { code: args[0].toInt32(), caller: moduleFor(this.returnAddress) });
        }
    });
    // WINHTTP: verificacion de suscripcion
    ['WinHttpOpen', 'WinHttpConnect', 'WinHttpOpenRequest', 'WinHttpSendRequest',
     'WinHttpReceiveResponse', 'WinHttpReadData', 'WinHttpCloseHandle'].forEach(name => {
        attachOnce('winhttp.dll', name, {
            onEnter(args) {
                emit('winhttp', {
                    op: name,
                    a0: name === 'WinHttpConnect' ? safeUtf16(args[1]) : args[0].toString(),
                    caller: moduleFor(this.returnAddress)
                });
            }
        });
    });
    // libcurl: la verificacion tambien puede ir por aqui (libeay/ssleay)
    ['curl_easy_init', 'curl_easy_setopt', 'curl_easy_perform',
     'curl_easy_cleanup', 'curl_global_init'].forEach(name => {
        attachOnce('libcurl.dll', name, {
            onEnter(args) {
                emit('libcurl', {
                    op: name,
                    a0: args[0].toString(),
                    a1: name === 'curl_easy_setopt' ? args[1].toInt32() : null,
                    caller: moduleFor(this.returnAddress)
                });
            }
        });
    });
    // ActivationFxMb.dll: TODOS sus exports
    const act = Process.findModuleByName('ActivationFxMb.dll');
    if (act !== null) {
        for (const exp of act.enumerateExports()) {
            const name = exp.name;
            const m = name.match(/@(\d+)$/);
            const argCount = m ? parseInt(m[1], 10) / 4 : 2;
            attachOnce('ActivationFxMb.dll', name, {
                onEnter(args) {
                    this.args = ptrs(args, Math.min(argCount, 4));
                    this.caller = moduleFor(this.returnAddress);
                },
                onLeave(retval) {
                    emit('ACT', {
                        fn: name, args: this.args,
                        result: retval.toInt32(), caller: this.caller
                    });
                }
            });
        }
    }
}

emit('diag_loaded', { pid: Process.id, arch: Process.arch });
installHooks();
setInterval(installHooks, 300);
