import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';
import 'package:path_provider/path_provider.dart';

import '../native/cuadernu_studio_bindings.dart';
import '../theme/tema_interfaz.dart';
import 'cuadernu_models.dart';

enum _StudySection { graph, database, flashcards }

class CuadernuKnowledgeStudyView extends StatefulWidget {
  final CuadernuStudioBindings? studio;
  final TokensInterfaz tokens;
  final bool reduceTransparency;
  final List<Map<String, dynamic>> Function()? onHarvestDocFlashcards;
  final void Function(String message)? onNotify;

  const CuadernuKnowledgeStudyView({
    super.key,
    required this.studio,
    required this.tokens,
    required this.reduceTransparency,
    this.onHarvestDocFlashcards,
    this.onNotify,
  });

  @override
  State<CuadernuKnowledgeStudyView> createState() =>
      _CuadernuKnowledgeStudyViewState();
}

class _CuadernuKnowledgeStudyViewState
    extends State<CuadernuKnowledgeStudyView> {
  _StudySection _section = _StudySection.graph;

  // --- Knowledge Graph ---
  List<CuadernuKnowledgeNote> _notes = [
    CuadernuKnowledgeNote(
      id: 'n1',
      title: 'Leyes de Kirchhoff',
      folder: 'Circuitos',
      body:
          'La suma de corrientes en un nodo es cero. Se combina con [[Análisis Nodal MNA]] y [[Ley de Ohm]].',
      tags: ['circuitos', 'mna'],
    ),
    CuadernuKnowledgeNote(
      id: 'n2',
      title: 'Análisis Nodal MNA',
      folder: 'Circuitos',
      body:
          'Aplica [[Leyes de Kirchhoff]] para obtener tensiones nodales y función de transferencia en [[Transformada de Laplace]].',
      tags: ['circuitos', 'laplace'],
    ),
    CuadernuKnowledgeNote(
      id: 'n3',
      title: 'Transformada de Laplace',
      folder: 'Matemática',
      body:
          'Convierte ecuaciones diferenciales en álgebra racional H(s) usada en [[Análisis Nodal MNA]].',
      tags: ['matematica', 'sistemas'],
    ),
    CuadernuKnowledgeNote(
      id: 'n4',
      title: 'Ley de Ohm',
      folder: 'Circuitos',
      body: 'V = I · R. Base lineal para [[Leyes de Kirchhoff]].',
      tags: ['circuitos'],
    ),
  ];
  Map<String, dynamic> _graphData = const {};
  int _selectedNoteIdx = 0;
  final TextEditingController _noteTitleCtrl = TextEditingController();
  final TextEditingController _noteBodyCtrl = TextEditingController();

  // --- Structured Database ---
  bool _kanbanMode = false;
  List<CuadernuDbItem> _dbItems = [
    CuadernuDbItem(
      id: 'd1',
      title: 'Laboratorio Filtro RC',
      course: 'EL2113',
      status: 'En curso',
      priority: 'Alta',
      dueDate: 'Vie 18:00',
    ),
    CuadernuDbItem(
      id: 'd2',
      title: 'Práctica Integrales Impropias',
      course: 'MA1102',
      status: 'Pendiente',
      priority: 'Media',
      dueDate: 'Lun 12:00',
    ),
    CuadernuDbItem(
      id: 'd3',
      title: 'Balanceo Estequiométrico',
      course: 'QU1106',
      status: 'Completado',
      priority: 'Baja',
      dueDate: 'Entregado',
    ),
  ];

  // --- SM-2 Flashcards + Pomodoro ---
  List<CuadernuFlashcard> _cards = [
    CuadernuFlashcard(
      id: 'c1',
      front: '¿Cuál es la frecuencia de corte de un filtro RC pasa-bajos?',
      back: 'fc = 1 / (2·π·R·C)',
      source: 'Circuitos',
    ),
    CuadernuFlashcard(
      id: 'c2',
      front: 'Ecuación balanceada de oxidación del hierro (Fe + O₂)',
      back: '4 Fe + 3 O₂ → 2 Fe₂O₃',
      source: 'Química',
    ),
  ];
  int _cardIdx = 0;
  bool _showBack = false;
  int _pomodoroSec = 25 * 60;
  bool _pomodoroRunning = false;
  Timer? _pomodoroTimer;

  @override
  void initState() {
    super.initState();
    _syncSelectedNoteControllers();
    unawaited(_loadPersistedData());
    _rebuildGraph();
  }

  @override
  void dispose() {
    _pomodoroTimer?.cancel();
    _noteTitleCtrl.dispose();
    _noteBodyCtrl.dispose();
    super.dispose();
  }

  Future<File> _stateFile() async {
    final dir = await getApplicationDocumentsDirectory();
    final folder = Directory('${dir.path}/cuadernu_vault');
    if (!await folder.exists()) await folder.create(recursive: true);
    return File('${folder.path}/study_state.json');
  }

  Future<void> _loadPersistedData() async {
    try {
      final f = await _stateFile();
      if (!await f.exists()) return;
      final map = jsonDecode(await f.readAsString());
      if (map is! Map<String, dynamic>) return;
      if (map['notes'] is List && (map['notes'] as List).isNotEmpty) {
        _notes = (map['notes'] as List)
            .whereType<Map>()
            .map((e) => CuadernuKnowledgeNote.fromJson(Map<String, dynamic>.from(e)))
            .toList();
      }
      if (map['dbItems'] is List) {
        _dbItems = (map['dbItems'] as List)
            .whereType<Map>()
            .map((e) => CuadernuDbItem.fromJson(Map<String, dynamic>.from(e)))
            .toList();
      }
      if (map['cards'] is List && (map['cards'] as List).isNotEmpty) {
        _cards = (map['cards'] as List)
            .whereType<Map>()
            .map((e) => CuadernuFlashcard.fromJson(Map<String, dynamic>.from(e)))
            .toList();
      }
      if (mounted) {
        _selectedNoteIdx = _selectedNoteIdx.clamp(0, _notes.length - 1);
        _syncSelectedNoteControllers();
        _rebuildGraph();
      }
    } catch (_) {}
  }

  Future<void> _persistData() async {
    try {
      final f = await _stateFile();
      final payload = jsonEncode({
        'notes': _notes.map((n) => n.toJson()).toList(),
        'dbItems': _dbItems.map((d) => d.toJson()).toList(),
        'cards': _cards.map((c) => c.toJson()).toList(),
      });
      await f.writeAsString(payload, flush: true);
    } catch (_) {}
  }

  void _syncSelectedNoteControllers() {
    if (_notes.isEmpty) return;
    final n = _notes[_selectedNoteIdx.clamp(0, _notes.length - 1)];
    _noteTitleCtrl.text = n.title;
    _noteBodyCtrl.text = n.body;
  }

  void _rebuildGraph() {
    final s = widget.studio;
    if (s == null || !s.isAvailable) return;
    final data = s.buildKnowledgeGraph(_notes.map((e) => e.toJson()).toList());
    setState(() => _graphData = data);
  }

  void _saveCurrentNote() {
    if (_notes.isEmpty) return;
    final n = _notes[_selectedNoteIdx];
    n.title = _noteTitleCtrl.text.trim().isEmpty
        ? 'Nota sin título'
        : _noteTitleCtrl.text.trim();
    n.body = _noteBodyCtrl.text;
    _rebuildGraph();
    unawaited(_persistData());
  }

  void _addNote() {
    final note = CuadernuKnowledgeNote(
      id: 'n_${DateTime.now().millisecondsSinceEpoch}',
      title: 'Nueva nota ${_notes.length + 1}',
      folder: 'General',
      body: 'Relacionada con [[${_notes.isNotEmpty ? _notes.first.title : 'Tema'}]].',
      tags: ['apuntes'],
    );
    setState(() {
      _notes.add(note);
      _selectedNoteIdx = _notes.length - 1;
      _syncSelectedNoteControllers();
    });
    _rebuildGraph();
    unawaited(_persistData());
  }

  void _harvestFromDocument() {
    final harvested = widget.onHarvestDocFlashcards?.call() ?? const [];
    if (harvested.isEmpty) {
      widget.onNotify?.call(
          'Agrega cintas de estudio, bloques matemáticos o tarjetas Casio en el cuaderno');
      return;
    }
    int added = 0;
    final existing = _cards.map((c) => c.front).toSet();
    for (final h in harvested) {
      final front =
          (h['question'] ?? h['front'])?.toString() ?? '';
      final back =
          (h['answer'] ?? h['back'])?.toString() ?? '';
      if (front.isNotEmpty && !existing.contains(front)) {
        _cards.add(CuadernuFlashcard(
          id: 'doc_${DateTime.now().microsecondsSinceEpoch}_$added',
          front: front,
          back: back,
          source: 'Cuaderno',
        ));
        existing.add(front);
        added++;
      }
    }
    setState(() {});
    unawaited(_persistData());
    widget.onNotify?.call(added > 0
        ? '$added tarjetas importadas del cuaderno activo'
        : 'Todas las tarjetas del cuaderno ya estaban sincronizadas');
  }

  void _rateFlashcard(int quality) {
    if (_cards.isEmpty) return;
    final card = _cards[_cardIdx % _cards.length];
    final s = widget.studio;
    if (s != null && s.isAvailable) {
      final next = s.reviewSm2(
        repetitions: card.repetitions,
        intervalDays: card.intervalDays,
        easeFactor: card.easeFactor,
        quality: quality,
      );
      card.repetitions = (next['repetitions'] as num?)?.toInt() ?? card.repetitions;
      card.intervalDays = (next['intervalDays'] as num?)?.toInt() ?? card.intervalDays;
      card.easeFactor = (next['easeFactor'] as num?)?.toDouble() ?? card.easeFactor;
      card.nextReviewEpoch =
          (next['dueEpochSec'] as num?)?.toInt() ?? card.nextReviewEpoch;
    }
    setState(() {
      _showBack = false;
      _cardIdx = (_cardIdx + 1) % _cards.length;
    });
    unawaited(_persistData());
  }

  void _togglePomodoro() {
    if (_pomodoroRunning) {
      _pomodoroTimer?.cancel();
      setState(() => _pomodoroRunning = false);
      return;
    }
    setState(() => _pomodoroRunning = true);
    _pomodoroTimer = Timer.periodic(const Duration(seconds: 1), (timer) {
      if (!mounted) {
        timer.cancel();
        return;
      }
      if (_pomodoroSec <= 1) {
        timer.cancel();
        setState(() {
          _pomodoroRunning = false;
          _pomodoroSec = 25 * 60;
        });
        widget.onNotify?.call('Sesión Pomodoro completada');
      } else {
        setState(() => _pomodoroSec--);
      }
    });
  }

  Future<void> _showAddDbItemDialog() async {
    final titleCtrl = TextEditingController();
    final courseCtrl = TextEditingController(text: 'MA1102');
    final dueCtrl = TextEditingController(text: 'Próxima semana');
    final ok = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        backgroundColor: widget.tokens.surface,
        title: const Text('Nuevo registro'),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            TextField(
              controller: titleCtrl,
              decoration: const InputDecoration(labelText: 'Título'),
            ),
            TextField(
              controller: courseCtrl,
              decoration: const InputDecoration(labelText: 'Curso'),
            ),
            TextField(
              controller: dueCtrl,
              decoration: const InputDecoration(labelText: 'Entrega'),
            ),
          ],
        ),
        actions: [
          TextButton(
              onPressed: () => Navigator.pop(ctx, false),
              child: const Text('Cancelar')),
          FilledButton(
              onPressed: () => Navigator.pop(ctx, true),
              child: const Text('Guardar')),
        ],
      ),
    );
    if (ok == true && titleCtrl.text.trim().isNotEmpty) {
      setState(() {
        _dbItems.add(CuadernuDbItem(
          id: 'db_${DateTime.now().millisecondsSinceEpoch}',
          title: titleCtrl.text.trim(),
          course: courseCtrl.text.trim(),
          status: 'Pendiente',
          priority: 'Media',
          dueDate: dueCtrl.text.trim(),
        ));
      });
      unawaited(_persistData());
    }
    titleCtrl.dispose();
    courseCtrl.dispose();
    dueCtrl.dispose();
  }

  Widget _panel(TokensInterfaz t, Widget child) => Container(
        width: double.infinity,
        padding: const EdgeInsets.all(14),
        decoration: BoxDecoration(
          color: t.surface,
          borderRadius: BorderRadius.circular(12),
          border: Border.all(color: t.border, width: t.borderWidth),
        ),
        child: child,
      );

  Widget _buildGraphTab(TokensInterfaz t) {
    final nodes = (_graphData['nodes'] as List?) ?? const [];
    final edges = (_graphData['edges'] as List?) ?? const [];
    final brokenCount = (_graphData['brokenLinkCount'] as num?)?.toInt() ?? 0;

    return ListView(
      padding: const EdgeInsets.all(14),
      children: [
        _panel(
          t,
          Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Row(
                children: [
                  Text(
                    '${nodes.length} notas · ${edges.length} enlaces'
                    '${brokenCount > 0 ? ' · $brokenCount sin destino' : ''}',
                    style: TextStyle(color: t.textSecondary, fontSize: 12.5),
                  ),
                  const Spacer(),
                  TextButton.icon(
                    onPressed: _addNote,
                    icon: const Icon(CupertinoIcons.add, size: 16),
                    label: const Text('Nueva nota'),
                  ),
                ],
              ),
              const SizedBox(height: 6),
              SizedBox(
                height: 220,
                child: LayoutBuilder(
                  builder: (ctx, c) => GestureDetector(
                    onTapUp: (details) {
                      final local = details.localPosition;
                      for (int i = 0; i < nodes.length; i++) {
                        final n = nodes[i] as Map;
                        final rawX = (n['x'] as num?)?.toDouble() ?? 400.0;
                        final rawY = (n['y'] as num?)?.toDouble() ?? 280.0;
                        final nx = (rawX / 800.0).clamp(0.0, 1.0) *
                                (c.maxWidth - 40) +
                            20;
                        final ny = (rawY / 560.0).clamp(0.0, 1.0) *
                                (c.maxHeight - 40) +
                            20;
                        if ((Offset(nx, ny) - local).distance <= 26) {
                          final label = n['label']?.toString() ?? '';
                          final idx =
                              _notes.indexWhere((x) => x.title == label);
                          if (idx >= 0) {
                            setState(() {
                              _selectedNoteIdx = idx;
                              _syncSelectedNoteControllers();
                            });
                          }
                          break;
                        }
                      }
                    },
                    child: CustomPaint(
                      size: Size(c.maxWidth, 220),
                      painter: _KnowledgeGraphPainter(
                        nodes: nodes,
                        edges: edges,
                        selectedLabel: _notes.isNotEmpty
                            ? _notes[_selectedNoteIdx].title
                            : '',
                        tokens: t,
                      ),
                    ),
                  ),
                ),
              ),
            ],
          ),
        ),
        const SizedBox(height: 12),
        if (_notes.isNotEmpty)
          _panel(
            t,
            Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                TextField(
                  controller: _noteTitleCtrl,
                  style: TextStyle(
                      color: t.textPrimary,
                      fontSize: 16,
                      fontWeight: FontWeight.w600),
                  decoration: const InputDecoration(
                    border: InputBorder.none,
                    hintText: 'Título de la nota',
                  ),
                  onChanged: (_) => _saveCurrentNote(),
                ),
                Divider(color: t.border, height: 12),
                TextField(
                  controller: _noteBodyCtrl,
                  maxLines: 5,
                  style: TextStyle(color: t.textPrimary, fontSize: 14),
                  decoration: const InputDecoration(
                    border: InputBorder.none,
                    hintText: 'Usa [[Nombre de Nota]] para enlazar conceptos',
                  ),
                  onChanged: (_) => _saveCurrentNote(),
                ),
              ],
            ),
          ),
      ],
    );
  }

  Widget _buildDatabaseTab(TokensInterfaz t) {
    const statuses = ['Pendiente', 'En curso', 'Completado'];
    return ListView(
      padding: const EdgeInsets.all(14),
      children: [
        Row(
          children: [
            SelectorSegmentado<bool>(
              tokens: t,
              value: _kanbanMode,
              segments: const [(false, 'Tabla'), (true, 'Kanban')],
              onChanged: (v) => setState(() => _kanbanMode = v),
            ),
            const Spacer(),
            FilledButton.icon(
              style: FilledButton.styleFrom(
                backgroundColor: t.accent,
                foregroundColor: Colors.white,
              ),
              onPressed: _showAddDbItemDialog,
              icon: const Icon(CupertinoIcons.add, size: 15),
              label: const Text('Nuevo'),
            ),
          ],
        ),
        const SizedBox(height: 12),
        if (!_kanbanMode)
          _panel(
            t,
            Column(
              children: [
                for (int i = 0; i < _dbItems.length; i++) ...[
                  if (i > 0) Divider(color: t.border, height: 16),
                  Row(
                    children: [
                      Expanded(
                        child: Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            Text(_dbItems[i].title,
                                style: TextStyle(
                                    color: t.textPrimary,
                                    fontSize: 14.5,
                                    fontWeight: FontWeight.w600)),
                            Text(
                              '${_dbItems[i].course} · ${_dbItems[i].dueDate}',
                              style: TextStyle(
                                  color: t.textSecondary, fontSize: 12.5),
                            ),
                          ],
                        ),
                      ),
                      TextButton(
                        onPressed: () {
                          final cur = statuses.indexOf(_dbItems[i].status);
                          setState(() {
                            _dbItems[i].status =
                                statuses[(cur + 1) % statuses.length];
                          });
                          unawaited(_persistData());
                        },
                        child: Text(_dbItems[i].status),
                      ),
                    ],
                  ),
                ],
              ],
            ),
          )
        else
          for (final st in statuses) ...[
            Padding(
              padding: const EdgeInsets.only(bottom: 6),
              child: Text(
                st,
                style: TextStyle(
                    color: t.textSecondary,
                    fontSize: 12.5,
                    fontWeight: FontWeight.w600),
              ),
            ),
            _panel(
              t,
              Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  for (final item
                      in _dbItems.where((x) => x.status == st))
                    ListTile(
                      dense: true,
                      contentPadding: EdgeInsets.zero,
                      title: Text(item.title,
                          style: TextStyle(color: t.textPrimary, fontSize: 14)),
                      subtitle: Text('${item.course} · ${item.dueDate}',
                          style: TextStyle(
                              color: t.textSecondary, fontSize: 12)),
                      trailing: IconButton(
                        icon: Icon(CupertinoIcons.arrow_right_circle,
                            color: t.accent, size: 20),
                        onPressed: () {
                          final cur = statuses.indexOf(item.status);
                          setState(() {
                            item.status = statuses[(cur + 1) % statuses.length];
                          });
                          unawaited(_persistData());
                        },
                      ),
                    ),
                  if (_dbItems.where((x) => x.status == st).isEmpty)
                    Text('Sin elementos',
                        style: TextStyle(color: t.textSecondary, fontSize: 13)),
                ],
              ),
            ),
            const SizedBox(height: 10),
          ],
      ],
    );
  }

  Widget _buildFlashcardsTab(TokensInterfaz t) {
    final mins = (_pomodoroSec ~/ 60).toString().padLeft(2, '0');
    final secs = (_pomodoroSec % 60).toString().padLeft(2, '0');
    final card = _cards.isNotEmpty ? _cards[_cardIdx % _cards.length] : null;

    return ListView(
      padding: const EdgeInsets.all(14),
      children: [
        _panel(
          t,
          Row(
            children: [
              Icon(CupertinoIcons.timer, color: t.accent, size: 20),
              const SizedBox(width: 10),
              Text(
                '$mins:$secs',
                style: TextStyle(
                  color: t.textPrimary,
                  fontSize: 20,
                  fontWeight: FontWeight.w600,
                  fontFeatures: const [FontFeature.tabularFigures()],
                ),
              ),
              const Spacer(),
              TextButton(
                onPressed: _togglePomodoro,
                child: Text(_pomodoroRunning ? 'Pausar' : 'Iniciar foco'),
              ),
              TextButton(
                onPressed: _harvestFromDocument,
                child: const Text('Importar del cuaderno'),
              ),
            ],
          ),
        ),
        const SizedBox(height: 14),
        if (card != null)
          GestureDetector(
            onTap: () => setState(() => _showBack = !_showBack),
            child: _panel(
              t,
              Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Row(
                    children: [
                      Text(
                        'Tarjeta ${(_cardIdx % _cards.length) + 1} / ${_cards.length} · ${card.source}',
                        style: TextStyle(color: t.textSecondary, fontSize: 12),
                      ),
                      const Spacer(),
                      Text(
                        'Intervalo: ${card.intervalDays} d · EF ${card.easeFactor.toStringAsFixed(2)}',
                        style: TextStyle(color: t.textSecondary, fontSize: 12),
                      ),
                    ],
                  ),
                  const SizedBox(height: 20),
                  Text(
                    _showBack ? card.back : card.front,
                    style: TextStyle(
                      color: t.textPrimary,
                      fontSize: 18,
                      fontWeight: FontWeight.w500,
                    ),
                  ),
                  const SizedBox(height: 20),
                  Text(
                    _showBack
                        ? 'Evalúa tu recuerdo (SM-2):'
                        : 'Toca para revelar la respuesta',
                    style: TextStyle(color: t.accent, fontSize: 12.5),
                  ),
                  if (_showBack) ...[
                    const SizedBox(height: 10),
                    Row(
                      mainAxisAlignment: MainAxisAlignment.spaceBetween,
                      children: [
                        for (final q in const [
                          (1, 'De nuevo'),
                          (3, 'Difícil'),
                          (4, 'Bien'),
                          (5, 'Fácil'),
                        ])
                          Expanded(
                            child: Padding(
                              padding: const EdgeInsets.symmetric(horizontal: 3),
                              child: OutlinedButton(
                                onPressed: () => _rateFlashcard(q.$1),
                                child: Text(q.$2,
                                    style: const TextStyle(fontSize: 12)),
                              ),
                            ),
                          ),
                      ],
                    ),
                  ],
                ],
              ),
            ),
          ),
      ],
    );
  }

  @override
  Widget build(BuildContext context) {
    final t = widget.tokens;
    return Column(
      children: [
        TranslucentSurface(
          tokens: t,
          reduceTransparency: widget.reduceTransparency,
          bottomBorderOnly: true,
          padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
          child: SelectorSegmentado<_StudySection>(
            tokens: t,
            value: _section,
            segments: const [
              (_StudySection.graph, 'Grafo'),
              (_StudySection.database, 'Bases de Datos'),
              (_StudySection.flashcards, 'Estudio SM-2'),
            ],
            onChanged: (s) => setState(() => _section = s),
          ),
        ),
        Expanded(
          child: switch (_section) {
            _StudySection.graph => _buildGraphTab(t),
            _StudySection.database => _buildDatabaseTab(t),
            _StudySection.flashcards => _buildFlashcardsTab(t),
          },
        ),
      ],
    );
  }
}

