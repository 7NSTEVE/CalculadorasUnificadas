import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';

import 'package:flutter_test/flutter_test.dart';

import 'package:calculadoras_unificadas/connectivity/cg50_local_bridge_server.dart';
import 'package:calculadoras_unificadas/keylog/cg50_keylog.dart';
import 'package:calculadoras_unificadas/native/calc_models.dart';
import 'package:calculadoras_unificadas/native/casio_ffi_bindings.dart';
import 'package:calculadoras_unificadas/ui/cg50_capture_and_lcd_sheets.dart';

class _FakeBridgeBackend implements Cg50BridgeBackend {
  final Map<String, Uint8List> files = {
    '/Geometry.g3a': Uint8List.fromList([0xAA, 0xAC, 0xBD, 0xAF]),
  };
  final Set<String> dirs = {'/'};
  bool usbConnected = false;
  bool linkEnabled = false;
  int remountCount = 0;
  final List<int> rxReceived = [];
  final StreamController<Uint8List> txCtrl = StreamController<Uint8List>.broadcast();
  final Uint8List _rgba = Uint8List.fromList(List<int>.filled(8 * 4 * 4, 0xCC));

  @override
  Future<Cg50SmemListing?> listSmem(String path) async {
    final norm = path.isEmpty ? '/' : path;
    final entries = <Cg50SmemEntry>[];
    for (final d in dirs) {
      if (d != '/' && d.startsWith(norm == '/' ? '/' : '$norm/')) {
        final name = d.split('/').last;
        entries.add(Cg50SmemEntry(
          name: name,
          shortName: name.toUpperCase(),
          path: d,
          isDir: true,
          size: 0,
          modDate: 0,
          modTime: 0,
        ));
      }
    }
    for (final e in files.entries) {
      final name = e.key.split('/').last;
      entries.add(Cg50SmemEntry(
        name: name,
        shortName: name.toUpperCase(),
        path: e.key,
        isDir: false,
        size: e.value.length,
        modDate: 0,
        modTime: 0,
      ));
    }
    return Cg50SmemListing(
      path: norm,
      volumeLabel: 'CASIO_CG50',
      totalBytes: 16 * 1024 * 1024,
      freeBytes: 12 * 1024 * 1024,
      usedBytes: 4 * 1024 * 1024,
      clusterSize: 4096,
      fatEqual: true,
      entries: entries,
    );
  }

  @override
  Future<Uint8List?> readSmemFile(String path) async => files[path];

  @override
  Future<bool> writeSmemFile(
    String path,
    Uint8List data, {
    bool overwrite = true,
    bool remountOs = false,
  }) async {
    if (!overwrite && files.containsKey(path)) return false;
    files[path] = Uint8List.fromList(data);
    if (remountOs) remountCount++;
    return true;
  }

  @override
  Future<bool> deleteSmemPath(
    String path, {
    bool recursive = false,
    bool remountOs = false,
  }) async {
    final removed = files.remove(path) != null || dirs.remove(path);
    if (removed && remountOs) remountCount++;
    return removed;
  }

  @override
  Future<bool> mkdirSmem(String path, {bool remountOs = false}) async {
    dirs.add(path);
    return true;
  }

  @override
  Future<bool> remountSmemOs() async {
    remountCount++;
    return true;
  }

  @override
  Future<bool> setUsbConnected(bool connected) async {
    usbConnected = connected;
    return true;
  }

  @override
  Future<Cg50UsbStatus?> getUsbStatus() async => Cg50UsbStatus(
        connected: usbConnected,
        vbus: usbConnected,
        configured: false,
        deviceAddress: 0,
        vid: 0x07CF,
        pid: 0x6102,
        mode: usbConnected ? 'vbus_ready' : 'disconnected',
        linkEnabled: linkEnabled,
      );

  @override
  Future<bool> setLinkEnabled(bool enabled) async {
    linkEnabled = enabled;
    return true;
  }

  @override
  Future<int> linkPushRx(Uint8List data) async {
    rxReceived.addAll(data);
    return data.length;
  }

  @override
  Stream<Uint8List> get onLinkTx => txCtrl.stream;

  @override
  Future<String?> decodeLcdQr() async =>
      'http://wes.casio.com/math/index.php?q=TEST';

  @override
  Uint8List? get lastRgbaBytes => _rgba;

  @override
  int get frameWidth => 8;

  @override
  int get frameHeight => 4;

  void dispose() => txCtrl.close();
}

