import 'dart:async';
import 'dart:convert';
import 'dart:ffi';
import 'dart:io';
import 'dart:isolate';
import 'dart:ui' as ui;

import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart' show rootBundle;
import 'package:path_provider/path_provider.dart';

import 'calc_models.dart';

// ---------------------------------------------------------------------------
// FFI Signatures
// ---------------------------------------------------------------------------
typedef _AllocNative = Pointer<Void> Function(IntPtr size);
typedef CasioAllocDart = Pointer<Void> Function(int size);
typedef _FreeNative = Void Function(Pointer<Void> ptr);
typedef CasioFreeDart = void Function(Pointer<Void> ptr);

typedef _NxCreateFromFileNative = Pointer<Void> Function(
    Int32 hwId, Pointer<Uint8> path);
typedef CasioNxCreateFromFileDart = Pointer<Void> Function(
    int hwId, Pointer<Uint8> path);
typedef _VoidPtrNative = Void Function(Pointer<Void> h);
typedef CasioVoidPtrDart = void Function(Pointer<Void> h);
typedef _AdvanceNative = Void Function(Pointer<Void> h, Uint32 ms);
typedef CasioAdvanceDart = void Function(Pointer<Void> h, int ms);
typedef _KeyDown32Native = Void Function(Pointer<Void> h, Uint32 code);
typedef CasioKeyDown32Dart = void Function(Pointer<Void> h, int code);
typedef _TapKeyNative = Int32 Function(
    Pointer<Void> h, Uint32 code, Uint32 holdMs, Uint32 settleMs);
typedef CasioTapKeyDart = int Function(
    Pointer<Void> h, int code, int holdMs, int settleMs);
typedef _IntQueryNative = Int32 Function(Pointer<Void> h);
typedef CasioIntQueryDart = int Function(Pointer<Void> h);
typedef _RenderWithIndNative = Int32 Function(
    Pointer<Void> h,
    Pointer<Uint8> outRgba,
    Int32 outSize,
    Pointer<Uint32> outIndicators,
    Int32 forceCopy);
typedef CasioRenderWithIndDart = int Function(
    Pointer<Void> h,
    Pointer<Uint8> outRgba,
    int outSize,
    Pointer<Uint32> outIndicators,
    int forceCopy);
typedef _FileOpNative = Int32 Function(Pointer<Void> h, Pointer<Uint8> path);
typedef CasioFileOpDart = int Function(Pointer<Void> h, Pointer<Uint8> path);

typedef _Cw2CreateFromFilesNative = Pointer<Void> Function(
    Pointer<Uint8> romPath, Pointer<Uint8> ramPath);
typedef CasioCw2CreateFromFilesDart = Pointer<Void> Function(
    Pointer<Uint8> romPath, Pointer<Uint8> ramPath);
typedef _ExportBufNative = IntPtr Function(
    Pointer<Void> h, Pointer<Uint8> outBuf, IntPtr outSize);
typedef CasioExportBufDart = int Function(
    Pointer<Void> h, Pointer<Uint8> outBuf, int outSize);
typedef _ImportBufNative = Int32 Function(
    Pointer<Void> h, Pointer<Uint8> buf, IntPtr size);
typedef CasioImportBufDart = int Function(
    Pointer<Void> h, Pointer<Uint8> buf, int size);

typedef _CgCreateNative = Pointer<Void> Function(Pointer<Uint8> dir);
typedef CasioCgCreateDart = Pointer<Void> Function(Pointer<Uint8> dir);
typedef _CgStepNative = Int32 Function(
    Pointer<Void> h, Uint64 runBudget, Uint64 sleepCycles);
typedef CasioCgStepDart = int Function(
    Pointer<Void> h, int runBudget, int sleepCycles);
typedef _CgKeyDownNative = Int32 Function(Pointer<Void> h, Uint16 code);
typedef CasioCgKeyDownDart = int Function(Pointer<Void> h, int code);
typedef _CgKeyUpNative = Void Function(Pointer<Void> h, Uint16 code);
typedef CasioCgKeyUpDart = void Function(Pointer<Void> h, int code);
typedef _CgStatsNative = Int32 Function(
    Pointer<Void> h,
    Pointer<Uint64> outInst,
    Pointer<Uint64> outCycles,
    Pointer<Uint32> outPc,
    Pointer<Uint64> outLcd);
typedef CasioCgStatsDart = int Function(
    Pointer<Void> h,
    Pointer<Uint64> outInst,
    Pointer<Uint64> outCycles,
    Pointer<Uint32> outPc,
    Pointer<Uint64> outLcd);
typedef _CgRenderNative = Int32 Function(
    Pointer<Void> h, Pointer<Uint8> outRgba, Int32 outSize);
typedef CasioCgRenderDart = int Function(
    Pointer<Void> h, Pointer<Uint8> outRgba, int outSize);
typedef _CgRenderExNative = Int32 Function(
    Pointer<Void> h, Pointer<Uint8> outRgba, Int32 outSize, Int32 forceCopy);
typedef CasioCgRenderExDart = int Function(
    Pointer<Void> h, Pointer<Uint8> outRgba, int outSize, int forceCopy);
typedef _CgSetRtcEpochNative = Int32 Function(
    Pointer<Void> h, Int64 localUnixSeconds);
typedef CasioCgSetRtcEpochDart = int Function(
    Pointer<Void> h, int localUnixSeconds);

class CasioNativeBindings {
  final DynamicLibrary lib;
  final String? libraryPath;

  late final CasioAllocDart alloc;
  late final CasioFreeDart free;

  // nX-U8 / nX-U16
  late final CasioNxCreateFromFileDart nxCreateFromFile;
  late final CasioVoidPtrDart nxDestroy;
  late final CasioVoidPtrDart nxPowerOn;
  late final CasioAdvanceDart nxAdvance;
  late final CasioKeyDown32Dart nxKeyDown;
  late final CasioVoidPtrDart nxKeyUp;
  late final CasioTapKeyDart nxTapKey;
  late final CasioIntQueryDart nxScreenWidth;
  late final CasioIntQueryDart nxScreenHeight;
  late final CasioRenderWithIndDart nxRenderRgba;
  late final CasioFileOpDart nxSaveStateFile;
  late final CasioFileOpDart nxLoadStateFile;

  // fx-991CW (ClassWiz II)
  late final CasioCw2CreateFromFilesDart cw2CreateFromFiles;
  late final CasioVoidPtrDart cw2Destroy;
  late final CasioVoidPtrDart cw2PowerOn;
  late final CasioAdvanceDart cw2Advance;
  late final CasioKeyDown32Dart cw2KeyDown;
  late final CasioVoidPtrDart cw2KeyUp;
  late final CasioTapKeyDart cw2TapKey;
  late final CasioIntQueryDart cw2ScreenWidth;
  late final CasioIntQueryDart cw2ScreenHeight;
  late final CasioRenderWithIndDart cw2RenderRgba;
  late final CasioExportBufDart cw2ExportRam;
  late final CasioImportBufDart cw2ImportRam;
  late final CasioFileOpDart cw2ExportRamFile;
  late final CasioFileOpDart cw2ImportRamFile;

