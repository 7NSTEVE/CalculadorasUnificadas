import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'dart:math' as math;
import 'dart:typed_data';
import 'dart:ui' as ui;

import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';
import 'package:path_provider/path_provider.dart';

import '../native/cuadernu_studio_bindings.dart';
import '../theme/tema_interfaz.dart';
import 'cuadernu_models.dart';

/// Manages the active `.cudoc` native document, vault index, decoded image assets
/// (CalcSessionCard / Image / PdfCard), and clean cross-tab insertions (Casio + STEM).
class CuadernuEditorController extends ChangeNotifier {
  CuadernuStudioBindings? _studio;
  CuadernuNativeDocument? _doc;

  String? _vaultDir;
  String? _activeFilePath;
  List<CuadernuVaultEntry> _vault = [];
  Map<String, dynamic> _snapshot = const {};
  int _activePage = 0;

  final Map<String, ui.Image> _decodedAssets = {};
  final Set<String> _decodingAssets = {};

  bool get isReady => _doc != null && !(_doc!.isDisposed);
  CuadernuStudioBindings? get studio => _studio;
  List<CuadernuVaultEntry> get vault => List.unmodifiable(_vault);
  String? get activeFilePath => _activeFilePath;
  Map<String, dynamic> get snapshot => _snapshot;
  int get activePage => _activePage;

  String get title => _snapshot['title']?.toString() ?? 'Cuaderno 1';
  bool get infiniteCanvas => ((_snapshot['mode'] as num?)?.toInt() ?? 0) == 1;
  int get paperPattern => (_snapshot['paperPattern'] as num?)?.toInt() ?? 2;
  bool get canUndo => _snapshot['canUndo'] == true;
  bool get canRedo => _snapshot['canRedo'] == true;
  int get pageCount {
    final pages = _snapshot['pages'];
    if (pages is List && pages.isNotEmpty) return pages.length;
    return 1;
  }

  ui.Image? assetImage(String key) => _decodedAssets[key];

  List<Map<String, dynamic>> get activeObjects {
    if (infiniteCanvas) {
      final list = _snapshot['canvasObjects'];
      if (list is List) {
        return list
            .whereType<Map>()
            .map((e) => Map<String, dynamic>.from(e))
            .toList();
      }
      return const [];
    }
    final pages = _snapshot['pages'];
    if (pages is List && pages.isNotEmpty) {
      final idx = _activePage.clamp(0, pages.length - 1);
      final objs = (pages[idx] as Map)['objects'];
      if (objs is List) {
        return objs
            .whereType<Map>()
            .map((e) => Map<String, dynamic>.from(e))
            .toList();
      }
    }
    return const [];
  }

  Future<void> initialize(CuadernuStudioBindings studio) async {
    _studio = studio;
    if (!studio.isAvailable) return;
    final base = await getApplicationDocumentsDirectory();
    final dir = Directory('${base.path}/cuadernu_vault');
    if (!await dir.exists()) await dir.create(recursive: true);
    _vaultDir = dir.path;

    await _loadVaultIndex();
    if (_vault.isNotEmpty && File(_vault.first.path).existsSync()) {
      await openNotebook(_vault.first.path);
    } else {
      await createNotebook(title: 'Cuaderno Principal', infiniteCanvas: false);
    }
  }

  Future<File> _indexFile() async => File('${_vaultDir!}/vault_index.json');

  Future<void> _loadVaultIndex() async {
    try {
      final f = await _indexFile();
      if (!await f.exists()) return;
      final decoded = jsonDecode(await f.readAsString());
      if (decoded is List) {
        _vault = decoded
            .whereType<Map>()
            .map((e) => CuadernuVaultEntry.fromJson(Map<String, dynamic>.from(e)))
            .where((e) => e.path.isNotEmpty && File(e.path).existsSync())
            .toList();
      }
    } catch (_) {}
  }

  Future<void> _saveVaultIndex() async {
    if (_vaultDir == null) return;
    try {
      final f = await _indexFile();
      await f.writeAsString(
        jsonEncode(_vault.map((e) => e.toJson()).toList()),
        flush: true,
      );
    } catch (_) {}
  }

  void _refreshSnapshot({bool saveToDisk = false}) {
    final doc = _doc;
    if (doc == null || doc.isDisposed) return;
    _snapshot = doc.getSnapshot();
    final pc = pageCount;
    if (_activePage >= pc) _activePage = math.max(0, pc - 1);
    _scheduleAssetDecode();
    if (saveToDisk && _activeFilePath != null) {
      doc.saveCudoc(_activeFilePath!);
      _updateVaultEntry();
    }
    notifyListeners();
  }

  void _updateVaultEntry() {
    final path = _activeFilePath;
    if (path == null) return;
    final entry = CuadernuVaultEntry(
      id: path.hashCode.toRadixString(16),
      title: title,
      path: path,
      updatedAt: DateTime.now(),
      infiniteCanvas: infiniteCanvas,
      pageCount: pageCount,
    );
    _vault.removeWhere((e) => e.path == path);
    _vault.insert(0, entry);
    unawaited(_saveVaultIndex());
  }

  void _scheduleAssetDecode() {
    final doc = _doc;
    if (doc == null || doc.isDisposed) return;
    final keys = <String>{};
    for (final obj in activeObjects) {
      final k = obj['assetKey']?.toString() ?? '';
      if (k.isNotEmpty) keys.add(k);
    }
    for (final key in keys) {
      if (_decodedAssets.containsKey(key) || _decodingAssets.contains(key)) {
        continue;
      }
      final bytes = doc.getAssetBytes(key);
      if (bytes == null || bytes.length < 8) continue;
      _decodingAssets.add(key);
      ui.decodeImageFromList(bytes, (img) {
        _decodingAssets.remove(key);
        _decodedAssets[key] = img;
        notifyListeners();
      });
    }
  }

  Future<void> createNotebook({
    required String title,
    required bool infiniteCanvas,
    int paperPattern = CuadernuPaperPattern.grid,
  }) async {
    final s = _studio;
    if (s == null || !s.isAvailable || _vaultDir == null) return;
    _doc?.dispose();
    _decodedAssets.clear();
    _doc = s.createDocument(infiniteCanvas: infiniteCanvas);
    _doc!.setTitle(title);
    _doc!.setPaper(paperPattern);
    _activePage = 0;
    final stamp = DateTime.now().millisecondsSinceEpoch;
    _activeFilePath = '$_vaultDir/cuaderno_$stamp.cudoc';
    _refreshSnapshot(saveToDisk: true);
  }

  Future<bool> openNotebook(String path) async {
    final s = _studio;
    if (s == null || !s.isAvailable) return false;
    final nextDoc = s.createDocument(infiniteCanvas: false);
    if (!nextDoc.loadCudoc(path)) {
      nextDoc.dispose();
      return false;
    }
    _doc?.dispose();
    _decodedAssets.clear();
    _doc = nextDoc;
    _activeFilePath = path;
    _activePage = 0;
    _refreshSnapshot(saveToDisk: false);
    _updateVaultEntry();
    return true;
  }