class _KnowledgeGraphPainter extends CustomPainter {
  final List<dynamic> nodes;
  final List<dynamic> edges;
  final String selectedLabel;
  final TokensInterfaz tokens;

  const _KnowledgeGraphPainter({
    required this.nodes,
    required this.edges,
    required this.selectedLabel,
    required this.tokens,
  });

  @override
  void paint(Canvas canvas, Size size) {
    final positions = <String, Offset>{};
    for (final raw in nodes) {
      if (raw is! Map) continue;
      final id = raw['id']?.toString() ?? '';
      final rawX = (raw['x'] as num?)?.toDouble() ?? 400.0;
      final rawY = (raw['y'] as num?)?.toDouble() ?? 280.0;
      final x = (rawX / 800.0).clamp(0.0, 1.0);
      final y = (rawY / 560.0).clamp(0.0, 1.0);
      positions[id] = Offset(20 + x * (size.width - 40),
          20 + y * (size.height - 40));
    }

    final edgePaint = Paint()
      ..color = tokens.border
      ..strokeWidth = 1.4;
    for (final e in edges) {
      if (e is! Map) continue;
      final a = positions[e['from']?.toString()];
      final b = positions[e['to']?.toString()];
      if (a != null && b != null) {
        canvas.drawLine(a, b, edgePaint);
      }
    }

    for (final raw in nodes) {
      if (raw is! Map) continue;
      final id = raw['id']?.toString() ?? '';
      final label = raw['label']?.toString() ?? id;
      final pos = positions[id];
      if (pos == null) continue;
      final selected = label == selectedLabel;
      canvas.drawCircle(
        pos,
        selected ? 11 : 8,
        Paint()..color = selected ? tokens.accent : tokens.textSecondary,
      );
      final tp = TextPainter(
        text: TextSpan(
          text: label,
          style: TextStyle(
            color: selected ? tokens.accent : tokens.textPrimary,
            fontSize: 11,
            fontWeight: selected ? FontWeight.w600 : FontWeight.w400,
          ),
        ),
        textDirection: TextDirection.ltr,
      )..layout(maxWidth: 120);
      tp.paint(canvas, Offset(pos.dx - tp.width / 2, pos.dy + 12));
    }
  }

  @override
  bool shouldRepaint(covariant _KnowledgeGraphPainter oldDelegate) =>
      oldDelegate.nodes != nodes ||
      oldDelegate.edges != edges ||
      oldDelegate.selectedLabel != selectedLabel ||
      oldDelegate.tokens != tokens;
}