  // fx-CG50 PRIZM
  late final CasioCgCreateDart cgCreateFromDir;
  late final CasioVoidPtrDart cgDestroy;
  late final CasioCgStepDart cgStep;
  late final CasioCgKeyDownDart cgKeyDown;
  late final CasioCgKeyUpDart cgKeyUp;
  late final CasioVoidPtrDart cgReleaseAllKeys;
  late final CasioCgStatsDart cgGetStats;
  late final CasioCgRenderDart cgRenderRgba;
  late final CasioCgRenderExDart cgRenderRgbaEx;
  late final CasioExportBufDart cgExportState;
  late final CasioImportBufDart cgImportState;
  late final CasioFileOpDart cgExportStateFile;
  late final CasioFileOpDart cgImportStateFile;
  late final CasioCgSetRtcEpochDart cgSetRtcEpoch;
  late final CasioFileOpDart cgExportUserRomFile;
  late final CasioFileOpDart cgImportUserRomFile;
  late final CasioFileOpDart cgLoadAddinFile;

  CasioNativeBindings(this.lib, [this.libraryPath]) {
    alloc = lib.lookupFunction<_AllocNative, CasioAllocDart>('casio_alloc');
    free = lib.lookupFunction<_FreeNative, CasioFreeDart>('casio_free');

    nxCreateFromFile =
        lib.lookupFunction<_NxCreateFromFileNative, CasioNxCreateFromFileDart>(
            'casio_nx_create_from_file');
    nxDestroy =
        lib.lookupFunction<_VoidPtrNative, CasioVoidPtrDart>('casio_nx_destroy');
    nxPowerOn =
        lib.lookupFunction<_VoidPtrNative, CasioVoidPtrDart>('casio_nx_power_on');
    nxAdvance =
        lib.lookupFunction<_AdvanceNative, CasioAdvanceDart>('casio_nx_advance');
    nxKeyDown =
        lib.lookupFunction<_KeyDown32Native, CasioKeyDown32Dart>('casio_nx_key_down');
    nxKeyUp =
        lib.lookupFunction<_VoidPtrNative, CasioVoidPtrDart>('casio_nx_key_up');
    nxTapKey =
        lib.lookupFunction<_TapKeyNative, CasioTapKeyDart>('casio_nx_tap_key');
    nxScreenWidth = lib.lookupFunction<_IntQueryNative, CasioIntQueryDart>(
        'casio_nx_screen_width');
    nxScreenHeight = lib.lookupFunction<_IntQueryNative, CasioIntQueryDart>(
        'casio_nx_screen_height');
    nxRenderRgba =
        lib.lookupFunction<_RenderWithIndNative, CasioRenderWithIndDart>(
            'casio_nx_render_rgba');
    nxSaveStateFile = lib.lookupFunction<_FileOpNative, CasioFileOpDart>(
        'casio_nx_save_state_file');
    nxLoadStateFile = lib.lookupFunction<_FileOpNative, CasioFileOpDart>(
        'casio_nx_load_state_file');

    cw2CreateFromFiles = lib.lookupFunction<_Cw2CreateFromFilesNative,
        CasioCw2CreateFromFilesDart>('casio_cw2_create_from_files');
    cw2Destroy =
        lib.lookupFunction<_VoidPtrNative, CasioVoidPtrDart>('casio_cw2_destroy');
    cw2PowerOn =
        lib.lookupFunction<_VoidPtrNative, CasioVoidPtrDart>('casio_cw2_power_on');
    cw2Advance =
        lib.lookupFunction<_AdvanceNative, CasioAdvanceDart>('casio_cw2_advance');
    cw2KeyDown =
        lib.lookupFunction<_KeyDown32Native, CasioKeyDown32Dart>('casio_cw2_key_down');
    cw2KeyUp =
        lib.lookupFunction<_VoidPtrNative, CasioVoidPtrDart>('casio_cw2_key_up');
    cw2TapKey =
        lib.lookupFunction<_TapKeyNative, CasioTapKeyDart>('casio_cw2_tap_key');
    cw2ScreenWidth = lib.lookupFunction<_IntQueryNative, CasioIntQueryDart>(
        'casio_cw2_screen_width');
    cw2ScreenHeight = lib.lookupFunction<_IntQueryNative, CasioIntQueryDart>(
        'casio_cw2_screen_height');
    cw2RenderRgba =
        lib.lookupFunction<_RenderWithIndNative, CasioRenderWithIndDart>(
            'casio_cw2_render_rgba');
    cw2ExportRam = lib.lookupFunction<_ExportBufNative, CasioExportBufDart>(
        'casio_cw2_export_ram');
    cw2ImportRam = lib.lookupFunction<_ImportBufNative, CasioImportBufDart>(
        'casio_cw2_import_ram');
    cw2ExportRamFile = lib.lookupFunction<_FileOpNative, CasioFileOpDart>(
        'casio_cw2_export_ram_file');
    cw2ImportRamFile = lib.lookupFunction<_FileOpNative, CasioFileOpDart>(
        'casio_cw2_import_ram_file');

    cgCreateFromDir = lib.lookupFunction<_CgCreateNative, CasioCgCreateDart>(
        'casio_cg50_create_from_dir');
    cgDestroy =
        lib.lookupFunction<_VoidPtrNative, CasioVoidPtrDart>('casio_cg50_destroy');
    cgStep = lib.lookupFunction<_CgStepNative, CasioCgStepDart>('casio_cg50_step');
    cgKeyDown = lib.lookupFunction<_CgKeyDownNative, CasioCgKeyDownDart>(
        'casio_cg50_key_down');
    cgKeyUp = lib.lookupFunction<_CgKeyUpNative, CasioCgKeyUpDart>(
        'casio_cg50_key_up');
    cgReleaseAllKeys = lib.lookupFunction<_VoidPtrNative, CasioVoidPtrDart>(
        'casio_cg50_release_all_keys');
    cgGetStats = lib.lookupFunction<_CgStatsNative, CasioCgStatsDart>(
        'casio_cg50_get_stats');
    cgRenderRgba = lib.lookupFunction<_CgRenderNative, CasioCgRenderDart>(
        'casio_cg50_render_rgba');
    cgRenderRgbaEx = lib.lookupFunction<_CgRenderExNative, CasioCgRenderExDart>(
        'casio_cg50_render_rgba_ex');
    cgExportState = lib.lookupFunction<_ExportBufNative, CasioExportBufDart>(
        'casio_cg50_export_state');
    cgImportState = lib.lookupFunction<_ImportBufNative, CasioImportBufDart>(
        'casio_cg50_import_state');
    cgExportStateFile = lib.lookupFunction<_FileOpNative, CasioFileOpDart>(
        'casio_cg50_export_state_file');
    cgImportStateFile = lib.lookupFunction<_FileOpNative, CasioFileOpDart>(
        'casio_cg50_import_state_file');
    cgSetRtcEpoch =
        lib.lookupFunction<_CgSetRtcEpochNative, CasioCgSetRtcEpochDart>(
            'casio_cg50_set_rtc_epoch');
    cgExportUserRomFile = lib.lookupFunction<_FileOpNative, CasioFileOpDart>(
        'casio_cg50_export_user_rom_file');
    cgImportUserRomFile = lib.lookupFunction<_FileOpNative, CasioFileOpDart>(
        'casio_cg50_import_user_rom_file');
    cgLoadAddinFile = lib.lookupFunction<_FileOpNative, CasioFileOpDart>(
        'casio_cg50_load_addin_file');
  }