  Future<void> deleteNotebook(String path) async {
    try {
      final f = File(path);
      if (await f.exists()) await f.delete();
    } catch (_) {}
    _vault.removeWhere((e) => e.path == path);
    await _saveVaultIndex();
    if (_activeFilePath == path) {
      if (_vault.isNotEmpty) {
        await openNotebook(_vault.first.path);
      } else {
        await createNotebook(title: 'Cuaderno Principal', infiniteCanvas: false);
      }
    } else {
      notifyListeners();
    }
  }

  void setTitle(String newTitle) {
    if (!isReady || newTitle.trim().isEmpty) return;
    _doc!.setTitle(newTitle.trim());
    _refreshSnapshot(saveToDisk: true);
  }

  void setInfiniteCanvas(bool infinite) {
    if (!isReady) return;
    _doc!.setMode(infiniteCanvas: infinite);
    _refreshSnapshot(saveToDisk: true);
  }

  void setPaperPattern(int pattern) {
    if (!isReady) return;
    _doc!.setPaper(pattern);
    _refreshSnapshot(saveToDisk: true);
  }

  void setActivePage(int page) {
    final next = page.clamp(0, math.max(0, pageCount - 1)).toInt();
    if (next == _activePage) return;
    _activePage = next;
    _scheduleAssetDecode();
    notifyListeners();
  }

  void addPage() {
    if (!isReady) return;
    final idx = _doc!.addPage(paperPattern);
    if (idx >= 0) _activePage = idx;
    _refreshSnapshot(saveToDisk: true);
  }

  void addInkStroke({
    required List<({double x, double y, double p})> points,
    required Color color,
    required double strokeWidth,
    int penStyle = CuadernuPenStyle.ballpoint,
    bool isHighlighter = false,
  }) {
    if (!isReady || points.length < 2) return;
    _doc!.addInkStroke(
      onCanvas: infiniteCanvas,
      pageIndex: _activePage,
      points: points,
      rgba: colorToStudioRgba(color),
      strokeWidth: strokeWidth,
      penStyle: penStyle,
      isHighlighter: isHighlighter,
    );
    _refreshSnapshot(saveToDisk: true);
  }

  void addShape({
    required int shapeKind,
    required double x,
    required double y,
    required double w,
    required double h,
    required Color strokeColor,
    double strokeWidth = 2.0,
  }) {
    if (!isReady) return;
    _doc!.addShape(
      onCanvas: infiniteCanvas,
      pageIndex: _activePage,
      shapeKind: shapeKind,
      x: x,
      y: y,
      w: w,
      h: h,
      strokeRgba: colorToStudioRgba(strokeColor),
      strokeWidth: strokeWidth,
    );
    _refreshSnapshot(saveToDisk: true);
  }

  void addTextBox({
    required double x,
    required double y,
    required String text,
    Color color = const Color(0xFF1D1D1F),
  }) {
    if (!isReady || text.trim().isEmpty) return;
    _doc!.addTextBox(
      onCanvas: infiniteCanvas,
      pageIndex: _activePage,
      x: x,
      y: y,
      w: 240,
      h: 72,
      text: text.trim(),
      fontSizePt: 14,
      rgba: colorToStudioRgba(color),
    );
    _refreshSnapshot(saveToDisk: true);
  }

  void addMathBlock({
    required double x,
    required double y,
    required String expr,
  }) {
    if (!isReady || expr.trim().isEmpty) return;
    _doc!.addMathBlock(
      onCanvas: infiniteCanvas,
      pageIndex: _activePage,
      x: x,
      y: y,
      w: 260,
      h: 64,
      expr: expr.trim(),
      angleDegrees: true,
    );
    _refreshSnapshot(saveToDisk: true);
  }

  void addStickyNote({
    required double x,
    required double y,
    required String text,
  }) {
    if (!isReady || text.trim().isEmpty) return;
    _doc!.addStickyNote(
      onCanvas: infiniteCanvas,
      pageIndex: _activePage,
      x: x,
      y: y,
      text: text.trim(),
    );
    _refreshSnapshot(saveToDisk: true);
  }

  void addStudyTape({
    required double x,
    required double y,
    required double w,
    required double h,
    required String label,
  }) {
    if (!isReady) return;
    _doc!.addStudyTape(
      onCanvas: infiniteCanvas,
      pageIndex: _activePage,
      x: x,
      y: y,
      w: w,
      h: h,
      label: label.isEmpty ? 'Oclusión de estudio' : label,
    );
    _refreshSnapshot(saveToDisk: true);
  }

  bool toggleStudyTapeAt(double x, double y) {
    if (!isReady) return false;
    for (final obj in activeObjects.reversed) {
      if ((obj['kind'] as num?)?.toInt() != 5) continue;
      final b = obj['bounds'];
      if (b is List && b.length >= 4) {
        final bx = (b[0] as num).toDouble();
        final by = (b[1] as num).toDouble();
        final bw = (b[2] as num).toDouble();
        final bh = (b[3] as num).toDouble();
        if (x >= bx && x <= bx + bw && y >= by && y <= by + bh) {
          final id = (obj['id'] as num?)?.toInt() ?? 0;
          if (id > 0) {
            _doc!.toggleStudyTape(
              onCanvas: infiniteCanvas,
              pageIndex: _activePage,
              objectId: id,
            );
            _refreshSnapshot(saveToDisk: true);
            return true;
          }
        }
      }
    }
    return false;
  }

  void eraseAt(double x, double y, {double radius = 16.0}) {
    if (!isReady) return;
    final erased = _doc!.eraseAt(
      onCanvas: infiniteCanvas,
      pageIndex: _activePage,
      x: x,
      y: y,
      radius: radius,
    );
    if (erased > 0) _refreshSnapshot(saveToDisk: true);
  }

  void undo() {
    if (!isReady) return;
    if (_doc!.undo()) _refreshSnapshot(saveToDisk: true);
  }

  void redo() {
    if (!isReady) return;
    if (_doc!.redo()) _refreshSnapshot(saveToDisk: true);
  }

  Map<String, dynamic> evaluateMathDag() {
    if (!isReady) return const {};
    final rep = _doc!.evaluateMathDag();
    _refreshSnapshot(saveToDisk: true);
    return rep;
  }

  List<Map<String, dynamic>> search(String query) {
    if (!isReady || query.trim().isEmpty) return const [];
    return _doc!.search(query.trim(), accentInsensitive: true);
  }

