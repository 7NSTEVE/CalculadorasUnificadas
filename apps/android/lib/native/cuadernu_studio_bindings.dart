import 'dart:convert';
import 'dart:ffi';
import 'dart:typed_data';

import 'casio_ffi_bindings.dart';

// ============================================================================
// Pure C ABI FFI signatures matching studio/c_api/cuadernu_studio_ffi.h
// Loaded from the same shared library (libcasio_native.so / casio_native.dll)
// ============================================================================

typedef _OutBufNative = Int32 Function(Pointer<Uint8> outBuf, Int32 outSize);
typedef _OutBufDart = int Function(Pointer<Uint8> outBuf, int outSize);

typedef _DocCreateNative = Pointer<Void> Function(Int32 mode);
typedef _DocCreateDart = Pointer<Void> Function(int mode);

typedef _DocVoidNative = Void Function(Pointer<Void> doc);
typedef _DocVoidDart = void Function(Pointer<Void> doc);

typedef _DocIntNative = Int32 Function(Pointer<Void> doc);
typedef _DocIntDart = int Function(Pointer<Void> doc);

typedef _DocIntArgNative = Int32 Function(Pointer<Void> doc, Int32 arg);
typedef _DocIntArgDart = int Function(Pointer<Void> doc, int arg);

typedef _DocStrArgNative = Int32 Function(Pointer<Void> doc, Pointer<Uint8> str);
typedef _DocStrArgDart = int Function(Pointer<Void> doc, Pointer<Uint8> str);

typedef _DocOutBufNative = Int32 Function(
    Pointer<Void> doc, Pointer<Uint8> outBuf, Int32 outSize);
typedef _DocOutBufDart = int Function(
    Pointer<Void> doc, Pointer<Uint8> outBuf, int outSize);

typedef _DocAddInkNative = Uint64 Function(
    Pointer<Void> doc,
    Int32 onCanvas,
    Int32 pageIndex,
    Pointer<Float> xyPressure,
    Int32 pointCount,
    Uint32 rgba,
    Double strokeWidth,
    Int32 penStyle,
    Int32 isHighlighter);
typedef _DocAddInkDart = int Function(
    Pointer<Void> doc,
    int onCanvas,
    int pageIndex,
    Pointer<Float> xyPressure,
    int pointCount,
    int rgba,
    double strokeWidth,
    int penStyle,
    int isHighlighter);

typedef _DocAddTextNative = Uint64 Function(
    Pointer<Void> doc,
    Int32 onCanvas,
    Int32 pageIndex,
    Double x,
    Double y,
    Double w,
    Double h,
    Pointer<Uint8> text,
    Double fontSizePt,
    Uint32 rgba);
typedef _DocAddTextDart = int Function(
    Pointer<Void> doc,
    int onCanvas,
    int pageIndex,
    double x,
    double y,
    double w,
    double h,
    Pointer<Uint8> text,
    double fontSizePt,
    int rgba);

typedef _DocAddShapeNative = Uint64 Function(
    Pointer<Void> doc,
    Int32 onCanvas,
    Int32 pageIndex,
    Int32 shapeKind,
    Double x,
    Double y,
    Double w,
    Double h,
    Uint32 strokeRgba,
    Uint32 fillRgba,
    Double strokeWidth);
typedef _DocAddShapeDart = int Function(
    Pointer<Void> doc,
    int onCanvas,
    int pageIndex,
    int shapeKind,
    double x,
    double y,
    double w,
    double h,
    int strokeRgba,
    int fillRgba,
    double strokeWidth);

typedef _DocAddMathNative = Uint64 Function(
    Pointer<Void> doc,
    Int32 onCanvas,
    Int32 pageIndex,
    Double x,
    Double y,
    Double w,
    Double h,
    Pointer<Uint8> expr,
    Int32 angleDegrees);
typedef _DocAddMathDart = int Function(
    Pointer<Void> doc,
    int onCanvas,
    int pageIndex,
    double x,
    double y,
    double w,
    double h,
    Pointer<Uint8> expr,
    int angleDegrees);

typedef _DocAddCasioCardNative = Uint64 Function(
    Pointer<Void> doc,
    Int32 onCanvas,
    Int32 pageIndex,
    Double x,
    Double y,
    Double w,
    Double h,
    Pointer<Uint8> model,
    Pointer<Uint8> expr,
    Pointer<Uint8> result,
    Pointer<Uint8> keySeq,
    Pointer<Uint8> pngBytes,
    IntPtr pngSize);
typedef _DocAddCasioCardDart = int Function(
    Pointer<Void> doc,
    int onCanvas,
    int pageIndex,
    double x,
    double y,
    double w,
    double h,
    Pointer<Uint8> model,
    Pointer<Uint8> expr,
    Pointer<Uint8> result,
    Pointer<Uint8> keySeq,
    Pointer<Uint8> pngBytes,
    int pngSize);

typedef _DocAddStickyNative = Uint64 Function(
    Pointer<Void> doc,
    Int32 onCanvas,
    Int32 pageIndex,
    Double x,
    Double y,
    Pointer<Uint8> text,
    Uint32 fillRgba);