  static CasioNativeBindings open([String? overridePath]) {
    if (overridePath != null && overridePath.isNotEmpty) {
      return CasioNativeBindings(DynamicLibrary.open(overridePath), overridePath);
    }
    if (Platform.isAndroid) {
      return CasioNativeBindings(DynamicLibrary.open('libcasio_native.so'));
    }
    if (Platform.isWindows) {
      return CasioNativeBindings(
          DynamicLibrary.open('casio_native.dll'), 'casio_native.dll');
    }
    return CasioNativeBindings(DynamicLibrary.open('libcasio_native.so'));
  }

  Pointer<Uint8> allocCString(String text) {
    final encoded = utf8.encode(text);
    final ptr = alloc(encoded.length + 1).cast<Uint8>();
    final view = ptr.asTypedList(encoded.length + 1);
    view.setAll(0, encoded);
    view[encoded.length] = 0;
    return ptr;
  }
}

// ---------------------------------------------------------------------------
// Persistent Runtime Staging (ApplicationSupportDirectory, never systemTemp)
// ---------------------------------------------------------------------------
class CasioRomStaging {
  static const Map<String, int> expectedRomSizes = {
    'fx991lax.rom': 262144,
    'fx570vnplus.rom': 196608,
    'fx991cw_rom.bin': 524288,
    'fx991cw_default_en.ram': 24832,
    'SystemROM.raw': 11927552,
    'UserROM.raw': 21626880,
    'C2ROM.raw': 4849664,
    'official-ILRAM-FD800000-4000.bin': 16384,
  };

  static Future<void> _writeAtomic(File target, List<int> bytes) async {
    final tmp = File('${target.path}.tmp');
    await tmp.writeAsBytes(bytes, flush: true);
    if (await target.exists()) {
      await target.delete();
    }
    await tmp.rename(target.path);
  }

  static Future<String> ensureStaged({String? customTargetDir}) async {
    final Directory dir;
    if (customTargetDir != null && customTargetDir.isNotEmpty) {
      dir = Directory(customTargetDir);
    } else {
      final support = await getApplicationSupportDirectory();
      dir = Directory('${support.path}/casio_runtime_v2');
    }
    if (!await dir.exists()) {
      await dir.create(recursive: true);
    }

    for (final entry in expectedRomSizes.entries) {
      final outFile = File('${dir.path}/${entry.key}');
      if (await outFile.exists() && await outFile.length() == entry.value) {
        continue;
      }
      final data = await rootBundle.load('assets_staged/roms/${entry.key}');
      final bytes = data.buffer.asUint8List(
        data.offsetInBytes,
        data.lengthInBytes,
      );
      await _writeAtomic(outFile, bytes);
    }

    // Seed fx-991CW live RAM on first launch
    final cw2LiveRam = File('${dir.path}/fx991cw_live.ram');
    if (!await cw2LiveRam.exists() || await cw2LiveRam.length() != 24832) {
      final defRam = File('${dir.path}/fx991cw_default_en.ram');
      if (await defRam.exists()) {
        await _writeAtomic(cw2LiveRam, await defRam.readAsBytes());
      }
    }

    // Decompress pre-baked CG50 warm state (.bin.gz) on first launch or core upgrade
    final warmBin = File('${dir.path}/cg50_warm_state.bin');
    final warmStamp = File('${dir.path}/cg50_warm_v43.stamp');
    if (!await warmBin.exists() ||
        await warmBin.length() < 1024 * 1024 ||
        !await warmStamp.exists()) {
      try {
        final gzData =
            await rootBundle.load('assets_staged/roms/cg50_warm_state.bin.gz');
        final gzBytes = gzData.buffer.asUint8List(
          gzData.offsetInBytes,
          gzData.lengthInBytes,
        );
        final decoded = await Isolate.run(() => gzip.decode(gzBytes));
        if (decoded.length > 1024 * 1024) {
          await _writeAtomic(warmBin, decoded);
          await _writeAtomic(warmStamp, utf8.encode('v42-cfi-sh4a'));
        }
      } catch (_) {
        // Optional warm state; CG50 falls back to cold boot if absent.
      }
    }

    return dir.path;
  }
}

// ---------------------------------------------------------------------------
// Unified Controller Base
// ---------------------------------------------------------------------------
abstract class CasioEngineController extends ChangeNotifier {
  CasioCalcKind get kind;
  bool get ready;
  bool get hasValidFrame;
  String get statusLabel;
  int get indicatorsMask;
  ui.Image? get lcdImage;
  Uint8List? get lastRgbaBytes;
  int get frameWidth;
  int get frameHeight;

  void pressDown(CasioKeyDef key);
  void releaseUp(CasioKeyDef key);
  void setPaused(bool paused);
  Future<bool> savePersistentState();
  void resetEngine({bool cold = false});
}

// ---------------------------------------------------------------------------
// 1. Worker-Isolate Engine for nX-U8 / nX-U16 (fx-570LA X & fx-570VN PLUS)
// ---------------------------------------------------------------------------
class _NxWorkerInit {
  final SendPort mainPort;
  final int hardwareId;
  final String romPath;
  final String statePath;
  final String? libraryPath;
  _NxWorkerInit(
    this.mainPort,
    this.hardwareId,
    this.romPath,
    this.statePath,
    this.libraryPath,
  );
}