  void insertCasioCaptureCard({
    required String calcModel,
    required String expr,
    required String result,
    required String keySeq,
    Uint8List? pngBytes,
  }) {
    if (!isReady) return;
    final offset = (activeObjects.length % 6) * 28.0;
    _doc!.addCasioCard(
      onCanvas: infiniteCanvas,
      pageIndex: _activePage,
      x: 42 + offset,
      y: 48 + offset,
      w: 268,
      h: 176,
      calcModel: calcModel,
      expr: expr,
      result: result,
      keySeq: keySeq,
      pngBytes: pngBytes,
    );
    _refreshSnapshot(saveToDisk: true);
  }

  void insertFromStem({
    required String title,
    required String content,
    String? mathExpr,
  }) {
    if (!isReady) return;
    final offset = (activeObjects.length % 7) * 24.0;
    if (mathExpr != null && mathExpr.trim().isNotEmpty) {
      addMathBlock(x: 44 + offset, y: 52 + offset, expr: mathExpr);
    } else {
      addTextBox(x: 44 + offset, y: 52 + offset, text: '$title\n$content');
    }
  }

  List<Map<String, dynamic>> harvestFlashcards() {
    if (!isReady) return const [];
    return _doc!.buildFlashcards();
  }

  bool saveCudoc() {
    if (!isReady || _activeFilePath == null) return false;
    final ok = _doc!.saveCudoc(_activeFilePath!);
    if (ok) _updateVaultEntry();
    return ok;
  }

  Future<String?> exportPdfToTemp() async {
    if (!isReady) return null;
    final tmp = await getTemporaryDirectory();
    final safe = title.replaceAll(RegExp(r'[^A-Za-z0-9._-]'), '_');
    final outPath = '${tmp.path}/${safe.isEmpty ? 'cuadernu' : safe}.pdf';
    return _doc!.exportPdf(outPath) ? outPath : null;
  }

  Future<String?> exportMarkdownToTemp() async {
    if (!isReady) return null;
    final md = _doc!.exportMarkdown();
    final tmp = await getTemporaryDirectory();
    final safe = title.replaceAll(RegExp(r'[^A-Za-z0-9._-]'), '_');
    final outPath = '${tmp.path}/${safe.isEmpty ? 'cuadernu' : safe}.md';
    await File(outPath).writeAsString(md, flush: true);
    return outPath;
  }

  Future<String?> exportDxfToTemp() async {
    if (!isReady) return null;
    final tmp = await getTemporaryDirectory();
    final safe = title.replaceAll(RegExp(r'[^A-Za-z0-9._-]'), '_');
    final outPath = '${tmp.path}/${safe.isEmpty ? 'cuadernu' : safe}.dxf';
    return _doc!.exportDxf(outPath) ? outPath : null;
  }

  Map<String, dynamic> importExternalFile(String path) {
    if (!isReady) return {'ok': false, 'error': 'Documento no listo'};
    if (path.toLowerCase().endsWith('.cudoc')) {
      final ok = _doc!.loadCudoc(path);
      if (ok) _refreshSnapshot(saveToDisk: true);
      return {'ok': ok, 'format': 'cudoc'};
    }
    final rep = _doc!.importFile(path, targetInfiniteCanvas: infiniteCanvas);
    _refreshSnapshot(saveToDisk: true);
    return rep;
  }

  @override
  void dispose() {
    _doc?.dispose();
    for (final img in _decodedAssets.values) {
      img.dispose();
    }
    _decodedAssets.clear();
    super.dispose();
  }
}

class CuadernuEditorView extends StatefulWidget {
  final CuadernuEditorController controller;
  final TokensInterfaz tokens;
  final bool reduceTransparency;
  final VoidCallback? onInsertActiveCasio;
  final Future<bool> Function({
    required String suggestedName,
    required String mimeType,
    required String sourcePath,
  }) onSaveFile;
  final Future<Map<String, String>?> Function({
    String mimeType,
    String prefix,
  }) onPickFile;
  final void Function(String message) onNotify;

  const CuadernuEditorView({
    super.key,
    required this.controller,
    required this.tokens,
    required this.reduceTransparency,
    required this.onSaveFile,
    required this.onPickFile,
    required this.onNotify,
    this.onInsertActiveCasio,
  });

  @override
  State<CuadernuEditorView> createState() => _CuadernuEditorViewState();
}

class _CuadernuEditorViewState extends State<CuadernuEditorView> {
  CuadernuToolMode _tool = CuadernuToolMode.pen;
  int _shapeKind = CuadernuShapeKind.rectangle;
  Color _inkColor = const Color(0xFF1D1D1F);
  final double _strokeWidth = 2.6;

  final List<({double x, double y, double p})> _liveStroke = [];
  Offset? _dragStart;
  Offset? _dragCurrent;

  bool _showSearch = false;
  final TextEditingController _searchCtrl = TextEditingController();
  List<Map<String, dynamic>> _searchHits = const [];

  static const List<Color> _inkPalette = [
    Color(0xFF1D1D1F),
    Color(0xFF0071E3),
    Color(0xFFD70015),
    Color(0xFF1E7E34),
    Color(0xFFD97706),
    Color(0xFF6D28D9),
  ];

  static const double _pageW = 595.0;
  static const double _pageH = 842.0;

  @override
  void dispose() {
    _searchCtrl.dispose();
    super.dispose();
  }

  void _openVaultSheet() {
    final t = widget.tokens;
    showModalBottomSheet<void>(
      context: context,
      isScrollControlled: true,
      backgroundColor: Colors.transparent,
      builder: (ctx) => Container(
        constraints: BoxConstraints(
          maxHeight: MediaQuery.of(ctx).size.height * 0.72,
        ),
        decoration: BoxDecoration(
          color: t.surface,
          borderRadius: const BorderRadius.vertical(top: Radius.circular(18)),
          border: Border.all(color: t.border, width: t.borderWidth),
        ),
        child: Column(
          children: [
            Padding(
              padding: const EdgeInsets.fromLTRB(16, 14, 10, 10),
              child: Row(
                children: [
                  Text(
                    'Biblioteca de cuadernos',
                    style: TextStyle(
                      color: t.textPrimary,
                      fontSize: 16.5,
                      fontWeight: FontWeight.w600,
                    ),
                  ),
                  const Spacer(),
                  TextButton.icon(
                    onPressed: () {
                      Navigator.pop(ctx);
                      unawaited(_promptCreateNotebook());
                    },
                    icon: const Icon(CupertinoIcons.add, size: 16),
                    label: const Text('Nuevo'),
                  ),
                ],
              ),
            ),
            Divider(color: t.border, height: 1),
            Expanded(
              child: ListView.separated(
                padding: const EdgeInsets.all(12),
                itemCount: widget.controller.vault.length,
                separatorBuilder: (_, _) => Divider(color: t.border, height: 1),
                itemBuilder: (_, idx) {
                  final item = widget.controller.vault[idx];
                  final active = item.path == widget.controller.activeFilePath;
                  return ListTile(
                    leading: Icon(
                      item.infiniteCanvas
                          ? CupertinoIcons.square_grid_2x2
                          : CupertinoIcons.doc_text,
                      color: active ? t.accent : t.textSecondary,
                    ),
                    title: Text(
                      item.title,
                      style: TextStyle(
                        color: t.textPrimary,
                        fontWeight: active ? FontWeight.w600 : FontWeight.w400,
                      ),
                    ),
                    subtitle: Text(
                      item.infiniteCanvas
                          ? 'Lienzo infinito'
                          : '${item.pageCount} pág.',
                      style: TextStyle(color: t.textSecondary, fontSize: 12),
                    ),
                    trailing: IconButton(
                      icon: Icon(CupertinoIcons.trash,
                          size: 18, color: t.destructive),
                      onPressed: () {
                        Navigator.pop(ctx);
                        unawaited(widget.controller.deleteNotebook(item.path));
                      },
                    ),
                    onTap: () {
                      Navigator.pop(ctx);
                      unawaited(widget.controller.openNotebook(item.path));
                    },
                  );
                },
              ),
            ),
          ],
        ),
      ),
    );
  }

