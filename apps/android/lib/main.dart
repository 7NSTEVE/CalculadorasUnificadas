import 'dart:async';
import 'dart:io';

import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:path_provider/path_provider.dart';

import 'connectivity/cg50_local_bridge_server.dart';
import 'cuadernu/cuadernu_editor_view.dart';
import 'cuadernu/cuadernu_knowledge_study_view.dart';
import 'cuadernu/cuadernu_models.dart';
import 'cuadernu/cuadernu_stem_lab_view.dart';
import 'keylog/cg50_keylog.dart';
import 'native/calc_models.dart';
import 'native/casio_ffi_bindings.dart';
import 'native/cuadernu_studio_bindings.dart';
import 'theme/tema_interfaz.dart';
import 'ui/cg50_capture_and_lcd_sheets.dart';
import 'ui/cg50_connectivity_sheet.dart';
import 'ui/cg50_keylog_sheet.dart';
import 'ui/native_calculator_view.dart';

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  runApp(const CuadernUApp());
}

/// Primary unified CuadernU application (Notebooks + Infinite Canvas + STEM Lab
/// + Knowledge/SM-2 Study + 4 native Casio v47 calculators).
class CuadernUApp extends CalculadorasUnificadasApp {
  const CuadernUApp({
    super.key,
    super.disableNativeBoot = false,
    super.initialTheme = AppThemePreset.porcelainLight,
    super.initialTab = CuadernuWorkspaceTab.notebook,
  });
}

class CalculadorasUnificadasApp extends StatefulWidget {
  final bool disableNativeBoot;
  final AppThemePreset initialTheme;
  final CuadernuWorkspaceTab initialTab;

  const CalculadorasUnificadasApp({
    super.key,
    this.disableNativeBoot = false,
    this.initialTheme = AppThemePreset.porcelainLight,
    this.initialTab = CuadernuWorkspaceTab.calculators,
  });

  @override
  State<CalculadorasUnificadasApp> createState() =>
      _CalculadorasUnificadasAppState();
}

class _CalculadorasUnificadasAppState extends State<CalculadorasUnificadasApp> {
  late AppThemePreset _themePreset;
  bool _reduceTransparency = false;

  @override
  void initState() {
    super.initState();
    _themePreset = widget.initialTheme;
  }

  @override
  Widget build(BuildContext context) {
    final tokens = TokensInterfaz.of(_themePreset);
    return MaterialApp(
      title: 'CuadernU',
      debugShowCheckedModeBanner: false,
      theme: tokens.toThemeData(),
      home: CalculadorasHomeScreen(
        disableNativeBoot: widget.disableNativeBoot,
        initialTab: widget.initialTab,
        tokens: tokens,
        themePreset: _themePreset,
        reduceTransparency: _reduceTransparency,
        onThemeChanged: (p) => setState(() => _themePreset = p),
        onReduceTransparencyChanged: (v) =>
            setState(() => _reduceTransparency = v),
      ),
    );
  }
}

class CalculadorasHomeScreen extends StatefulWidget {
  final bool disableNativeBoot;
  final CuadernuWorkspaceTab initialTab;
  final TokensInterfaz tokens;
  final AppThemePreset themePreset;
  final bool reduceTransparency;
  final ValueChanged<AppThemePreset> onThemeChanged;
  final ValueChanged<bool> onReduceTransparencyChanged;

  const CalculadorasHomeScreen({
    super.key,
    required this.disableNativeBoot,
    required this.tokens,
    required this.themePreset,
    required this.reduceTransparency,
    required this.onThemeChanged,
    required this.onReduceTransparencyChanged,
    this.initialTab = CuadernuWorkspaceTab.notebook,
  });

  @override
  State<CalculadorasHomeScreen> createState() => _CalculadorasHomeScreenState();
}