void _nxIsolateMain(_NxWorkerInit init) {
  final cmdPort = ReceivePort();
  init.mainPort.send({'type': 'port', 'port': cmdPort.sendPort});

  final b = CasioNativeBindings.open(init.libraryPath);
  final pathPtr = b.allocCString(init.romPath);
  Pointer<Void> handle = nullptr;
  try {
    handle = b.nxCreateFromFile(init.hardwareId, pathPtr);
  } finally {
    b.free(pathPtr.cast());
  }
  if (handle == nullptr) {
    init.mainPort.send({'type': 'error', 'message': 'No disponible'});
    cmdPort.close();
    return;
  }

  // Restore persisted state if valid
  if (init.statePath.isNotEmpty && File(init.statePath).existsSync()) {
    final sPtr = b.allocCString(init.statePath);
    try {
      b.nxLoadStateFile(handle, sPtr);
    } finally {
      b.free(sPtr.cast());
    }
  }

  final int w = b.nxScreenWidth(handle);
  final int h = b.nxScreenHeight(handle);
  final int rgbaBytes = w * h * 4;
  final rgbaPtr = b.alloc(rgbaBytes).cast<Uint8>();
  final indPtr = b.alloc(4).cast<Uint32>();

  void pushFrame(bool forceCopy) {
    final res = b.nxRenderRgba(handle, rgbaPtr, rgbaBytes, indPtr, forceCopy ? 1 : 0);
    if (res > 0) {
      final copy = Uint8List.fromList(rgbaPtr.asTypedList(rgbaBytes));
      init.mainPort.send({
        'type': 'frame',
        'w': w,
        'h': h,
        'ind': indPtr.value,
        'rgba': TransferableTypedData.fromList([copy]),
      });
    }
  }

  pushFrame(true);

  bool running = true;
  bool paused = false;
  int? heldCode;
  int heldMs = 0;
  bool releasePending = false;
  final List<Map<String, dynamic>> queue = [];
  Completer<void>? wakeup;

  void notifyWakeup() {
    if (wakeup != null && !wakeup!.isCompleted) {
      wakeup!.complete();
    }
  }

  cmdPort.listen((msg) {
    if (msg is Map<String, dynamic>) {
      queue.add(msg);
      notifyWakeup();
    }
  });

  Future<void> pump() async {
    while (running) {
      while (queue.isNotEmpty) {
        final cmd = queue.removeAt(0);
        final op = cmd['op'] as String?;
        if (op == 'dispose') {
          running = false;
          break;
        } else if (op == 'pause') {
          paused = cmd['value'] as bool? ?? false;
          if (!paused) pushFrame(true);
        } else if (op == 'down') {
          final code = cmd['code'] as int;
          if (heldCode != null) {
            if (heldMs < 140) b.nxAdvance(handle, 140 - heldMs);
            b.nxKeyUp(handle);
            b.nxAdvance(handle, 60);
            heldCode = null;
            heldMs = 0;
            releasePending = false;
          }
          if (code == 0xFF) {
            b.nxPowerOn(handle);
            pushFrame(true);
          } else {
            heldCode = code;
            heldMs = 80;
            releasePending = false;
            b.nxKeyDown(handle, code);
            b.nxAdvance(handle, 80);
            pushFrame(true);
          }
        } else if (op == 'up') {
          final code = cmd['code'] as int;
          if (heldCode == code) {
            releasePending = true;
            if (heldMs < 140) b.nxAdvance(handle, 140 - heldMs);
            b.nxKeyUp(handle);
            b.nxAdvance(handle, 80);
            heldCode = null;
            heldMs = 0;
            releasePending = false;
            pushFrame(true);
          }
        } else if (op == 'reset') {
          b.nxPowerOn(handle);
          pushFrame(true);
        } else if (op == 'save_state') {
          final reqId = cmd['reqId'] as int? ?? 0;
          bool ok = false;
          if (init.statePath.isNotEmpty) {
            final sPtr = b.allocCString(init.statePath);
            try {
              ok = b.nxSaveStateFile(handle, sPtr) == 1;
            } finally {
              b.free(sPtr.cast());
            }
          }
          init.mainPort.send({'type': 'saved', 'reqId': reqId, 'ok': ok});
        }
      }
      if (!running) break;

      if (paused && queue.isEmpty) {
        wakeup = Completer<void>();
        await wakeup!.future;
        wakeup = null;
        continue;
      }

      b.nxAdvance(handle, 33);
      if (heldCode != null) {
        heldMs += 33;
        if (releasePending && heldMs >= 140) {
          b.nxKeyUp(handle);
          b.nxAdvance(handle, 80);
          heldCode = null;
          heldMs = 0;
          releasePending = false;
        }
      }
      pushFrame(false);
      await Future<void>.delayed(const Duration(milliseconds: 33));
    }

    b.nxDestroy(handle);
    b.free(rgbaPtr.cast());
    b.free(indPtr.cast());
    cmdPort.close();
  }

  unawaited(pump());
}

class CasioNxEngine extends CasioEngineController {
  @override
  final CasioCalcKind kind;
  final String romPath;
  final String statePath;
  final String? libraryPath;

  Isolate? _isolate;
  ReceivePort? _rxPort;
  SendPort? _txPort;
  bool _disposed = false;
  bool _ready = false;
  bool _decoding = false;
  int _w = 0;
  int _h = 0;
  int _indicators = 0;
  ui.Image? _image;
  Uint8List? _lastRgba;
  String _statusLabel = 'Iniciando...';
  int _nextReqId = 1;
  final Map<int, Completer<bool>> _saveWaiters = {};

  CasioNxEngine({
    required this.kind,
    required this.romPath,
    required this.statePath,
    this.libraryPath,
  });

  @override
  bool get ready => _ready;
  @override
  bool get hasValidFrame => _image != null;
  @override
  String get statusLabel => _statusLabel;
  @override
  int get indicatorsMask => _indicators;
  @override
  ui.Image? get lcdImage => _image;
  @override
  Uint8List? get lastRgbaBytes => _lastRgba;
  @override
  int get frameWidth => _w;
  @override
  int get frameHeight => _h;

  Future<void> start() async {
    _rxPort = ReceivePort();
    _rxPort!.listen(_onMessage);
    final hwId = kind == CasioCalcKind.classwiz ? 4 : 3;
    _isolate = await Isolate.spawn(
      _nxIsolateMain,
      _NxWorkerInit(_rxPort!.sendPort, hwId, romPath, statePath, libraryPath),
    );
  }

  void _onMessage(dynamic msg) {
    if (_disposed || msg is! Map) return;
    final type = msg['type'] as String?;
    if (type == 'port') {
      _txPort = msg['port'] as SendPort;
      _ready = true;
      _statusLabel = 'Lista';
      notifyListeners();
    } else if (type == 'error') {
      _statusLabel = msg['message'] as String? ?? 'No disponible';
      notifyListeners();
    } else if (type == 'saved') {
      final reqId = msg['reqId'] as int? ?? 0;
      _saveWaiters.remove(reqId)?.complete(msg['ok'] as bool? ?? false);
    } else if (type == 'frame') {
      _w = msg['w'] as int? ?? _w;
      _h = msg['h'] as int? ?? _h;
      _indicators = msg['ind'] as int? ?? _indicators;
      final xfer = msg['rgba'] as TransferableTypedData?;
      if (xfer != null && !_decoding && _w > 0 && _h > 0) {
        final bytes = xfer.materialize().asUint8List();
        _lastRgba = bytes;
        _decoding = true;
        ui.decodeImageFromPixels(bytes, _w, _h, ui.PixelFormat.rgba8888, (img) {
          _decoding = false;
          if (_disposed) {
            img.dispose();
            return;
          }
          _image?.dispose();
          _image = img;
          notifyListeners();
        });
      } else {
        notifyListeners();
      }
    }
  }

  @override
  void pressDown(CasioKeyDef key) {
    _txPort?.send({'op': 'down', 'code': key.matrixCode});
  }

  @override
  void releaseUp(CasioKeyDef key) {
    _txPort?.send({'op': 'up', 'code': key.matrixCode});
  }

