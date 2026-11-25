import 'dart:io';
import 'dart:typed_data';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

import 'package:calculadoras_unificadas/main.dart';
import 'package:calculadoras_unificadas/native/casio_ffi_bindings.dart';
import 'package:calculadoras_unificadas/native/cuadernu_studio_bindings.dart';

import 'widget_test.dart' show resolveCasioNativeDll;

void main() {
  testWidgets('Unified CuadernUApp navigates cleanly across Cuaderno, Calculadoras, STEM & Estudio',
      (WidgetTester tester) async {
    await tester.pumpWidget(const CuadernUApp(disableNativeBoot: true));

    // Title + Porcelain Light default
    final scaffold = tester.widget<Scaffold>(find.byType(Scaffold));
    expect(scaffold.backgroundColor, const Color(0xFFF5F5F7));
    expect(find.text('CuadernU'), findsOneWidget);

    // All 4 unified sections visible in bottom navigation
    expect(find.text('Cuaderno'), findsOneWidget);
    expect(find.text('Calculadoras'), findsOneWidget);
    expect(find.text('STEM'), findsOneWidget);
    expect(find.text('Estudio'), findsOneWidget);

    // Switch to Calculadoras -> all 4 Casio calculators present
    await tester.tap(find.text('Calculadoras'));
    await tester.pumpAndSettle();
    expect(find.text('CG50'), findsOneWidget);
    expect(find.text('991CW'), findsOneWidget);
    expect(find.text('570LA X'), findsOneWidget);
    expect(find.text('570VN+'), findsOneWidget);

    // Switch to STEM -> CAS, Química, Circuitos, Horarios TEC
    await tester.tap(find.text('STEM'));
    await tester.pumpAndSettle();
    expect(find.text('CAS'), findsOneWidget);
    expect(find.text('Química'), findsOneWidget);
    expect(find.text('Circuitos'), findsOneWidget);
    expect(find.text('Horarios TEC'), findsOneWidget);

    // Switch to Estudio -> Grafo, Bases de Datos, Estudio SM-2
    await tester.tap(find.text('Estudio'));
    await tester.pumpAndSettle();
    expect(find.text('Grafo'), findsOneWidget);
    expect(find.text('Bases de Datos'), findsOneWidget);
    expect(find.text('Estudio SM-2'), findsOneWidget);
  });

  test('End-to-end CuadernU Studio C++17 FFI through Dart bindings (.cudoc + CAS + Chem + MNA + Graph + SM-2 + Horarios)',
      () {
    if (!Platform.isWindows) return;
    final casio = CasioNativeBindings.open(resolveCasioNativeDll());
    final studio = CuadernuStudioBindings.fromCasio(casio);
    expect(studio.isAvailable, isTrue,
        reason: 'cuadernu_* FFI symbols must be exported from casio_native.dll');

    final caps = studio.capabilities();
    final features = (caps['features'] as List?) ?? const [];
    expect(features, contains('cudoc_zip_interop'));
    expect(features, contains('engineering_cas_varsub'));
    expect(features, contains('mna_circuit_dc_ac_bode_transient_laplace'));
    expect(features, contains('horarios_tec_solver'));

    // 1. Document CRUD + .cudoc binary save/reload + PDF/MD/DXF export
    final tmpDir = Directory.systemTemp.createTempSync('cuadernu_dart_ffi_');
    try {
      final doc = studio.createDocument(infiniteCanvas: false);
      try {
        doc.setTitle('Apuntes Circuitos TEC');
        doc.setPaper(CuadernuPaperPattern.grid);
        expect(doc.pageCount, 1);

        final inkId = doc.addInkStroke(
          onCanvas: false,
          pageIndex: 0,
          points: const [
            (x: 20, y: 20, p: 0.7),
            (x: 80, y: 50, p: 0.8),
            (x: 140, y: 35, p: 0.6),
          ],
          rgba: 0x0071E3FF,
          strokeWidth: 2.5,
        );
        expect(inkId, greaterThan(0));
        expect(doc.canUndo, isTrue);

        final mathId = doc.addMathBlock(
          onCanvas: false,
          pageIndex: 0,
          x: 40,
          y: 120,
          w: 240,
          h: 60,
          expr: 'I = 12 V / 2 kohm | mA',
        );
        expect(mathId, greaterThan(0));

        final fakePng = Uint8List.fromList(const [
          0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D
        ]);
        final cardId = doc.addCasioCard(
          onCanvas: false,
          pageIndex: 0,
          x: 40,
          y: 220,
          w: 260,
          h: 160,
          calcModel: 'fx-CG50',
          expr: 'sin(30)',
          result: '0.5',
          keySeq: 'sin 3 0 EXE',
          pngBytes: fakePng,
        );
        expect(cardId, greaterThan(0));

        final tapeId = doc.addStudyTape(
          onCanvas: false,
          pageIndex: 0,
          x: 40,
          y: 400,
          w: 160,
          h: 32,
          label: 'Ley de Ohm',
        );
        expect(tapeId, greaterThan(0));
        expect(doc.toggleStudyTape(onCanvas: false, pageIndex: 0, objectId: tapeId), isTrue);

        final hits = doc.search('ohm', accentInsensitive: true);
        expect(hits, isNotEmpty);

        final cudocPath = '${tmpDir.path}/prueba.cudoc';
        expect(doc.saveCudoc(cudocPath), isTrue);
        expect(File(cudocPath).lengthSync(), greaterThan(100));

        final pdfPath = '${tmpDir.path}/prueba.pdf';
        expect(doc.exportPdf(pdfPath), isTrue);
        expect(File(pdfPath).readAsBytesSync().sublist(0, 4),
            equals(const [0x25, 0x50, 0x44, 0x46])); // %PDF

        final md = doc.exportMarkdown();
        expect(md, contains('Apuntes Circuitos TEC'));

        final dxfPath = '${tmpDir.path}/prueba.dxf';
        expect(doc.exportDxf(dxfPath), isTrue);
        expect(File(dxfPath).lengthSync(), greaterThan(50));

        final flashcards = doc.buildFlashcards();
        expect(flashcards, isNotEmpty);

        // Reload into fresh native doc -> byte/model parity
        final doc2 = studio.createDocument();
        try {
          expect(doc2.loadCudoc(cudocPath), isTrue);
          final snap = doc2.getSnapshot();
          expect(snap['title'], 'Apuntes Circuitos TEC');
          final pages = snap['pages'] as List;
          expect((pages.first['objects'] as List).length, greaterThanOrEqualTo(4));
        } finally {
          doc2.dispose();
        }
      } finally {
        doc.dispose();
      }
    } finally {
      tmpDir.deleteSync(recursive: true);
    }

    // 2. Engineering CAS + Chemistry + MNA + K-Map + Graph + SM-2 + Horarios
    final cas = studio.evalCas('12 V / 2 kohm | mA');
    expect(cas['ok'], isTrue);
    expect((cas['real'] as num).toDouble(), closeTo(0.006, 1e-6));

    final eq = studio.solveEquation('x^2 - 5*x + 6 = 0', varName: 'x');
    expect(eq['ok'], isTrue);

    final sel = studio.solveLinearSystem('2*x + y = 7; x - y = 2');
    expect(sel['ok'], isTrue);

    final mat = studio.evalMatrix('det([[2, 1], [5, 4]])');
    expect(mat['ok'], isTrue);
    expect((mat['scalarValue'] as num).toDouble(), closeTo(3.0, 1e-6));

    final plot = studio.samplePlot('sin(x)', xMin: 0, xMax: 6.28, numSamples: 32);
    expect(plot['ok'], isTrue);
    expect((plot['points'] as List).length, greaterThan(20));

    final chem = studio.analyzeChemistry('Fe + O2 -> Fe2O3');
    expect(chem['valid'], isTrue);
    expect(chem['balanced'], isTrue);

    final dc = studio.solveCircuit('V1 1 0 12\nR1 1 2 1k\nR2 2 0 1k', mode: 'dc');
    expect(dc['ok'], isTrue);
    expect(((dc['nodeVoltages']['2']['mag']) as num).toDouble(), closeTo(6.0, 1e-3));

    final kmap = studio.simplifyKMap(3, '0,1,2,3');
    expect(kmap['ok'], isTrue);

    final graph = studio.buildKnowledgeGraph([
      {'id': 'a', 'title': 'Ohm', 'folder': 'F', 'body': 'Ver [[Kirchhoff]]', 'tags': []},
      {'id': 'b', 'title': 'Kirchhoff', 'folder': 'F', 'body': 'Base', 'tags': []},
    ]);
    expect((graph['nodes'] as List).length, 2);
    expect((graph['edges'] as List).length, 1);

    final sm2 = studio.reviewSm2(
        repetitions: 1, intervalDays: 1, easeFactor: 2.5, quality: 5);
    expect(sm2['repetitions'], 2);
    expect((sm2['intervalDays'] as num).toInt(), greaterThanOrEqualTo(6));

    final horarios = studio.solveHorariosTec({
      'courses': [
        {
          'code': 'MA1102',
          'name': 'Cálculo',
          'credits': 4,
          'groups': [
            {
              'id': 'Gr 1',
              'professor': 'Prof. A',
              'sessions': [
                {'day': 'L', 'start': '07:30', 'end': '09:20'}
              ]
            }
          ]
        }
      ]
    });
    expect(horarios['ok'], isTrue);
    expect((horarios['schedules'] as List).length, greaterThanOrEqualTo(1));
  });
}