void main() {
  test('Cg50LocalBridgeServer direct no-PIN HTTP CRUD, screen PNG, QR and WS Link',
      () async {
    final backend = _FakeBridgeBackend();
    final server = Cg50LocalBridgeServer(backend);
    expect(await server.start(port: 0), isTrue);
    final base = Uri.parse('http://127.0.0.1:${server.port}');
    final client = HttpClient();

    try {
      // 1. Web UI served directly with zero PIN/password
      final reqHtml = await client.getUrl(base.resolve('/'));
      final resHtml = await reqHtml.close();
      final html = await utf8.decoder.bind(resHtml).join();
      expect(resHtml.statusCode, 200);
      expect(html, contains('Casio fx-CG50'));
      expect(html.toLowerCase().contains('pin'), isFalse);

      // 2. Info endpoint
      final reqInfo = await client.getUrl(base.resolve('/api/info'));
      final resInfo = await reqInfo.close();
      final info = jsonDecode(await utf8.decoder.bind(resInfo).join())
          as Map<String, dynamic>;
      expect(resInfo.statusCode, 200);
      expect(info['device'], 'Casio fx-CG50');

      // 3. Upload -> download -> delete file roundtrip
      final payload = utf8.encode('print(6*7)\n');
      final reqPut = await client
          .postUrl(base.resolve('/api/file?path=/hola.py&remount=1'));
      reqPut.add(payload);
      final resPut = await reqPut.close();
      expect(resPut.statusCode, 200);
      expect(backend.remountCount, 1);

      final reqGet =
          await client.getUrl(base.resolve('/api/file?path=/hola.py'));
      final resGet = await reqGet.close();
      final got = await resGet.fold<List<int>>(<int>[], (a, b) => a..addAll(b));
      expect(resGet.statusCode, 200);
      expect(got, payload);

      final reqDel =
          await client.deleteUrl(base.resolve('/api/file?path=/hola.py'));
      final resDel = await reqDel.close();
      expect(resDel.statusCode, 200);

      final reqMissing =
          await client.getUrl(base.resolve('/api/file?path=/hola.py'));
      final resMissing = await reqMissing.close();
      expect(resMissing.statusCode, 404);

      // 4. Screen PNG + QR endpoints
      final reqPng = await client.getUrl(base.resolve('/api/screen.png'));
      final resPng = await reqPng.close();
      final png = await resPng.fold<List<int>>(<int>[], (a, b) => a..addAll(b));
      expect(resPng.statusCode, 200);
      expect(png.sublist(0, 8), [137, 80, 78, 71, 13, 10, 26, 10]);

      final reqQr = await client.getUrl(base.resolve('/api/qr'));
      final resQr = await reqQr.close();
      final qrJson = jsonDecode(await utf8.decoder.bind(resQr).join())
          as Map<String, dynamic>;
      expect(qrJson['qr'], 'http://wes.casio.com/math/index.php?q=TEST');

      // 5. Bidirectional WebSocket Calc <-> Calc SCIF0 Link tunnel
      final ws = await WebSocket.connect('ws://127.0.0.1:${server.port}/ws/link');
      final receivedFromCalc = Completer<List<int>>();
      ws.listen((dynamic data) {
        if (!receivedFromCalc.isCompleted && data is List<int>) {
          receivedFromCalc.complete(data);
        }
      });
      ws.add(Uint8List.fromList([0x05, 0x30, 0x30]));
      await Future<void>.delayed(const Duration(milliseconds: 50));
      expect(backend.linkEnabled, isTrue);
      expect(backend.rxReceived, [0x05, 0x30, 0x30]);

      backend.txCtrl.add(Uint8List.fromList([0x06, 0x4F, 0x4B]));
      final echoed = await receivedFromCalc.future
          .timeout(const Duration(seconds: 2));
      expect(echoed, [0x06, 0x4F, 0x4B]);
      await ws.close();
    } finally {
      client.close(force: true);
      await server.stop();
      backend.dispose();
    }
  });

  test('Cg50KeyLogController recording, editing and JSON roundtrip', () async {
    final kl = Cg50KeyLogController();
    await kl.startRecording(null);
    kl.recordKeyPress(const CasioKeyDef(6, 0, 0, 10, 10, 0x0709, 'SHIFT', 'SHIFT'));
    kl.recordKeyPress(const CasioKeyDef(35, 0, 0, 10, 10, 0x0704, '4', '4'));
    kl.insertPause(800);
    kl.addNote('Abrir catálogo');
    kl.stop();

    expect(kl.activeRecord.steps.length, 4);
    expect(kl.formatStepsAsText(), '[SHIFT] [4] [PAUSA] (Abrir catálogo)');

    final jsonStr = kl.exportJsonString();
    final kl2 = Cg50KeyLogController();
    expect(kl2.importJsonString(jsonStr), isTrue);
    expect(kl2.activeRecord.steps.length, 4);
    expect(kl2.activeRecord.steps[0].keyCode, 0x0709);
    expect(kl2.activeRecord.steps[2].isPause, isTrue);
    expect(kl2.activeRecord.steps[3].note, 'Abrir catálogo');
    // Negative: malformed JSON rejected
    expect(kl2.importJsonString('{"invalid":true}'), isFalse);
  });

  test('Capture scaling with outer frame and PNG/BMP encoding', () {
    final src = Uint8List.fromList(List<int>.filled(4 * 2 * 4, 0xFF));
    final scaled = composeScaledCapture(
      srcRgba: src,
      srcW: 4,
      srcH: 2,
      scale: 2,
      includeFrame: true,
    );
    expect(scaled.width, 4 * 2 + 40);
    expect(scaled.height, 2 * 2 + 40);
    final png = encodeRgbaToPng(scaled.rgba, scaled.width, scaled.height);
    expect(png.sublist(0, 8), [137, 80, 78, 71, 13, 10, 26, 10]);
    final bmp = encodeRgbaToBmp(scaled.rgba, scaled.width, scaled.height);
    expect(bmp[0], 0x42); // 'B'
    expect(bmp[1], 0x4D); // 'M'
  });
}