  @override
  void setPaused(bool paused) {
    _txPort?.send({'op': 'pause', 'value': paused});
  }

  @override
  Future<bool> savePersistentState() {
    final port = _txPort;
    if (!_ready || port == null || _disposed) return Future.value(false);
    final reqId = _nextReqId++;
    final c = Completer<bool>();
    _saveWaiters[reqId] = c;
    port.send({'op': 'save_state', 'reqId': reqId});
    return c.future.timeout(const Duration(seconds: 4), onTimeout: () {
      _saveWaiters.remove(reqId);
      return false;
    });
  }

  @override
  void resetEngine({bool cold = false}) {
    _txPort?.send({'op': 'reset'});
  }

  @override
  void dispose() {
    _disposed = true;
    for (final w in _saveWaiters.values) {
      if (!w.isCompleted) w.complete(false);
    }
    _saveWaiters.clear();
    _txPort?.send({'op': 'dispose'});
    _rxPort?.close();
    _isolate?.kill(priority: Isolate.beforeNextEvent);
    _image?.dispose();
    _image = null;
    super.dispose();
  }
}

// ---------------------------------------------------------------------------
// 2. Worker-Isolate Engine for fx-991CW (ClassWiz II, 4-gray, 24832 B RAM)
// ---------------------------------------------------------------------------
class _Cw2WorkerInit {
  final SendPort mainPort;
  final String romPath;
  final String defaultRamPath;
  final String liveRamPath;
  final String? libraryPath;
  _Cw2WorkerInit(
    this.mainPort,
    this.romPath,
    this.defaultRamPath,
    this.liveRamPath,
    this.libraryPath,
  );
}

void _cw2IsolateMain(_Cw2WorkerInit init) {
  final cmdPort = ReceivePort();
  init.mainPort.send({'type': 'port', 'port': cmdPort.sendPort});

  final b = CasioNativeBindings.open(init.libraryPath);

  Pointer<Void> createCore(bool useDefaultRam) {
    final ramSrc = (!useDefaultRam &&
            init.liveRamPath.isNotEmpty &&
            File(init.liveRamPath).existsSync() &&
            File(init.liveRamPath).lengthSync() == 24832)
        ? init.liveRamPath
        : init.defaultRamPath;
    final romPtr = b.allocCString(init.romPath);
    final ramPtr = b.allocCString(ramSrc);
    try {
      final h = b.cw2CreateFromFiles(romPtr, ramPtr);
      if (h != nullptr) {
        b.cw2PowerOn(h);
        b.cw2Advance(h, 250);
      }
      return h;
    } finally {
      b.free(romPtr.cast());
      b.free(ramPtr.cast());
    }
  }

  Pointer<Void> handle = createCore(false);
  if (handle == nullptr) {
    init.mainPort.send({'type': 'error', 'message': 'No disponible'});
    cmdPort.close();
    return;
  }

  final int w = b.cw2ScreenWidth(handle);
  final int h = b.cw2ScreenHeight(handle);
  final int rgbaBytes = w * h * 4;
  final rgbaPtr = b.alloc(rgbaBytes).cast<Uint8>();
  final indPtr = b.alloc(4).cast<Uint32>();

  void pushFrame(bool forceCopy) {
    final res =
        b.cw2RenderRgba(handle, rgbaPtr, rgbaBytes, indPtr, forceCopy ? 1 : 0);
    if (res > 0) {
      final copy = Uint8List.fromList(rgbaPtr.asTypedList(rgbaBytes));
      init.mainPort.send({
        'type': 'frame',
        'w': w,
        'h': h,
        'ind': indPtr.value,
        'rgba': TransferableTypedData.fromList([copy]),
      });
    }
  }

  pushFrame(true);

  bool running = true;
  bool paused = false;
  int? heldCode;
  int heldMs = 0;
  bool releasePending = false;
  final List<Map<String, dynamic>> queue = [];
  Completer<void>? wakeup;

  void notifyWakeup() {
    if (wakeup != null && !wakeup!.isCompleted) {
      wakeup!.complete();
    }
  }

  cmdPort.listen((msg) {
    if (msg is Map<String, dynamic>) {
      queue.add(msg);
      notifyWakeup();
    }
  });

  Future<void> pump() async {
    while (running) {
      while (queue.isNotEmpty) {
        final cmd = queue.removeAt(0);
        final op = cmd['op'] as String?;
        if (op == 'dispose') {
          running = false;
          break;
        } else if (op == 'pause') {
          paused = cmd['value'] as bool? ?? false;
          if (!paused) pushFrame(true);
        } else if (op == 'down') {
          final code = cmd['code'] as int;
          if (heldCode != null) {
            if (heldMs < 90) b.cw2Advance(handle, 90 - heldMs);
            b.cw2KeyUp(handle);
            b.cw2Advance(handle, 60);
            heldCode = null;
            heldMs = 0;
            releasePending = false;
          }
          if (code == 0xFF) {
            b.cw2PowerOn(handle);
            pushFrame(true);
          } else {
            heldCode = code;
            heldMs = 70;
            releasePending = false;
            b.cw2KeyDown(handle, code);
            b.cw2Advance(handle, 70);
            pushFrame(true);
          }
        } else if (op == 'up') {
          final code = cmd['code'] as int;
          if (heldCode == code) {
            releasePending = true;
            if (heldMs < 90) b.cw2Advance(handle, 90 - heldMs);
            b.cw2KeyUp(handle);
            b.cw2Advance(handle, 80);
            heldCode = null;
            heldMs = 0;
            releasePending = false;
            pushFrame(true);
          }
        } else if (op == 'reset') {
          final cold = cmd['cold'] as bool? ?? false;
          if (cold) {
            b.cw2Destroy(handle);
            handle = createCore(true);
            if (handle == nullptr) {
              running = false;
              break;
            }
          } else {
            b.cw2PowerOn(handle);
          }
          pushFrame(true);
        } else if (op == 'save_state') {
          final reqId = cmd['reqId'] as int? ?? 0;
          bool ok = false;
          if (init.liveRamPath.isNotEmpty) {
            final pPtr = b.allocCString(init.liveRamPath);
            try {
              ok = b.cw2ExportRamFile(handle, pPtr) == 1;
            } finally {
              b.free(pPtr.cast());
            }
          }
          init.mainPort.send({'type': 'saved', 'reqId': reqId, 'ok': ok});
        }
      }
      if (!running) break;

      if (paused && queue.isEmpty) {
        wakeup = Completer<void>();
        await wakeup!.future;
        wakeup = null;
        continue;
      }

      b.cw2Advance(handle, 33);
      if (heldCode != null) {
        heldMs += 33;
        if (releasePending && heldMs >= 90) {
          b.cw2KeyUp(handle);
          b.cw2Advance(handle, 80);
          heldCode = null;
          heldMs = 0;
          releasePending = false;
        }
      }
      pushFrame(false);
      await Future<void>.delayed(const Duration(milliseconds: 33));
    }

    if (handle != nullptr) b.cw2Destroy(handle);
    b.free(rgbaPtr.cast());
    b.free(indPtr.cast());
    cmdPort.close();
  }

  unawaited(pump());
}

