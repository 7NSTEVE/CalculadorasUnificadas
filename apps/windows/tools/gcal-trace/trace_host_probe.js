'use strict';

const installed = new Set();
const deviceCallCounts = new Map();
let devicesReported = false;

function emit(event, fields) {
    console.log(JSON.stringify(Object.assign({ event }, fields || {})));
}

function safeUtf8(value) {
    if (value.isNull()) return null;
    try { return value.readUtf8String(1024); } catch (_) { return null; }
}

function safeUtf16(value) {
    if (value.isNull()) return null;
    try { return value.readUtf16String(1024); } catch (_) { return null; }
}

function ptrs(args, count) {
    const values = [];
    for (let index = 0; index < count; ++index) values.push(args[index].toString());
    return values;
}

function callerName(address) {
    const module = Process.findModuleByAddress(address);
    if (module === null) return address.toString();
    return module.name + '+0x' + address.sub(module.base).toString(16);
}

function attach(moduleName, exportName, handlers) {
    const key = moduleName.toLowerCase() + '!' + exportName;
    if (installed.has(key)) return;
    const module = Process.findModuleByName(moduleName);
    if (module === null) return;
    const address = module.findExportByName(exportName);
    if (address === null) return;
    Interceptor.attach(address, handlers);
    installed.add(key);
    emit('hooked', { module: moduleName, symbol: exportName, address: address.toString() });
}

function attachAddress(key, address, handlers, details) {
    if (installed.has(key)) return;
    Interceptor.attach(address, handlers);
    installed.add(key);
    emit('hooked_internal', Object.assign({ key, address: address.toString() }, details || {}));
}

function fromDlsim(address) {
    const module = Process.findModuleByAddress(address);
    return module !== null && module.name.toLowerCase() === 'dlsim32e.dll';
}

function interestingObjectName(name) {
    if (name === null) return true;
    return name.toLowerCase().includes('dlsim') ||
        name.toLowerCase().includes('gcai') ||
        name.toLowerCase().includes('gcsim');
}

function interestingPath(path) {
    if (path === null) return false;
    const lower = path.toLowerCase();
    return lower.includes('calculadora unificada') ||
        lower.includes('default.g1w') ||
        lower.endsWith('.dlp') || lower.endsWith('.dlm') ||
        lower.endsWith('.mem') || lower.endsWith('.dll');
}

function dumpShared(phase) {
    try {
        const module = Process.findModuleByName('GCalAppIF.dll');
        if (module === null) return;
        const shared = module.base.add(0x679c).readPointer();
        const words = [];
        if (!shared.isNull()) {
            for (let index = 0; index < 32; ++index) {
                words.push(shared.add(index * 4).readU32());
            }
        }
        emit('GCAI.shared', { phase, address: shared.toString(), words });
    } catch (error) {
        emit('GCAI.shared_error', { phase, error: String(error) });
    }
}

function inspectDlsimDevices() {
    if (devicesReported) return;
    const module = Process.findModuleByName('DLSim32E.dll');
    if (module === null) return;

    const slots = [
        ['cpu', 0x150600],
        ['display', 0x150610],
        ['key', 0x150620]
    ];
    const report = {};
    for (const [name, offset] of slots) {
        let object = NULL;
        try { object = module.base.add(offset).readPointer(); } catch (_) {}
        if (object.isNull()) return;
        const functions = [];
        for (let index = 0; index < 12; ++index) {
            try {
                const value = object.add(index * Process.pointerSize).readPointer();
                functions.push({
                    index,
                    address: value.toString(),
                    target: value.isNull() ? null : callerName(value)
                });
            } catch (_) {
                functions.push({ index, address: null, target: null });
            }
        }
        report[name] = { object: object.toString(), functions };

        // CPUExecute calls the CPU vtable at very high frequency.  The table
        // addresses are useful, but tracing every CPU call obscures the LCD
        // and keyboard events that this probe is meant to characterize.
        if (name === 'cpu') continue;

        for (const entry of functions) {
            if (entry.address === null || entry.address === '0x0') continue;
            const key = 'device:' + name + ':' + entry.index + ':' + entry.address;
            const address = ptr(entry.address);
            const range = Process.findRangeByAddress(address);
            if (range === null || !range.protection.includes('x')) continue;
            attachAddress(key, address, {
                onEnter(args) {
                    const callKey = name + ':' + entry.index;
                    const count = deviceCallCounts.get(callKey) || 0;
                    deviceCallCounts.set(callKey, count + 1);
                    this.shouldEmit = count < 12;
                    if (!this.shouldEmit) return;
                    this.args = ptrs(args, 6);
                    this.caller = callerName(this.returnAddress);
                    this.slot = name;
                    this.index = entry.index;
                },
                onLeave(retval) {
                    if (!this.shouldEmit) return;
                    emit('DLSIM.device_call', {
                        device: this.slot,
                        index: this.index,
                        args: this.args,
                        caller: this.caller,
                        result: retval.toString()
                    });
                }
            }, { device: name, index: entry.index, target: entry.target });
        }
    }
    devicesReported = true;
    emit('DLSIM.devices', report);
}