typedef _DocAddStickyDart = int Function(
    Pointer<Void> doc,
    int onCanvas,
    int pageIndex,
    double x,
    double y,
    Pointer<Uint8> text,
    int fillRgba);

typedef _DocAddTapeNative = Uint64 Function(
    Pointer<Void> doc,
    Int32 onCanvas,
    Int32 pageIndex,
    Double x,
    Double y,
    Double w,
    Double h,
    Pointer<Uint8> label);
typedef _DocAddTapeDart = int Function(
    Pointer<Void> doc,
    int onCanvas,
    int pageIndex,
    double x,
    double y,
    double w,
    double h,
    Pointer<Uint8> label);

typedef _DocObjOpNative = Int32 Function(
    Pointer<Void> doc, Int32 onCanvas, Int32 pageIndex, Uint64 objectId);
typedef _DocObjOpDart = int Function(
    Pointer<Void> doc, int onCanvas, int pageIndex, int objectId);

typedef _DocEraseAtNative = Int32 Function(
    Pointer<Void> doc, Int32 onCanvas, Int32 pageIndex, Double x, Double y, Double r);
typedef _DocEraseAtDart = int Function(
    Pointer<Void> doc, int onCanvas, int pageIndex, double x, double y, double r);

typedef _DocAssetBytesNative = Int64 Function(
    Pointer<Void> doc, Pointer<Uint8> key, Pointer<Uint8> outBuf, IntPtr outSize);
typedef _DocAssetBytesDart = int Function(
    Pointer<Void> doc, Pointer<Uint8> key, Pointer<Uint8> outBuf, int outSize);

typedef _DocSearchNative = Int32 Function(
    Pointer<Void> doc,
    Pointer<Uint8> query,
    Int32 accentInsensitive,
    Pointer<Uint8> outJson,
    Int32 outSize);
typedef _DocSearchDart = int Function(
    Pointer<Void> doc,
    Pointer<Uint8> query,
    int accentInsensitive,
    Pointer<Uint8> outJson,
    int outSize);

typedef _DocExportDxfNative = Int32 Function(
    Pointer<Void> doc, Pointer<Uint8> path, Pointer<Uint8> scale);
typedef _DocExportDxfDart = int Function(
    Pointer<Void> doc, Pointer<Uint8> path, Pointer<Uint8> scale);

typedef _DocImportFileNative = Int32 Function(
    Pointer<Void> doc,
    Pointer<Uint8> path,
    Int32 targetInfiniteCanvas,
    Pointer<Uint8> outJson,
    Int32 outSize);
typedef _DocImportFileDart = int Function(
    Pointer<Void> doc,
    Pointer<Uint8> path,
    int targetInfiniteCanvas,
    Pointer<Uint8> outJson,
    int outSize);

typedef _CasEvalNative = Int32 Function(
    Pointer<Uint8> expr,
    Pointer<Uint8> envJson,
    Int32 angleDeg,
    Pointer<Uint8> outJson,
    Int32 outSize);
typedef _CasEvalDart = int Function(
    Pointer<Uint8> expr,
    Pointer<Uint8> envJson,
    int angleDeg,
    Pointer<Uint8> outJson,
    int outSize);

typedef _TwoStrOutNative = Int32 Function(
    Pointer<Uint8> a, Pointer<Uint8> b, Pointer<Uint8> outJson, Int32 outSize);
typedef _TwoStrOutDart = int Function(
    Pointer<Uint8> a, Pointer<Uint8> b, Pointer<Uint8> outJson, int outSize);

typedef _OneStrOutNative = Int32 Function(
    Pointer<Uint8> a, Pointer<Uint8> outJson, Int32 outSize);
typedef _OneStrOutDart = int Function(
    Pointer<Uint8> a, Pointer<Uint8> outJson, int outSize);

typedef _ThreeStrOutNative = Int32 Function(
    Pointer<Uint8> a,
    Pointer<Uint8> b,
    Pointer<Uint8> c,
    Pointer<Uint8> outJson,
    Int32 outSize);
typedef _ThreeStrOutDart = int Function(
    Pointer<Uint8> a,
    Pointer<Uint8> b,
    Pointer<Uint8> c,
    Pointer<Uint8> outJson,
    int outSize);

typedef _SamplePlotNative = Int32 Function(
    Pointer<Uint8> expr,
    Pointer<Uint8> varName,
    Double xMin,
    Double xMax,
    Int32 numSamples,
    Pointer<Uint8> outJson,
    Int32 outSize);
typedef _SamplePlotDart = int Function(
    Pointer<Uint8> expr,
    Pointer<Uint8> varName,
    double xMin,
    double xMax,
    int numSamples,
    Pointer<Uint8> outJson,
    int outSize);

typedef _CircuitSolveNative = Int32 Function(
    Pointer<Uint8> netlist,
    Pointer<Uint8> mode,
    Double param1,
    Double param2,
    Int32 probeNode,
    Pointer<Uint8> outJson,
    Int32 outSize);
typedef _CircuitSolveDart = int Function(
    Pointer<Uint8> netlist,
    Pointer<Uint8> mode,
    double param1,
    double param2,
    int probeNode,
    Pointer<Uint8> outJson,
    int outSize);