class CasioCw2Engine extends CasioEngineController {
  @override
  CasioCalcKind get kind => CasioCalcKind.cw2;
  final String romPath;
  final String defaultRamPath;
  final String liveRamPath;
  final String? libraryPath;

  Isolate? _isolate;
  ReceivePort? _rxPort;
  SendPort? _txPort;
  bool _disposed = false;
  bool _ready = false;
  bool _decoding = false;
  int _w = 192;
  int _h = 63;
  int _indicators = 0;
  ui.Image? _image;
  Uint8List? _lastRgba;
  String _statusLabel = 'Iniciando...';
  int _nextReqId = 1;
  final Map<int, Completer<bool>> _saveWaiters = {};

  CasioCw2Engine({
    required this.romPath,
    required this.defaultRamPath,
    required this.liveRamPath,
    this.libraryPath,
  });

  @override
  bool get ready => _ready;
  @override
  bool get hasValidFrame => _image != null;
  @override
  String get statusLabel => _statusLabel;
  @override
  int get indicatorsMask => _indicators;
  @override
  ui.Image? get lcdImage => _image;
  @override
  Uint8List? get lastRgbaBytes => _lastRgba;
  @override
  int get frameWidth => _w;
  @override
  int get frameHeight => _h;

  Future<void> start() async {
    _rxPort = ReceivePort();
    _rxPort!.listen(_onMessage);
    _isolate = await Isolate.spawn(
      _cw2IsolateMain,
      _Cw2WorkerInit(
        _rxPort!.sendPort,
        romPath,
        defaultRamPath,
        liveRamPath,
        libraryPath,
      ),
    );
  }

  void _onMessage(dynamic msg) {
    if (_disposed || msg is! Map) return;
    final type = msg['type'] as String?;
    if (type == 'port') {
      _txPort = msg['port'] as SendPort;
      _ready = true;
      _statusLabel = 'Lista';
      notifyListeners();
    } else if (type == 'error') {
      _statusLabel = msg['message'] as String? ?? 'No disponible';
      notifyListeners();
    } else if (type == 'saved') {
      final reqId = msg['reqId'] as int? ?? 0;
      _saveWaiters.remove(reqId)?.complete(msg['ok'] as bool? ?? false);
    } else if (type == 'frame') {
      _w = msg['w'] as int? ?? _w;
      _h = msg['h'] as int? ?? _h;
      _indicators = msg['ind'] as int? ?? _indicators;
      final xfer = msg['rgba'] as TransferableTypedData?;
      if (xfer != null && !_decoding && _w > 0 && _h > 0) {
        final bytes = xfer.materialize().asUint8List();
        _lastRgba = bytes;
        _decoding = true;
        ui.decodeImageFromPixels(bytes, _w, _h, ui.PixelFormat.rgba8888, (img) {
          _decoding = false;
          if (_disposed) {
            img.dispose();
            return;
          }
          _image?.dispose();
          _image = img;
          notifyListeners();
        });
      } else {
        notifyListeners();
      }
    }
  }

  @override
  void pressDown(CasioKeyDef key) {
    _txPort?.send({'op': 'down', 'code': key.matrixCode});
  }

  @override
  void releaseUp(CasioKeyDef key) {
    _txPort?.send({'op': 'up', 'code': key.matrixCode});
  }

  @override
  void setPaused(bool paused) {
    _txPort?.send({'op': 'pause', 'value': paused});
  }

  @override
  Future<bool> savePersistentState() {
    final port = _txPort;
    if (!_ready || port == null || _disposed) return Future.value(false);
    final reqId = _nextReqId++;
    final c = Completer<bool>();
    _saveWaiters[reqId] = c;
    port.send({'op': 'save_state', 'reqId': reqId});
    return c.future.timeout(const Duration(seconds: 4), onTimeout: () {
      _saveWaiters.remove(reqId);
      return false;
    });
  }

  @override
  void resetEngine({bool cold = false}) {
    _txPort?.send({'op': 'reset', 'cold': cold});
  }

  @override
  void dispose() {
    _disposed = true;
    for (final w in _saveWaiters.values) {
      if (!w.isCompleted) w.complete(false);
    }
    _saveWaiters.clear();
    _txPort?.send({'op': 'dispose'});
    _rxPort?.close();
    _isolate?.kill(priority: Isolate.beforeNextEvent);
    _image?.dispose();
    _image = null;
    super.dispose();
  }
}

// ---------------------------------------------------------------------------
// 3. Worker-Isolate Engine for fx-CG50 PRIZM (C++ File State + Dirty Frame)
// ---------------------------------------------------------------------------
class _CgWorkerInit {
  final SendPort mainPort;
  final String romDir;
  final String? libraryPath;
  final String warmSnapshotPath;
  final String liveSnapshotPath;
  final bool preferSnapshot;
  _CgWorkerInit(
    this.mainPort,
    this.romDir,
    this.libraryPath, {
    required this.warmSnapshotPath,
    required this.liveSnapshotPath,
    this.preferSnapshot = true,
  });
}

bool _importCgFile(CasioNativeBindings b, Pointer<Void> cg, String path) {
  if (path.isEmpty || !File(path).existsSync()) return false;
  final pPtr = b.allocCString(path);
  try {
    return b.cgImportStateFile(cg, pPtr) == 1;
  } finally {
    b.free(pPtr.cast());
  }
}

bool _exportCgFile(CasioNativeBindings b, Pointer<Void> cg, String path) {
  if (path.isEmpty) return false;
  final pPtr = b.allocCString(path);
  try {
    return b.cgExportStateFile(cg, pPtr) == 1;
  } finally {
    b.free(pPtr.cast());
  }
}

int _currentLocalUnixSeconds() {
  final now = DateTime.now();
  return (now.millisecondsSinceEpoch ~/ 1000) + now.timeZoneOffset.inSeconds;
}

bool _cgFileOp(
    CasioNativeBindings b, CasioFileOpDart fn, Pointer<Void> cg, String path) {
  if (path.isEmpty) return false;
  final pPtr = b.allocCString(path);
  try {
    return fn(cg, pPtr) == 1;
  } finally {
    b.free(pPtr.cast());
  }
}