  Future<void> _promptCreateNotebook() async {
    final titleCtrl = TextEditingController(
        text: 'Cuaderno ${widget.controller.vault.length + 1}');
    bool infinite = false;
    final created = await showDialog<bool>(
      context: context,
      builder: (ctx) => StatefulBuilder(
        builder: (ctx, setDlg) => AlertDialog(
          backgroundColor: widget.tokens.surface,
          title: const Text('Nuevo cuaderno'),
          content: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              TextField(
                controller: titleCtrl,
                decoration: const InputDecoration(labelText: 'Título'),
              ),
              const SizedBox(height: 12),
              SwitchListTile(
                contentPadding: EdgeInsets.zero,
                title: const Text('Lienzo infinito'),
                value: infinite,
                onChanged: (v) => setDlg(() => infinite = v),
              ),
            ],
          ),
          actions: [
            TextButton(
                onPressed: () => Navigator.pop(ctx, false),
                child: const Text('Cancelar')),
            FilledButton(
                onPressed: () => Navigator.pop(ctx, true),
                child: const Text('Crear')),
          ],
        ),
      ),
    );
    if (created == true) {
      await widget.controller.createNotebook(
        title: titleCtrl.text.trim().isEmpty
            ? 'Cuaderno'
            : titleCtrl.text.trim(),
        infiniteCanvas: infinite,
      );
    }
    titleCtrl.dispose();
  }

  Future<void> _promptInsertTextLike(Offset docPos, CuadernuToolMode mode) async {
    final ctrl = TextEditingController(
      text: mode == CuadernuToolMode.math ? 'I = 12 V / 470 ohm | mA' : '',
    );
    final label = switch (mode) {
      CuadernuToolMode.math => 'Bloque matemático (#varsub)',
      CuadernuToolMode.sticky => 'Nota adhesiva',
      CuadernuToolMode.tape => 'Cinta de estudio (toca para revelar)',
      _ => 'Cuadro de texto',
    };
    final ok = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        backgroundColor: widget.tokens.surface,
        title: Text(label),
        content: TextField(
          controller: ctrl,
          maxLines: mode == CuadernuToolMode.math ? 2 : 4,
          autofocus: true,
          decoration: InputDecoration(
            hintText: mode == CuadernuToolMode.math
                ? 'Ej. F = 10 kg * 9.81 m/s^2'
                : 'Escribe aquí…',
          ),
        ),
        actions: [
          TextButton(
              onPressed: () => Navigator.pop(ctx, false),
              child: const Text('Cancelar')),
          FilledButton(
              onPressed: () => Navigator.pop(ctx, true),
              child: const Text('Insertar')),
        ],
      ),
    );
    if (ok == true) {
      switch (mode) {
        case CuadernuToolMode.math:
          widget.controller.addMathBlock(
              x: docPos.dx, y: docPos.dy, expr: ctrl.text);
          break;
        case CuadernuToolMode.sticky:
          widget.controller.addStickyNote(
              x: docPos.dx, y: docPos.dy, text: ctrl.text);
          break;
        case CuadernuToolMode.tape:
          widget.controller.addStudyTape(
            x: docPos.dx,
            y: docPos.dy,
            w: 180,
            h: 36,
            label: ctrl.text,
          );
          break;
        default:
          widget.controller.addTextBox(
            x: docPos.dx,
            y: docPos.dy,
            text: ctrl.text,
            color: _inkColor,
          );
          break;
      }
    }
    ctrl.dispose();
  }

  Future<void> _handleExport(String format) async {
    String? path;
    String mime;
    String ext;
    switch (format) {
      case 'pdf':
        path = await widget.controller.exportPdfToTemp();
        mime = 'application/pdf';
        ext = 'pdf';
        break;
      case 'md':
        path = await widget.controller.exportMarkdownToTemp();
        mime = 'text/markdown';
        ext = 'md';
        break;
      case 'dxf':
        path = await widget.controller.exportDxfToTemp();
        mime = 'application/dxf';
        ext = 'dxf';
        break;
      default:
        widget.controller.saveCudoc();
        path = widget.controller.activeFilePath;
        mime = 'application/octet-stream';
        ext = 'cudoc';
        break;
    }
    if (path == null) {
      widget.onNotify('No se pudo exportar el documento');
      return;
    }
    final safe =
        widget.controller.title.replaceAll(RegExp(r'[^A-Za-z0-9._-]'), '_');
    final saved = await widget.onSaveFile(
      suggestedName: '${safe.isEmpty ? 'cuadernu' : safe}.$ext',
      mimeType: mime,
      sourcePath: path,
    );
    if (saved) widget.onNotify('Exportado como .$ext');
  }

  Future<void> _handleImport() async {
    final picked =
        await widget.onPickFile(mimeType: '*/*', prefix: 'cuadernu_import');
    if (picked == null) return;
    final path = picked['path'] ?? '';
    if (path.isEmpty) return;
    final rep = widget.controller.importExternalFile(path);
    final ok = rep['ok'] == true;
    widget.onNotify(ok
        ? 'Archivo importado (${picked['name'] ?? ''})'
        : 'Error al importar: ${rep['error'] ?? 'formato no soportado'}');
  }

  void _handlePointerDown(PointerDownEvent e) {
    final pos = e.localPosition;
    if (_tool == CuadernuToolMode.pan) {
      widget.controller.toggleStudyTapeAt(pos.dx, pos.dy);
      return;
    }
    if (_tool == CuadernuToolMode.pen ||
        _tool == CuadernuToolMode.highlighter) {
      final p = e.pressure > 0 ? e.pressure.clamp(0.15, 1.0) : 0.65;
      setState(() {
        _liveStroke
          ..clear()
          ..add((x: pos.dx, y: pos.dy, p: p));
      });
    } else if (_tool == CuadernuToolMode.eraser) {
      widget.controller.eraseAt(pos.dx, pos.dy, radius: 18);
    } else if (_tool == CuadernuToolMode.shape) {
      setState(() {
        _dragStart = pos;
        _dragCurrent = pos;
      });
    } else if (_tool == CuadernuToolMode.text ||
        _tool == CuadernuToolMode.math ||
        _tool == CuadernuToolMode.sticky ||
        _tool == CuadernuToolMode.tape) {
      unawaited(_promptInsertTextLike(pos, _tool));
    }
  }

  void _handlePointerMove(PointerMoveEvent e) {
    final pos = e.localPosition;
    if (_tool == CuadernuToolMode.pen ||
        _tool == CuadernuToolMode.highlighter) {
      final p = e.pressure > 0 ? e.pressure.clamp(0.15, 1.0) : 0.65;
      setState(() => _liveStroke.add((x: pos.dx, y: pos.dy, p: p)));
    } else if (_tool == CuadernuToolMode.eraser) {
      widget.controller.eraseAt(pos.dx, pos.dy, radius: 18);
    } else if (_tool == CuadernuToolMode.shape && _dragStart != null) {
      setState(() => _dragCurrent = pos);
    }
  }

  void _handlePointerUp(PointerUpEvent e) {
    if ((_tool == CuadernuToolMode.pen ||
            _tool == CuadernuToolMode.highlighter) &&
        _liveStroke.length >= 2) {
      final isHi = _tool == CuadernuToolMode.highlighter;
      final col = isHi ? _inkColor.withValues(alpha: 0.34) : _inkColor;
      widget.controller.addInkStroke(
        points: List.of(_liveStroke),
        color: col,
        strokeWidth: isHi ? 14.0 : _strokeWidth,
        penStyle: isHi ? 4 : CuadernuPenStyle.fountain,
        isHighlighter: isHi,
      );
      setState(() => _liveStroke.clear());
    } else if (_tool == CuadernuToolMode.shape &&
        _dragStart != null &&
        _dragCurrent != null) {
      final rect = Rect.fromPoints(_dragStart!, _dragCurrent!);
      if (rect.width > 6 || rect.height > 6) {
        widget.controller.addShape(
          shapeKind: _shapeKind,
          x: rect.left,
          y: rect.top,
          w: math.max(12, rect.width),
          h: math.max(12, rect.height),
          strokeColor: _inkColor,
          strokeWidth: _strokeWidth,
        );
      }
      setState(() {
        _dragStart = null;
        _dragCurrent = null;
      });
    }
  }

  Widget _toolButton(
    TokensInterfaz t,
    CuadernuToolMode mode,
    IconData icon,
    String tooltip,
  ) {
    final selected = _tool == mode;
    return IconButton(
      tooltip: tooltip,
      visualDensity: VisualDensity.compact,
      onPressed: () => setState(() => _tool = mode),
      style: IconButton.styleFrom(
        backgroundColor: selected ? t.accent.withValues(alpha: 0.14) : null,
      ),
      icon: Icon(
        icon,
        size: 19,
        color: selected ? t.accent : t.textPrimary,
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final t = widget.tokens;
    return AnimatedBuilder(
      animation: widget.controller,
      builder: (context, _) {
        final ctrl = widget.controller;
        if (!ctrl.isReady) {
          return Center(
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                CircularProgressIndicator(strokeWidth: 2, color: t.accent),
                const SizedBox(height: 10),
                Text('Preparando cuaderno…',
                    style: TextStyle(color: t.textSecondary, fontSize: 13.5)),
              ],
            ),
          );
        }

        final canvasSize = ctrl.infiniteCanvas
            ? const Size(1600, 1600)
            : const Size(_pageW, _pageH);

        return Column(
          children: [
            // Secondary clean notebook toolbar (guías de interfaz)
            TranslucentSurface(
              tokens: t,
              reduceTransparency: widget.reduceTransparency,
              bottomBorderOnly: true,
              padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 5),
              child: SingleChildScrollView(
                scrollDirection: Axis.horizontal,
                child: Row(
                  children: [
                    TextButton.icon(
                      onPressed: _openVaultSheet,
                      icon: Icon(CupertinoIcons.book,
                          size: 17, color: t.accent),
                      label: Text(
                        ctrl.title,
                        style: TextStyle(
                          color: t.textPrimary,
                          fontSize: 13.5,
                          fontWeight: FontWeight.w600,
                        ),
                      ),
                    ),
                    const SizedBox(width: 4),
                    _toolButton(t, CuadernuToolMode.pen,
                        CupertinoIcons.pencil, 'Pluma'),
                    _toolButton(t, CuadernuToolMode.highlighter,
                        CupertinoIcons.paintbrush, 'Resaltador'),
                    _toolButton(t, CuadernuToolMode.eraser,
                        CupertinoIcons.clear_thick, 'Borrador'),
                    _toolButton(t, CuadernuToolMode.pan,
                        CupertinoIcons.hand_draw, 'Mover / Revelar cinta'),
                    _toolButton(t, CuadernuToolMode.text,
                        CupertinoIcons.textbox, 'Texto'),
                    _toolButton(t, CuadernuToolMode.shape,
                        CupertinoIcons.square_on_circle, 'Forma'),
                    _toolButton(t, CuadernuToolMode.math,
                        CupertinoIcons.function, 'Bloque matemático'),
                    _toolButton(t, CuadernuToolMode.sticky,
                        CupertinoIcons.square_pencil, 'Nota adhesiva'),
                    _toolButton(t, CuadernuToolMode.tape,
                        CupertinoIcons.eye_slash, 'Cinta de estudio'),
                    const SizedBox(width: 4),
                    for (final c in _inkPalette)
                      GestureDetector(
                        onTap: () => setState(() => _inkColor = c),
                        child: Container(
                          width: 18,
                          height: 18,
                          margin: const EdgeInsets.symmetric(horizontal: 3),
                          decoration: BoxDecoration(
                            color: c,
                            shape: BoxShape.circle,
                            border: Border.all(
                              color: _inkColor == c ? t.accent : t.border,
                              width: _inkColor == c ? 2.2 : 1,
                            ),
                          ),
                        ),
                      ),
                    const SizedBox(width: 4),
                    IconButton(
                      tooltip: 'Deshacer',
                      visualDensity: VisualDensity.compact,
                      onPressed: ctrl.canUndo ? ctrl.undo : null,
                      icon: Icon(CupertinoIcons.arrow_uturn_left,
                          size: 18,
                          color: ctrl.canUndo ? t.textPrimary : t.border),
                    ),
                    IconButton(
                      tooltip: 'Rehacer',
                      visualDensity: VisualDensity.compact,
                      onPressed: ctrl.canRedo ? ctrl.redo : null,
                      icon: Icon(CupertinoIcons.arrow_uturn_right,
                          size: 18,
                          color: ctrl.canRedo ? t.textPrimary : t.border),
                    ),
                    if (widget.onInsertActiveCasio != null)
                      IconButton(
                        tooltip: 'Insertar pantalla Casio',
                        visualDensity: VisualDensity.compact,
                        onPressed: widget.onInsertActiveCasio,
                        icon: Icon(CupertinoIcons.camera_viewfinder,
                            size: 19, color: t.accent),
                      ),
                    IconButton(
                      tooltip: 'Buscar en el cuaderno',
                      visualDensity: VisualDensity.compact,
                      onPressed: () =>
                          setState(() => _showSearch = !_showSearch),
                      icon: Icon(CupertinoIcons.search,
                          size: 18,
                          color: _showSearch ? t.accent : t.textPrimary),
                    ),
                    PopupMenuButton<String>(
                      tooltip: 'Documento',
                      icon: Icon(CupertinoIcons.doc_plaintext,
                          size: 19, color: t.textPrimary),
                      onSelected: (v) {
                        if (v == 'toggle_mode') {
                          ctrl.setInfiniteCanvas(!ctrl.infiniteCanvas);
                        } else if (v == 'add_page') {
                          ctrl.addPage();
                        } else if (v == 'eval_dag') {
                          final rep = ctrl.evaluateMathDag();
                          widget.onNotify(
                              'Fórmulas evaluadas: ${rep['evaluatedBlocks'] ?? 0}');
                        } else if (v == 'import') {
                          unawaited(_handleImport());
                        } else if (v.startsWith('export_')) {
                          unawaited(_handleExport(v.substring(7)));
                        } else if (v.startsWith('paper_')) {
                          ctrl.setPaperPattern(
                              int.parse(v.substring(6)));
                        } else if (v.startsWith('shape_')) {
                          setState(() {
                            _shapeKind = int.parse(v.substring(6));
                            _tool = CuadernuToolMode.shape;
                          });
                        }
                      },
                      itemBuilder: (_) => [
                        PopupMenuItem(
                          value: 'toggle_mode',
                          child: Text(ctrl.infiniteCanvas
                              ? 'Cambiar a cuaderno paginado'
                              : 'Cambiar a lienzo infinito'),
                        ),
                        if (!ctrl.infiniteCanvas)
                          const PopupMenuItem(
                            value: 'add_page',
                            child: Text('Agregar página'),
                          ),
                        const PopupMenuItem(
                          value: 'eval_dag',
                          child: Text('Evaluar fórmulas (=)'),
                        ),
                        const PopupMenuDivider(),
                        for (final p in CuadernuPaperPattern.all.take(6))
                          CheckedPopupMenuItem(
                            value: 'paper_${p.$1}',
                            checked: ctrl.paperPattern == p.$1,
                            child: Text('Papel: ${p.$2}'),
                          ),
                        const PopupMenuDivider(),
                        const PopupMenuItem(
                            value: 'shape_0', child: Text('Forma: Línea')),
                        const PopupMenuItem(
                            value: 'shape_1', child: Text('Forma: Flecha')),
                        const PopupMenuItem(
                            value: 'shape_2', child: Text('Forma: Rectángulo')),
                        const PopupMenuItem(
                            value: 'shape_3', child: Text('Forma: Elipse')),
                        const PopupMenuDivider(),
                        const PopupMenuItem(
                          value: 'import',
                          child: Text('Importar PDF / DOCX / PPTX / CSV / .cudoc…'),
                        ),
                        const PopupMenuItem(
                          value: 'export_pdf',
                          child: Text('Exportar PDF vectorial…'),
                        ),
                        const PopupMenuItem(
                          value: 'export_cudoc',
                          child: Text('Exportar .cudoc (compatible Windows)…'),
                        ),
                        const PopupMenuItem(
                          value: 'export_md',
                          child: Text('Exportar Markdown…'),
                        ),
                        const PopupMenuItem(
                          value: 'export_dxf',
                          child: Text('Exportar CAD DXF…'),
                        ),
                      ],
                    ),
                  ],
                ),
              ),
            ),
            if (_showSearch)
              Container(
                padding:
                    const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
                color: t.surface,
                child: Row(
                  children: [
                    Expanded(
                      child: TextField(
                        controller: _searchCtrl,
                        style: TextStyle(color: t.textPrimary, fontSize: 13.5),
                        decoration: InputDecoration(
                          isDense: true,
                          hintText: 'Buscar texto o fórmulas (sin tildes)…',
                          hintStyle: TextStyle(color: t.textSecondary),
                          border: InputBorder.none,
                        ),
                        onChanged: (q) => setState(
                            () => _searchHits = ctrl.search(q)),
                      ),
                    ),
                    Text(
                      '${_searchHits.length} resultados',
                      style: TextStyle(color: t.textSecondary, fontSize: 12),
                    ),
                  ],
                ),
              ),
            Expanded(
              child: InteractiveViewer(
                panEnabled: _tool == CuadernuToolMode.pan,
                scaleEnabled: _tool == CuadernuToolMode.pan,
                minScale: 0.45,
                maxScale: 4.0,
                boundaryMargin: const EdgeInsets.all(120),
                child: Center(
                  child: Container(
                    margin: const EdgeInsets.all(14),
                    decoration: BoxDecoration(
                      color: Colors.white,
                      borderRadius: BorderRadius.circular(6),
                      border: Border.all(color: t.border, width: t.borderWidth),
                      boxShadow: const [
                        BoxShadow(
                          color: Color(0x14000000),
                          blurRadius: 12,
                          offset: Offset(0, 4),
                        ),
                      ],
                    ),
                    child: Listener(
                      onPointerDown: _handlePointerDown,
                      onPointerMove: _handlePointerMove,
                      onPointerUp: _handlePointerUp,
                      child: ClipRRect(
                        borderRadius: BorderRadius.circular(6),
                        child: CustomPaint(
                          size: canvasSize,
                          painter: _CuadernuCanvasPainter(
                            paperPattern: ctrl.paperPattern,
                            objects: ctrl.activeObjects,
                            liveStroke: _liveStroke,
                            liveColor: _tool == CuadernuToolMode.highlighter
                                ? _inkColor.withValues(alpha: 0.34)
                                : _inkColor,
                            liveWidth: _tool == CuadernuToolMode.highlighter
                                ? 14.0
                                : _strokeWidth,
                            dragStart: _dragStart,
                            dragCurrent: _dragCurrent,
                            dragShapeKind: _shapeKind,
                            accent: t.accent,
                            assetResolver: ctrl.assetImage,
                          ),
                        ),
                      ),
                    ),
                  ),
                ),
              ),
            ),
            if (!ctrl.infiniteCanvas && ctrl.pageCount > 1)
              Container(
                padding:
                    const EdgeInsets.symmetric(horizontal: 14, vertical: 6),
                decoration: BoxDecoration(
                  color: t.surface,
                  border: Border(
                      top: BorderSide(color: t.border, width: t.borderWidth)),
                ),
                child: Row(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    IconButton(
                      visualDensity: VisualDensity.compact,
                      onPressed: ctrl.activePage > 0
                          ? () => ctrl.setActivePage(ctrl.activePage - 1)
                          : null,
                      icon: const Icon(CupertinoIcons.chevron_left, size: 17),
                    ),
                    Text(
                      'Página ${ctrl.activePage + 1} / ${ctrl.pageCount}',
                      style: TextStyle(color: t.textPrimary, fontSize: 13),
                    ),
                    IconButton(
                      visualDensity: VisualDensity.compact,
                      onPressed: ctrl.activePage < ctrl.pageCount - 1
                          ? () => ctrl.setActivePage(ctrl.activePage + 1)
                          : null,
                      icon: const Icon(CupertinoIcons.chevron_right, size: 17),
                    ),
                  ],
                ),
              ),
          ],
        );
      },
    );
  }
}