function install() {
    attach('KERNEL32.dll', 'LoadLibraryA', {
        onEnter(args) { this.path = safeUtf8(args[0]); this.caller = callerName(this.returnAddress); },
        onLeave(retval) {
            if (interestingPath(this.path)) emit('LoadLibraryA', { path: this.path, caller: this.caller, result: retval.toString() });
        }
    });
    attach('KERNEL32.dll', 'LoadLibraryW', {
        onEnter(args) { this.path = safeUtf16(args[0]); this.caller = callerName(this.returnAddress); },
        onLeave(retval) {
            if (interestingPath(this.path)) emit('LoadLibraryW', { path: this.path, caller: this.caller, result: retval.toString() });
        }
    });
    attach('KERNEL32.dll', 'LoadLibraryExA', {
        onEnter(args) { this.path = safeUtf8(args[0]); this.caller = callerName(this.returnAddress); },
        onLeave(retval) {
            if (interestingPath(this.path)) emit('LoadLibraryExA', { path: this.path, caller: this.caller, result: retval.toString() });
        }
    });
    attach('KERNEL32.dll', 'LoadLibraryExW', {
        onEnter(args) { this.path = safeUtf16(args[0]); this.caller = callerName(this.returnAddress); },
        onLeave(retval) {
            if (interestingPath(this.path)) emit('LoadLibraryExW', { path: this.path, caller: this.caller, result: retval.toString() });
        }
    });
    attach('KERNEL32.dll', 'CreateFileA', {
        onEnter(args) { this.path = safeUtf8(args[0]); this.caller = callerName(this.returnAddress); },
        onLeave(retval) {
            if (interestingPath(this.path)) emit('CreateFileA', { path: this.path, caller: this.caller, result: retval.toString() });
        }
    });
    attach('KERNEL32.dll', 'CreateFileW', {
        onEnter(args) { this.path = safeUtf16(args[0]); this.caller = callerName(this.returnAddress); },
        onLeave(retval) {
            if (interestingPath(this.path)) emit('CreateFileW', { path: this.path, caller: this.caller, result: retval.toString() });
        }
    });

    attach('KERNEL32.dll', 'CreateFileMappingA', {
        onEnter(args) {
            this.callerAddress = this.returnAddress;
            this.caller = callerName(this.returnAddress);
            this.name = safeUtf8(args[5]);
            this.trace = fromDlsim(this.returnAddress) && interestingObjectName(this.name);
        },
        onLeave(retval) {
            if (this.trace) emit('CreateFileMappingA', {
                name: this.name,
                caller: this.caller,
                result: retval.toString(),
                lastError: this.lastError
            });
        }
    });
    attach('KERNEL32.dll', 'OpenFileMappingA', {
        onEnter(args) {
            this.caller = callerName(this.returnAddress);
            this.name = safeUtf8(args[2]);
            this.trace = fromDlsim(this.returnAddress) && interestingObjectName(this.name);
        },
        onLeave(retval) {
            if (this.trace) emit('OpenFileMappingA', {
                name: this.name,
                caller: this.caller,
                result: retval.toString(),
                lastError: this.lastError
            });
        }
    });
    attach('KERNEL32.dll', 'CreateMutexA', {
        onEnter(args) {
            this.caller = callerName(this.returnAddress);
            this.name = safeUtf8(args[2]);
            this.trace = fromDlsim(this.returnAddress) && interestingObjectName(this.name);
        },
        onLeave(retval) {
            if (this.trace) emit('CreateMutexA', {
                name: this.name,
                caller: this.caller,
                result: retval.toString(),
                lastError: this.lastError
            });
        }
    });
    attach('KERNEL32.dll', 'CreateSemaphoreA', {
        onEnter(args) {
            this.caller = callerName(this.returnAddress);
            this.name = safeUtf8(args[3]);
            this.trace = fromDlsim(this.returnAddress) && interestingObjectName(this.name);
        },
        onLeave(retval) {
            if (this.trace) emit('CreateSemaphoreA', {
                name: this.name,
                caller: this.caller,
                result: retval.toString(),
                lastError: this.lastError
            });
        }
    });
    attach('KERNEL32.dll', 'CreateThread', {
        onEnter(args) {
            this.trace = fromDlsim(this.returnAddress);
            this.caller = callerName(this.returnAddress);
            this.start = args[2].toString();
        },
        onLeave(retval) {
            if (this.trace) emit('CreateThread', {
                caller: this.caller,
                start: this.start,
                result: retval.toString(),
                lastError: this.lastError
            });
        }
    });
    attach('KERNEL32.dll', 'MapViewOfFile', {
        onEnter(args) {
            this.trace = fromDlsim(this.returnAddress);
            this.caller = callerName(this.returnAddress);
            this.mapping = args[0].toString();
        },
        onLeave(retval) {
            if (this.trace) emit('MapViewOfFile', {
                caller: this.caller,
                mapping: this.mapping,
                result: retval.toString(),
                lastError: this.lastError
            });
        }
    });

    attach('GCalAppIF.dll', '_GCAI_CreateEmulator@28', {
        onEnter(args) {
            this.output = args[4];
            emit('GCAI_CreateEmulator.enter', {
                args: ptrs(args, 7),
                baseDirectory: safeUtf8(args[0]),
                projectPath: safeUtf8(args[1])
            });
        },
        onLeave(retval) {
            let handle = null;
            try { handle = this.output.readPointer().toString(); } catch (_) {}
            emit('GCAI_CreateEmulator.leave', { result: retval.toInt32(), handle });
            dumpShared('after-create');
        }
    });
    attach('GCalAppIF.dll', '_GCAI_TerminateEmulator@4', {
        onEnter(args) { this.args = ptrs(args, 1); },
        onLeave(retval) { emit('GCAI_TerminateEmulator.leave', { args: this.args, result: retval.toInt32() }); }
    });
    attach('GCalAppIF.dll', '_GCAI_RequestTask@20', {
        onEnter(args) {
            this.args = ptrs(args, 5);
            let requestWords = null;
            try {
                requestWords = [];
                for (let index = 0; index < 8; ++index) {
                    requestWords.push(args[1].add(index * 4).readU32());
                }
            } catch (_) {}
            emit('GCAI_RequestTask.enter', { args: this.args, requestWords });
        },
        onLeave(retval) { emit('GCAI_RequestTask.leave', { args: this.args, result: retval.toInt32() }); }
    });
    attach('GCalAppIF.dll', '_GCAI_CreateResidentTask@12', {
        onEnter(args) { this.args = ptrs(args, 3); },
        onLeave(retval) {
            emit('GCAI_CreateResidentTask.leave', { args: this.args, result: retval.toInt32() });
            dumpShared('after-create-resident');
        }
    });
    attach('GCalAppIF.dll', '_GCAI_RequestResidentTask@12', {
        onEnter(args) {
            this.args = ptrs(args, 3);
            let requestWords = null;
            try {
                requestWords = [];
                for (let index = 0; index < 8; ++index) {
                    requestWords.push(args[1].add(index * 4).readU32());
                }
            } catch (_) {}
            emit('GCAI_RequestResidentTask.enter', { args: this.args, requestWords });
            dumpShared('before-request-resident');
        },
        onLeave(retval) { emit('GCAI_RequestResidentTask.leave', { args: this.args, result: retval.toInt32() }); }
    });
    attach('GCalAppIF.dll', '_GCAI_TerminateResidentTask@4', {
        onEnter(args) { this.args = ptrs(args, 1); },
        onLeave(retval) { emit('GCAI_TerminateResidentTask.leave', { args: this.args, result: retval.toInt32() }); }
    });
    attach('CASIO GCSim.dll', '_GCSim_Init@16', {
        onEnter(args) {
            emit('GCSim_Init.enter', {
                args: ptrs(args, 4),
                baseDirectory: safeUtf8(args[2]),
                projectPath: safeUtf8(args[3])
            });
        },
        onLeave(retval) { emit('GCSim_Init.leave', { result: retval.toInt32() }); }
    });

    const returnHooks = [
        ['_DLSIM_Init@4', 1],
        ['_DLSIM_InitExt2@12', 3],
        ['_DLSIM_ProjectLoad@12', 3],
        ['_DLSIM_ModelLoad@4', 1],
        ['_DLSIM_ModelSetup@0', 0],
        ['_DLSIM_ModelLoadMemory@4', 1],
        ['_DLSIM_ModelNew@0', 0],
        ['_DLSIM_SetExtendedFunction@8', 2],
        ['_DLSIM_GCAIKeyPress@8', 2],
        ['_DLSIM_GCAIUpdateDisplay@4', 1],
        ['_DLSIM_WindowIsCheckNeeded@4', 1],
        ['_DLSIM_GCAISendNotify@8', 2],
        ['_DLSIM_GCAIGetRequest@8', 2],
        ['_DLSIM_CPUExecute@4', 1]
    ];
    for (const [name, count] of returnHooks) {
        let observations = 0;
        attach('DLSim32E.dll', name, {
            onEnter(args) {
                this.args = ptrs(args, count);
                this.text0 = count > 0 ? safeUtf8(args[0]) : null;
                this.caller = callerName(this.returnAddress);
            },
            onLeave(retval) {
                if ((name === '_DLSIM_CPUExecute@4' ||
                     name === '_DLSIM_GCAIGetRequest@8' ||
                     name === '_DLSIM_WindowIsCheckNeeded@4') &&
                    observations++ >= 20) return;
                emit(name + '.leave', {
                    args: this.args,
                    text0: this.text0,
                    caller: this.caller,
                    result: retval.toInt32()
                });
            }
        });
    }
    attach('DLSim32E.dll', '_DLSIM_ErrorText@0', {
        onLeave(retval) { emit('DLSIM_ErrorText', { pointer: retval.toString(), text: safeUtf8(retval) }); }
    });

    const dlsim = Process.findModuleByName('DLSim32E.dll');
    if (dlsim !== null) {
        attachAddress('DLSim32E.dll+0x3d4a', dlsim.base.add(0x3d4a), {
            onEnter(args) {
                this.args = ptrs(args, 3);
                this.caller = callerName(this.returnAddress);
                emit('DLSIM.internal_init.enter', { args: this.args, caller: this.caller });
            },
            onLeave(retval) {
                emit('DLSIM.internal_init.leave', {
                    args: this.args,
                    caller: this.caller,
                    result: retval.toInt32()
                });
                dumpShared('after-dlsim-internal-init');
            }
        }, { module: dlsim.name, offset: '0x3d4a' });

        attachAddress('DLSim32E.dll+0x3d34', dlsim.base.add(0x3d34), {
            onEnter(args) {
                this.value = args[0].toUInt32();
                this.caller = callerName(this.returnAddress);
                emit('DLSIM.request_channel_set.enter', {
                    value: this.value,
                    caller: this.caller
                });
            },
            onLeave() {
                dumpShared('after-dlsim-request-channel-set');
            }
        }, { module: dlsim.name, offset: '0x3d34' });
    }
    inspectDlsimDevices();
}

emit('trace_host_probe_loaded', { pid: Process.id, arch: Process.arch });
install();
setInterval(install, 25);