class _CalculadorasHomeScreenState extends State<CalculadorasHomeScreen>
    with WidgetsBindingObserver {
  late CuadernuWorkspaceTab _activeTab;
  bool _dockedCalcOnNotebook = false;

  CasioCalcKind _activeKind = CasioCalcKind.cg50;
  CasioCalcKind _secondaryKind = CasioCalcKind.cw2;
  bool _sideBySide = false;

  bool _staging = true;
  String? _stageError;
  String? _runtimeDir;
  String? _libraryPath;

  CuadernuStudioBindings? _studio;
  final CuadernuEditorController _editorCtrl = CuadernuEditorController();

  final Map<CasioCalcKind, CasioEngineController> _engines = {};
  final Set<CasioCalcKind> _booting = {};
  final Set<CasioCalcKind> _dirtyEngines = {};
  final Set<CasioCalcKind> _savingEngines = {};
  final List<CapturedHistoryEntry> _history = [];
  final Cg50KeyLogController _keyLog = Cg50KeyLogController();
  Cg50LocalBridgeServer? _bridgeServer;
  Cg50LinkPeerClient? _linkClient;
  bool _checkingQr = false;

  @override
  void initState() {
    super.initState();
    _activeTab = widget.initialTab;
    WidgetsBinding.instance.addObserver(this);
    if (!widget.disableNativeBoot) {
      unawaited(_initRuntime());
    } else {
      _staging = false;
    }
  }

  Future<bool> _persistEngine(CasioCalcKind kind, {bool force = false}) async {
    final engine = _engines[kind];
    if (engine == null) return false;
    if (!force && !_dirtyEngines.contains(kind)) return true;
    if (_savingEngines.contains(kind)) return true;
    _dirtyEngines.remove(kind);
    _savingEngines.add(kind);
    try {
      final ok = await engine.savePersistentState();
      if (!ok) _dirtyEngines.add(kind);
      return ok;
    } finally {
      _savingEngines.remove(kind);
    }
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    if (state == AppLifecycleState.inactive ||
        state == AppLifecycleState.paused ||
        state == AppLifecycleState.hidden ||
        state == AppLifecycleState.detached) {
      _editorCtrl.saveCudoc();
      for (final entry in _engines.entries) {
        entry.value.setPaused(true);
        if (_dirtyEngines.contains(entry.key)) {
          unawaited(_persistEngine(entry.key));
        }
      }
    } else if (state == AppLifecycleState.resumed) {
      _syncEnginePauseStates();
    }
  }

  Future<void> _initRuntime() async {
    try {
      final dir = await CasioRomStaging.ensureStaged();
      final bindings = CasioNativeBindings.open();
      final studio = CuadernuStudioBindings.fromCasio(bindings);
      if (studio.isAvailable) {
        await _editorCtrl.initialize(studio);
      }
      if (!mounted) return;
      setState(() {
        _runtimeDir = dir;
        _libraryPath = bindings.libraryPath;
        _studio = studio;
        _staging = false;
      });
      await _ensureEngineStarted(_activeKind);
      _syncEnginePauseStates();
    } catch (e) {
      if (!mounted) return;
      setState(() {
        _staging = false;
        _stageError = 'No se pudieron preparar los recursos: $e';
      });
    }
  }

  Future<void> _ensureEngineStarted(CasioCalcKind kind) async {
    if (widget.disableNativeBoot || _runtimeDir == null) return;
    if (_engines.containsKey(kind) || _booting.contains(kind)) return;
    _booting.add(kind);
    final dir = _runtimeDir!;
    CasioEngineController engine;
    switch (kind) {
      case CasioCalcKind.cg50:
        final cg = CasioCg50Engine(
          romDir: dir,
          libraryPath: _libraryPath,
          warmSnapshotPath: '$dir/cg50_warm_state.bin',
          liveSnapshotPath: '$dir/cg50_live_state.bin',
        );
        await cg.start();
        final backend = Cg50EngineBridgeBackend(cg);
        _bridgeServer = Cg50LocalBridgeServer(backend);
        _linkClient = Cg50LinkPeerClient(backend);
        engine = cg;
        break;
      case CasioCalcKind.cw2:
        final cw2 = CasioCw2Engine(
          romPath: '$dir/fx991cw_rom.bin',
          defaultRamPath: '$dir/fx991cw_default_en.ram',
          liveRamPath: '$dir/fx991cw_live.ram',
          libraryPath: _libraryPath,
        );
        await cw2.start();
        engine = cw2;
        break;
      case CasioCalcKind.classwiz:
        final cw = CasioNxEngine(
          kind: CasioCalcKind.classwiz,
          romPath: '$dir/fx991lax.rom',
          statePath: '$dir/nx_classwiz_live.snp',
          libraryPath: _libraryPath,
        );
        await cw.start();
        engine = cw;
        break;
      case CasioCalcKind.naturalEs:
        final vn = CasioNxEngine(
          kind: CasioCalcKind.naturalEs,
          romPath: '$dir/fx570vnplus.rom',
          statePath: '$dir/nx_esplus_live.snp',
          libraryPath: _libraryPath,
        );
        await vn.start();
        engine = vn;
        break;
    }

    _booting.remove(kind);
    if (!mounted) {
      engine.dispose();
      return;
    }
    setState(() => _engines[kind] = engine);
    _syncEnginePauseStates();
  }

  Set<CasioCalcKind> _visibleKinds(double width) {
    if (_activeTab == CuadernuWorkspaceTab.calculators) {
      if (_sideBySide && width >= 720) return {_activeKind, _secondaryKind};
      return {_activeKind};
    }
    if (_activeTab == CuadernuWorkspaceTab.notebook &&
        _dockedCalcOnNotebook &&
        width >= 720) {
      return {_activeKind};
    }
    return const {};
  }

  void _syncEnginePauseStates([double? width]) {
    final w = width ?? MediaQuery.maybeOf(context)?.size.width ?? 0;
    final visible = _visibleKinds(w);
    for (final entry in _engines.entries) {
      final isVis = visible.contains(entry.key);
      entry.value.setPaused(!isVis);
      if (!isVis && _dirtyEngines.contains(entry.key)) {
        unawaited(_persistEngine(entry.key));
      }
    }
  }

  void _selectTab(CuadernuWorkspaceTab tab) {
    if (tab == _activeTab) return;
    setState(() => _activeTab = tab);
    if (tab == CuadernuWorkspaceTab.calculators) {
      unawaited(_ensureEngineStarted(_activeKind));
    }
    _syncEnginePauseStates();
  }

  void _selectCalculator(CasioCalcKind kind) {
    if (kind == _activeKind) return;
    setState(() {
      if (_sideBySide && kind == _secondaryKind) {
        _secondaryKind = _activeKind;
      }
      _activeKind = kind;
    });
    unawaited(_ensureEngineStarted(kind));
    _syncEnginePauseStates();
  }

  void _toggleSideBySide() {
    setState(() {
      _sideBySide = !_sideBySide;
      if (_secondaryKind == _activeKind) {
        _secondaryKind = _activeKind == CasioCalcKind.cg50
            ? CasioCalcKind.cw2
            : CasioCalcKind.cg50;
      }
    });
    if (_sideBySide) {
      unawaited(_ensureEngineStarted(_secondaryKind));
    }
    _syncEnginePauseStates();
  }

  void _toggleDockedCalculator() {
    setState(() => _dockedCalcOnNotebook = !_dockedCalcOnNotebook);
    if (_dockedCalcOnNotebook) {
      unawaited(_ensureEngineStarted(_activeKind));
    }
    _syncEnginePauseStates();
  }

  void _sendActiveCalcToCuadernu() {
    if (!_editorCtrl.isReady) {
      _showStatusSnack('Cuaderno aún preparándose');
      return;
    }
    final engine = _engines[_activeKind];
    final skin = skinForKind(_activeKind);
    Uint8List? png;
    if (engine != null &&
        engine.lastRgbaBytes != null &&
        engine.frameWidth > 0 &&
        engine.frameHeight > 0) {
      final composed = composeScaledCapture(
        srcRgba: engine.lastRgbaBytes!,
        srcW: engine.frameWidth,
        srcH: engine.frameHeight,
        scale: 2,
        includeFrame: true,
      );
      png = encodeRgbaToPng(composed.rgba, composed.width, composed.height);
    }
    final recentKeys = _history
        .where((h) => h.kind == _activeKind)
        .take(12)
        .map((h) => h.keyLabel)
        .join(' ');
    _editorCtrl.insertCasioCaptureCard(
      calcModel: skin.title,
      expr: recentKeys.isNotEmpty ? recentKeys : 'Captura de pantalla',
      result: '',
      keySeq: recentKeys,
      pngBytes: png,
    );
    _showStatusSnack('Pantalla de ${skin.title} insertada en ${_editorCtrl.title}');
  }

  void _onKeyLogged(CasioKeyDef key, CasioEngineController engine) {
    _dirtyEngines.add(engine.kind);
    if (engine.kind == CasioCalcKind.cg50 && _keyLog.isRecording) {
      _keyLog.recordKeyPress(key);
    }
    final skin = skinForKind(engine.kind);
    final bool eval =
        key.name == 'IGUAL' || key.name == 'EXE' || key.name == 'OK';
    setState(() {
      _history.add(
        CapturedHistoryEntry(
          kind: engine.kind,
          calcTitle: skin.title,
          keyName: key.name,
          keyLabel: key.displayLabel,
          isEvalKey: eval,
          timestamp: DateTime.now(),
          indicatorsMask: engine.indicatorsMask,
          isCw2Indicators: skin.isCw2IndicatorLayout,
          pixelWidth: engine.frameWidth,
          pixelHeight: engine.frameHeight,
          rgbaBytes: engine.lastRgbaBytes,
          lcdBg: skin.lcdBackground,
          lcdFg: skin.lcdForeground,
        ),
      );
      if (_history.length > 60) _history.removeAt(0);
    });
  }

  void _openHistorySheet() {
    showModalBottomSheet<void>(
      context: context,
      isScrollControlled: true,
      backgroundColor: Colors.transparent,
      builder: (_) => StatefulBuilder(
        builder: (ctx, setSheetState) => CasioHistorySheet(
          entries: _history,
          tokens: widget.tokens,
          reduceTransparency: widget.reduceTransparency,
          onClear: () {
            setState(() => _history.clear());
            setSheetState(() {});
          },
        ),
      ),
    );
  }

  static const MethodChannel _fileChannel =
      MethodChannel('com.calculadorasunificadas.app/files');

  void _showStatusSnack(String message) {
    if (!mounted) return;
    ScaffoldMessenger.of(context)
      ..hideCurrentSnackBar()
      ..showSnackBar(
        SnackBar(
          duration: const Duration(seconds: 3),
          content: Text(message),
        ),
      );
  }

  String _timestampTag() {
    final now = DateTime.now();
    String two(int n) => n.toString().padLeft(2, '0');
    return '${now.year}${two(now.month)}${two(now.day)}_${two(now.hour)}${two(now.minute)}${two(now.second)}';
  }

  Future<bool> _saveViaSystemDialog({
    required String suggestedName,
    required String mimeType,
    required String sourcePath,
  }) async {
    if (Platform.isAndroid) {
      try {
        final saved = await _fileChannel.invokeMethod<bool>('saveFile', {
          'suggestedName': suggestedName,
          'mimeType': mimeType,
          'sourcePath': sourcePath,
        });
        return saved ?? false;
      } catch (_) {
        return false;
      }
    }
    final docs = await getApplicationDocumentsDirectory();
    final dst = File('${docs.path}/$suggestedName');
    await File(sourcePath).copy(dst.path);
    return true;
  }

  Future<String?> _pickViaSystemDialog({
    required String mimeType,
    required String prefix,
  }) async {
    if (!Platform.isAndroid) return null;
    try {
      return await _fileChannel.invokeMethod<String>('pickFile', {
        'mimeType': mimeType,
        'prefix': prefix,
      });
    } catch (_) {
      return null;
    }
  }

  Future<Map<String, String>?> _pickFileWithMeta({
    String mimeType = '*/*',
    String prefix = 'picked',
  }) async {
    if (!Platform.isAndroid) return null;
    try {
      final raw = await _fileChannel.invokeMethod<dynamic>('pickFileWithMeta', {
        'mimeType': mimeType,
        'prefix': prefix,
      });
      if (raw is Map) {
        final path = raw['path']?.toString() ?? '';
        final name = raw['name']?.toString() ?? 'archivo.bin';
        if (path.isNotEmpty) return {'path': path, 'name': name};
      }
    } catch (_) {}
    return null;
  }

  Future<bool> _copyImageToClipboard({
    required String sourcePath,
    required String fileName,
  }) async {
    if (!Platform.isAndroid) return false;
    try {
      final ok = await _fileChannel.invokeMethod<bool>('copyImageToClipboard', {
        'sourcePath': sourcePath,
        'fileName': fileName,
      });
      return ok ?? false;
    } catch (_) {
      return false;
    }
  }

  Future<bool> _openExternalUrl(String url) async {
    if (!Platform.isAndroid) return false;
    try {
      final ok = await _fileChannel.invokeMethod<bool>('openExternalUrl', {
        'url': url,
      });
      return ok ?? false;
    } catch (_) {
      return false;
    }
  }

  Future<void> _scanAndOpenQr({bool silentWhenAbsent = false}) async {
    if (_checkingQr) return;
    final engine = _engines[CasioCalcKind.cg50];
    if (engine is! CasioCg50Engine) return;
    _checkingQr = true;
    try {
      final url = await engine.decodeLcdQr();
      if (!mounted) return;
      if (url == null || url.isEmpty) {
        if (!silentWhenAbsent) {
          _showStatusSnack('No se detectó un código QR en la pantalla');
        }
        return;
      }
      final action = await showDialog<String>(
        context: context,
        builder: (ctx) => AlertDialog(
          backgroundColor: widget.tokens.surface,
          title: Text(
            'Código QR detectado',
            style: TextStyle(
                color: widget.tokens.textPrimary,
                fontSize: 17,
                fontWeight: FontWeight.w600),
          ),
          content: SelectableText(
            url,
            style: TextStyle(color: widget.tokens.accent, fontSize: 13.5),
          ),
          actions: [
            TextButton(
              onPressed: () => Navigator.of(ctx).pop('copy'),
              child: const Text('Copiar'),
            ),
            if (url.startsWith('http://') || url.startsWith('https://'))
              FilledButton(
                style: FilledButton.styleFrom(
                  backgroundColor: widget.tokens.accent,
                  foregroundColor: Colors.white,
                ),
                onPressed: () => Navigator.of(ctx).pop('open'),
                child: const Text('Abrir'),
              )
            else
              TextButton(
                onPressed: () => Navigator.of(ctx).pop(null),
                child: const Text('Cerrar'),
              ),
          ],
        ),
      );
      if (action == 'copy') {
        await Clipboard.setData(ClipboardData(text: url));
        _showStatusSnack('Enlace QR copiado');
      } else if (action == 'open') {
        final ok = await _openExternalUrl(url);
        if (!ok) _showStatusSnack('No se pudo abrir el navegador');
      }
    } finally {
      _checkingQr = false;
    }
  }

  void _openConnectivitySheet() {
    final engine = _engines[CasioCalcKind.cg50];
    final srv = _bridgeServer;
    final cli = _linkClient;
    if (engine is! CasioCg50Engine || srv == null || cli == null) {
      _showStatusSnack('Calculadora fx-CG50 aún iniciando');
      return;
    }
    showModalBottomSheet<void>(
      context: context,
      isScrollControlled: true,
      backgroundColor: Colors.transparent,
      builder: (_) => Cg50ConnectivitySheet(
        engine: engine,
        bridgeServer: srv,
        linkClient: cli,
        tokens: widget.tokens,
        onPickFileWithMeta: _pickFileWithMeta,
        onSaveFile: _saveViaSystemDialog,
        onExportFullMemory: _exportCg50Memory,
        onImportFullMemory: _importCg50Memory,
        onNotify: _showStatusSnack,
      ),
    );
  }

  void _openKeyLogSheet() {
    final engine = _engines[CasioCalcKind.cg50];
    if (engine is! CasioCg50Engine) {
      _showStatusSnack('Calculadora fx-CG50 aún iniciando');
      return;
    }
    showModalBottomSheet<void>(
      context: context,
      isScrollControlled: true,
      backgroundColor: Colors.transparent,
      builder: (_) => Cg50KeyLogSheet(
        engine: engine,
        controller: _keyLog,
        tokens: widget.tokens,
        onPickFileWithMeta: _pickFileWithMeta,
        onSaveFile: _saveViaSystemDialog,
        onNotify: _showStatusSnack,
      ),
    );
  }

  void _openCaptureSheet() {
    final engine = _engines[_activeKind];
    if (engine == null || engine.lastRgbaBytes == null) {
      _showStatusSnack('Pantalla aún no disponible');
      return;
    }
    showModalBottomSheet<void>(
      context: context,
      isScrollControlled: true,
      backgroundColor: Colors.transparent,
      builder: (_) => Cg50CaptureSheet(
        engine: engine,
        tokens: widget.tokens,
        onSaveFile: _saveViaSystemDialog,
        onCopyImageToClipboard: _copyImageToClipboard,
        onNotify: _showStatusSnack,
      ),
    );
  }

  void _openEnlargedLcdSheet() {
    final engine = _engines[_activeKind];
    if (engine == null) {
      _showStatusSnack('Pantalla aún no disponible');
      return;
    }
    showModalBottomSheet<void>(
      context: context,
      isScrollControlled: true,
      backgroundColor: Colors.transparent,
      builder: (_) => Cg50EnlargedLcdSheet(
        engine: engine,
        tokens: widget.tokens,
        onOpenCapture: _openCaptureSheet,
        onScanQr: _activeKind == CasioCalcKind.cg50
            ? () => _scanAndOpenQr(silentWhenAbsent: false)
            : null,
      ),
    );
  }

  Future<void> _exportCg50Memory() async {
    final engine = _engines[CasioCalcKind.cg50];
    if (engine is! CasioCg50Engine) {
      _showStatusSnack('Calculadora no iniciada');
      return;
    }
    final tmpDir = await getTemporaryDirectory();
    final name = 'cg50_memoria_${_timestampTag()}.raw';
    final tmpFile = File('${tmpDir.path}/$name');
    final ok = await engine.exportUserRomToFile(tmpFile.path);
    if (!ok || !await tmpFile.exists()) {
      _showStatusSnack('No se pudo exportar la memoria');
      return;
    }
    try {
      final saved = await _saveViaSystemDialog(
        suggestedName: name,
        mimeType: 'application/octet-stream',
        sourcePath: tmpFile.path,
      );
      if (saved) _showStatusSnack('Respaldo de memoria guardado');
    } finally {
      if (await tmpFile.exists()) await tmpFile.delete();
    }
  }

  Future<void> _importCg50Memory() async {
    final engine = _engines[CasioCalcKind.cg50];
    if (engine is! CasioCg50Engine) {
      _showStatusSnack('Calculadora no iniciada');
      return;
    }
    final picked = await _pickViaSystemDialog(
      mimeType: '*/*',
      prefix: 'cg50_mem',
    );
    if (picked == null || picked.isEmpty) return;
    try {
      final ok = await engine.importUserRomFromFile(picked);
      if (ok) {
        _dirtyEngines.add(CasioCalcKind.cg50);
        _showStatusSnack('Memoria restaurada; reiniciando');
      } else {
        _showStatusSnack('Archivo de memoria no válido');
      }
    } finally {
      final f = File(picked);
      if (await f.exists()) await f.delete();
    }
  }

  Future<void> _saveActiveState() async {
    final ok = await _persistEngine(_activeKind, force: true);
    _showStatusSnack(
        ok ? 'Estado guardado' : 'No se pudo guardar el estado');
  }

  void _resetActive({required bool cold}) {
    _dirtyEngines.add(_activeKind);
    _engines[_activeKind]?.resetEngine(cold: cold);
  }

  Widget _buildPane(CasioCalcKind kind, {required bool primary}) {
    final skin = skinForKind(kind);
    final engine = _engines[kind];
    if (engine == null) {
      if (!widget.disableNativeBoot && _runtimeDir != null) {
        unawaited(_ensureEngineStarted(kind));
      }
      return Center(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            if (!widget.disableNativeBoot) ...[
              SizedBox(
                width: 22,
                height: 22,
                child: CircularProgressIndicator(
                  strokeWidth: 2,
                  color: widget.tokens.accent,
                ),
              ),
              const SizedBox(height: 10),
            ],
            Text(
              skin.title,
              style: TextStyle(
                color: widget.tokens.textPrimary,
                fontSize: 15,
                fontWeight: FontWeight.w600,
              ),
            ),
            const SizedBox(height: 4),
            Text(
              skin.subtitle,
              style: TextStyle(
                color: widget.tokens.textSecondary,
                fontSize: 12.5,
              ),
            ),
          ],
        ),
      );
    }
    if (kind == CasioCalcKind.cg50) {
      return AnimatedBuilder(
        animation: _keyLog,
        builder: (context, _) => NativeCalculatorFaceplate(
          key: ValueKey(kind),
          skin: skin,
          engine: engine,
          tokens: widget.tokens,
          onKeyLogged: _onKeyLogged,
          enableHardwareKeyboard: primary,
          highlightedCurrentKeyCode: _keyLog.highlightedCurrentKeyCode,
          highlightedNextKeyCode: _keyLog.highlightedNextKeyCode,
          onLcdTap: () => unawaited(_scanAndOpenQr(silentWhenAbsent: true)),
        ),
      );
    }
    return NativeCalculatorFaceplate(
      key: ValueKey(kind),
      skin: skin,
      engine: engine,
      tokens: widget.tokens,
      onKeyLogged: _onKeyLogged,
      enableHardwareKeyboard: primary,
    );
  }

  Widget _buildKeyLogBanner(TokensInterfaz t) {
    final cg = _engines[CasioCalcKind.cg50];
    if (cg is! CasioCg50Engine ||
        _activeKind != CasioCalcKind.cg50 ||
        (!_keyLog.isRecording && !_keyLog.isPlaying)) {
      return const SizedBox.shrink();
    }
    final rec = _keyLog.activeRecord;
    final statusText = _keyLog.isRecording
        ? 'Grabando Key-Log · ${rec.steps.length} pasos'
        : 'Key-Log · paso ${_keyLog.playCursor + 1}/${rec.steps.length}';
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
      decoration: BoxDecoration(
        color: t.surface,
        border: Border(bottom: BorderSide(color: t.border, width: t.borderWidth)),
      ),
      child: Row(
        children: [
          Icon(
            _keyLog.isRecording
                ? CupertinoIcons.circle_fill
                : CupertinoIcons.play_fill,
            size: 12,
            color: _keyLog.isRecording ? t.destructive : t.accent,
          ),
          const SizedBox(width: 8),
          Expanded(
            child: Text(
              statusText,
              style: TextStyle(
                  color: t.textPrimary,
                  fontSize: 12.5,
                  fontWeight: FontWeight.w500),
            ),
          ),
          if (_keyLog.isPlaying) ...[
            TextButton(
              onPressed: () {
                if (_keyLog.mode == KeyLogMode.playingAuto) {
                  _keyLog.pausePlayback();
                } else {
                  unawaited(_keyLog.startAutoPlay(cg));
                }
              },
              child: Text(_keyLog.mode == KeyLogMode.playingAuto
                  ? 'Pausa'
                  : 'Reanudar'),
            ),
            TextButton(
              onPressed: () => unawaited(_keyLog.stepNext(cg)),
              child: const Text('Paso'),
            ),
          ],
          TextButton(
            onPressed: _keyLog.stop,
            style: TextButton.styleFrom(foregroundColor: t.destructive),
            child: const Text('Detener'),
          ),
        ],
      ),
    );
  }

  Widget _buildOverflowMenu(TokensInterfaz t, {required bool includeCalcItems}) {
    return PopupMenuButton<String>(
      tooltip: 'Opciones',
      padding: EdgeInsets.zero,
      icon: Icon(
        CupertinoIcons.ellipsis_circle,
        size: 21,
        color: t.textPrimary,
      ),
      onSelected: (value) {
        switch (value) {
          case 'theme_light':
            widget.onThemeChanged(AppThemePreset.porcelainLight);
            break;
          case 'theme_dark':
            widget.onThemeChanged(AppThemePreset.calibratedDark);
            break;
          case 'theme_hc':
            widget.onThemeChanged(AppThemePreset.highContrast);
            break;
          case 'toggle_trans':
            widget.onReduceTransparencyChanged(!widget.reduceTransparency);
            break;
          case 'send_to_notebook':
            _sendActiveCalcToCuadernu();
            break;
          case 'connect_files':
            _openConnectivitySheet();
            break;
          case 'keylog':
            _openKeyLogSheet();
            break;
          case 'enlarged_lcd':
            _openEnlargedLcdSheet();
            break;
          case 'capture_lcd':
            _openCaptureSheet();
            break;
          case 'scan_qr':
            unawaited(_scanAndOpenQr(silentWhenAbsent: false));
            break;
          case 'save':
            unawaited(_saveActiveState());
            break;
          case 'warm_reset':
            _resetActive(cold: false);
            break;
          case 'cold_reset':
            _resetActive(cold: true);
            break;
        }
      },
      itemBuilder: (context) => [
        CheckedPopupMenuItem<String>(
          value: 'theme_light',
          checked: widget.themePreset == AppThemePreset.porcelainLight,
          child: const Text('Porcelana Claro'),
        ),
        CheckedPopupMenuItem<String>(
          value: 'theme_dark',
          checked: widget.themePreset == AppThemePreset.calibratedDark,
          child: const Text('Oscuro Calibrado'),
        ),
        CheckedPopupMenuItem<String>(
          value: 'theme_hc',
          checked: widget.themePreset == AppThemePreset.highContrast,
          child: const Text('Alto Contraste'),
        ),
        CheckedPopupMenuItem<String>(
          value: 'toggle_trans',
          checked: widget.reduceTransparency,
          child: const Text('Reducir transparencia'),
        ),
        if (includeCalcItems) ...[
          const PopupMenuDivider(),
          const PopupMenuItem<String>(
            value: 'send_to_notebook',
            child: Text('Enviar pantalla al cuaderno'),
          ),
          if (_activeKind == CasioCalcKind.cg50) ...[
            const PopupMenuItem<String>(
              value: 'connect_files',
              child: Text('Conectar y archivos…'),
            ),
            const PopupMenuItem<String>(
              value: 'keylog',
              child: Text('Key-Log…'),
            ),
            const PopupMenuItem<String>(
              value: 'scan_qr',
              child: Text('Leer QR en pantalla'),
            ),
          ],
          const PopupMenuItem<String>(
            value: 'enlarged_lcd',
            child: Text('Pantalla ampliada…'),
          ),
          const PopupMenuItem<String>(
            value: 'capture_lcd',
            child: Text('Capturar pantalla…'),
          ),
          const PopupMenuDivider(),
          const PopupMenuItem<String>(
            value: 'save',
            child: Text('Guardar estado'),
          ),
          const PopupMenuItem<String>(
            value: 'warm_reset',
            child: Text('Reiniciar calculadora'),
          ),
          const PopupMenuItem<String>(
            value: 'cold_reset',
            child: Text('Reinicio frío'),
          ),
        ],
      ],
    );
  }

  Widget _buildCalculatorBar(TokensInterfaz t, bool canSideBySide, bool showSplit) {
    return TranslucentSurface(
      tokens: t,
      reduceTransparency: widget.reduceTransparency,
      bottomBorderOnly: true,
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 7),
      child: Row(
        children: [
          Expanded(
            child: SingleChildScrollView(
              scrollDirection: Axis.horizontal,
              child: SelectorSegmentado<CasioCalcKind>(
                tokens: t,
                value: _activeKind,
                segments: const [
                  (CasioCalcKind.cg50, 'CG50'),
                  (CasioCalcKind.cw2, '991CW'),
                  (CasioCalcKind.classwiz, '570LA X'),
                  (CasioCalcKind.naturalEs, '570VN+'),
                ],
                onChanged: _selectCalculator,
              ),
            ),
          ),
          const SizedBox(width: 6),
          if (_editorCtrl.isReady)
            IconButton(
              tooltip: 'Enviar pantalla a CuadernU',
              visualDensity: VisualDensity.compact,
              onPressed: _sendActiveCalcToCuadernu,
              icon: Icon(CupertinoIcons.doc_append, size: 19, color: t.accent),
            ),
          if (canSideBySide)
            IconButton(
              tooltip: 'Vista lado a lado',
              visualDensity: VisualDensity.compact,
              onPressed: _toggleSideBySide,
              icon: Icon(
                showSplit
                    ? CupertinoIcons.rectangle_split_3x1_fill
                    : CupertinoIcons.rectangle_split_3x1,
                size: 20,
                color: showSplit ? t.accent : t.textSecondary,
              ),
            ),
          if (_activeKind == CasioCalcKind.cg50)
            AnimatedBuilder(
              animation: Listenable.merge([?_bridgeServer, ?_linkClient]),
              builder: (context, _) {
                final activeConn = (_bridgeServer?.isRunning ?? false) ||
                    (_linkClient?.isConnected ?? false);
                return IconButton(
                  tooltip: 'Conectar y archivos',
                  visualDensity: VisualDensity.compact,
                  onPressed: _openConnectivitySheet,
                  icon: Badge(
                    isLabelVisible: activeConn,
                    smallSize: 7,
                    backgroundColor: const Color(0xFF34C759),
                    child: Icon(
                      CupertinoIcons.folder,
                      size: 20,
                      color: activeConn ? t.accent : t.textPrimary,
                    ),
                  ),
                );
              },
            ),
          IconButton(
            tooltip: 'Historial',
            visualDensity: VisualDensity.compact,
            onPressed: _openHistorySheet,
            icon: Badge(
              isLabelVisible: _history.isNotEmpty,
              backgroundColor: t.accent,
              label: Text('${_history.length}'),
              child: Icon(CupertinoIcons.clock, size: 20, color: t.textPrimary),
            ),
          ),
          _buildOverflowMenu(t, includeCalcItems: true),
        ],
      ),
    );
  }

  Widget _buildCuadernuHeader(TokensInterfaz t, bool canSplitNotebookCalc) {
    return TranslucentSurface(
      tokens: t,
      reduceTransparency: widget.reduceTransparency,
      bottomBorderOnly: true,
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 7),
      child: Row(
        children: [
          Text(
            'CuadernU',
            style: TextStyle(
              color: t.textPrimary,
              fontSize: 16,
              fontWeight: FontWeight.w600,
              letterSpacing: -0.2,
            ),
          ),
          const Spacer(),
          if (_activeTab == CuadernuWorkspaceTab.notebook &&
              canSplitNotebookCalc)
            TextButton.icon(
              onPressed: _toggleDockedCalculator,
              icon: Icon(
                CupertinoIcons.square_split_2x1,
                size: 17,
                color: _dockedCalcOnNotebook ? t.accent : t.textSecondary,
              ),
              label: Text(
                _dockedCalcOnNotebook ? 'Ocultar Casio' : 'Casio acoplada',
                style: TextStyle(
                  color: _dockedCalcOnNotebook ? t.accent : t.textSecondary,
                  fontSize: 12.5,
                ),
              ),
            ),
          _buildOverflowMenu(t, includeCalcItems: false),
        ],
      ),
    );
  }

  @override
  void dispose() {
    WidgetsBinding.instance.removeObserver(this);
    _editorCtrl.saveCudoc();
    _editorCtrl.dispose();
    _bridgeServer?.dispose();
    _linkClient?.dispose();
    _keyLog.dispose();
    for (final e in _engines.values) {
      e.dispose();
    }
    _engines.clear();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final t = widget.tokens;
    return Scaffold(
      backgroundColor: t.canvas,
      body: SafeArea(
        child: LayoutBuilder(
          builder: (context, constraints) {
            final bool canSideBySide = constraints.maxWidth >= 720;
            final bool showSplitCalc = _sideBySide && canSideBySide;
            final bool showDockedCalc = _activeTab == CuadernuWorkspaceTab.notebook &&
                _dockedCalcOnNotebook &&
                canSideBySide;

            Widget bodyContent;
            if (_staging) {
              bodyContent = Center(
                child: Column(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    CircularProgressIndicator(strokeWidth: 2.2, color: t.accent),
                    const SizedBox(height: 12),
                    Text(
                      'Preparando CuadernU…',
                      style: TextStyle(color: t.textSecondary, fontSize: 13.5),
                    ),
                  ],
                ),
              );
            } else if (_stageError != null) {
              bodyContent = Center(
                child: Padding(
                  padding: const EdgeInsets.all(24),
                  child: Text(
                    _stageError!,
                    textAlign: TextAlign.center,
                    style: TextStyle(color: t.destructive),
                  ),
                ),
              );
            } else {
              switch (_activeTab) {
                case CuadernuWorkspaceTab.notebook:
                  final editor = CuadernuEditorView(
                    controller: _editorCtrl,
                    tokens: t,
                    reduceTransparency: widget.reduceTransparency,
                    onInsertActiveCasio: _sendActiveCalcToCuadernu,
                    onSaveFile: _saveViaSystemDialog,
                    onPickFile: _pickFileWithMeta,
                    onNotify: _showStatusSnack,
                  );
                  bodyContent = showDockedCalc
                      ? Row(
                          children: [
                            Expanded(flex: 3, child: editor),
                            Container(width: 1, color: t.border),
                            Expanded(
                              flex: 2,
                              child: Column(
                                children: [
                                  Padding(
                                    padding: const EdgeInsets.symmetric(
                                        horizontal: 8, vertical: 6),
                                    child: SingleChildScrollView(
                                      scrollDirection: Axis.horizontal,
                                      child: SelectorSegmentado<CasioCalcKind>(
                                        tokens: t,
                                        value: _activeKind,
                                        segments: const [
                                          (CasioCalcKind.cg50, 'CG50'),
                                          (CasioCalcKind.cw2, '991CW'),
                                          (CasioCalcKind.classwiz, '570LA X'),
                                          (CasioCalcKind.naturalEs, '570VN+'),
                                        ],
                                        onChanged: _selectCalculator,
                                      ),
                                    ),
                                  ),
                                  Expanded(
                                    child: Padding(
                                      padding: const EdgeInsets.all(8),
                                      child: _buildPane(_activeKind,
                                          primary: true),
                                    ),
                                  ),
                                ],
                              ),
                            ),
                          ],
                        )
                      : editor;
                  break;

                case CuadernuWorkspaceTab.calculators:
                  bodyContent = Column(
                    children: [
                      AnimatedBuilder(
                        animation: _keyLog,
                        builder: (context, _) => _buildKeyLogBanner(t),
                      ),
                      Expanded(
                        child: Padding(
                          padding: const EdgeInsets.fromLTRB(8, 8, 8, 8),
                          child: showSplitCalc
                              ? Row(
                                  children: [
                                    Expanded(
                                      child: _buildPane(_activeKind,
                                          primary: true),
                                    ),
                                    const SizedBox(width: 12),
                                    Expanded(
                                      child: _buildPane(_secondaryKind,
                                          primary: false),
                                    ),
                                  ],
                                )
                              : _buildPane(_activeKind, primary: true),
                        ),
                      ),
                    ],
                  );
                  break;

                case CuadernuWorkspaceTab.stem:
                  bodyContent = CuadernuStemLabView(
                    studio: _studio,
                    tokens: t,
                    reduceTransparency: widget.reduceTransparency,
                    onInsertToNotebook: (title, content, {mathExpr}) {
                      _editorCtrl.insertFromStem(
                        title: title,
                        content: content,
                        mathExpr: mathExpr,
                      );
                      _showStatusSnack('Insertado en ${_editorCtrl.title}');
                    },
                    onNotify: _showStatusSnack,
                  );
                  break;

                case CuadernuWorkspaceTab.study:
                  bodyContent = CuadernuKnowledgeStudyView(
                    studio: _studio,
                    tokens: t,
                    reduceTransparency: widget.reduceTransparency,
                    onHarvestDocFlashcards: _editorCtrl.harvestFlashcards,
                    onNotify: _showStatusSnack,
                  );
                  break;
              }
            }

            return Column(
              children: [
                if (_activeTab == CuadernuWorkspaceTab.calculators)
                  _buildCalculatorBar(t, canSideBySide, showSplitCalc)
                else
                  _buildCuadernuHeader(t, canSideBySide),
                Expanded(child: bodyContent),
                // Unified guías de interfaz bottom navigation bar (Cupertino vector icons, zero emojis)
                Container(
                  decoration: BoxDecoration(
                    color: t.surface,
                    border: Border(
                      top: BorderSide(color: t.border, width: t.borderWidth),
                    ),
                  ),
                  child: Row(
                    children: [
                      for (final item in const [
                        (
                          CuadernuWorkspaceTab.notebook,
                          CupertinoIcons.doc_text,
                          'Cuaderno'
                        ),
                        (
                          CuadernuWorkspaceTab.calculators,
                          CupertinoIcons.square_grid_2x2,
                          'Calculadoras'
                        ),
                        (
                          CuadernuWorkspaceTab.stem,
                          CupertinoIcons.function,
                          'STEM'
                        ),
                        (
                          CuadernuWorkspaceTab.study,
                          CupertinoIcons.layers_alt,
                          'Estudio'
                        ),
                      ])
                        Expanded(
                          child: InkWell(
                            onTap: () => _selectTab(item.$1),
                            child: Padding(
                              padding: const EdgeInsets.symmetric(vertical: 8),
                              child: Column(
                                mainAxisSize: MainAxisSize.min,
                                children: [
                                  Icon(
                                    item.$2,
                                    size: 20,
                                    color: _activeTab == item.$1
                                        ? t.accent
                                        : t.textSecondary,
                                  ),
                                  const SizedBox(height: 2),
                                  Text(
                                    item.$3,
                                    style: TextStyle(
                                      fontSize: 11,
                                      fontWeight: _activeTab == item.$1
                                          ? FontWeight.w600
                                          : FontWeight.w400,
                                      color: _activeTab == item.$1
                                          ? t.accent
                                          : t.textSecondary,
                                    ),
                                  ),
                                ],
                              ),
                            ),
                          ),
                        ),
                    ],
                  ),
                ),
              ],
            );
          },
        ),
      ),
    );
  }
}
