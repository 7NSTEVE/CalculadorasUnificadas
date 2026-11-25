import 'dart:math' as math;

import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';

import '../native/cuadernu_studio_bindings.dart';
import '../theme/tema_interfaz.dart';

enum _StemSection { cas, chemistry, circuits, schedules }

enum _CasMode { eval, equation, linearSystem, matrix, plot, checkStep }

class CuadernuStemLabView extends StatefulWidget {
  final CuadernuStudioBindings? studio;
  final TokensInterfaz tokens;
  final bool reduceTransparency;
  final void Function(String title, String content, {String? mathExpr})?
      onInsertToNotebook;
  final void Function(String message)? onNotify;

  const CuadernuStemLabView({
    super.key,
    required this.studio,
    required this.tokens,
    required this.reduceTransparency,
    this.onInsertToNotebook,
    this.onNotify,
  });

  @override
  State<CuadernuStemLabView> createState() => _CuadernuStemLabViewState();
}

class _CuadernuStemLabViewState extends State<CuadernuStemLabView> {
  _StemSection _section = _StemSection.cas;

  // --- CAS state ---
  _CasMode _casMode = _CasMode.eval;
  final TextEditingController _casInput =
      TextEditingController(text: '12 V / 470 ohm | mA');
  final TextEditingController _casAuxInput =
      TextEditingController(text: '2*x + 6');
  final TextEditingController _casVarCtrl = TextEditingController(text: 'x');
  bool _casAngleDeg = true;
  Map<String, dynamic>? _casResult;
  List<(double, double)> _plotPoints = const [];

  // --- Chemistry state ---
  final TextEditingController _chemInput =
      TextEditingController(text: 'Fe + O2 -> Fe2O3');
  Map<String, dynamic>? _chemResult;

  // --- Circuits / K-Map state ---
  String _circuitMode = 'dc'; // dc | ac | bode | transient | laplace | kmap
  final TextEditingController _netlistCtrl = TextEditingController(
    text: 'V1 1 0 12\nR1 1 2 1k\nR2 2 0 1k',
  );
  final TextEditingController _probeNodeCtrl = TextEditingController(text: '2');
  final TextEditingController _param1Ctrl = TextEditingController(text: '1000');
  final TextEditingController _kmapMintermsCtrl =
      TextEditingController(text: '0,1,2,5,6,7,8,9,10,14');
  final TextEditingController _kmapDcCtrl = TextEditingController(text: '');
  int _kmapVars = 4;
  Map<String, dynamic>? _circuitResult;

  // --- Horarios TEC state ---
  late final List<Map<String, dynamic>> _courses = [
    {
      'code': 'MA1102',
      'name': 'Cálculo Diferencial e Integral',
      'credits': 4,
      'groups': [
        {
          'id': 'Gr 1',
          'professor': 'Prof. Méndez',
          'sessions': [
            {'day': 'L', 'start': '07:30', 'end': '09:20'},
            {'day': 'K', 'start': '07:30', 'end': '09:20'},
          ],
        },
        {
          'id': 'Gr 2',
          'professor': 'Prof. Rojas',
          'sessions': [
            {'day': 'M', 'start': '13:00', 'end': '14:50'},
            {'day': 'V', 'start': '13:00', 'end': '14:50'},
          ],
        },
      ],
    },
    {
      'code': 'FI1101',
      'name': 'Física General I',
      'credits': 4,
      'groups': [
        {
          'id': 'Gr 1',
          'professor': 'Prof. Salas',
          'sessions': [
            {'day': 'L', 'start': '09:30', 'end': '11:20'},
            {'day': 'J', 'start': '09:30', 'end': '11:20'},
          ],
        },
        {
          'id': 'Gr 2',
          'professor': 'Prof. Vargas',
          'sessions': [
            {'day': 'K', 'start': '07:30', 'end': '09:20'},
            {'day': 'J', 'start': '07:30', 'end': '09:20'},
          ],
        },
      ],
    },
    {
      'code': 'EL2113',
      'name': 'Circuitos Eléctricos I',
      'credits': 4,
      'groups': [
        {
          'id': 'Gr 1',
          'professor': 'Prof. Quesada',
          'sessions': [
            {'day': 'M', 'start': '09:30', 'end': '11:20'},
            {'day': 'V', 'start': '09:30', 'end': '11:20'},
          ],
        },
      ],
    },
  ];
  Map<String, dynamic>? _horariosResult;
  int _selectedScheduleIdx = 0;

  @override
  void initState() {
    super.initState();
    if (widget.studio?.isAvailable == true) {
      _runCas();
    }
  }

  @override
  void dispose() {
    _casInput.dispose();
    _casAuxInput.dispose();
    _casVarCtrl.dispose();
    _chemInput.dispose();
    _netlistCtrl.dispose();
    _probeNodeCtrl.dispose();
    _param1Ctrl.dispose();
    _kmapMintermsCtrl.dispose();
    _kmapDcCtrl.dispose();
    super.dispose();
  }