typedef _KMapNative = Int32 Function(
    Int32 numVars,
    Pointer<Uint8> minterms,
    Pointer<Uint8> dontcares,
    Pointer<Uint8> outJson,
    Int32 outSize);
typedef _KMapDart = int Function(
    int numVars,
    Pointer<Uint8> minterms,
    Pointer<Uint8> dontcares,
    Pointer<Uint8> outJson,
    int outSize);

typedef _Sm2Native = Int32 Function(
    Int32 reps,
    Int32 intervalDays,
    Double easeFactor,
    Int32 quality,
    Uint64 nowEpochSec,
    Pointer<Uint8> outJson,
    Int32 outSize);
typedef _Sm2Dart = int Function(
    int reps,
    int intervalDays,
    double easeFactor,
    int quality,
    int nowEpochSec,
    Pointer<Uint8> outJson,
    int outSize);

// ============================================================================
// Paper patterns and shape kinds matching studio/core/document_model.hpp
// ============================================================================
class CuadernuPaperPattern {
  static const int blank = 0;
  static const int ruled = 1;
  static const int grid = 2;
  static const int dotGrid = 3;
  static const int millimeter = 4;
  static const int isometric = 5;
  static const int logLog = 6;
  static const int polar = 7;
  static const int smith = 8;
  static const int cornell = 9;
  static const int musicStaff = 10;

  static const List<(int, String)> all = [
    (blank, 'Blanco'),
    (ruled, 'Rayado'),
    (grid, 'Cuadrícula'),
    (dotGrid, 'Puntos'),
    (millimeter, 'Milimetrado'),
    (isometric, 'Isométrico'),
    (logLog, 'Log-Log'),
    (polar, 'Polar'),
    (smith, 'Carta de Smith'),
    (cornell, 'Cornell'),
    (musicStaff, 'Pentagrama'),
  ];
}

class CuadernuShapeKind {
  static const int line = 0;
  static const int arrow = 1;
  static const int rectangle = 2;
  static const int ellipse = 3;
  static const int triangle = 4;
}

class CuadernuPenStyle {
  static const int ballpoint = 0;
  static const int fountain = 1;
  static const int brush = 2;
  static const int pencil = 3;
  static const int technical = 4;
}

/// Bindings to the shared C++17 CuadernU Studio engine bundled inside
/// `libcasio_native.so` / `casio_native.dll`.
class CuadernuStudioBindings {
  final CasioNativeBindings casio;
  final bool isAvailable;

  late final _OutBufDart _capabilitiesJson;
  late final _DocCreateDart _docCreate;
  late final _DocVoidDart _docDestroy;
  late final _DocStrArgDart _docSetTitle;
  late final _DocIntArgDart _docSetMode;
  late final _DocIntArgDart _docSetPaper;
  late final _DocIntArgDart _docAddPage;
  late final _DocIntDart _docPageCount;
  late final _DocAddInkDart _docAddInk;
  late final _DocAddTextDart _docAddText;
  late final _DocAddShapeDart _docAddShape;
  late final _DocAddMathDart _docAddMath;
  late final _DocAddCasioCardDart _docAddCasioCard;
  late final _DocAddStickyDart _docAddSticky;
  late final _DocAddTapeDart _docAddTape;
  late final _DocObjOpDart _docToggleTape;
  late final _DocEraseAtDart _docEraseAt;
  late final _DocObjOpDart _docRemoveObject;
  late final _DocIntDart _docUndo;
  late final _DocIntDart _docRedo;
  late final _DocIntDart _docCanUndo;
  late final _DocIntDart _docCanRedo;
  late final _DocOutBufDart _docGetSnapshotJson;
  late final _DocAssetBytesDart _docGetAssetBytes;
  late final _DocSearchDart _docSearchJson;
  late final _DocOutBufDart _docEvalMathDagJson;
  late final _DocStrArgDart _docSaveCudoc;
  late final _DocStrArgDart _docLoadCudoc;
  late final _DocStrArgDart _docExportPdf;
  late final _DocOutBufDart _docExportMarkdown;
  late final _DocExportDxfDart _docExportDxf;
  late final _DocImportFileDart _docImportFileJson;
  late final _DocOutBufDart _docBuildFlashcardsJson;

  late final _CasEvalDart _casEvalJson;
  late final _TwoStrOutDart _casSolveEqJson;
  late final _OneStrOutDart _casSolveSelJson;
  late final _OneStrOutDart _casMatrixEvalJson;
  late final _SamplePlotDart _casSamplePlotJson;
  late final _ThreeStrOutDart _casCheckStepJson;
  late final _OneStrOutDart _chemAnalyzeJson;
  late final _CircuitSolveDart _circuitSolveJson;
  late final _KMapDart _circuitKmapJson;
  late final _OneStrOutDart _graphBuildJson;
  late final _Sm2Dart _sm2ReviewJson;
  late final _TwoStrOutDart _horariosSolveJson;

