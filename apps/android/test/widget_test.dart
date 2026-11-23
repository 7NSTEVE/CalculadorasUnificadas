import 'dart:ffi' hide Size;
import 'dart:io';

import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

import 'package:calculadoras_unificadas/main.dart';
import 'package:calculadoras_unificadas/native/calc_models.dart';
import 'package:calculadoras_unificadas/native/casio_ffi_bindings.dart';
import 'package:calculadoras_unificadas/ui/native_calculator_view.dart';

String resolveCasioNativeDll() {
  final envPath = Platform.environment['CASIO_NATIVE_LIB'];
  final candidates = <String>[
    if (envPath != null && envPath.isNotEmpty) envPath,
    '../../build-core-host/Release/casio_native.dll',
    r'D:\Proyectos\CalculadorasUnificadas\build-core-host\Release\casio_native.dll',
  ];
  for (final c in candidates) {
    final f = File(c);
    if (f.existsSync()) return f.path;
  }
  throw StateError('Unable to resolve casio_native.dll in $candidates');
}

const String kMonoRomsDir = r'D:\Proyectos\CalculadorasUnificadas\assets\roms';

void main() {
  test('All 4 skins define exact key counts (50 CG50, 48 CW2, 50 LAX, 50 VN) with accurate hitTest',
      () {
    final expectedCounts = {
      CasioCalcKind.cg50: 50,
      CasioCalcKind.cw2: 48,
      CasioCalcKind.classwiz: 50,
      CasioCalcKind.naturalEs: 50,
    };

    for (final skin in kAllSkins) {
      expect(skin.keys.length, expectedCounts[skin.kind],
          reason: '${skin.title} key count mismatch');
      final renderSize = Size(skin.skinWidth, skin.skinHeight);
      for (final key in skin.keys) {
        final hit = skin.hitTest(key.cx, key.cy, renderSize);
        expect(hit?.id, key.id, reason: '${skin.title} key ${key.name} missed');
        expect(hit?.name, key.name);
      }
      expect(skin.keys.map((e) => e.id).toSet().length, skin.keys.length);
      expect(skin.keys.map((e) => e.matrixCode).toSet().length, skin.keys.length);
    }

    // Pin canonical CG50 matrix codes against cg50_keys_exact.h
    final cgById = {for (final k in kCg50Keys) k.id: k};
    expect(cgById[0]?.matrixCode, 0x070A);  // F1
    expect(cgById[5]?.matrixCode, 0x020A);  // F6
    expect(cgById[6]?.matrixCode, 0x0709);  // SHIFT
    expect(cgById[7]?.matrixCode, 0x0609);  // OPTN
    expect(cgById[9]?.matrixCode, 0x0409);  // MENU
    expect(cgById[16]?.matrixCode, 0x0508); // ^ (power)
    expect(cgById[25]?.matrixCode, 0x0606); // S<->D
    expect(cgById[34]?.matrixCode, 0x0101); // AC/ON
    expect(cgById[48]?.matrixCode, 0x0402); // (-) unary / SHIFT=Ans
    expect(cgById[49]?.matrixCode, 0x0302); // EXE
  });

  testWidgets('CW2 20-indicator strip renders S, Math, D, f(x), i, arrows accurately',
      (WidgetTester tester) async {
    await tester.pumpWidget(
      const MaterialApp(
        home: Scaffold(
          body: CasioTopIndicatorStrip(
            mask: CasioCw2IndicatorBits.shift |
                CasioCw2IndicatorBits.math |
                CasioCw2IndicatorBits.deg |
                CasioCw2IndicatorBits.fx |
                CasioCw2IndicatorBits.cmplx |
                CasioCw2IndicatorBits.right,
            isCw2Layout: true,
            foreground: Color(0xFF14243A),
            background: Color(0xFFE4EBE4),
            height: 20,
          ),
        ),
      ),
    );
    expect(find.text('S'), findsOneWidget);
    expect(find.text('Math'), findsOneWidget);
    expect(find.text('D'), findsOneWidget);
    expect(find.text('f(x)'), findsOneWidget);
    expect(find.text('i'), findsOneWidget);
    expect(find.text('▶'), findsOneWidget);
    expect(find.text('R'), findsNothing);
  });

  testWidgets('guías de interfaz Porcelain Light default, 4-model switcher, themes & history work cleanly',
      (WidgetTester tester) async {
    await tester.pumpWidget(
      const CalculadorasUnificadasApp(disableNativeBoot: true),
    );

    // Default theme must be Porcelain Light (#F5F5F7), NOT dark neon
    final scaffold = tester.widget<Scaffold>(find.byType(Scaffold));
    expect(scaffold.backgroundColor, const Color(0xFFF5F5F7));

    // All 4 calculators in segmented selector
    expect(find.text('CG50'), findsOneWidget);
    expect(find.text('991CW'), findsOneWidget);
    expect(find.text('570LA X'), findsOneWidget);
    expect(find.text('570VN+'), findsOneWidget);

    // Zero internal jargon
    expect(find.textContaining('ROM Oficial'), findsNothing);
    expect(find.textContaining('SH7305'), findsNothing);
    expect(find.textContaining('UserROM'), findsNothing);

    // CG50 contextual menu exposes capture + memory backup/restore + .g3a loader cleanly
    await tester.tap(find.byIcon(CupertinoIcons.ellipsis_circle));
    await tester.pumpAndSettle();
    expect(find.text('Capturar pantalla (PNG)'), findsOneWidget);
    expect(find.text('Exportar memoria'), findsOneWidget);
    expect(find.text('Restaurar memoria'), findsOneWidget);
    expect(find.text('Cargar complemento (.g3a)'), findsOneWidget);
    // Dismiss menu
    await tester.tapAt(const Offset(10, 10));
    await tester.pumpAndSettle();

    await tester.tap(find.text('991CW'));
    await tester.pump();
    expect(find.text('fx-991CW'), findsOneWidget);

    // Switch theme via overflow menu (Calibrated Dark & High Contrast)
    await tester.tap(find.byIcon(CupertinoIcons.ellipsis_circle));
    await tester.pumpAndSettle();
    expect(find.text('Porcelana Claro'), findsOneWidget);
    expect(find.text('Oscuro Calibrado'), findsOneWidget);
    expect(find.text('Alto Contraste'), findsOneWidget);
    expect(find.text('Reducir transparencia'), findsOneWidget);
    expect(find.text('Capturar pantalla (PNG)'), findsOneWidget);
    expect(find.text('Exportar memoria'), findsNothing);

    await tester.tap(
      find
          .ancestor(
            of: find.text('Oscuro Calibrado'),
            matching: find.byWidgetPredicate((w) => w is PopupMenuItem),
          )
          .first,
    );
    await tester.pumpAndSettle();
    final darkScaffold = tester.widget<Scaffold>(find.byType(Scaffold));
    expect(darkScaffold.backgroundColor, const Color(0xFF121417));

    await tester.tap(find.byIcon(CupertinoIcons.ellipsis_circle));
    await tester.pumpAndSettle();
    await tester.tap(
      find
          .ancestor(
            of: find.text('Alto Contraste'),
            matching: find.byWidgetPredicate((w) => w is PopupMenuItem),
          )
          .first,
    );
    await tester.pumpAndSettle();
    final hcScaffold = tester.widget<Scaffold>(find.byType(Scaffold));
    expect(hcScaffold.backgroundColor, const Color(0xFFFFFFFF));

    // History sheet
    await tester.tap(find.byIcon(CupertinoIcons.clock));
    await tester.pumpAndSettle();
    expect(find.text('Historial (0)'), findsOneWidget);
  });

  test('End-to-end FFI verification for all 4 calculators on host', () {
    if (!Platform.isWindows) return;
    final b = CasioNativeBindings.open(resolveCasioNativeDll());

    // 1. fx-991CW (ClassWiz II)
    final cw2Rom = b.allocCString('$kMonoRomsDir/fx991cw_rom.bin');
    final cw2Ram = b.allocCString('$kMonoRomsDir/fx991cw_default_en.ram');
    final cw2 = b.cw2CreateFromFiles(cw2Rom, cw2Ram);
    b.free(cw2Rom.cast());
    b.free(cw2Ram.cast());
    expect(cw2.address, isNonZero);
    try {
      expect(b.cw2ScreenWidth(cw2), 192);
      expect(b.cw2ScreenHeight(cw2), 63);
      b.cw2PowerOn(cw2);
      b.cw2Advance(cw2, 250);
      final rgba = b.alloc(192 * 63 * 4).cast<Uint8>();
      final ind = b.alloc(4).cast<Uint32>();
      try {
        expect(b.cw2RenderRgba(cw2, rgba, 192 * 63 * 4, ind, 1), 1);
        expect(b.cw2RenderRgba(cw2, rgba, 192 * 63 * 4, ind, 0), 0); // dirty skip
        b.cw2TapKey(cw2, 0x05, 90, 140); // SHIFT
        b.cw2RenderRgba(cw2, rgba, 192 * 63 * 4, ind, 1);
        expect(ind.value & CasioCw2IndicatorBits.shift, isNonZero);
        expect(b.cw2ExportRam(cw2, nullptr, 0), 24832);
      } finally {
        b.free(rgba.cast());
        b.free(ind.cast());
      }
    } finally {
      b.cw2Destroy(cw2);
    }

    // 2. fx-570LA X + fx-570VN PLUS
    for (final entry in [
      (4, '$kMonoRomsDir/fx991lax.rom', 192, 63),
      (3, '$kMonoRomsDir/fx570vnplus.rom', 96, 31),
    ]) {
      final p = b.allocCString(entry.$2);
      final h = b.nxCreateFromFile(entry.$1, p);
      b.free(p.cast());
      expect(h.address, isNonZero);
      try {
        expect(b.nxScreenWidth(h), entry.$3);
        expect(b.nxScreenHeight(h), entry.$4);
        final rgba = b.alloc(entry.$3 * entry.$4 * 4).cast<Uint8>();
        final ind = b.alloc(4).cast<Uint32>();
        try {
          b.nxTapKey(h, 0x07, 150, 150);
          b.nxRenderRgba(h, rgba, entry.$3 * entry.$4 * 4, ind, 1);
          expect(ind.value & CasioIndicatorBits.shift, isNonZero);
        } finally {
          b.free(rgba.cast());
          b.free(ind.cast());
        }
      } finally {
        b.nxDestroy(h);
      }
    }

    // 3. fx-CG50 PRIZM warm boot + dirty skip + negative guard
    final dPtr = b.allocCString(kMonoRomsDir);
    final cg = b.cgCreateFromDir(dPtr);
    b.free(dPtr.cast());
    expect(cg.address, isNonZero);
    try {
      final warmPtr = b.allocCString('$kMonoRomsDir/cg50_warm_state.bin');
      expect(b.cgImportStateFile(cg, warmPtr), 1);
      b.free(warmPtr.cast());
      final fb = b.alloc(384 * 216 * 4).cast<Uint8>();
      try {
        expect(b.cgRenderRgba(cg, fb, 384 * 216 * 4), 1);
        expect(b.cgRenderRgba(cg, fb, 384 * 216 * 4), 0); // dirty skip
      } finally {
        b.free(fb.cast());
      }
      final badPtr = b.allocCString('$kMonoRomsDir/nonexistent.bin');
      expect(b.cgImportStateFile(cg, badPtr), 0);
      expect(b.cgImportUserRomFile(cg, badPtr), 0);
      expect(b.cgLoadAddinFile(cg, badPtr), 0);
      b.free(badPtr.cast());

      // RTC wall-clock sync + UserROM export contract
      expect(b.cgSetRtcEpoch(cg, 1795392000), 1);
      final tmpUr = File('${Directory.systemTemp.path}/cg50_test_userrom.raw');
      final urPtr = b.allocCString(tmpUr.path);
      try {
        expect(b.cgExportUserRomFile(cg, urPtr), 1);
        expect(tmpUr.existsSync(), isTrue);
        expect(tmpUr.lengthSync(), 21626880);
      } finally {
        b.free(urPtr.cast());
        if (tmpUr.existsSync()) tmpUr.deleteSync();
      }
    } finally {
      b.cgDestroy(cg);
    }
  });
}