class _CuadernuCanvasPainter extends CustomPainter {
  final int paperPattern;
  final List<Map<String, dynamic>> objects;
  final List<({double x, double y, double p})> liveStroke;
  final Color liveColor;
  final double liveWidth;
  final Offset? dragStart;
  final Offset? dragCurrent;
  final int dragShapeKind;
  final Color accent;
  final ui.Image? Function(String key) assetResolver;

  const _CuadernuCanvasPainter({
    required this.paperPattern,
    required this.objects,
    required this.liveStroke,
    required this.liveColor,
    required this.liveWidth,
    required this.dragStart,
    required this.dragCurrent,
    required this.dragShapeKind,
    required this.accent,
    required this.assetResolver,
  });

  void _paintPaper(Canvas canvas, Size size) {
    final linePaint = Paint()
      ..color = const Color(0xFFE5E7EB)
      ..strokeWidth = 0.75;
    switch (paperPattern) {
      case CuadernuPaperPattern.ruled:
        for (double y = 32; y < size.height; y += 26) {
          canvas.drawLine(Offset(0, y), Offset(size.width, y), linePaint);
        }
        canvas.drawLine(
          const Offset(56, 0),
          Offset(56, size.height),
          Paint()
            ..color = const Color(0xFFFCA5A5)
            ..strokeWidth = 0.9,
        );
        break;
      case CuadernuPaperPattern.grid:
      case CuadernuPaperPattern.millimeter:
        final step = paperPattern == CuadernuPaperPattern.millimeter ? 14.0 : 24.0;
        for (double x = step; x < size.width; x += step) {
          canvas.drawLine(Offset(x, 0), Offset(x, size.height), linePaint);
        }
        for (double y = step; y < size.height; y += step) {
          canvas.drawLine(Offset(0, y), Offset(size.width, y), linePaint);
        }
        break;
      case CuadernuPaperPattern.dotGrid:
        final dotPaint = Paint()..color = const Color(0xFFCBD5E1);
        for (double x = 24; x < size.width; x += 24) {
          for (double y = 24; y < size.height; y += 24) {
            canvas.drawCircle(Offset(x, y), 1.1, dotPaint);
          }
        }
        break;
      case CuadernuPaperPattern.cornell:
        for (double y = 70; y < size.height - 90; y += 26) {
          canvas.drawLine(Offset(0, y), Offset(size.width, y), linePaint);
        }
        final divider = Paint()
          ..color = const Color(0xFF93C5FD)
          ..strokeWidth = 1.1;
        canvas.drawLine(Offset(150, 70), Offset(150, size.height - 90), divider);
        canvas.drawLine(Offset(0, size.height - 90),
            Offset(size.width, size.height - 90), divider);
        break;
      case CuadernuPaperPattern.polar:
        final c = Offset(size.width / 2, size.height / 2);
        final maxR = math.min(size.width, size.height) * 0.44;
        for (int i = 1; i <= 5; i++) {
          canvas.drawCircle(c, maxR * i / 5, linePaint..style = PaintingStyle.stroke);
        }
        for (int deg = 0; deg < 360; deg += 30) {
          final rad = deg * math.pi / 180.0;
          canvas.drawLine(
            c,
            Offset(c.dx + math.cos(rad) * maxR, c.dy + math.sin(rad) * maxR),
            linePaint,
          );
        }
        break;
      default:
        break;
    }
  }