  CuadernuStudioBindings._(this.casio, this.isAvailable) {
    if (!isAvailable) return;
    final lib = casio.lib;
    _capabilitiesJson =
        lib.lookupFunction<_OutBufNative, _OutBufDart>('cuadernu_ffi_capabilities_json');
    _docCreate =
        lib.lookupFunction<_DocCreateNative, _DocCreateDart>('cuadernu_doc_create');
    _docDestroy =
        lib.lookupFunction<_DocVoidNative, _DocVoidDart>('cuadernu_doc_destroy');
    _docSetTitle =
        lib.lookupFunction<_DocStrArgNative, _DocStrArgDart>('cuadernu_doc_set_title');
    _docSetMode =
        lib.lookupFunction<_DocIntArgNative, _DocIntArgDart>('cuadernu_doc_set_mode');
    _docSetPaper =
        lib.lookupFunction<_DocIntArgNative, _DocIntArgDart>('cuadernu_doc_set_paper');
    _docAddPage =
        lib.lookupFunction<_DocIntArgNative, _DocIntArgDart>('cuadernu_doc_add_page');
    _docPageCount =
        lib.lookupFunction<_DocIntNative, _DocIntDart>('cuadernu_doc_page_count');
    _docAddInk =
        lib.lookupFunction<_DocAddInkNative, _DocAddInkDart>('cuadernu_doc_add_ink_stroke');
    _docAddText =
        lib.lookupFunction<_DocAddTextNative, _DocAddTextDart>('cuadernu_doc_add_text_box');
    _docAddShape =
        lib.lookupFunction<_DocAddShapeNative, _DocAddShapeDart>('cuadernu_doc_add_shape');
    _docAddMath =
        lib.lookupFunction<_DocAddMathNative, _DocAddMathDart>('cuadernu_doc_add_math_block');
    _docAddCasioCard = lib.lookupFunction<_DocAddCasioCardNative, _DocAddCasioCardDart>(
        'cuadernu_doc_add_casio_card');
    _docAddSticky = lib.lookupFunction<_DocAddStickyNative, _DocAddStickyDart>(
        'cuadernu_doc_add_sticky_note');
    _docAddTape = lib.lookupFunction<_DocAddTapeNative, _DocAddTapeDart>(
        'cuadernu_doc_add_study_tape');
    _docToggleTape = lib.lookupFunction<_DocObjOpNative, _DocObjOpDart>(
        'cuadernu_doc_toggle_study_tape');
    _docEraseAt =
        lib.lookupFunction<_DocEraseAtNative, _DocEraseAtDart>('cuadernu_doc_erase_at');
    _docRemoveObject = lib.lookupFunction<_DocObjOpNative, _DocObjOpDart>(
        'cuadernu_doc_remove_object');
    _docUndo = lib.lookupFunction<_DocIntNative, _DocIntDart>('cuadernu_doc_undo');
    _docRedo = lib.lookupFunction<_DocIntNative, _DocIntDart>('cuadernu_doc_redo');
    _docCanUndo =
        lib.lookupFunction<_DocIntNative, _DocIntDart>('cuadernu_doc_can_undo');
    _docCanRedo =
        lib.lookupFunction<_DocIntNative, _DocIntDart>('cuadernu_doc_can_redo');
    _docGetSnapshotJson = lib.lookupFunction<_DocOutBufNative, _DocOutBufDart>(
        'cuadernu_doc_get_snapshot_json');
    _docGetAssetBytes = lib.lookupFunction<_DocAssetBytesNative, _DocAssetBytesDart>(
        'cuadernu_doc_get_asset_bytes');
    _docSearchJson = lib.lookupFunction<_DocSearchNative, _DocSearchDart>(
        'cuadernu_doc_search_json');
    _docEvalMathDagJson = lib.lookupFunction<_DocOutBufNative, _DocOutBufDart>(
        'cuadernu_doc_evaluate_math_dag_json');
    _docSaveCudoc =
        lib.lookupFunction<_DocStrArgNative, _DocStrArgDart>('cuadernu_doc_save_cudoc');
    _docLoadCudoc =
        lib.lookupFunction<_DocStrArgNative, _DocStrArgDart>('cuadernu_doc_load_cudoc');
    _docExportPdf =
        lib.lookupFunction<_DocStrArgNative, _DocStrArgDart>('cuadernu_doc_export_pdf');
    _docExportMarkdown = lib.lookupFunction<_DocOutBufNative, _DocOutBufDart>(
        'cuadernu_doc_export_markdown');
    _docExportDxf = lib.lookupFunction<_DocExportDxfNative, _DocExportDxfDart>(
        'cuadernu_doc_export_dxf');
    _docImportFileJson = lib.lookupFunction<_DocImportFileNative, _DocImportFileDart>(
        'cuadernu_doc_import_file_json');
    _docBuildFlashcardsJson = lib.lookupFunction<_DocOutBufNative, _DocOutBufDart>(
        'cuadernu_doc_build_flashcards_json');

    _casEvalJson =
        lib.lookupFunction<_CasEvalNative, _CasEvalDart>('cuadernu_cas_eval_json');
    _casSolveEqJson = lib.lookupFunction<_TwoStrOutNative, _TwoStrOutDart>(
        'cuadernu_cas_solve_equation_json');
    _casSolveSelJson = lib.lookupFunction<_OneStrOutNative, _OneStrOutDart>(
        'cuadernu_cas_solve_linear_system_json');
    _casMatrixEvalJson = lib.lookupFunction<_OneStrOutNative, _OneStrOutDart>(
        'cuadernu_cas_matrix_eval_json');
    _casSamplePlotJson = lib.lookupFunction<_SamplePlotNative, _SamplePlotDart>(
        'cuadernu_cas_sample_plot_json');
    _casCheckStepJson = lib.lookupFunction<_ThreeStrOutNative, _ThreeStrOutDart>(
        'cuadernu_cas_check_step_json');
    _chemAnalyzeJson = lib.lookupFunction<_OneStrOutNative, _OneStrOutDart>(
        'cuadernu_chem_analyze_json');
    _circuitSolveJson = lib.lookupFunction<_CircuitSolveNative, _CircuitSolveDart>(
        'cuadernu_circuit_solve_json');
    _circuitKmapJson =
        lib.lookupFunction<_KMapNative, _KMapDart>('cuadernu_circuit_kmap_json');
    _graphBuildJson = lib.lookupFunction<_OneStrOutNative, _OneStrOutDart>(
        'cuadernu_graph_build_json');
    _sm2ReviewJson =
        lib.lookupFunction<_Sm2Native, _Sm2Dart>('cuadernu_sm2_review_json');
    _horariosSolveJson = lib.lookupFunction<_TwoStrOutNative, _TwoStrOutDart>(
        'cuadernu_horarios_solve_json');
  }