  void _runCas() {
    final s = widget.studio;
    if (s == null || !s.isAvailable) {
      setState(() => _casResult = {'ok': false, 'error': 'Motor nativo cargando...'});
      return;
    }
    final expr = _casInput.text.trim();
    if (expr.isEmpty) return;
    final variable =
        _casVarCtrl.text.trim().isEmpty ? 'x' : _casVarCtrl.text.trim();
    Map<String, dynamic> res;
    List<(double, double)> pts = const [];
    switch (_casMode) {
      case _CasMode.eval:
        res = s.evalCas(expr, angleDegrees: _casAngleDeg);
        break;
      case _CasMode.equation:
        res = s.solveEquation(expr, varName: variable);
        break;
      case _CasMode.linearSystem:
        res = s.solveLinearSystem(expr);
        break;
      case _CasMode.matrix:
        res = s.evalMatrix(expr);
        break;
      case _CasMode.plot:
        res = s.samplePlot(expr, varName: variable, xMin: -10, xMax: 10, numSamples: 220);
        final rawPts = res['points'];
        if (rawPts is List) {
          pts = rawPts
              .whereType<List>()
              .where((p) => p.length >= 2)
              .map((p) => ((p[0] as num).toDouble(), (p[1] as num).toDouble()))
              .toList();
        }
        break;
      case _CasMode.checkStep:
        res = s.checkMathStep(expr, _casAuxInput.text.trim(), varName: variable);
        break;
    }
    setState(() {
      _casResult = res;
      _plotPoints = pts;
    });
  }

  void _runChemistry() {
    final s = widget.studio;
    if (s == null || !s.isAvailable) return;
    final input = _chemInput.text.trim();
    if (input.isEmpty) return;
    setState(() => _chemResult = s.analyzeChemistry(input));
  }

  void _runCircuit() {
    final s = widget.studio;
    if (s == null || !s.isAvailable) return;
    if (_circuitMode == 'kmap') {
      final res = s.simplifyKMap(
        _kmapVars,
        _kmapMintermsCtrl.text.trim(),
        dontCaresCsv: _kmapDcCtrl.text.trim(),
      );
      setState(() => _circuitResult = res);
      return;
    }
    final probe = int.tryParse(_probeNodeCtrl.text.trim()) ?? 2;
    final p1 = double.tryParse(_param1Ctrl.text.trim()) ??
        (_circuitMode == 'transient'
            ? 0.01
            : _circuitMode == 'bode'
                ? 10.0
                : 1000.0);
    final p2 = _circuitMode == 'bode' ? 1e6 : 0.0;
    final res = s.solveCircuit(
      _netlistCtrl.text,
      mode: _circuitMode,
      param1: p1,
      param2: p2,
      probeNode: probe,
    );
    setState(() => _circuitResult = res);
  }

  void _runHorarios() {
    final s = widget.studio;
    if (s == null || !s.isAvailable) return;
    final res = s.solveHorariosTec({'courses': _courses});
    setState(() {
      _horariosResult = res;
      _selectedScheduleIdx = 0;
    });
  }

  void _applyCircuitPreset(String preset) {
    setState(() {
      switch (preset) {
        case 'divider':
          _circuitMode = 'dc';
          _netlistCtrl.text = 'V1 1 0 12\nR1 1 2 1k\nR2 2 0 1k';
          _probeNodeCtrl.text = '2';
          _param1Ctrl.text = '0';
          break;
        case 'rc_bode':
          _circuitMode = 'bode';
          _netlistCtrl.text = 'V1 1 0 1\nR1 1 2 1k\nC1 2 0 100n';
          _probeNodeCtrl.text = '2';
          _param1Ctrl.text = '10';
          break;
        case 'rc_transient':
          _circuitMode = 'transient';
          _netlistCtrl.text = 'V1 1 0 5\nR1 1 2 1k\nC1 2 0 1u';
          _probeNodeCtrl.text = '2';
          _param1Ctrl.text = '0.006';
          break;
        case 'rc_laplace':
          _circuitMode = 'laplace';
          _netlistCtrl.text = 'V1 1 0 1\nR1 1 2 1k\nC1 2 0 1u';
          _probeNodeCtrl.text = '2';
          break;
        case 'kmap':
          _circuitMode = 'kmap';
          _kmapVars = 4;
          _kmapMintermsCtrl.text = '0,1,2,5,6,7,8,9,10,14';
          _kmapDcCtrl.text = '';
          break;
      }
    });
    _runCircuit();
  }

  InputDecoration _fieldDecoration(TokensInterfaz t, String hint) =>
      InputDecoration(
        hintText: hint,
        hintStyle: TextStyle(color: t.textSecondary, fontSize: 13.5),
        filled: true,
        fillColor: t.surface,
        contentPadding:
            const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
        border: OutlineInputBorder(
          borderRadius: BorderRadius.circular(10),
          borderSide: BorderSide(color: t.border, width: t.borderWidth),
        ),
        enabledBorder: OutlineInputBorder(
          borderRadius: BorderRadius.circular(10),
          borderSide: BorderSide(color: t.border, width: t.borderWidth),
        ),
        focusedBorder: OutlineInputBorder(
          borderRadius: BorderRadius.circular(10),
          borderSide: BorderSide(color: t.accent, width: 1.25),
        ),
      );

