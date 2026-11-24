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
typedef _CgSmemListNative = Int32 Function(
    Pointer<Void> h, Pointer<Uint8> path, Pointer<Uint8> outJson, Int32 outSize);
typedef CasioCgSmemListDart = int Function(
    Pointer<Void> h, Pointer<Uint8> path, Pointer<Uint8> outJson, int outSize);
typedef _CgSmemReadNative = Int64 Function(
    Pointer<Void> h, Pointer<Uint8> path, Pointer<Uint8> outBuf, IntPtr outSize);
typedef CasioCgSmemReadDart = int Function(
    Pointer<Void> h, Pointer<Uint8> path, Pointer<Uint8> outBuf, int outSize);
typedef _CgSmemWriteNative = Int32 Function(
    Pointer<Void> h, Pointer<Uint8> path, Pointer<Uint8> data, IntPtr size, Int32 overwrite);
typedef CasioCgSmemWriteDart = int Function(
    Pointer<Void> h, Pointer<Uint8> path, Pointer<Uint8> data, int size, int overwrite);
typedef _CgSmemDeleteNative = Int32 Function(
    Pointer<Void> h, Pointer<Uint8> path, Int32 recursive);
typedef CasioCgSmemDeleteDart = int Function(
    Pointer<Void> h, Pointer<Uint8> path, int recursive);
typedef _CgBoolSetNative = Int32 Function(Pointer<Void> h, Int32 flag);
typedef CasioCgBoolSetDart = int Function(Pointer<Void> h, int flag);
typedef _CgStringQueryNative = Int32 Function(
    Pointer<Void> h, Pointer<Uint8> outBuf, Int32 outSize);
typedef CasioCgStringQueryDart = int Function(
    Pointer<Void> h, Pointer<Uint8> outBuf, int outSize);
typedef _CgLinkBufNative = Int32 Function(
    Pointer<Void> h, Pointer<Uint8> buf, IntPtr len);