  static CuadernuStudioBindings fromCasio(CasioNativeBindings casio) {
    bool available = false;
    try {
      casio.lib.lookup('cuadernu_ffi_capabilities_json');
      available = true;
    } catch (_) {
      available = false;
    }
    return CuadernuStudioBindings._(casio, available);
  }

  static CuadernuStudioBindings open([String? overridePath]) =>
      fromCasio(CasioNativeBindings.open(overridePath));

  T _withCString<T>(String value, T Function(Pointer<Uint8> ptr) fn) {
    final ptr = casio.allocCString(value);
    try {
      return fn(ptr);
    } finally {
      casio.free(ptr.cast());
    }
  }

  String _callStringBuffer(int Function(Pointer<Uint8> buf, int size) fn,
      {int initialSize = 16384}) {
    int cap = initialSize;
    for (int attempt = 0; attempt < 2; attempt++) {
      final buf = casio.alloc(cap).cast<Uint8>();
      try {
        final n = fn(buf, cap);
        if (n < 0) return '';
        if (n < cap) {
          return utf8.decode(buf.asTypedList(n), allowMalformed: true);
        }
        cap = n + 32;
      } finally {
        casio.free(buf.cast());
      }
    }
    return '';
  }

  dynamic _callJson(int Function(Pointer<Uint8> buf, int size) fn,
      {int initialSize = 16384}) {
    final raw = _callStringBuffer(fn, initialSize: initialSize);
    if (raw.isEmpty) return <String, dynamic>{};
    return jsonDecode(raw);
  }

  Map<String, dynamic> _callJsonMap(
      int Function(Pointer<Uint8> buf, int size) fn,
      {int initialSize = 16384}) {
    final decoded = _callJson(fn, initialSize: initialSize);
    if (decoded is Map<String, dynamic>) return decoded;
    return <String, dynamic>{};
  }

  List<Map<String, dynamic>> _callJsonList(
      int Function(Pointer<Uint8> buf, int size) fn,
      {int initialSize = 16384}) {
    final decoded = _callJson(fn, initialSize: initialSize);
    if (decoded is List) {
      return decoded
          .whereType<Map>()
          .map((m) => Map<String, dynamic>.from(m))
          .toList();
    }
    return const [];
  }

  Map<String, dynamic> capabilities() {
    if (!isAvailable) return {'available': false};
    return _callJsonMap((b, s) => _capabilitiesJson(b, s), initialSize: 4096);
  }

  CuadernuNativeDocument createDocument({bool infiniteCanvas = false}) {
    if (!isAvailable) {
      throw StateError('CuadernU Studio native FFI not available');
    }
    final h = _docCreate(infiniteCanvas ? 1 : 0);
    if (h.address == 0) {
      throw StateError('cuadernu_doc_create returned null');
    }
    return CuadernuNativeDocument._(this, h);
  }

  // ==========================================================================
  // Stateless Engineering CAS, Chemistry 118, MNA Circuits, K-Map, Graph, SM-2
  // ==========================================================================

  Map<String, dynamic> evalCas(
    String expr, {
    Map<String, dynamic>? env,
    bool angleDegrees = true,
  }) {
    if (!isAvailable) return {'ok': false, 'error': 'Motor nativo no disponible'};
    final envStr = env == null || env.isEmpty ? '{}' : jsonEncode(env);
    return _withCString(
      expr,
      (ePtr) => _withCString(
        envStr,
        (envPtr) => _callJsonMap(
          (b, s) => _casEvalJson(ePtr, envPtr, angleDegrees ? 1 : 0, b, s),
        ),
      ),
    );
  }