  Widget _sectionPanel(TokensInterfaz t, Widget child) => Container(
        width: double.infinity,
        padding: const EdgeInsets.all(14),
        decoration: BoxDecoration(
          color: t.surface,
          borderRadius: BorderRadius.circular(12),
          border: Border.all(color: t.border, width: t.borderWidth),
        ),
        child: child,
      );

  Widget _buildCasSection(TokensInterfaz t) {
    final res = _casResult;
    return ListView(
      padding: const EdgeInsets.all(14),
      children: [
        SingleChildScrollView(
          scrollDirection: Axis.horizontal,
          child: SelectorSegmentado<_CasMode>(
            tokens: t,
            value: _casMode,
            segments: const [
              (_CasMode.eval, 'Evaluar'),
              (_CasMode.equation, 'Ecuación'),
              (_CasMode.linearSystem, 'SEL'),
              (_CasMode.matrix, 'Matrices'),
              (_CasMode.plot, 'Gráfica 2D'),
              (_CasMode.checkStep, 'Verificar paso'),
            ],
            onChanged: (m) {
              setState(() {
                _casMode = m;
                switch (m) {
                  case _CasMode.eval:
                    _casInput.text = '12 V / 470 ohm | mA';
                    break;
                  case _CasMode.equation:
                    _casInput.text = 'x^2 - 5*x + 6 = 0';
                    break;
                  case _CasMode.linearSystem:
                    _casInput.text = '2*x + y = 7; x - y = 2';
                    break;
                  case _CasMode.matrix:
                    _casInput.text = 'det([[2, 1], [5, 4]])';
                    break;
                  case _CasMode.plot:
                    _casInput.text = 'sin(x) * exp(-0.12 * x)';
                    break;
                  case _CasMode.checkStep:
                    _casInput.text = '2*(x + 3)';
                    _casAuxInput.text = '2*x + 6';
                    break;
                }
              });
              _runCas();
            },
          ),
        ),
        const SizedBox(height: 12),
        TextField(
          controller: _casInput,
          style: TextStyle(color: t.textPrimary, fontSize: 15),
          maxLines: _casMode == _CasMode.linearSystem ? 3 : 1,
          decoration: _fieldDecoration(
            t,
            _casMode == _CasMode.eval
                ? 'Ej. 12 V / 470 ohm | mA  o  sin(30)'
                : _casMode == _CasMode.equation
                    ? 'Ej. x^2 - 5*x + 6 = 0'
                    : _casMode == _CasMode.linearSystem
                        ? 'Ej. 2*x + y = 7; x - y = 2'
                        : _casMode == _CasMode.matrix
                            ? 'Ej. det([[1,2],[3,4]]) o inv([[4,7],[2,6]])'
                            : _casMode == _CasMode.plot
                                ? 'Ej. sin(x) * cos(0.5*x)'
                                : 'Expresión anterior',
          ),
          onSubmitted: (_) => _runCas(),
        ),
        if (_casMode == _CasMode.checkStep) ...[
          const SizedBox(height: 8),
          TextField(
            controller: _casAuxInput,
            style: TextStyle(color: t.textPrimary, fontSize: 15),
            decoration: _fieldDecoration(t, 'Siguiente paso algebraico'),
            onSubmitted: (_) => _runCas(),
          ),
        ],
        const SizedBox(height: 10),
        Row(
          children: [
            if (_casMode == _CasMode.eval)
              ActionChip(
                label: Text(_casAngleDeg ? 'DEG' : 'RAD',
                    style: TextStyle(
                        color: t.textPrimary,
                        fontSize: 12.5,
                        fontWeight: FontWeight.w600)),
                backgroundColor: t.controlTrack,
                side: BorderSide.none,
                onPressed: () {
                  setState(() => _casAngleDeg = !_casAngleDeg);
                  _runCas();
                },
              ),
            if (_casMode == _CasMode.equation ||
                _casMode == _CasMode.plot ||
                _casMode == _CasMode.checkStep) ...[
              SizedBox(
                width: 68,
                child: TextField(
                  controller: _casVarCtrl,
                  style: TextStyle(color: t.textPrimary, fontSize: 13.5),
                  decoration: _fieldDecoration(t, 'Var'),
                ),
              ),
              const SizedBox(width: 8),
            ],
            const Spacer(),
            if (widget.onInsertToNotebook != null && res != null)
              OutlinedButton.icon(
                onPressed: () {
                  final formatted = res['formatted']?.toString() ??
                      res['summary']?.toString() ??
                      res['message']?.toString() ??
                      '';
                  widget.onInsertToNotebook?.call(
                    'CAS',
                    '${_casInput.text.trim()}\n= $formatted',
                    mathExpr: _casMode == _CasMode.eval ? _casInput.text.trim() : null,
                  );
                },
                icon: const Icon(CupertinoIcons.doc_append, size: 16),
                label: const Text('Al cuaderno'),
              ),
            const SizedBox(width: 8),
            FilledButton.icon(
              style: FilledButton.styleFrom(
                backgroundColor: t.accent,
                foregroundColor: Colors.white,
              ),
              onPressed: _runCas,
              icon: const Icon(CupertinoIcons.play_fill, size: 14),
              label: const Text('Calcular'),
            ),
          ],
        ),
        const SizedBox(height: 14),
        if (_casMode == _CasMode.plot && _plotPoints.isNotEmpty)
          _sectionPanel(
            t,
            SizedBox(
              height: 240,
              child: CustomPaint(
                painter: _MathPlotPainter(
                  points: _plotPoints,
                  tokens: t,
                  label: 'f(${_casVarCtrl.text.trim()}) = ${_casInput.text.trim()}',
                ),
              ),
            ),
          )
        else if (res != null)
          _sectionPanel(
            t,
            Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                if (res['error'] != null &&
                    res['error'].toString().isNotEmpty &&
                    res['ok'] == false)
                  Text(
                    res['error'].toString(),
                    style: TextStyle(color: t.destructive, fontSize: 13.5),
                  )
                else ...[
                  Text(
                    res['formatted']?.toString() ??
                        res['summary']?.toString() ??
                        res['message']?.toString() ??
                        (res['equivalent'] == true
                            ? 'Paso algebraicamente equivalente'
                            : 'Resultado listo'),
                    style: TextStyle(
                      color: t.textPrimary,
                      fontSize: 19,
                      fontWeight: FontWeight.w600,
                    ),
                  ),
                  if ((res['physicalQuantity']?.toString() ?? '').isNotEmpty) ...[
                    const SizedBox(height: 4),
                    Text(
                      '${res['physicalQuantity']}  ·  ${res['siUnits'] ?? ''}',
                      style: TextStyle(color: t.textSecondary, fontSize: 13),
                    ),
                  ],
                  if ((res['varSub']?.toString() ?? '').isNotEmpty) ...[
                    const SizedBox(height: 6),
                    Text(
                      res['varSub'].toString(),
                      style: TextStyle(color: t.textSecondary, fontSize: 13),
                    ),
                  ],
                  if (res['roots'] is List && (res['roots'] as List).isNotEmpty) ...[
                    const SizedBox(height: 8),
                    Text(
                      'Raíces: ${(res['roots'] as List).join(', ')}',
                      style: TextStyle(
                        color: t.accent,
                        fontSize: 14.5,
                        fontWeight: FontWeight.w600,
                      ),
                    ),
                  ],
                  if (res['steps'] is List && (res['steps'] as List).isNotEmpty) ...[
                    const SizedBox(height: 10),
                    for (final step in (res['steps'] as List))
                      Padding(
                        padding: const EdgeInsets.only(bottom: 4),
                        child: Text(
                          step.toString(),
                          style: TextStyle(color: t.textSecondary, fontSize: 13),
                        ),
                      ),
                  ],
                  if ((res['stepsTrace']?.toString() ?? '').isNotEmpty) ...[
                    const SizedBox(height: 8),
                    Text(
                      res['stepsTrace'].toString(),
                      style: TextStyle(
                        color: t.textSecondary,
                        fontSize: 12.5,
                        fontFamily: 'monospace',
                      ),
                    ),
                  ],
                ],
              ],
            ),
          ),
      ],
    );
  }

  Widget _buildChemistrySection(TokensInterfaz t) {
    final res = _chemResult;
    return ListView(
      padding: const EdgeInsets.all(14),
      children: [
        TextField(
          controller: _chemInput,
          style: TextStyle(color: t.textPrimary, fontSize: 15),
          decoration:
              _fieldDecoration(t, 'Ej. Fe + O2 -> Fe2O3  o  H2SO4'),
          onSubmitted: (_) => _runChemistry(),
        ),
        const SizedBox(height: 10),
        Wrap(
          spacing: 8,
          runSpacing: 6,
          children: [
            for (final sample in const [
              'Fe + O2 -> Fe2O3',
              'C6H12O6 + O2 -> CO2 + H2O',
              'H2SO4',
              'KMnO4 + HCl -> KCl + MnCl2 + H2O + Cl2',
            ])
              ActionChip(
                label: Text(sample,
                    style: TextStyle(color: t.textPrimary, fontSize: 12)),
                backgroundColor: t.controlTrack,
                side: BorderSide.none,
                onPressed: () {
                  _chemInput.text = sample;
                  _runChemistry();
                },
              ),
          ],
        ),
        const SizedBox(height: 10),
        Row(
          mainAxisAlignment: MainAxisAlignment.end,
          children: [
            if (widget.onInsertToNotebook != null && res != null && res['valid'] == true)
              OutlinedButton.icon(
                onPressed: () {
                  final text = (res['natural2D']?.toString().isNotEmpty == true)
                      ? res['natural2D'].toString()
                      : res['balancedEquation']?.toString() ?? _chemInput.text;
                  widget.onInsertToNotebook?.call(
                    'Química',
                    '$text\nMasa molar: ${(res['molarMassGPerMol'] as num?)?.toStringAsFixed(3) ?? ''} g/mol',
                  );
                },
                icon: const Icon(CupertinoIcons.doc_append, size: 16),
                label: const Text('Al cuaderno'),
              ),
            const SizedBox(width: 8),
            FilledButton.icon(
              style: FilledButton.styleFrom(
                backgroundColor: t.accent,
                foregroundColor: Colors.white,
              ),
              onPressed: _runChemistry,
              icon: const Icon(CupertinoIcons.lab_flask, size: 15),
              label: const Text('Analizar y balancear'),
            ),
          ],
        ),
        const SizedBox(height: 14),
        if (res != null)
          _sectionPanel(
            t,
            Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  (res['natural2D']?.toString().isNotEmpty == true)
                      ? res['natural2D'].toString()
                      : (res['balancedEquation']?.toString().isNotEmpty == true
                          ? res['balancedEquation'].toString()
                          : 'Fórmula no reconocida'),
                  style: TextStyle(
                    color: t.textPrimary,
                    fontSize: 20,
                    fontWeight: FontWeight.w600,
                  ),
                ),
                const SizedBox(height: 8),
                if (res['isReaction'] == true)
                  Text(
                    res['balanced'] == true
                        ? 'Reacción balanceada con conservación exacta de masa'
                        : 'No se encontró solución estequiométrica entera',
                    style: TextStyle(
                      color: res['balanced'] == true ? t.accent : t.destructive,
                      fontSize: 13.5,
                      fontWeight: FontWeight.w500,
                    ),
                  ),
                if ((res['molarMassGPerMol'] as num? ?? 0) > 0) ...[
                  const SizedBox(height: 6),
                  Text(
                    'Masa molar: ${(res['molarMassGPerMol'] as num).toStringAsFixed(4)} g/mol',
                    style: TextStyle(color: t.textSecondary, fontSize: 14),
                  ),
                ],
                if (res['atomsLeft'] is Map && (res['atomsLeft'] as Map).isNotEmpty) ...[
                  const SizedBox(height: 8),
                  Text(
                    'Átomos: ${(res['atomsLeft'] as Map).entries.map((e) => '${e.key}: ${e.value}').join('  ·  ')}',
                    style: TextStyle(color: t.textSecondary, fontSize: 12.5),
                  ),
                ],
              ],
            ),
          ),
      ],
    );
  }

  Widget _buildCircuitsSection(TokensInterfaz t) {
    final res = _circuitResult;
    return ListView(
      padding: const EdgeInsets.all(14),
      children: [
        SingleChildScrollView(
          scrollDirection: Axis.horizontal,
          child: SelectorSegmentado<String>(
            tokens: t,
            value: _circuitMode,
            segments: const [
              ('dc', 'DC'),
              ('ac', 'AC'),
              ('bode', 'Bode'),
              ('transient', 'Transitorio'),
              ('laplace', 'Laplace'),
              ('kmap', 'K-Map'),
            ],
            onChanged: (m) {
              setState(() => _circuitMode = m);
              _runCircuit();
            },
          ),
        ),
        const SizedBox(height: 10),
        Wrap(
          spacing: 8,
          runSpacing: 6,
          children: [
            for (final p in const [
              ('divider', 'Divisor DC'),
              ('rc_bode', 'Filtro RC Bode'),
              ('rc_transient', 'RC Transitorio'),
              ('rc_laplace', 'RC Laplace H(s)'),
              ('kmap', 'K-Map 4 vars'),
            ])
              ActionChip(
                label: Text(p.$2,
                    style: TextStyle(color: t.textPrimary, fontSize: 12)),
                backgroundColor: t.controlTrack,
                side: BorderSide.none,
                onPressed: () => _applyCircuitPreset(p.$1),
              ),
          ],
        ),
        const SizedBox(height: 10),
        if (_circuitMode == 'kmap') ...[
          Row(
            children: [
              SelectorSegmentado<int>(
                tokens: t,
                value: _kmapVars,
                segments: const [(2, '2v'), (3, '3v'), (4, '4v')],
                onChanged: (v) => setState(() => _kmapVars = v),
              ),
              const SizedBox(width: 10),
              Expanded(
                child: TextField(
                  controller: _kmapMintermsCtrl,
                  style: TextStyle(color: t.textPrimary, fontSize: 14),
                  decoration: _fieldDecoration(t, 'Minterms (ej. 0,1,2,5,6,7)'),
                ),
              ),
            ],
          ),
          const SizedBox(height: 8),
          TextField(
            controller: _kmapDcCtrl,
            style: TextStyle(color: t.textPrimary, fontSize: 14),
            decoration: _fieldDecoration(t, 'Don\'t cares opcional (ej. 3,15)'),
          ),
        ] else ...[
          TextField(
            controller: _netlistCtrl,
            maxLines: 5,
            style: TextStyle(
                color: t.textPrimary, fontSize: 13.5, fontFamily: 'monospace'),
            decoration: _fieldDecoration(
                t, 'Netlist SPICE (ej. V1 1 0 12 / R1 1 2 1k / C1 2 0 100n)'),
          ),
          const SizedBox(height: 8),
          Row(
            children: [
              SizedBox(
                width: 96,
                child: TextField(
                  controller: _probeNodeCtrl,
                  keyboardType: TextInputType.number,
                  style: TextStyle(color: t.textPrimary, fontSize: 13.5),
                  decoration: _fieldDecoration(t, 'Nodo'),
                ),
              ),
              const SizedBox(width: 8),
              if (_circuitMode == 'ac' ||
                  _circuitMode == 'bode' ||
                  _circuitMode == 'transient')
                Expanded(
                  child: TextField(
                    controller: _param1Ctrl,
                    keyboardType: TextInputType.number,
                    style: TextStyle(color: t.textPrimary, fontSize: 13.5),
                    decoration: _fieldDecoration(
                      t,
                      _circuitMode == 'transient'
                          ? 'Duración (s)'
                          : 'Frecuencia (Hz)',
                    ),
                  ),
                ),
            ],
          ),
        ],
        const SizedBox(height: 10),
        Row(
          mainAxisAlignment: MainAxisAlignment.end,
          children: [
            FilledButton.icon(
              style: FilledButton.styleFrom(
                backgroundColor: t.accent,
                foregroundColor: Colors.white,
              ),
              onPressed: _runCircuit,
              icon: const Icon(CupertinoIcons.bolt_fill, size: 15),
              label: const Text('Simular'),
            ),
          ],
        ),
        const SizedBox(height: 14),
        if (res != null) _buildCircuitOutput(t, res),
      ],
    );
  }

  Widget _buildCircuitOutput(TokensInterfaz t, Map<String, dynamic> res) {
    if (res['ok'] == false) {
      return _sectionPanel(
        t,
        Text(res['error']?.toString() ?? 'No se pudo resolver el circuito',
            style: TextStyle(color: t.destructive)),
      );
    }

    if (_circuitMode == 'kmap') {
      return _sectionPanel(
        t,
        Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              res['minimizedSop']?.toString() ?? '',
              style: TextStyle(
                  color: t.textPrimary,
                  fontSize: 20,
                  fontWeight: FontWeight.w600),
            ),
            if (res['primeImplicants'] is List) ...[
              const SizedBox(height: 6),
              Text(
                'Implicantes primos: ${(res['primeImplicants'] as List).join(', ')}',
                style: TextStyle(color: t.textSecondary, fontSize: 13),
              ),
            ],
          ],
        ),
      );
    }

    if (_circuitMode == 'bode' && res['points'] is List) {
      final pts = (res['points'] as List)
          .whereType<Map>()
          .map((p) => (
                math.log((p['freqHz'] as num).toDouble().clamp(1e-3, 1e12)) /
                    math.ln10,
                (p['magDb'] as num).toDouble(),
              ))
          .toList();
      return _sectionPanel(
        t,
        SizedBox(
          height: 220,
          child: CustomPaint(
            painter: _MathPlotPainter(
              points: pts,
              tokens: t,
              label: 'Bode |H(jω)| dB vs log10(f)',
            ),
          ),
        ),
      );
    }

    if (_circuitMode == 'transient' && res['points'] is List) {
      final pts = (res['points'] as List)
          .whereType<List>()
          .where((p) => p.length >= 2)
          .map((p) => ((p[0] as num).toDouble() * 1000.0, (p[1] as num).toDouble()))
          .toList();
      return _sectionPanel(
        t,
        Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            SizedBox(
              height: 220,
              child: CustomPaint(
                painter: _MathPlotPainter(
                  points: pts,
                  tokens: t,
                  label: 'V(t) nodo ${res['probedNode'] ?? ''} (ms)',
                ),
              ),
            ),
            if ((res['summary']?.toString() ?? '').isNotEmpty) ...[
              const SizedBox(height: 8),
              Text(res['summary'].toString(),
                  style: TextStyle(color: t.textSecondary, fontSize: 12.5)),
            ],
          ],
        ),
      );
    }

    if (_circuitMode == 'laplace') {
      final eqs = (res['nodalEquations'] as List?) ?? const [];
      return _sectionPanel(
        t,
        Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              res['transferFunction']?.toString() ?? 'H(s)',
              style: TextStyle(
                  color: t.textPrimary,
                  fontSize: 17,
                  fontWeight: FontWeight.w600),
            ),
            const SizedBox(height: 6),
            Text(
              'Ganancia DC: ${(res['dcGain'] as num?)?.toStringAsFixed(4) ?? '—'}',
              style: TextStyle(color: t.accent, fontSize: 13.5),
            ),
            if (eqs.isNotEmpty) ...[
              const SizedBox(height: 8),
              for (final e in eqs)
                Text(e.toString(),
                    style: TextStyle(
                        color: t.textSecondary,
                        fontSize: 12.5,
                        fontFamily: 'monospace')),
            ],
          ],
        ),
      );
    }

    final nodes = (res['nodeVoltages'] as Map?) ?? const {};
    final branches = (res['branches'] as List?) ?? const [];
    return _sectionPanel(
      t,
      Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          if ((res['summary']?.toString() ?? '').isNotEmpty)
            Text(
              res['summary'].toString(),
              style: TextStyle(
                  color: t.textPrimary,
                  fontSize: 14,
                  fontWeight: FontWeight.w600),
            ),
          const SizedBox(height: 8),
          for (final entry in nodes.entries)
            if (entry.value is Map)
              Padding(
                padding: const EdgeInsets.only(bottom: 4),
                child: Text(
                  'V(n${entry.key}) = ${(entry.value['mag'] as num).toStringAsFixed(4)} V'
                  '${_circuitMode == 'ac' ? ' ∠ ${(entry.value['phaseDeg'] as num).toStringAsFixed(2)}°' : ''}',
                  style: TextStyle(
                      color: t.textPrimary,
                      fontSize: 14,
                      fontFamily: 'monospace'),
                ),
              ),
          for (final b in branches)
            if (b is Map)
              Padding(
                padding: const EdgeInsets.only(bottom: 4),
                child: Text(
                  'I(${b['name']}) = ${((b['currentMagA'] as num) * 1000).toStringAsFixed(3)} mA  ·  P = ${((b['powerW'] as num) * 1000).toStringAsFixed(2)} mW',
                  style: TextStyle(
                      color: t.textSecondary,
                      fontSize: 13,
                      fontFamily: 'monospace'),
                ),
              ),
        ],
      ),
    );
  }

  Widget _buildHorariosSection(TokensInterfaz t) {
    final res = _horariosResult;
    final schedules = (res?['schedules'] as List?) ?? const [];
    return ListView(
      padding: const EdgeInsets.all(14),
      children: [
        _sectionPanel(
          t,
          Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Row(
                children: [
                  Expanded(
                    child: Text(
                      'Cursos configurados (${_courses.length})',
                      style: TextStyle(
                          color: t.textPrimary,
                          fontSize: 15,
                          fontWeight: FontWeight.w600),
                    ),
                  ),
                  FilledButton.icon(
                    style: FilledButton.styleFrom(
                      backgroundColor: t.accent,
                      foregroundColor: Colors.white,
                    ),
                    onPressed: _runHorarios,
                    icon: const Icon(CupertinoIcons.calendar, size: 15),
                    label: const Text('Generar horarios'),
                  ),
                ],
              ),
              const SizedBox(height: 10),
              for (final c in _courses)
                Padding(
                  padding: const EdgeInsets.only(bottom: 6),
                  child: Text(
                    '${c['code']} · ${c['name']} (${(c['groups'] as List).length} grupos)',
                    style: TextStyle(color: t.textSecondary, fontSize: 13),
                  ),
                ),
            ],
          ),
        ),
        const SizedBox(height: 12),
        if (schedules.isNotEmpty) ...[
          SingleChildScrollView(
            scrollDirection: Axis.horizontal,
            child: SelectorSegmentado<int>(
              tokens: t,
              value: _selectedScheduleIdx.clamp(0, schedules.length - 1),
              segments: [
                for (int i = 0; i < math.min(schedules.length, 6); i++)
                  (i, 'Opción ${i + 1}'),
              ],
              onChanged: (idx) => setState(() => _selectedScheduleIdx = idx),
            ),
          ),
          const SizedBox(height: 10),
          _sectionPanel(
            t,
            Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                for (final sel in (schedules[_selectedScheduleIdx
                        .clamp(0, schedules.length - 1)]['selections']
                    as List? ??
                    const []))
                  if (sel is Map)
                    Padding(
                      padding: const EdgeInsets.only(bottom: 8),
                      child: Text(
                        '${sel['courseCode']} ${(sel['group'] as Map?)?['id'] ?? ''} · ${(sel['group'] as Map?)?['professor'] ?? ''}\n'
                        '  ${(((sel['group'] as Map?)?['sessions']) as List? ?? const []).map((b) => '${b['day']} ${b['start']}–${b['end']}').join('  |  ')}',
                        style: TextStyle(color: t.textPrimary, fontSize: 13.5),
                      ),
                    ),
              ],
            ),
          ),
        ] else if (res != null)
          _sectionPanel(
            t,
            Text(
              'Sin combinaciones libres de choques.',
              style: TextStyle(color: t.destructive),
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
          child: SingleChildScrollView(
            scrollDirection: Axis.horizontal,
            child: SelectorSegmentado<_StemSection>(
              tokens: t,
              value: _section,
              segments: const [
                (_StemSection.cas, 'CAS'),
                (_StemSection.chemistry, 'Química'),
                (_StemSection.circuits, 'Circuitos'),
                (_StemSection.schedules, 'Horarios TEC'),
              ],
              onChanged: (s) {
                setState(() => _section = s);
                if (s == _StemSection.chemistry && _chemResult == null) {
                  _runChemistry();
                } else if (s == _StemSection.circuits && _circuitResult == null) {
                  _runCircuit();
                } else if (s == _StemSection.schedules && _horariosResult == null) {
                  _runHorarios();
                }
              },
            ),
          ),
        ),
        Expanded(
          child: switch (_section) {
            _StemSection.cas => _buildCasSection(t),
            _StemSection.chemistry => _buildChemistrySection(t),
            _StemSection.circuits => _buildCircuitsSection(t),
            _StemSection.schedules => _buildHorariosSection(t),
          },
        ),
      ],
    );
  }
}