void _cg50IsolateMain(_CgWorkerInit init) {
  final cmdPort = ReceivePort();
  init.mainPort.send({'type': 'port', 'port': cmdPort.sendPort});

  final b = CasioNativeBindings.open(init.libraryPath);
  final dirPtr = b.allocCString(init.romDir);
  Pointer<Void> cg = nullptr;
  try {
    cg = b.cgCreateFromDir(dirPtr);
  } finally {
    b.free(dirPtr.cast());
  }
  if (cg == nullptr) {
    init.mainPort.send({'type': 'error', 'message': 'No disponible'});
    cmdPort.close();
    return;
  }

  const int fbBytes = 384 * 216 * 4;
  final fbPtr = b.alloc(fbBytes).cast<Uint8>();
  final instPtr = b.alloc(8).cast<Uint64>();
  final cycPtr = b.alloc(8).cast<Uint64>();
  final pcPtr = b.alloc(4).cast<Uint32>();
  final lcdPtr = b.alloc(8).cast<Uint64>();

  bool bootedOnce = false;
  if (init.preferSnapshot) {
    bootedOnce = _importCgFile(b, cg, init.liveSnapshotPath) ||
        _importCgFile(b, cg, init.warmSnapshotPath);
    if (bootedOnce) {
      b.cgSetRtcEpoch(cg, _currentLocalUnixSeconds());
      b.cgStep(cg, 200000, 4000);
    }
  } else {
    b.cgSetRtcEpoch(cg, _currentLocalUnixSeconds());
  }

  bool running = true;
  bool paused = false;
  int sleepStreak = bootedOnce ? 4 : 0;
  int heldKeyCode = -1;
  int heldKeySteps = 0;
  bool releaseRequested = false;
  int idleCounter = 0;

  final List<Map<String, dynamic>> queue = [];
  Completer<void>? wakeup;

  void notifyWakeup() {
    if (wakeup != null && !wakeup!.isCompleted) {
      wakeup!.complete();
    }
  }

  cmdPort.listen((msg) {
    if (msg is Map<String, dynamic>) {
      queue.add(msg);
      notifyWakeup();
    }
  });

  void reportFrame(int state, {bool forceCopy = false}) {
    b.cgGetStats(cg, instPtr, cycPtr, pcPtr, lcdPtr);
    final hasFb = forceCopy
        ? b.cgRenderRgbaEx(cg, fbPtr, fbBytes, 1)
        : b.cgRenderRgba(cg, fbPtr, fbBytes);
    Uint8List? bytes;
    if (hasFb > 0) {
      bytes = Uint8List.fromList(fbPtr.asTypedList(fbBytes));
    }
    if (bytes != null || !bootedOnce) {
      init.mainPort.send({
        'type': 'frame',
        'state': state,
        'booted': bootedOnce,
        'inst': instPtr.value,
        'hasFb': bytes != null,
        'rgba':
            bytes != null ? TransferableTypedData.fromList([bytes]) : null,
      });
    }
  }

  if (bootedOnce) {
    reportFrame(2, forceCopy: true);
  }

  Future<void> pump() async {
    while (running) {
      while (queue.isNotEmpty) {
        final cmd = queue.removeAt(0);
        final op = cmd['op'] as String?;
        if (op == 'dispose') {
          running = false;
          break;
        } else if (op == 'pause') {
          paused = cmd['value'] as bool? ?? false;
          if (!paused) {
            b.cgSetRtcEpoch(cg, _currentLocalUnixSeconds());
            if (bootedOnce) reportFrame(2, forceCopy: true);
          }
        } else if (op == 'sync_rtc') {
          final int epoch =
              (cmd['epoch'] as int?) ?? _currentLocalUnixSeconds();
          b.cgSetRtcEpoch(cg, epoch);
        } else if (op == 'down') {
          final code = cmd['code'] as int;
          if (heldKeyCode >= 0 && heldKeyCode != code) {
            // Guarantee minimum KIU hold + clean release settle before rapid next key
            while (heldKeySteps < 5) {
              b.cgStep(cg, 1500000, 20000);
              heldKeySteps++;
            }
            b.cgKeyUp(cg, heldKeyCode);
            b.cgStep(cg, 1000000, 24000);
          }
          b.cgKeyDown(cg, code);
          heldKeyCode = code;
          heldKeySteps = 0;
          releaseRequested = false;
          b.cgStep(cg, 1200000, 20000);
        } else if (op == 'up') {
          final code = cmd['code'] as int;
          if (heldKeyCode == code) {
            releaseRequested = true;
          } else {
            b.cgKeyUp(cg, code);
          }
        } else if (op == 'reset') {
          final bool cold = cmd['cold'] as bool? ?? false;
          b.cgDestroy(cg);
          final dPtr = b.allocCString(init.romDir);
          try {
            cg = b.cgCreateFromDir(dPtr);
          } finally {
            b.free(dPtr.cast());
          }
          if (cg == nullptr) {
            running = false;
            init.mainPort.send({'type': 'error', 'message': 'No disponible'});
            break;
          }
          bootedOnce = !cold && _importCgFile(b, cg, init.warmSnapshotPath);
          sleepStreak = bootedOnce ? 4 : 0;
          heldKeyCode = -1;
          heldKeySteps = 0;
          releaseRequested = false;
          idleCounter = 2;
          if (bootedOnce) {
            b.cgSetRtcEpoch(cg, _currentLocalUnixSeconds());
            b.cgStep(cg, 200000, 4000);
            reportFrame(2, forceCopy: true);
          } else {
            b.cgSetRtcEpoch(cg, _currentLocalUnixSeconds());
          }
        } else if (op == 'save_state') {
          final int reqId = cmd['reqId'] as int? ?? 0;
          final String targetPath =
              (cmd['path'] as String?) ?? init.liveSnapshotPath;
          final bool ok = _exportCgFile(b, cg, targetPath);
          init.mainPort.send({'type': 'saved', 'reqId': reqId, 'ok': ok});
        } else if (op == 'export_user_rom') {
          final int reqId = cmd['reqId'] as int? ?? 0;
          final String targetPath = (cmd['path'] as String?) ?? '';
          final bool ok =
              _cgFileOp(b, b.cgExportUserRomFile, cg, targetPath);
          init.mainPort.send({'type': 'saved', 'reqId': reqId, 'ok': ok});
        } else if (op == 'import_user_rom') {
          final int reqId = cmd['reqId'] as int? ?? 0;
          final String srcPath = (cmd['path'] as String?) ?? '';
          final bool ok =
              _cgFileOp(b, b.cgImportUserRomFile, cg, srcPath);
          if (ok) {
            b.cgSetRtcEpoch(cg, _currentLocalUnixSeconds());
            bootedOnce = false;
            sleepStreak = 0;
            heldKeyCode = -1;
            heldKeySteps = 0;
            releaseRequested = false;
            idleCounter = 2;
          }
          init.mainPort.send({'type': 'saved', 'reqId': reqId, 'ok': ok});
        } else if (op == 'load_addin') {
          final int reqId = cmd['reqId'] as int? ?? 0;
          final String srcPath = (cmd['path'] as String?) ?? '';
          final bool ok = _cgFileOp(b, b.cgLoadAddinFile, cg, srcPath);
          init.mainPort.send({'type': 'saved', 'reqId': reqId, 'ok': ok});
        }
      }
      if (!running) break;

      if (paused && queue.isEmpty) {
        wakeup = Completer<void>();
        await wakeup!.future;
        wakeup = null;
        continue;
      }

      final int st = bootedOnce
          ? b.cgStep(cg, 1500000, heldKeyCode >= 0 ? 20000 : 12000)
          : b.cgStep(cg, 4000000, 10000);

      if (heldKeyCode >= 0) {
        heldKeySteps++;
        // Hold at least ~120 ms simulated (20 ms initial + 5 x 20 ms) so quick
        // touchscreen taps always satisfy the SH7305 KIU debounce scan window.
        if (releaseRequested && heldKeySteps >= 5) {
          b.cgKeyUp(cg, heldKeyCode);
          heldKeyCode = -1;
          heldKeySteps = 0;
          releaseRequested = false;
          b.cgStep(cg, 1500000, 24000);
        }
      }

      if (!bootedOnce) {
        if (st == 2) {
          sleepStreak++;
          if (sleepStreak >= 4) {
            bootedOnce = true;
          }
        } else {
          sleepStreak = 0;
        }
      }

      idleCounter++;
      if (bootedOnce || (idleCounter % 3 == 0)) {
        reportFrame(st);
      }

      if (bootedOnce && st == 2 && heldKeyCode < 0 && queue.isEmpty) {
        await Future<void>.delayed(const Duration(milliseconds: 28));
      } else {
        await Future<void>.delayed(Duration.zero);
      }
    }

    if (cg != nullptr) b.cgDestroy(cg);
    b.free(fbPtr.cast());
    b.free(instPtr.cast());
    b.free(cycPtr.cast());
    b.free(pcPtr.cast());
    b.free(lcdPtr.cast());
    cmdPort.close();
  }

  unawaited(pump());
}