  Map<String, dynamic> solveEquation(String equation, {String varName = 'x'}) {
    if (!isAvailable) return {'ok': false, 'error': 'Motor nativo no disponible'};
    return _withCString(
      equation,
      (eqPtr) => _withCString(
        varName,
        (vPtr) => _callJsonMap((b, s) => _casSolveEqJson(eqPtr, vPtr, b, s)),
      ),
    );
  }

  Map<String, dynamic> solveLinearSystem(String equations) {
    if (!isAvailable) return {'ok': false, 'error': 'Motor nativo no disponible'};
    return _withCString(
      equations,
      (eqPtr) => _callJsonMap((b, s) => _casSolveSelJson(eqPtr, b, s)),
    );
  }

  Map<String, dynamic> evalMatrix(String expr) {
    if (!isAvailable) return {'ok': false, 'error': 'Motor nativo no disponible'};
    return _withCString(
      expr,
      (ePtr) => _callJsonMap((b, s) => _casMatrixEvalJson(ePtr, b, s)),
    );
  }

  Map<String, dynamic> samplePlot(
    String expr, {
    String varName = 'x',
    double xMin = -10.0,
    double xMax = 10.0,
    int numSamples = 200,
  }) {
    if (!isAvailable) return {'ok': false, 'error': 'Motor nativo no disponible'};
    return _withCString(
      expr,
      (ePtr) => _withCString(
        varName,
        (vPtr) => _callJsonMap(
          (b, s) => _casSamplePlotJson(ePtr, vPtr, xMin, xMax, numSamples, b, s),
          initialSize: 32768,
        ),
      ),
    );
  }

  Map<String, dynamic> checkMathStep(
    String prevExpr,
    String nextExpr, {
    String varName = 'x',
  }) {
    if (!isAvailable) return {'ok': false, 'error': 'Motor nativo no disponible'};
    return _withCString(
      prevExpr,
      (pPtr) => _withCString(
        nextExpr,
        (nPtr) => _withCString(
          varName,
          (vPtr) => _callJsonMap(
              (b, s) => _casCheckStepJson(pPtr, nPtr, vPtr, b, s)),
        ),
      ),
    );
  }

  Map<String, dynamic> analyzeChemistry(String formulaOrReaction) {
    if (!isAvailable) return {'ok': false, 'error': 'Motor nativo no disponible'};
    return _withCString(
      formulaOrReaction,
      (fPtr) => _callJsonMap((b, s) => _chemAnalyzeJson(fPtr, b, s)),
    );
  }

  Map<String, dynamic> solveCircuit(
    String netlist, {
    String mode = 'dc',
    double param1 = 1000.0,
    double param2 = 0.0,
    int probeNode = 1,
  }) {
    if (!isAvailable) return {'ok': false, 'error': 'Motor nativo no disponible'};
    return _withCString(
      netlist,
      (nPtr) => _withCString(
        mode,
        (mPtr) => _callJsonMap(
          (b, s) => _circuitSolveJson(nPtr, mPtr, param1, param2, probeNode, b, s),
          initialSize: 32768,
        ),
      ),
    );
  }

  Map<String, dynamic> simplifyKMap(
    int numVars,
    String mintermsCsv, {
    String dontCaresCsv = '',
  }) {
    if (!isAvailable) return {'ok': false, 'error': 'Motor nativo no disponible'};
    return _withCString(
      mintermsCsv,
      (mPtr) => _withCString(
        dontCaresCsv,
        (dPtr) => _callJsonMap(
          (b, s) => _circuitKmapJson(numVars, mPtr, dPtr, b, s),
        ),
      ),
    );
  }

  Map<String, dynamic> buildKnowledgeGraph(List<Map<String, dynamic>> notes) {
    if (!isAvailable) return {'nodes': [], 'edges': [], 'brokenLinks': []};
    final jsonStr = jsonEncode(notes);
    return _withCString(
      jsonStr,
      (ptr) => _callJsonMap(
        (b, s) => _graphBuildJson(ptr, b, s),
        initialSize: 32768,
      ),
    );
  }

  Map<String, dynamic> reviewSm2({
    required int repetitions,
    required int intervalDays,
    required double easeFactor,
    required int quality,
    int? nowEpochSec,
  }) {
    if (!isAvailable) return {'ok': false};
    final now = nowEpochSec ?? (DateTime.now().millisecondsSinceEpoch ~/ 1000);
    return _callJsonMap(
      (b, s) => _sm2ReviewJson(
          repetitions, intervalDays, easeFactor, quality, now, b, s),
      initialSize: 2048,
    );
  }

  Map<String, dynamic> solveHorariosTec(
    Map<String, dynamic> catalog, {
    List<Map<String, dynamic>> busySlots = const [],
  }) {
    if (!isAvailable) return {'ok': false, 'solutions': []};
    final catStr = jsonEncode(catalog);
    final busyStr = jsonEncode(busySlots);
    return _withCString(
      catStr,
      (cPtr) => _withCString(
        busyStr,
        (bPtr) => _callJsonMap(
          (b, s) => _horariosSolveJson(cPtr, bPtr, b, s),
          initialSize: 65536,
        ),
      ),
    );
  }
}