class _MathPlotPainter extends CustomPainter {
  final List<(double, double)> points;
  final TokensInterfaz tokens;
  final String label;

  const _MathPlotPainter({
    required this.points,
    required this.tokens,
    required this.label,
  });

  @override
  void paint(Canvas canvas, Size size) {
    if (points.isEmpty) return;
    double minX = points.first.$1, maxX = points.first.$1;
    double minY = points.first.$2, maxY = points.first.$2;
    for (final p in points) {
      if (p.$1 < minX) minX = p.$1;
      if (p.$1 > maxX) maxX = p.$1;
      if (p.$2 < minY) minY = p.$2;
      if (p.$2 > maxY) maxY = p.$2;
    }
    if ((maxX - minX).abs() < 1e-9) {
      minX -= 1;
      maxX += 1;
    }
    if ((maxY - minY).abs() < 1e-9) {
      minY -= 1;
      maxY += 1;
    } else {
      final pad = (maxY - minY) * 0.1;
      minY -= pad;
      maxY += pad;
    }

    const double padL = 40, padR = 12, padT = 22, padB = 22;
    final plotW = (size.width - padL - padR).clamp(10.0, size.width);
    final plotH = (size.height - padT - padB).clamp(10.0, size.height);

    double toX(double x) => padL + ((x - minX) / (maxX - minX)) * plotW;
    double toY(double y) => padT + (1.0 - (y - minY) / (maxY - minY)) * plotH;

    final gridPaint = Paint()
      ..color = tokens.border
      ..strokeWidth = 0.75;
    for (int i = 0; i <= 4; i++) {
      final gy = padT + (plotH * i / 4.0);
      canvas.drawLine(Offset(padL, gy), Offset(padL + plotW, gy), gridPaint);
      final gx = padL + (plotW * i / 4.0);
      canvas.drawLine(Offset(gx, padT), Offset(gx, padT + plotH), gridPaint);
    }

    final axisPaint = Paint()
      ..color = tokens.textSecondary.withValues(alpha: 0.45)
      ..strokeWidth = 1.0;
    if (minY <= 0 && maxY >= 0) {
      final y0 = toY(0);
      canvas.drawLine(Offset(padL, y0), Offset(padL + plotW, y0), axisPaint);
    }
    if (minX <= 0 && maxX >= 0) {
      final x0 = toX(0);
      canvas.drawLine(Offset(x0, padT), Offset(x0, padT + plotH), axisPaint);
    }

    final curvePaint = Paint()
      ..color = tokens.accent
      ..strokeWidth = 2.0
      ..style = PaintingStyle.stroke
      ..strokeCap = StrokeCap.round
      ..strokeJoin = StrokeJoin.round;

    final path = Path();
    bool started = false;
    for (final p in points) {
      final sx = toX(p.$1);
      final sy = toY(p.$2);
      if (!started) {
        path.moveTo(sx, sy);
        started = true;
      } else {
        path.lineTo(sx, sy);
      }
    }
    canvas.drawPath(path, curvePaint);

    void drawText(String txt, Offset pos, {double fontSize = 10.5}) {
      final tp = TextPainter(
        text: TextSpan(
          text: txt,
          style: TextStyle(color: tokens.textSecondary, fontSize: fontSize),
        ),
        textDirection: TextDirection.ltr,
      )..layout();
      tp.paint(canvas, pos);
    }

    drawText(label, const Offset(padL, 2), fontSize: 11.5);
    drawText(maxY.toStringAsPrecision(3), const Offset(2, padT - 4));
    drawText(minY.toStringAsPrecision(3), Offset(2, padT + plotH - 10));
    drawText(minX.toStringAsPrecision(3), Offset(padL, padT + plotH + 4));
    drawText(maxX.toStringAsPrecision(3),
        Offset(padL + plotW - 34, padT + plotH + 4));
  }

  @override
  bool shouldRepaint(covariant _MathPlotPainter oldDelegate) =>
      oldDelegate.points != points ||
      oldDelegate.tokens != tokens ||
      oldDelegate.label != label;
}