  void _paintStrokePoints(
    Canvas canvas,
    List<({double x, double y, double p})> pts,
    Color color,
    double width,
  ) {
    if (pts.length < 2) return;
    final paint = Paint()
      ..color = color
      ..strokeCap = StrokeCap.round
      ..strokeJoin = StrokeJoin.round
      ..style = PaintingStyle.stroke;
    for (int i = 1; i < pts.length; i++) {
      final a = pts[i - 1];
      final b = pts[i];
      paint.strokeWidth = width * (0.65 + 0.55 * ((a.p + b.p) * 0.5));
      canvas.drawLine(Offset(a.x, a.y), Offset(b.x, b.y), paint);
    }
  }

  void _paintShape(Canvas canvas, int kind, Rect rect, Color stroke, double w) {
    final p = Paint()
      ..color = stroke
      ..strokeWidth = w
      ..style = PaintingStyle.stroke;
    switch (kind) {
      case CuadernuShapeKind.line:
        canvas.drawLine(rect.topLeft, rect.bottomRight, p);
        break;
      case CuadernuShapeKind.arrow:
        canvas.drawLine(rect.topLeft, rect.bottomRight, p);
        final angle = math.atan2(rect.height, rect.width);
        const head = 11.0;
        final end = rect.bottomRight;
        canvas.drawLine(
          end,
          Offset(end.dx - head * math.cos(angle - 0.5),
              end.dy - head * math.sin(angle - 0.5)),
          p,
        );
        canvas.drawLine(
          end,
          Offset(end.dx - head * math.cos(angle + 0.5),
              end.dy - head * math.sin(angle + 0.5)),
          p,
        );
        break;
      case CuadernuShapeKind.ellipse:
        canvas.drawOval(rect, p);
        break;
      default:
        canvas.drawRRect(
            RRect.fromRectAndRadius(rect, const Radius.circular(4)), p);
        break;
    }
  }