/// Stateful handle wrapping a C++17 `CuadernuDocHandle*` (.cudoc binary format,
/// infinite canvas + paginated pages, CommandStack undo/redo, ink, MathDAG,
/// Casio session cards, vector PDF/Markdown/DXF export, multi-format import).
class CuadernuNativeDocument {
  final CuadernuStudioBindings api;
  Pointer<Void> _handle;
  bool _disposed = false;

  CuadernuNativeDocument._(this.api, this._handle);

  bool get isDisposed => _disposed;

  void _checkAlive() {
    if (_disposed || _handle.address == 0) {
      throw StateError('CuadernuNativeDocument already disposed');
    }
  }

  void dispose() {
    if (!_disposed && _handle.address != 0) {
      api._docDestroy(_handle);
      _handle = nullptr;
      _disposed = true;
    }
  }

  void setTitle(String title) {
    _checkAlive();
    api._withCString(title, (p) => api._docSetTitle(_handle, p));
  }

  void setMode({required bool infiniteCanvas}) {
    _checkAlive();
    api._docSetMode(_handle, infiniteCanvas ? 1 : 0);
  }

  void setPaper(int paperPattern) {
    _checkAlive();
    api._docSetPaper(_handle, paperPattern);
  }

  int addPage(int paperPattern) {
    _checkAlive();
    return api._docAddPage(_handle, paperPattern);
  }

  int get pageCount {
    _checkAlive();
    return api._docPageCount(_handle);
  }

  int addInkStroke({
    required bool onCanvas,
    required int pageIndex,
    required List<({double x, double y, double p})> points,
    required int rgba,
    required double strokeWidth,
    int penStyle = CuadernuPenStyle.ballpoint,
    bool isHighlighter = false,
  }) {
    _checkAlive();
    if (points.isEmpty) return 0;
    final floats = api.casio.alloc(points.length * 3 * 4).cast<Float>();
    try {
      final list = floats.asTypedList(points.length * 3);
      for (int i = 0; i < points.length; i++) {
        list[i * 3] = points[i].x;
        list[i * 3 + 1] = points[i].y;
        list[i * 3 + 2] = points[i].p;
      }
      return api._docAddInk(
        _handle,
        onCanvas ? 1 : 0,
        pageIndex,
        floats,
        points.length,
        rgba,
        strokeWidth,
        penStyle,
        isHighlighter ? 1 : 0,
      );
    } finally {
      api.casio.free(floats.cast());
    }
  }

  int addTextBox({
    required bool onCanvas,
    required int pageIndex,
    required double x,
    required double y,
    required double w,
    required double h,
    required String text,
    double fontSizePt = 14.0,
    int rgba = 0x1D1D1FFF,
  }) {
    _checkAlive();
    return api._withCString(
      text,
      (p) => api._docAddText(
          _handle, onCanvas ? 1 : 0, pageIndex, x, y, w, h, p, fontSizePt, rgba),
    );
  }

  int addShape({
    required bool onCanvas,
    required int pageIndex,
    required int shapeKind,
    required double x,
    required double y,
    required double w,
    required double h,
    required int strokeRgba,
    int fillRgba = 0,
    double strokeWidth = 2.0,
  }) {
    _checkAlive();
    return api._docAddShape(
      _handle,
      onCanvas ? 1 : 0,
      pageIndex,
      shapeKind,
      x,
      y,
      w,
      h,
      strokeRgba,
      fillRgba,
      strokeWidth,
    );
  }

  int addMathBlock({
    required bool onCanvas,
    required int pageIndex,
    required double x,
    required double y,
    required double w,
    required double h,
    required String expr,
    bool angleDegrees = true,
  }) {
    _checkAlive();
    return api._withCString(
      expr,
      (p) => api._docAddMath(
        _handle,
        onCanvas ? 1 : 0,
        pageIndex,
        x,
        y,
        w,
        h,
        p,
        angleDegrees ? 1 : 0,
      ),
    );
  }

  int addCasioCard({
    required bool onCanvas,
    required int pageIndex,
    required double x,
    required double y,
    required double w,
    required double h,
    required String calcModel,
    required String expr,
    required String result,
    required String keySeq,
    Uint8List? pngBytes,
  }) {
    _checkAlive();
    Pointer<Uint8> pngPtr = nullptr;
    final int pngLen = pngBytes?.length ?? 0;
    if (pngBytes != null && pngLen > 0) {
      pngPtr = api.casio.alloc(pngLen).cast<Uint8>();
      pngPtr.asTypedList(pngLen).setAll(0, pngBytes);
    }
    try {
      return api._withCString(
        calcModel,
        (mPtr) => api._withCString(
          expr,
          (ePtr) => api._withCString(
            result,
            (rPtr) => api._withCString(
              keySeq,
              (kPtr) => api._docAddCasioCard(
                _handle,
                onCanvas ? 1 : 0,
                pageIndex,
                x,
                y,
                w,
                h,
                mPtr,
                ePtr,
                rPtr,
                kPtr,
                pngPtr,
                pngLen,
              ),
            ),
          ),
        ),
      );
    } finally {
      if (pngPtr.address != 0) api.casio.free(pngPtr.cast());
    }
  }