typedef CasioCgLinkBufDart = int Function(
    Pointer<Void> h, Pointer<Uint8> buf, int len);

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
  late final CasioCgSmemListDart cgSmemListJson;
  late final CasioCgSmemReadDart cgSmemReadFile;
  late final CasioCgSmemWriteDart cgSmemWriteFile;
  late final CasioCgSmemDeleteDart cgSmemDelete;
  late final CasioFileOpDart cgSmemMkdir;
  late final CasioIntQueryDart cgSmemRemountOs;
  late final CasioCgBoolSetDart cgUsbSetConnected;
  late final CasioCgStringQueryDart cgUsbGetStatusJson;
  late final CasioCgBoolSetDart cgLinkSetEnabled;
  late final CasioCgLinkBufDart cgLinkPushRx;
  late final CasioCgLinkBufDart cgLinkPopTx;
  late final CasioCgStringQueryDart cgDecodeLcdQr;

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
    cgSmemListJson =
        lib.lookupFunction<_CgSmemListNative, CasioCgSmemListDart>(
            'casio_cg50_smem_list_json');
    cgSmemReadFile =
        lib.lookupFunction<_CgSmemReadNative, CasioCgSmemReadDart>(
            'casio_cg50_smem_read_file');
    cgSmemWriteFile =
        lib.lookupFunction<_CgSmemWriteNative, CasioCgSmemWriteDart>(
            'casio_cg50_smem_write_file');
    cgSmemDelete =
        lib.lookupFunction<_CgSmemDeleteNative, CasioCgSmemDeleteDart>(
            'casio_cg50_smem_delete');
    cgSmemMkdir = lib.lookupFunction<_FileOpNative, CasioFileOpDart>(
        'casio_cg50_smem_mkdir');
    cgSmemRemountOs = lib.lookupFunction<_IntQueryNative, CasioIntQueryDart>(
        'casio_cg50_smem_remount_os');
    cgUsbSetConnected =
        lib.lookupFunction<_CgBoolSetNative, CasioCgBoolSetDart>(
            'casio_cg50_usb_set_connected');
    cgUsbGetStatusJson =
        lib.lookupFunction<_CgStringQueryNative, CasioCgStringQueryDart>(
            'casio_cg50_usb_get_status_json');
    cgLinkSetEnabled =
        lib.lookupFunction<_CgBoolSetNative, CasioCgBoolSetDart>(
            'casio_cg50_link_set_enabled');
    cgLinkPushRx = lib.lookupFunction<_CgLinkBufNative, CasioCgLinkBufDart>(
        'casio_cg50_link_push_rx');
    cgLinkPopTx = lib.lookupFunction<_CgLinkBufNative, CasioCgLinkBufDart>(
        'casio_cg50_link_pop_tx');
    cgDecodeLcdQr =
        lib.lookupFunction<_CgStringQueryNative, CasioCgStringQueryDart>(
            'casio_cg50_decode_lcd_qr');
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
    final warmStamp = File('${dir.path}/cg50_warm_v44.stamp');
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
          await _writeAtomic(warmStamp, utf8.encode('v44-sh7305-dsp-cache-nohle'));
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
  void pressCodeDown(int matrixCode);
  void releaseCodeUp(int matrixCode);
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
  void pressCodeDown(int matrixCode) {
    _txPort?.send({'op': 'down', 'code': matrixCode});
  }

  @override
  void releaseCodeUp(int matrixCode) {
    _txPort?.send({'op': 'up', 'code': matrixCode});
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
  void pressCodeDown(int matrixCode) {
    _txPort?.send({'op': 'down', 'code': matrixCode});
  }

  @override
  void releaseCodeUp(int matrixCode) {
    _txPort?.send({'op': 'up', 'code': matrixCode});
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

class Cg50SmemEntry {
  final String name;
  final String shortName;
  final String path;
  final bool isDir;
  final int size;
  final int modDate;
  final int modTime;

  const Cg50SmemEntry({
    required this.name,
    required this.shortName,
    required this.path,
    required this.isDir,
    required this.size,
    required this.modDate,
    required this.modTime,
  });

  factory Cg50SmemEntry.fromJson(Map<String, dynamic> j) {
    return Cg50SmemEntry(
      name: j['name'] as String? ?? '',
      shortName: j['shortName'] as String? ?? '',
      path: j['path'] as String? ?? '/',
      isDir: j['isDir'] as bool? ?? false,
      size: (j['size'] as num?)?.toInt() ?? 0,
      modDate: (j['modDate'] as num?)?.toInt() ?? 0,
      modTime: (j['modTime'] as num?)?.toInt() ?? 0,
    );
  }

  Map<String, dynamic> toJson() => {
        'name': name,
        'shortName': shortName,
        'path': path,
        'isDir': isDir,
        'size': size,
        'modDate': modDate,
        'modTime': modTime,
      };
}

class Cg50SmemListing {
  final String path;
  final String volumeLabel;
  final int totalBytes;
  final int freeBytes;
  final int usedBytes;
  final int clusterSize;
  final bool fatEqual;
  final List<Cg50SmemEntry> entries;

  const Cg50SmemListing({
    required this.path,
    required this.volumeLabel,
    required this.totalBytes,
    required this.freeBytes,
    required this.usedBytes,
    required this.clusterSize,
    required this.fatEqual,
    required this.entries,
  });

  factory Cg50SmemListing.fromJson(Map<String, dynamic> j) {
    final rawList = (j['entries'] as List<dynamic>?) ?? const [];
    return Cg50SmemListing(
      path: j['path'] as String? ?? '/',
      volumeLabel: j['volumeLabel'] as String? ?? '',
      totalBytes: (j['totalBytes'] as num?)?.toInt() ?? 0,
      freeBytes: (j['freeBytes'] as num?)?.toInt() ?? 0,
      usedBytes: (j['usedBytes'] as num?)?.toInt() ?? 0,
      clusterSize: (j['clusterSize'] as num?)?.toInt() ?? 4096,
      fatEqual: j['fatEqual'] as bool? ?? false,
      entries: rawList
          .whereType<Map<String, dynamic>>()
          .map(Cg50SmemEntry.fromJson)
          .toList(),
    );
  }

  Map<String, dynamic> toJson() => {
        'path': path,
        'volumeLabel': volumeLabel,
        'totalBytes': totalBytes,
        'freeBytes': freeBytes,
        'usedBytes': usedBytes,
        'clusterSize': clusterSize,
        'fatEqual': fatEqual,
        'entries': entries.map((e) => e.toJson()).toList(),
      };
}

class Cg50UsbStatus {
  final bool connected;
  final bool vbus;
  final bool configured;
  final int deviceAddress;
  final int vid;
  final int pid;
  final String mode;
  final bool linkEnabled;

  const Cg50UsbStatus({
    required this.connected,
    required this.vbus,
    required this.configured,
    required this.deviceAddress,
    required this.vid,
    required this.pid,
    required this.mode,
    required this.linkEnabled,
  });

  factory Cg50UsbStatus.fromJson(Map<String, dynamic> j) {
    return Cg50UsbStatus(
      connected: j['connected'] as bool? ?? false,
      vbus: j['vbus'] as bool? ?? false,
      configured: j['configured'] as bool? ?? false,
      deviceAddress: (j['deviceAddress'] as num?)?.toInt() ?? 0,
      vid: (j['vid'] as num?)?.toInt() ?? 0x07CF,
      pid: (j['pid'] as num?)?.toInt() ?? 0x6102,
      mode: j['mode'] as String? ?? 'disconnected',
      linkEnabled: j['linkEnabled'] as bool? ?? false,
    );
  }

  Map<String, dynamic> toJson() => {
        'connected': connected,
        'vbus': vbus,
        'configured': configured,
        'deviceAddress': deviceAddress,
        'vid': vid,
        'pid': pid,
        'mode': mode,
        'linkEnabled': linkEnabled,
      };
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
  final linkTxPtr = b.alloc(1024).cast<Uint8>();

  bool bootedOnce = false;
  bool linkEnabled = false;
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
        } else if (op == 'smem_list') {
          final int reqId = cmd['reqId'] as int? ?? 0;
          final String path = (cmd['path'] as String?) ?? '/';
          final pPtr = b.allocCString(path);
          String? jsonOut;
          try {
            final needed = b.cgSmemListJson(cg, pPtr, nullptr, 0);
            if (needed > 0) {
              final buf = b.alloc(needed + 16).cast<Uint8>();
              try {
                final got = b.cgSmemListJson(cg, pPtr, buf, needed + 16);
                if (got >= 0) {
                  jsonOut = utf8.decode(buf.asTypedList(got), allowMalformed: true);
                }
              } finally {
                b.free(buf.cast());
              }
            }
          } finally {
            b.free(pPtr.cast());
          }
          init.mainPort.send({'type': 'rpc', 'reqId': reqId, 'data': jsonOut});
        } else if (op == 'smem_read') {
          final int reqId = cmd['reqId'] as int? ?? 0;
          final String path = (cmd['path'] as String?) ?? '';
          final pPtr = b.allocCString(path);
          TransferableTypedData? xfer;
          try {
            final sz = b.cgSmemReadFile(cg, pPtr, nullptr, 0);
            if (sz >= 0) {
              if (sz == 0) {
                xfer = TransferableTypedData.fromList([Uint8List(0)]);
              } else {
                final buf = b.alloc(sz).cast<Uint8>();
                try {
                  final got = b.cgSmemReadFile(cg, pPtr, buf, sz);
                  if (got == sz) {
                    xfer = TransferableTypedData.fromList(
                        [Uint8List.fromList(buf.asTypedList(sz))]);
                  }
                } finally {
                  b.free(buf.cast());
                }
              }
            }
          } finally {
            b.free(pPtr.cast());
          }
          init.mainPort.send({'type': 'rpc', 'reqId': reqId, 'data': xfer});
        } else if (op == 'smem_write') {
          final int reqId = cmd['reqId'] as int? ?? 0;
          final String path = (cmd['path'] as String?) ?? '';
          final xfer = cmd['bytes'] as TransferableTypedData?;
          final bool overwrite = cmd['overwrite'] as bool? ?? true;
          final bool remount = cmd['remountOs'] as bool? ?? false;
          bool ok = false;
          if (xfer != null && path.isNotEmpty) {
            final bytes = xfer.materialize().asUint8List();
            final pPtr = b.allocCString(path);
            final dPtr = b.alloc(bytes.isEmpty ? 1 : bytes.length).cast<Uint8>();
            try {
              if (bytes.isNotEmpty) {
                dPtr.asTypedList(bytes.length).setAll(0, bytes);
              }
              ok = b.cgSmemWriteFile(
                      cg, pPtr, dPtr, bytes.length, overwrite ? 1 : 0) ==
                  1;
            } finally {
              b.free(pPtr.cast());
              b.free(dPtr.cast());
            }
          }
          if (ok && remount) {
            b.cgSmemRemountOs(cg);
            b.cgSetRtcEpoch(cg, _currentLocalUnixSeconds());
            bootedOnce = false;
            sleepStreak = 0;
            heldKeyCode = -1;
            heldKeySteps = 0;
            releaseRequested = false;
            idleCounter = 2;
          }
          init.mainPort.send({'type': 'rpc', 'reqId': reqId, 'data': ok});
        } else if (op == 'smem_delete') {
          final int reqId = cmd['reqId'] as int? ?? 0;
          final String path = (cmd['path'] as String?) ?? '';
          final bool recursive = cmd['recursive'] as bool? ?? false;
          final bool remount = cmd['remountOs'] as bool? ?? false;
          bool ok = false;
          if (path.isNotEmpty) {
            final pPtr = b.allocCString(path);
            try {
              ok = b.cgSmemDelete(cg, pPtr, recursive ? 1 : 0) == 1;
            } finally {
              b.free(pPtr.cast());
            }
          }
          if (ok && remount) {
            b.cgSmemRemountOs(cg);
            b.cgSetRtcEpoch(cg, _currentLocalUnixSeconds());
            bootedOnce = false;
            sleepStreak = 0;
            heldKeyCode = -1;
            heldKeySteps = 0;
            releaseRequested = false;
            idleCounter = 2;
          }
          init.mainPort.send({'type': 'rpc', 'reqId': reqId, 'data': ok});
        } else if (op == 'smem_mkdir') {
          final int reqId = cmd['reqId'] as int? ?? 0;
          final String path = (cmd['path'] as String?) ?? '';
          final bool remount = cmd['remountOs'] as bool? ?? false;
          final bool ok = _cgFileOp(b, b.cgSmemMkdir, cg, path);
          if (ok && remount) {
            b.cgSmemRemountOs(cg);
            b.cgSetRtcEpoch(cg, _currentLocalUnixSeconds());
            bootedOnce = false;
            sleepStreak = 0;
            heldKeyCode = -1;
            heldKeySteps = 0;
            releaseRequested = false;
            idleCounter = 2;
          }
          init.mainPort.send({'type': 'rpc', 'reqId': reqId, 'data': ok});
        } else if (op == 'smem_remount') {
          final int reqId = cmd['reqId'] as int? ?? 0;
          final bool ok = b.cgSmemRemountOs(cg) == 1;
          if (ok) {
            b.cgSetRtcEpoch(cg, _currentLocalUnixSeconds());
            bootedOnce = false;
            sleepStreak = 0;
            heldKeyCode = -1;
            heldKeySteps = 0;
            releaseRequested = false;
            idleCounter = 2;
          }
          init.mainPort.send({'type': 'rpc', 'reqId': reqId, 'data': ok});
        } else if (op == 'usb_set') {
          final int reqId = cmd['reqId'] as int? ?? 0;
          final bool conn = cmd['connected'] as bool? ?? false;
          final bool ok = b.cgUsbSetConnected(cg, conn ? 1 : 0) == 1;
          init.mainPort.send({'type': 'rpc', 'reqId': reqId, 'data': ok});
        } else if (op == 'usb_status') {
          final int reqId = cmd['reqId'] as int? ?? 0;
          final buf = b.alloc(1024).cast<Uint8>();
          String? jsonOut;
          try {
            final got = b.cgUsbGetStatusJson(cg, buf, 1024);
            if (got > 0 && got < 1024) {
              jsonOut = utf8.decode(buf.asTypedList(got), allowMalformed: true);
            }
          } finally {
            b.free(buf.cast());
          }
          init.mainPort.send({'type': 'rpc', 'reqId': reqId, 'data': jsonOut});
        } else if (op == 'link_set') {
          final int reqId = cmd['reqId'] as int? ?? 0;
          final bool en = cmd['enabled'] as bool? ?? false;
          final bool ok = b.cgLinkSetEnabled(cg, en ? 1 : 0) == 1;
          if (ok) linkEnabled = en;
          init.mainPort.send({'type': 'rpc', 'reqId': reqId, 'data': ok});
        } else if (op == 'link_push_rx') {
          final int reqId = cmd['reqId'] as int? ?? 0;
          final xfer = cmd['bytes'] as TransferableTypedData?;
          int pushed = 0;
          if (xfer != null) {
            final bytes = xfer.materialize().asUint8List();
            if (bytes.isNotEmpty) {
              final dPtr = b.alloc(bytes.length).cast<Uint8>();
              try {
                dPtr.asTypedList(bytes.length).setAll(0, bytes);
                pushed = b.cgLinkPushRx(cg, dPtr, bytes.length);
              } finally {
                b.free(dPtr.cast());
              }
            }
          }
          init.mainPort.send({'type': 'rpc', 'reqId': reqId, 'data': pushed});
        } else if (op == 'decode_qr') {
          final int reqId = cmd['reqId'] as int? ?? 0;
          final buf = b.alloc(2048).cast<Uint8>();
          String? decoded;
          try {
            final got = b.cgDecodeLcdQr(cg, buf, 2048);
            if (got > 0 && got < 2048) {
              decoded = utf8.decode(buf.asTypedList(got), allowMalformed: true);
            }
          } finally {
            b.free(buf.cast());
          }
          init.mainPort.send({'type': 'rpc', 'reqId': reqId, 'data': decoded});
        } else if (op == 'export_state_bytes') {
          final int reqId = cmd['reqId'] as int? ?? 0;
          TransferableTypedData? xfer;
          final needed = b.cgExportState(cg, nullptr, 0);
          if (needed > 0) {
            final buf = b.alloc(needed).cast<Uint8>();
            try {
              final got = b.cgExportState(cg, buf, needed);
              if (got == needed) {
                xfer = TransferableTypedData.fromList(
                    [Uint8List.fromList(buf.asTypedList(needed))]);
              }
            } finally {
              b.free(buf.cast());
            }
          }
          init.mainPort.send({'type': 'rpc', 'reqId': reqId, 'data': xfer});
        } else if (op == 'import_state_bytes') {
          final int reqId = cmd['reqId'] as int? ?? 0;
          final xfer = cmd['bytes'] as TransferableTypedData?;
          bool ok = false;
          if (xfer != null) {
            final bytes = xfer.materialize().asUint8List();
            if (bytes.isNotEmpty) {
              final buf = b.alloc(bytes.length).cast<Uint8>();
              try {
                buf.asTypedList(bytes.length).setAll(0, bytes);
                ok = b.cgImportState(cg, buf, bytes.length) == 1;
              } finally {
                b.free(buf.cast());
              }
            }
          }
          if (ok) {
            b.cgSetRtcEpoch(cg, _currentLocalUnixSeconds());
            bootedOnce = true;
            sleepStreak = 4;
            heldKeyCode = -1;
            heldKeySteps = 0;
            releaseRequested = false;
            reportFrame(2, forceCopy: true);
          }
          init.mainPort.send({'type': 'rpc', 'reqId': reqId, 'data': ok});
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

      if (linkEnabled) {
        final txLen = b.cgLinkPopTx(cg, linkTxPtr, 1024);
        if (txLen > 0) {
          final copy = Uint8List.fromList(linkTxPtr.asTypedList(txLen));
          init.mainPort.send({
            'type': 'link_tx',
            'bytes': TransferableTypedData.fromList([copy]),
          });
        }
      }

      idleCounter++;
      if (bootedOnce || (idleCounter % 3 == 0)) {
        reportFrame(st);
      }

      if (bootedOnce && st == 2 && heldKeyCode < 0 && queue.isEmpty) {
        await Future<void>.delayed(Duration(milliseconds: linkEnabled ? 12 : 28));
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
    b.free(linkTxPtr.cast());
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
  final Map<int, Completer<dynamic>> _rpcWaiters = {};
  final StreamController<Uint8List> _linkTxController =
      StreamController<Uint8List>.broadcast();

  Stream<Uint8List> get onLinkTx => _linkTxController.stream;

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
    } else if (type == 'rpc') {
      final reqId = msg['reqId'] as int? ?? 0;
      _rpcWaiters.remove(reqId)?.complete(msg['data']);
    } else if (type == 'link_tx') {
      final xfer = msg['bytes'] as TransferableTypedData?;
      if (xfer != null && !_linkTxController.isClosed) {
        _linkTxController.add(xfer.materialize().asUint8List());
      }
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
  void pressCodeDown(int matrixCode) {
    _txPort?.send({'op': 'down', 'code': matrixCode});
  }

  @override
  void releaseCodeUp(int matrixCode) {
    _txPort?.send({'op': 'up', 'code': matrixCode});
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

  Future<dynamic> _sendRpc(
    Map<String, dynamic> payload, {
    Duration timeout = const Duration(seconds: 10),
  }) {
    final port = _txPort;
    if (!_ready || port == null || _disposed) {
      return Future.value(null);
    }
    final reqId = _nextReqId++;
    final c = Completer<dynamic>();
    _rpcWaiters[reqId] = c;
    port.send({...payload, 'reqId': reqId});
    return c.future.timeout(timeout, onTimeout: () {
      _rpcWaiters.remove(reqId);
      return null;
    });
  }

  Future<Cg50SmemListing?> listSmem(String path) async {
    final res = await _sendRpc({'op': 'smem_list', 'path': path});
    if (res is! String || res.isEmpty) return null;
    try {
      final decoded = jsonDecode(res);
      if (decoded is Map<String, dynamic>) {
        return Cg50SmemListing.fromJson(decoded);
      }
    } catch (_) {}
    return null;
  }

  Future<Uint8List?> readSmemFile(String path) async {
    final res = await _sendRpc({'op': 'smem_read', 'path': path},
        timeout: const Duration(seconds: 15));
    if (res is TransferableTypedData) {
      return res.materialize().asUint8List();
    }
    return null;
  }

  Future<bool> writeSmemFile(
    String path,
    Uint8List data, {
    bool overwrite = true,
    bool remountOs = false,
  }) async {
    final res = await _sendRpc(
      {
        'op': 'smem_write',
        'path': path,
        'bytes': TransferableTypedData.fromList([data]),
        'overwrite': overwrite,
        'remountOs': remountOs,
      },
      timeout: const Duration(seconds: 15),
    );
    return res == true;
  }

  Future<bool> deleteSmemPath(
    String path, {
    bool recursive = false,
    bool remountOs = false,
  }) async {
    final res = await _sendRpc({
      'op': 'smem_delete',
      'path': path,
      'recursive': recursive,
      'remountOs': remountOs,
    });
    return res == true;
  }

  Future<bool> mkdirSmem(String path, {bool remountOs = false}) async {
    final res = await _sendRpc({
      'op': 'smem_mkdir',
      'path': path,
      'remountOs': remountOs,
    });
    return res == true;
  }

  Future<bool> remountSmemOs() async {
    final res = await _sendRpc({'op': 'smem_remount'});
    return res == true;
  }

  Future<bool> setUsbConnected(bool connected) async {
    final res = await _sendRpc({'op': 'usb_set', 'connected': connected});
    return res == true;
  }

  Future<Cg50UsbStatus?> getUsbStatus() async {
    final res = await _sendRpc({'op': 'usb_status'});
    if (res is! String || res.isEmpty) return null;
    try {
      final decoded = jsonDecode(res);
      if (decoded is Map<String, dynamic>) {
        return Cg50UsbStatus.fromJson(decoded);
      }
    } catch (_) {}
    return null;
  }

  Future<bool> setLinkEnabled(bool enabled) async {
    final res = await _sendRpc({'op': 'link_set', 'enabled': enabled});
    return res == true;
  }

  Future<int> linkPushRx(Uint8List data) async {
    final res = await _sendRpc({
      'op': 'link_push_rx',
      'bytes': TransferableTypedData.fromList([data]),
    });
    return (res is int) ? res : 0;
  }

  Future<String?> decodeLcdQr() async {
    final res = await _sendRpc({'op': 'decode_qr'});
    return (res is String && res.isNotEmpty) ? res : null;
  }

  Future<Uint8List?> exportStateBytes() async {
    final res = await _sendRpc({'op': 'export_state_bytes'},
        timeout: const Duration(seconds: 12));
    if (res is TransferableTypedData) {
      return res.materialize().asUint8List();
    }
    return null;
  }

  Future<bool> importStateBytes(Uint8List bytes) async {
    final res = await _sendRpc(
      {
        'op': 'import_state_bytes',
        'bytes': TransferableTypedData.fromList([bytes]),
      },
      timeout: const Duration(seconds: 12),
    );
    return res == true;
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
    for (final w in _rpcWaiters.values) {
      if (!w.isCompleted) w.complete(null);
    }
    _rpcWaiters.clear();
    unawaited(_linkTxController.close());
    _txPort?.send({'op': 'dispose'});
    _rxPort?.close();
    _isolate?.kill(priority: Isolate.beforeNextEvent);
    _image?.dispose();
    _image = null;
    super.dispose();
  }
}