  void _drawLabel(
    Canvas canvas,
    String text,
    Rect bounds, {
    required Color color,
    double fontSize = 13,
    FontWeight weight = FontWeight.w400,
  }) {
    if (text.isEmpty) return;
    final tp = TextPainter(
      text: TextSpan(
        text: text,
        style: TextStyle(color: color, fontSize: fontSize, fontWeight: weight),
      ),
      textDirection: TextDirection.ltr,
      maxLines: 12,
      ellipsis: '…',
    )..layout(maxWidth: math.max(20, bounds.width - 12));
    tp.paint(canvas, Offset(bounds.left + 6, bounds.top + 6));
  }

  @override
  void paint(Canvas canvas, Size size) {
    _paintPaper(canvas, size);

    for (final obj in objects) {
      final kind = (obj['kind'] as num?)?.toInt() ?? 0;
      final b = obj['bounds'] as List? ?? const [0, 0, 100, 40];
      final rect = Rect.fromLTWH(
        (b[0] as num).toDouble(),
        (b[1] as num).toDouble(),
        (b[2] as num).toDouble(),
        (b[3] as num).toDouble(),
      );
      final stroke = studioRgbaToColor(
          (obj['strokeRgba'] as num?)?.toInt() ?? 0x1D1D1FFF);
      final fill =
          studioRgbaToColor((obj['fillRgba'] as num?)?.toInt() ?? 0x00000000);
      final sw = (obj['strokeWidth'] as num?)?.toDouble() ?? 2.2;

      if (kind == 0) {
        final rawPts = obj['inkPoints'] as List? ?? const [];
        final pts = rawPts
            .whereType<List>()
            .where((p) => p.length >= 2)
            .map((p) => (
                  x: (p[0] as num).toDouble(),
                  y: (p[1] as num).toDouble(),
                  p: p.length >= 3 ? (p[2] as num).toDouble() : 0.65,
                ))
            .toList();
        _paintStrokePoints(canvas, pts, stroke, sw);
      } else if (kind == 1) {
        final sk = (obj['shapeKind'] as num?)?.toInt() ?? 2;
        if (fill.a > 0) {
          canvas.drawRRect(
              RRect.fromRectAndRadius(rect, const Radius.circular(6)),
              Paint()..color = fill);
        }
        _paintShape(canvas, sk, rect, stroke, sw);
        final txt = obj['text']?.toString() ?? '';
        if (txt.isNotEmpty) {
          _drawLabel(canvas, txt, rect, color: const Color(0xFF1D1D1F));
        }
      } else if (kind == 2 || kind == 11) {
        if (fill.a > 0) {
          canvas.drawRRect(
            RRect.fromRectAndRadius(rect, const Radius.circular(6)),
            Paint()..color = fill,
          );
        }
        _drawLabel(
          canvas,
          obj['text']?.toString() ?? '',
          rect,
          color: stroke.a > 0 ? stroke : const Color(0xFF1D1D1F),
          fontSize: (obj['fontSizePt'] as num?)?.toDouble() ?? 13.5,
        );
      } else if (kind == 5) {
        // Study Tape
        final revealed = obj['tapeRevealed'] == true;
        final rrect = RRect.fromRectAndRadius(rect, const Radius.circular(6));
        canvas.drawRRect(
          rrect,
          Paint()
            ..color = revealed
                ? const Color(0x22F59E0B)
                : const Color(0xFFF59E0B),
        );
        canvas.drawRRect(
          rrect,
          Paint()
            ..color = const Color(0xFFB45309)
            ..style = PaintingStyle.stroke
            ..strokeWidth = 1.2,
        );
        _drawLabel(
          canvas,
          revealed
              ? (obj['text']?.toString() ?? 'Revelado')
              : 'Cinta de estudio (toca para ver)',
          rect,
          color: revealed ? const Color(0xFF92400E) : Colors.white,
          fontSize: 12,
          weight: FontWeight.w600,
        );
      } else if (kind == 6) {
        // MathBlock (#varsub)
        final rrect = RRect.fromRectAndRadius(rect, const Radius.circular(8));
        canvas.drawRRect(rrect, Paint()..color = const Color(0xFFF0F6FF));
        canvas.drawRRect(
          rrect,
          Paint()
            ..color = accent.withValues(alpha: 0.45)
            ..style = PaintingStyle.stroke
            ..strokeWidth = 1.0,
        );
        final expr = obj['formulaExpr']?.toString() ?? '';
        final evalRes = obj['evaluatedResult']?.toString() ?? '';
        _drawLabel(
          canvas,
          '$expr\n= $evalRes',
          rect,
          color: const Color(0xFF0F172A),
          fontSize: 13.5,
          weight: FontWeight.w600,
        );
      } else if (kind == 8) {
        // CalcSessionCard (live Casio capture embedded inside notebook!)
        final rrect = RRect.fromRectAndRadius(rect, const Radius.circular(10));
        canvas.drawRRect(rrect, Paint()..color = const Color(0xFFF8FAFC));
        canvas.drawRRect(
          rrect,
          Paint()
            ..color = const Color(0xFFCBD5E1)
            ..style = PaintingStyle.stroke
            ..strokeWidth = 1.0,
        );
        final model = obj['calcModelId']?.toString() ?? 'Casio';
        final keySeq = obj['calcKeySeq']?.toString() ?? '';
        _drawLabel(
          canvas,
          model,
          Rect.fromLTWH(rect.left, rect.top, rect.width, 24),
          color: accent,
          fontSize: 11.5,
          weight: FontWeight.w700,
        );
        final assetKey = obj['assetKey']?.toString() ?? '';
        final img = assetKey.isNotEmpty ? assetResolver(assetKey) : null;
        final imgRect = Rect.fromLTWH(
          rect.left + 8,
          rect.top + 24,
          rect.width - 16,
          math.max(24, rect.height - 52),
        );
        if (img != null) {
          canvas.drawImageRect(
            img,
            Rect.fromLTWH(0, 0, img.width.toDouble(), img.height.toDouble()),
            imgRect,
            Paint()..filterQuality = FilterQuality.none,
          );
        } else {
          canvas.drawRect(imgRect, Paint()..color = const Color(0xFFE2E8F0));
        }
        if (keySeq.isNotEmpty) {
          _drawLabel(
            canvas,
            keySeq,
            Rect.fromLTWH(rect.left, rect.bottom - 24, rect.width, 22),
            color: const Color(0xFF475569),
            fontSize: 10.5,
          );
        }
      } else if (kind == 3 || kind == 4) {
        final rrect = RRect.fromRectAndRadius(rect, const Radius.circular(8));
        canvas.drawRRect(rrect, Paint()..color = const Color(0xFFF8FAFC));
        canvas.drawRRect(
          rrect,
          Paint()
            ..color = const Color(0xFFCBD5E1)
            ..style = PaintingStyle.stroke
            ..strokeWidth = 1.0,
        );
        _drawLabel(
          canvas,
          obj['printoutText']?.toString().isNotEmpty == true
              ? obj['printoutText'].toString()
              : (obj['caption']?.toString() ?? 'Documento adjunto'),
          rect,
          color: const Color(0xFF1E293B),
          fontSize: 12,
        );
      }
    }

    if (liveStroke.length >= 2) {
      _paintStrokePoints(canvas, liveStroke, liveColor, liveWidth);
    }
    if (dragStart != null && dragCurrent != null) {
      _paintShape(
        canvas,
        dragShapeKind,
        Rect.fromPoints(dragStart!, dragCurrent!),
        liveColor,
        liveWidth,
      );
    }
  }

  @override
  bool shouldRepaint(covariant _CuadernuCanvasPainter oldDelegate) => true;
}