  int addStickyNote({
    required bool onCanvas,
    required int pageIndex,
    required double x,
    required double y,
    required String text,
    int fillRgba = 0xFFF3B0FF,
  }) {
    _checkAlive();
    return api._withCString(
      text,
      (p) => api._docAddSticky(
          _handle, onCanvas ? 1 : 0, pageIndex, x, y, p, fillRgba),
    );
  }

  int addStudyTape({
    required bool onCanvas,
    required int pageIndex,
    required double x,
    required double y,
    required double w,
    required double h,
    required String label,
  }) {
    _checkAlive();
    return api._withCString(
      label,
      (p) => api._docAddTape(
          _handle, onCanvas ? 1 : 0, pageIndex, x, y, w, h, p),
    );
  }

  bool toggleStudyTape({
    required bool onCanvas,
    required int pageIndex,
    required int objectId,
  }) {
    _checkAlive();
    return api._docToggleTape(
            _handle, onCanvas ? 1 : 0, pageIndex, objectId) !=
        0;
  }

  int eraseAt({
    required bool onCanvas,
    required int pageIndex,
    required double x,
    required double y,
    required double radius,
  }) {
    _checkAlive();
    return api._docEraseAt(
        _handle, onCanvas ? 1 : 0, pageIndex, x, y, radius);
  }

  bool removeObject({
    required bool onCanvas,
    required int pageIndex,
    required int objectId,
  }) {
    _checkAlive();
    return api._docRemoveObject(
            _handle, onCanvas ? 1 : 0, pageIndex, objectId) !=
        0;
  }

  bool undo() {
    _checkAlive();
    return api._docUndo(_handle) != 0;
  }

  bool redo() {
    _checkAlive();
    return api._docRedo(_handle) != 0;
  }

  bool get canUndo {
    _checkAlive();
    return api._docCanUndo(_handle) != 0;
  }

  bool get canRedo {
    _checkAlive();
    return api._docCanRedo(_handle) != 0;
  }

  Map<String, dynamic> getSnapshot() {
    _checkAlive();
    return api._callJsonMap(
      (b, s) => api._docGetSnapshotJson(_handle, b, s),
      initialSize: 65536,
    );
  }

  Uint8List? getAssetBytes(String assetKey) {
    _checkAlive();
    return api._withCString(assetKey, (kPtr) {
      final sz = api._docGetAssetBytes(_handle, kPtr, nullptr, 0);
      if (sz <= 0) return null;
      final buf = api.casio.alloc(sz).cast<Uint8>();
      try {
        final written = api._docGetAssetBytes(_handle, kPtr, buf, sz);
        if (written <= 0) return null;
        return Uint8List.fromList(buf.asTypedList(written));
      } finally {
        api.casio.free(buf.cast());
      }
    });
  }

  List<Map<String, dynamic>> search(String query,
      {bool accentInsensitive = true}) {
    _checkAlive();
    return api._withCString(
      query,
      (qPtr) => api._callJsonList(
        (b, s) => api._docSearchJson(
            _handle, qPtr, accentInsensitive ? 1 : 0, b, s),
      ),
    );
  }

  Map<String, dynamic> evaluateMathDag() {
    _checkAlive();
    return api._callJsonMap(
      (b, s) => api._docEvalMathDagJson(_handle, b, s),
    );
  }

  bool saveCudoc(String path) {
    _checkAlive();
    return api._withCString(path, (p) => api._docSaveCudoc(_handle, p)) != 0;
  }

  bool loadCudoc(String path) {
    _checkAlive();
    return api._withCString(path, (p) => api._docLoadCudoc(_handle, p)) != 0;
  }

  bool exportPdf(String path) {
    _checkAlive();
    return api._withCString(path, (p) => api._docExportPdf(_handle, p)) != 0;
  }

  String exportMarkdown() {
    _checkAlive();
    return api._callStringBuffer(
      (b, s) => api._docExportMarkdown(_handle, b, s),
      initialSize: 32768,
    );
  }

  bool exportDxf(String path, {String scaleRatio = '1:1'}) {
    _checkAlive();
    return api._withCString(
          path,
          (pPtr) => api._withCString(
            scaleRatio,
            (sPtr) => api._docExportDxf(_handle, pPtr, sPtr),
          ),
        ) !=
        0;
  }

  Map<String, dynamic> importFile(String path,
      {bool targetInfiniteCanvas = false}) {
    _checkAlive();
    return api._withCString(
      path,
      (pPtr) => api._callJsonMap(
        (b, s) => api._docImportFileJson(
            _handle, pPtr, targetInfiniteCanvas ? 1 : 0, b, s),
      ),
    );
  }

  List<Map<String, dynamic>> buildFlashcards() {
    _checkAlive();
    final map = api._callJsonMap(
      (b, s) => api._docBuildFlashcardsJson(_handle, b, s),
    );
    final cards = map['cards'];
    if (cards is List) {
      return cards
          .whereType<Map>()
          .map((e) => Map<String, dynamic>.from(e))
          .toList();
    }
    return const [];
  }
}