class CasioCg50Engine extends CasioEngineController {
  @override
  CasioCalcKind get kind => CasioCalcKind.cg50;
  final String romDir;
  final String? libraryPath;
  final String warmSnapshotPath;
  final String liveSnapshotPath;
  final bool preferSnapshot;

  Isolate? _isolate;
  ReceivePort? _rxPort;
  SendPort? _txPort;
  bool _disposed = false;
  bool _ready = false;
  bool _hasValidFrame = false;
  bool _decoding = false;
  String _statusLabel = 'Iniciando...';
  ui.Image? _image;
  Uint8List? _lastRgba;
  int _nextReqId = 1;
  final Map<int, Completer<bool>> _saveWaiters = {};

  CasioCg50Engine({
    required this.romDir,
    required this.warmSnapshotPath,
    required this.liveSnapshotPath,
    this.libraryPath,
    this.preferSnapshot = true,
  });

  @override
  bool get ready => _ready;
  @override
  bool get hasValidFrame => _hasValidFrame;
  @override
  String get statusLabel => _statusLabel;
  @override
  int get indicatorsMask => 0;
  @override
  ui.Image? get lcdImage => _image;
  @override
  Uint8List? get lastRgbaBytes => _lastRgba;
  @override
  int get frameWidth => 384;
  @override
  int get frameHeight => 216;

  Future<void> start() async {
    _rxPort = ReceivePort();
    _rxPort!.listen(_onMessage);
    _isolate = await Isolate.spawn(
      _cg50IsolateMain,
      _CgWorkerInit(
        _rxPort!.sendPort,
        romDir,
        libraryPath,
        warmSnapshotPath: warmSnapshotPath,
        liveSnapshotPath: liveSnapshotPath,
        preferSnapshot: preferSnapshot,
      ),
    );
  }

  void _onMessage(dynamic msg) {
    if (_disposed || msg is! Map) return;
    final type = msg['type'] as String?;
    if (type == 'port') {
      _txPort = msg['port'] as SendPort;
      _ready = true;
      notifyListeners();
    } else if (type == 'error') {
      _statusLabel = msg['message'] as String? ?? 'No disponible';
      notifyListeners();
    } else if (type == 'saved') {
      final reqId = msg['reqId'] as int? ?? 0;
      _saveWaiters.remove(reqId)?.complete(msg['ok'] as bool? ?? false);
    } else if (type == 'frame') {
      final booted = msg['booted'] as bool? ?? false;
      _statusLabel = booted ? 'Lista' : 'Iniciando...';
      final hasFb = msg['hasFb'] as bool? ?? false;
      final xfer = msg['rgba'] as TransferableTypedData?;
      if (hasFb && xfer != null && !_decoding) {
        final bytes = xfer.materialize().asUint8List();
        _lastRgba = bytes;
        _decoding = true;
        ui.decodeImageFromPixels(bytes, 384, 216, ui.PixelFormat.rgba8888,
            (img) {
          _decoding = false;
          if (_disposed) {
            img.dispose();
            return;
          }
          _image?.dispose();
          _image = img;
          _hasValidFrame = true;
          notifyListeners();
        });
      } else if (!booted) {
        notifyListeners();
      }
    }
  }

  @override
  void pressDown(CasioKeyDef key) {
    _txPort?.send({'op': 'down', 'code': key.matrixCode});
  }

  @override
  void releaseUp(CasioKeyDef key) {
    _txPort?.send({'op': 'up', 'code': key.matrixCode});
  }

  @override
  void setPaused(bool paused) {
    _txPort?.send({'op': 'pause', 'value': paused});
  }

  @override
  Future<bool> savePersistentState() {
    return _sendFileRequest('save_state', liveSnapshotPath,
        timeout: const Duration(seconds: 6));
  }

  void syncRtcNow() {
    _txPort?.send({'op': 'sync_rtc', 'epoch': _currentLocalUnixSeconds()});
  }

  Future<bool> exportUserRomToFile(String path) {
    return _sendFileRequest('export_user_rom', path,
        timeout: const Duration(seconds: 12));
  }

  Future<bool> importUserRomFromFile(String path) {
    return _sendFileRequest('import_user_rom', path,
        timeout: const Duration(seconds: 12));
  }

  Future<bool> loadAddinFromFile(String path) {
    return _sendFileRequest('load_addin', path,
        timeout: const Duration(seconds: 8));
  }

  Future<bool> _sendFileRequest(
    String op,
    String path, {
    required Duration timeout,
  }) {
    final port = _txPort;
    if (!_ready || port == null || _disposed || path.isEmpty) {
      return Future.value(false);
    }
    final reqId = _nextReqId++;
    final c = Completer<bool>();
    _saveWaiters[reqId] = c;
    port.send({'op': op, 'reqId': reqId, 'path': path});
    return c.future.timeout(timeout, onTimeout: () {
      _saveWaiters.remove(reqId);
      return false;
    });
  }

  @override
  void resetEngine({bool cold = false}) {
    _hasValidFrame = false;
    _statusLabel = 'Iniciando...';
    notifyListeners();
    _txPort?.send({'op': 'reset', 'cold': cold});
  }

  @override
  void dispose() {
    _disposed = true;
    for (final w in _saveWaiters.values) {
      if (!w.isCompleted) w.complete(false);
    }
    _saveWaiters.clear();
    _txPort?.send({'op': 'dispose'});
    _rxPort?.close();
    _isolate?.kill(priority: Isolate.beforeNextEvent);
    _image?.dispose();
    _image = null;
    super.dispose();
  }
}
