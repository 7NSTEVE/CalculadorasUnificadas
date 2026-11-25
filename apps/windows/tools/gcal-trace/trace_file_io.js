'use strict';

// Trace file I/O during CG50 engine initialization.
// Goal: determine if NOR flash driver opens and reads CasioROMInit.mem.

var openHandles = {};
var fileCount = 0;

function safeStr(p, encoding) {
    if (p.isNull()) return null;
    try {
        if (encoding === 'utf16') return p.readUtf16String(520);
        return p.readUtf8String(520);
    } catch (e) { return null; }
}

function emit(event, fields) {
    console.log(JSON.stringify(Object.assign({ event: event }, fields || {})));
}

function isInteresting(path) {
    if (!path) return false;
    return /\.(mem|dlm|dlp|dll|ini|lic|g3a)$/i.test(path) ||
           /Casio|DLSim|GCAI|Emulator|NOR|ROM|RAM/i.test(path);
}

function hookCreateFile(name, encoding) {
    var addr = Module.findExportByName('kernel32.dll', name);
    if (!addr) return;
    Interceptor.attach(addr, {
        onEnter: function(args) {
            this.path = safeStr(args[0], encoding);
            this.access = args[1].toUInt32();
            this.disposition = args[4].toUInt32();
        },
        onLeave: function(retval) {
            var handle = retval.toUInt32();
            var valid = (handle !== 0 && handle !== 0xFFFFFFFF);
            var path = this.path || '';
            if (isInteresting(path)) {
                fileCount++;
                if (valid) {
                    openHandles[handle] = { path: path, id: fileCount };
                }
                emit(name, {
                    id: fileCount,
                    path: path,
                    access: '0x' + this.access.toString(16),
                    disposition: this.disposition,
                    handle: valid ? '0x' + handle.toString(16) : 'INVALID',
                    valid: valid
                });
            }
        }
    });
    emit('hooked', { fn: name });
}

function hookReadFile() {
    var addr = Module.findExportByName('kernel32.dll', 'ReadFile');
    if (!addr) return;
    Interceptor.attach(addr, {
        onEnter: function(args) {
            var h = args[0].toUInt32();
            this.info = openHandles[h] || null;
            this.requested = args[2].toUInt32();
            this.pBytesRead = args[3];
        },
        onLeave: function(retval) {
            if (this.info && /\.(mem|dlm|dlp)$/i.test(this.info.path)) {
                var br = 0;
                try { br = this.pBytesRead.readU32(); } catch(e) {}
                emit('ReadFile', {
                    fileId: this.info.id,
                    path: this.info.path,
                    requested: this.requested,
                    read: br,
                    ok: retval.toInt32() !== 0
                });
            }
        }
    });
    emit('hooked', { fn: 'ReadFile' });
}

function hookCloseHandle() {
    var addr = Module.findExportByName('kernel32.dll', 'CloseHandle');
    if (!addr) return;
    Interceptor.attach(addr, {
        onEnter: function(args) {
            var h = args[0].toUInt32();
            var info = openHandles[h];
            if (info) {
                emit('CloseHandle', { fileId: info.id, path: info.path });
                delete openHandles[h];
            }
        }
    });
}

emit('trace_file_io_loaded', { pid: Process.id, arch: Process.arch });
hookCreateFile('CreateFileW', 'utf16');
hookCreateFile('CreateFileA', 'utf8');
hookReadFile();
hookCloseHandle();
