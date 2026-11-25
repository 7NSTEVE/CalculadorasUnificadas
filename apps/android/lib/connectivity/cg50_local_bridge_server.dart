import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';

import 'package:flutter/foundation.dart';

import '../native/casio_ffi_bindings.dart';

// ============================================================================
// Pure-Dart lossless PNG & BMP encoders (zero external packages, works in
// Flutter Android, desktop, and headless unit tests).
// ============================================================================
final Uint32List _crc32Table = () {
  final table = Uint32List(256);
  for (int n = 0; n < 256; n++) {
    int c = n;
    for (int k = 0; k < 8; k++) {
      c = ((c & 1) != 0) ? (0xEDB88320 ^ (c >>> 1)) : (c >>> 1);
    }
    table[n] = c;
  }
  return table;
}();

int _crc32(List<int> bytes, [int seed = 0]) {
  int c = seed ^ 0xFFFFFFFF;
  for (int i = 0; i < bytes.length; i++) {
    c = _crc32Table[(c ^ bytes[i]) & 0xFF] ^ (c >>> 8);
  }
  return (c ^ 0xFFFFFFFF) & 0xFFFFFFFF;
}

void _writePngChunk(BytesBuilder out, String type, List<int> data) {
  final hdr = ByteData(4)..setUint32(0, data.length, Endian.big);
  final typeBytes = ascii.encode(type);
  out.add(hdr.buffer.asUint8List());
  out.add(typeBytes);
  out.add(data);
  int crc = _crc32(typeBytes);
  crc = _crc32(data, crc);
  final crcBuf = ByteData(4)..setUint32(0, crc, Endian.big);
  out.add(crcBuf.buffer.asUint8List());
}

Uint8List encodeRgbaToPng(Uint8List rgba, int width, int height) {
  final expected = width * height * 4;
  if (width <= 0 || height <= 0 || rgba.length < expected) {
    return Uint8List(0);
  }
  final out = BytesBuilder(copy: false);
  out.add(const [137, 80, 78, 71, 13, 10, 26, 10]); // PNG signature

  final ihdr = ByteData(13)
    ..setUint32(0, width, Endian.big)
    ..setUint32(4, height, Endian.big)
    ..setUint8(8, 8) // bit depth
    ..setUint8(9, 6) // RGBA
    ..setUint8(10, 0)
    ..setUint8(11, 0)
    ..setUint8(12, 0);
  _writePngChunk(out, 'IHDR', ihdr.buffer.asUint8List());

  final rowBytes = width * 4;
  final raw = Uint8List((rowBytes + 1) * height);
  for (int y = 0; y < height; y++) {
    final dstOff = y * (rowBytes + 1);
    raw[dstOff] = 0; // filter None
    raw.setRange(dstOff + 1, dstOff + 1 + rowBytes, rgba, y * rowBytes);
  }
  final compressed = ZLibEncoder(level: 3).convert(raw);
  _writePngChunk(out, 'IDAT', compressed);
  _writePngChunk(out, 'IEND', const []);
  return out.takeBytes();
}

Uint8List encodeRgbaToBmp(Uint8List rgba, int width, int height) {
  final expected = width * height * 4;
  if (width <= 0 || height <= 0 || rgba.length < expected) {
    return Uint8List(0);
  }
  final rowStride = ((width * 3 + 3) ~/ 4) * 4;
  final pixelArraySize = rowStride * height;
  final fileSize = 54 + pixelArraySize;
  final bmp = Uint8List(fileSize);
  final bd = ByteData.sublistView(bmp);
  // BITMAPFILEHEADER
  bmp[0] = 0x42; // 'B'
  bmp[1] = 0x4D; // 'M'
  bd.setUint32(2, fileSize, Endian.little);
  bd.setUint32(10, 54, Endian.little);
  // BITMAPINFOHEADER
  bd.setUint32(14, 40, Endian.little);
  bd.setInt32(18, width, Endian.little);
  bd.setInt32(22, height, Endian.little); // bottom-up
  bd.setUint16(26, 1, Endian.little);
  bd.setUint16(28, 24, Endian.little); // 24-bit BGR
  bd.setUint32(34, pixelArraySize, Endian.little);

  for (int y = 0; y < height; y++) {
    final dstRow = 54 + (height - 1 - y) * rowStride;
    final srcRow = y * width * 4;
    for (int x = 0; x < width; x++) {
      final s = srcRow + x * 4;
      final d = dstRow + x * 3;
      bmp[d] = rgba[s + 2]; // B
      bmp[d + 1] = rgba[s + 1]; // G
      bmp[d + 2] = rgba[s]; // R
    }
  }
  return bmp;
}

// ============================================================================
// Decoupled Backend Interface (real CasioCg50Engine or unit-test fake)
// ============================================================================
abstract class Cg50BridgeBackend {
  Future<Cg50SmemListing?> listSmem(String path);
  Future<Uint8List?> readSmemFile(String path);
  Future<bool> writeSmemFile(
    String path,
    Uint8List data, {
    bool overwrite = true,
    bool remountOs = false,
  });
  Future<bool> deleteSmemPath(
    String path, {
    bool recursive = false,
    bool remountOs = false,
  });
  Future<bool> mkdirSmem(String path, {bool remountOs = false});
  Future<bool> remountSmemOs();
  Future<bool> setUsbConnected(bool connected);
  Future<Cg50UsbStatus?> getUsbStatus();
  Future<bool> setLinkEnabled(bool enabled);
  Future<int> linkPushRx(Uint8List data);
  Stream<Uint8List> get onLinkTx;
  Future<String?> decodeLcdQr();
  Uint8List? get lastRgbaBytes;
  int get frameWidth;
  int get frameHeight;
}

class Cg50EngineBridgeBackend implements Cg50BridgeBackend {
  final CasioCg50Engine engine;
  Cg50EngineBridgeBackend(this.engine);

  @override
  Future<Cg50SmemListing?> listSmem(String path) => engine.listSmem(path);

  @override
  Future<Uint8List?> readSmemFile(String path) => engine.readSmemFile(path);

  @override
  Future<bool> writeSmemFile(
    String path,
    Uint8List data, {
    bool overwrite = true,
    bool remountOs = false,
  }) =>
      engine.writeSmemFile(path, data,
          overwrite: overwrite, remountOs: remountOs);

  @override
  Future<bool> deleteSmemPath(
    String path, {
    bool recursive = false,
    bool remountOs = false,
  }) =>
      engine.deleteSmemPath(path,
          recursive: recursive, remountOs: remountOs);

  @override
  Future<bool> mkdirSmem(String path, {bool remountOs = false}) =>
      engine.mkdirSmem(path, remountOs: remountOs);

  @override
  Future<bool> remountSmemOs() => engine.remountSmemOs();

  @override
  Future<bool> setUsbConnected(bool connected) =>
      engine.setUsbConnected(connected);

  @override
  Future<Cg50UsbStatus?> getUsbStatus() => engine.getUsbStatus();

  @override
  Future<bool> setLinkEnabled(bool enabled) => engine.setLinkEnabled(enabled);

  @override
  Future<int> linkPushRx(Uint8List data) => engine.linkPushRx(data);

  @override
  Stream<Uint8List> get onLinkTx => engine.onLinkTx;

  @override
  Future<String?> decodeLcdQr() => engine.decodeLcdQr();

  @override
  Uint8List? get lastRgbaBytes => engine.lastRgbaBytes;

  @override
  int get frameWidth => engine.frameWidth;

  @override
  int get frameHeight => engine.frameHeight;
}

// ============================================================================
// Direct No-PIN / No-Password Local PC Bridge + Calc-to-Calc Link Server
// OFF by default; starts only when explicitly enabled by the user.
// ============================================================================
class Cg50LocalBridgeServer extends ChangeNotifier {
  static const int defaultPort = 8750;

  final Cg50BridgeBackend backend;
  HttpServer? _server;
  int _boundPort = 0;
  List<String> _addresses = const [];
  String? _lastError;

  // Calc <-> Calc SCIF0 WebSocket Link peers
  final Set<WebSocket> _linkPeers = {};
  StreamSubscription<Uint8List>? _txSub;
  int _linkTxBytes = 0;
  int _linkRxBytes = 0;

  // H-006 opt-in security controls (preserve direct no-PIN default for local UX)
  bool readOnlyMode = false;
  bool loopbackOnly = false;
  String? optionalPairingToken;
  bool requireConfirmForRecursiveDelete = false;

  Cg50LocalBridgeServer(
    this.backend, {
    this.readOnlyMode = false,
    this.loopbackOnly = false,
    this.optionalPairingToken,
    this.requireConfirmForRecursiveDelete = false,
  });

  bool get isRunning => _server != null;
  int get port => _boundPort;
  List<String> get addresses => _addresses;
  String? get lastError => _lastError;
  int get linkPeerCount => _linkPeers.length;
  int get linkTxBytes => _linkTxBytes;
  int get linkRxBytes => _linkRxBytes;

  Future<bool> start({int port = defaultPort}) async {
    if (_server != null) return true;
    _lastError = null;
    try {
      final bindAddr =
          loopbackOnly ? InternetAddress.loopbackIPv4 : InternetAddress.anyIPv4;
      _server = await HttpServer.bind(bindAddr, port, shared: true);
      _boundPort = _server!.port;
      _addresses = loopbackOnly ? const ['127.0.0.1'] : await _discoverLanAddresses();
      _server!.listen(_handleRequest, onError: (_) {});
      _txSub = backend.onLinkTx.listen(_broadcastLinkTx);
      notifyListeners();
      return true;
    } catch (e) {
      _lastError = 'No se pudo iniciar el servidor local: $e';
      notifyListeners();
      return false;
    }
  }

  Future<void> stop() async {
    await _txSub?.cancel();
    _txSub = null;
    for (final ws in _linkPeers.toList()) {
      unawaited(ws.close());
    }
    _linkPeers.clear();
    if (_server != null) {
      await _server!.close(force: true);
      _server = null;
    }
    _boundPort = 0;
    notifyListeners();
  }

  Future<List<String>> _discoverLanAddresses() async {
    final out = <String>[];
    try {
      final ifaces = await NetworkInterface.list(
        includeLoopback: false,
        type: InternetAddressType.IPv4,
      );
      for (final iface in ifaces) {
        for (final addr in iface.addresses) {
          if (!addr.isLoopback && !out.contains(addr.address)) {
            out.add(addr.address);
          }
        }
      }
    } catch (_) {}
    if (!out.contains('127.0.0.1')) {
      out.add('127.0.0.1');
    }
    return out;
  }

  void _broadcastLinkTx(Uint8List bytes) {
    if (bytes.isEmpty || _linkPeers.isEmpty) return;
    _linkTxBytes += bytes.length;
    for (final ws in _linkPeers.toList()) {
      if (ws.readyState == WebSocket.open) {
        ws.add(bytes);
      }
    }
    notifyListeners();
  }

  static bool _isTrustedLocalOrigin(String origin) {
    final uri = Uri.tryParse(origin);
    if (uri == null) return false;
    final host = uri.host.toLowerCase();
    if (host == 'localhost' || host == '127.0.0.1' || host == '::1') return true;
    if (host.startsWith('192.168.') || host.startsWith('10.')) return true;
    if (host.startsWith('172.')) {
      final parts = host.split('.');
      if (parts.length == 4) {
        final second = int.tryParse(parts[1]) ?? -1;
        if (second >= 16 && second <= 31) return true;
      }
    }
    return false;
  }

  void _applyCors(HttpRequest req, HttpResponse res) {
    final origin = req.headers.value('origin');
    if (origin != null && _isTrustedLocalOrigin(origin)) {
      res.headers.set('Access-Control-Allow-Origin', origin);
      res.headers.set('Vary', 'Origin');
    }
    res.headers.set('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS');
    res.headers.set('Access-Control-Allow-Headers', 'Content-Type, X-CG50-Token');
    res.headers.set('Cache-Control', 'no-store');
  }

  bool _isAuthorized(HttpRequest req) {
    final expected = optionalPairingToken;
    if (expected == null || expected.isEmpty) return true;
    final fromHeader = req.headers.value('x-cg50-token');
    final fromQuery = req.uri.queryParameters['token'];
    return fromHeader == expected || fromQuery == expected;
  }

  static bool _isMutatingMethod(String method) =>
      method == 'POST' || method == 'PUT' || method == 'DELETE';

  Future<void> _sendJson(HttpResponse res, int status, Object body) async {
    res.statusCode = status;
    res.headers.contentType = ContentType.json;
    res.write(jsonEncode(body));
    await res.close();
  }

  Future<Uint8List> _readRequestBody(HttpRequest req, {int maxBytes = 16 * 1024 * 1024}) async {
    final bb = BytesBuilder(copy: false);
    int total = 0;
    await for (final chunk in req) {
      total += chunk.length;
      if (total > maxBytes) {
        throw const HttpException('Archivo excede el limite de 16 MB');
      }
      bb.add(chunk);
    }
    return bb.takeBytes();
  }

  Future<void> _handleRequest(HttpRequest req) async {
    final res = req.response;
    _applyCors(req, res);

    if (req.method == 'OPTIONS') {
      res.statusCode = HttpStatus.noContent;
      await res.close();
      return;
    }

    final uri = req.uri;
    final path = uri.path;

    // Enforce optional pairing token on API and WebSocket endpoints (H-006)
    if ((path.startsWith('/api/') || path == '/ws/link') && !_isAuthorized(req)) {
      await _sendJson(res, HttpStatus.unauthorized, {
        'ok': false,
        'error': 'Token de emparejamiento requerido o invalido',
      });
      return;
    }

    // Enforce optional read-only protection on mutating endpoints (H-006)
    if (readOnlyMode && path.startsWith('/api/') && _isMutatingMethod(req.method)) {
      await _sendJson(res, HttpStatus.forbidden, {
        'ok': false,
        'error': 'Servidor local en modo solo lectura',
      });
      return;
    }

    try {
      // WebSocket Calc <-> Calc SCIF0 Link tunnel
      if (path == '/ws/link') {
        if (WebSocketTransformer.isUpgradeRequest(req)) {
          final ws = await WebSocketTransformer.upgrade(req);
          _linkPeers.add(ws);
          await backend.setLinkEnabled(true);
          notifyListeners();
          ws.listen(
            (dynamic msg) async {
              Uint8List? bytes;
              if (msg is Uint8List) {
                bytes = msg;
              } else if (msg is List<int>) {
                bytes = Uint8List.fromList(msg);
              } else if (msg is String) {
                bytes = Uint8List.fromList(utf8.encode(msg));
              }
              if (bytes != null && bytes.isNotEmpty) {
                _linkRxBytes += bytes.length;
                await backend.linkPushRx(bytes);
                notifyListeners();
              }
            },
            onDone: () async {
              _linkPeers.remove(ws);
              if (_linkPeers.isEmpty) {
                await backend.setLinkEnabled(false);
              }
              notifyListeners();
            },
            onError: (_) {
              _linkPeers.remove(ws);
              notifyListeners();
            },
          );
          return;
        }
        await _sendJson(res, HttpStatus.badRequest, {'error': 'Se requiere WebSocket'});
        return;
      }

      if (path == '/' || path == '/index.html') {
        res.statusCode = HttpStatus.ok;
        res.headers.contentType = ContentType.html;
        res.write(_webUiHtml);
        await res.close();
        return;
      }

      if (path == '/api/info' && req.method == 'GET') {
        final usb = await backend.getUsbStatus();
        final listing = await backend.listSmem('/');
        await _sendJson(res, HttpStatus.ok, {
          'device': 'Casio fx-CG50',
          'port': _boundPort,
          'addresses': _addresses,
          'usb': usb?.toJson(),
          'storage': listing == null
              ? null
              : {
                  'volumeLabel': listing.volumeLabel,
                  'totalBytes': listing.totalBytes,
                  'freeBytes': listing.freeBytes,
                  'usedBytes': listing.usedBytes,
                  'fatEqual': listing.fatEqual,
                },
          'linkPeers': _linkPeers.length,
          'linkTxBytes': _linkTxBytes,
          'linkRxBytes': _linkRxBytes,
        });
        return;
      }

      if (path == '/api/files' && req.method == 'GET') {
        final dir = uri.queryParameters['path'] ?? '/';
        final listing = await backend.listSmem(dir);
        if (listing == null) {
          await _sendJson(res, HttpStatus.notFound, {'error': 'Directorio no encontrado'});
          return;
        }
        await _sendJson(res, HttpStatus.ok, listing.toJson());
        return;
      }

      if (path == '/api/file') {
        final remotePath = uri.queryParameters['path'] ?? '';
        if (remotePath.isEmpty) {
          await _sendJson(res, HttpStatus.badRequest, {'error': 'Falta parametro path'});
          return;
        }
        if (req.method == 'GET') {
          final data = await backend.readSmemFile(remotePath);
          if (data == null) {
            await _sendJson(res, HttpStatus.notFound, {'error': 'Archivo no encontrado'});
            return;
          }
          final fileName = remotePath.split('/').where((s) => s.isNotEmpty).last;
          res.statusCode = HttpStatus.ok;
          res.headers.contentType = ContentType.binary;
          res.headers.set(
            'Content-Disposition',
            'attachment; filename="${fileName.replaceAll('"', '_')}"',
          );
          res.add(data);
          await res.close();
          return;
        }
        if (req.method == 'POST' || req.method == 'PUT') {
          final body = await _readRequestBody(req);
          final overwrite = (uri.queryParameters['overwrite'] ?? '1') != '0';
          final remount = (uri.queryParameters['remount'] ?? '0') == '1';
          final ok = await backend.writeSmemFile(
            remotePath,
            body,
            overwrite: overwrite,
            remountOs: remount,
          );
          if (!ok) {
            await _sendJson(res, HttpStatus.badRequest,
                {'ok': false, 'error': 'No se pudo escribir en SMEM'});
            return;
          }
          await _sendJson(res, HttpStatus.ok, {
            'ok': true,
            'path': remotePath,
            'bytes': body.length,
            'remounted': remount,
          });
          return;
        }
        if (req.method == 'DELETE') {
          final recursive = (uri.queryParameters['recursive'] ?? '0') == '1';
          final confirmed = (uri.queryParameters['confirm'] ?? '0') == '1';
          if (recursive && requireConfirmForRecursiveDelete && !confirmed) {
            await _sendJson(res, HttpStatus.badRequest, {
              'ok': false,
              'error': 'Confirmacion explicita (confirm=1) requerida para eliminacion recursiva',
            });
            return;
          }
          final remount = (uri.queryParameters['remount'] ?? '0') == '1';
          final ok = await backend.deleteSmemPath(
            remotePath,
            recursive: recursive,
            remountOs: remount,
          );
          if (!ok) {
            await _sendJson(res, HttpStatus.badRequest,
                {'ok': false, 'error': 'No se pudo eliminar la ruta'});
            return;
          }
          await _sendJson(res, HttpStatus.ok, {'ok': true, 'path': remotePath});
          return;
        }
      }

      if (path == '/api/mkdir' && req.method == 'POST') {
        final remotePath = uri.queryParameters['path'] ?? '';
        if (remotePath.isEmpty) {
          await _sendJson(res, HttpStatus.badRequest, {'error': 'Falta parametro path'});
          return;
        }
        final ok = await backend.mkdirSmem(remotePath);
        await _sendJson(
          res,
          ok ? HttpStatus.ok : HttpStatus.badRequest,
          {'ok': ok, 'path': remotePath},
        );
        return;
      }

      if (path == '/api/remount' && req.method == 'POST') {
        final ok = await backend.remountSmemOs();
        await _sendJson(res, ok ? HttpStatus.ok : HttpStatus.internalServerError, {'ok': ok});
        return;
      }

      if (path == '/api/usb') {
        if (req.method == 'POST') {
          final conn = (uri.queryParameters['connected'] ?? '1') == '1';
          await backend.setUsbConnected(conn);
        }
        final st = await backend.getUsbStatus();
        await _sendJson(res, HttpStatus.ok, st?.toJson() ?? {'connected': false});
        return;
      }

      if (path == '/api/qr' && req.method == 'GET') {
        final qr = await backend.decodeLcdQr();
        await _sendJson(res, HttpStatus.ok, {'qr': qr});
        return;
      }

      if (path == '/api/screen.png' && req.method == 'GET') {
        final rgba = backend.lastRgbaBytes;
        if (rgba == null || backend.frameWidth <= 0 || backend.frameHeight <= 0) {
          res.statusCode = HttpStatus.serviceUnavailable;
          await res.close();
          return;
        }
        final png = encodeRgbaToPng(rgba, backend.frameWidth, backend.frameHeight);
        res.statusCode = HttpStatus.ok;
        res.headers.contentType = ContentType('image', 'png');
        res.add(png);
        await res.close();
        return;
      }

      await _sendJson(res, HttpStatus.notFound, {'error': 'Ruta no encontrada'});
    } catch (e) {
      try {
        await _sendJson(res, HttpStatus.internalServerError, {'error': e.toString()});
      } catch (_) {}
    }
  }

  @override
  void dispose() {
    unawaited(stop());
    super.dispose();
  }
}

// ============================================================================
// Calc <-> Calc Link WebSocket Client (connects local SCIF0 to remote CG50)
// ============================================================================
class Cg50LinkPeerClient extends ChangeNotifier {
  final Cg50BridgeBackend backend;
  WebSocket? _ws;
  StreamSubscription<Uint8List>? _txSub;
  String? _peerUrl;
  String? _lastError;
  int _txBytes = 0;
  int _rxBytes = 0;

  Cg50LinkPeerClient(this.backend);

  bool get isConnected => _ws != null && _ws!.readyState == WebSocket.open;
  String? get peerUrl => _peerUrl;
  String? get lastError => _lastError;
  int get txBytes => _txBytes;
  int get rxBytes => _rxBytes;

  Future<bool> connect(String hostAndPort) async {
    await disconnect();
    _lastError = null;
    final clean = hostAndPort.trim().replaceFirst(RegExp(r'^https?://'), '').replaceFirst(RegExp(r'^wss?://'), '');
    final url = clean.contains(':') ? 'ws://$clean/ws/link' : 'ws://$clean:8750/ws/link';
    try {
      final ws = await WebSocket.connect(url).timeout(const Duration(seconds: 5));
      _ws = ws;
      _peerUrl = url;
      await backend.setLinkEnabled(true);
      _txSub = backend.onLinkTx.listen((bytes) {
        if (_ws != null && _ws!.readyState == WebSocket.open && bytes.isNotEmpty) {
          _txBytes += bytes.length;
          _ws!.add(bytes);
          notifyListeners();
        }
      });
      ws.listen(
        (dynamic msg) async {
          Uint8List? bytes;
          if (msg is Uint8List) {
            bytes = msg;
          } else if (msg is List<int>) {
            bytes = Uint8List.fromList(msg);
          }
          if (bytes != null && bytes.isNotEmpty) {
            _rxBytes += bytes.length;
            await backend.linkPushRx(bytes);
            notifyListeners();
          }
        },
        onDone: () {
          unawaited(disconnect());
        },
        onError: (_) {
          unawaited(disconnect());
        },
      );
      notifyListeners();
      return true;
    } catch (e) {
      _lastError = 'No se pudo conectar con $url';
      notifyListeners();
      return false;
    }
  }

  Future<void> disconnect() async {
    await _txSub?.cancel();
    _txSub = null;
    final ws = _ws;
    _ws = null;
    _peerUrl = null;
    if (ws != null) {
      try {
        await ws.close();
      } catch (_) {}
    }
    await backend.setLinkEnabled(false);
    notifyListeners();
  }

  @override
  void dispose() {
    unawaited(disconnect());
    super.dispose();
  }
}

// ============================================================================
// Minimalist guías de interfaz Porcelana Claro Web UI (zero emojis, SVG icons only,
// zero PIN/password, direct drag-and-drop SMEM + live ScreenRecv).
// ============================================================================
const String _webUiHtml = r'''<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>fx-CG50 · Archivos y pantalla</title>
<style>
  :root {
    --bg: #F5F5F7;
    --surface: #FFFFFF;
    --line: rgba(0,0,0,0.08);
    --ink: #1D1D1F;
    --muted: #6E6E73;
    --accent: #0071E3;
    --danger: #D70015;
    --radius: 14px;
  }
  * { box-sizing: border-box; }
  body {
    margin: 0;
    font-family: system-ui, BlinkMacSystemFont, "SF Pro Text", "Segoe UI", Roboto, sans-serif;
    background: var(--bg);
    color: var(--ink);
    -webkit-font-smoothing: antialiased;
  }
  header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 16px 24px;
    background: rgba(255,255,255,0.82);
    backdrop-filter: blur(18px);
    border-bottom: 1px solid var(--line);
    position: sticky;
    top: 0;
    z-index: 10;
  }
  header h1 { font-size: 17px; font-weight: 600; margin: 0; letter-spacing: -0.01em; }
  .storage { font-size: 13px; color: var(--muted); }
  main {
    max-width: 1040px;
    margin: 24px auto;
    padding: 0 20px 40px;
    display: grid;
    grid-template-columns: 360px 1fr;
    gap: 20px;
  }
  @media (max-width: 820px) { main { grid-template-columns: 1fr; } }
  .panel {
    background: var(--surface);
    border: 1px solid var(--line);
    border-radius: var(--radius);
    padding: 18px;
  }
  .section-title {
    font-size: 13px;
    font-weight: 600;
    color: var(--muted);
    margin: 0 0 12px;
    display: flex;
    align-items: center;
    justify-content: space-between;
  }
  .lcd-wrap {
    background: #1C1C1E;
    border-radius: 10px;
    padding: 10px;
    display: flex;
    justify-content: center;
  }
  #lcd {
    width: 100%;
    max-width: 384px;
    aspect-ratio: 384 / 216;
    image-rendering: pixelated;
    border-radius: 4px;
    background: #FFFFFF;
  }
  .row-actions {
    display: flex;
    gap: 8px;
    margin-top: 12px;
    flex-wrap: wrap;
  }
  button {
    appearance: none;
    border: 1px solid var(--line);
    background: var(--surface);
    color: var(--ink);
    font-size: 13px;
    font-weight: 500;
    padding: 7px 12px;
    border-radius: 8px;
    cursor: pointer;
    display: inline-flex;
    align-items: center;
    gap: 6px;
  }
  button:hover { background: #F0F0F2; }
  button.primary { background: var(--accent); color: #FFF; border-color: transparent; }
  button.primary:hover { background: #0066CC; }
  button.danger { color: var(--danger); }
  .toolbar {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 10px;
    margin-bottom: 12px;
    flex-wrap: wrap;
  }
  .crumbs { font-size: 14px; font-weight: 600; display: flex; align-items: center; gap: 4px; }
  .crumbs a { color: var(--accent); text-decoration: none; cursor: pointer; }
  .dropzone {
    border: 1.5px dashed rgba(0,113,227,0.35);
    border-radius: 10px;
    padding: 14px;
    text-align: center;
    font-size: 13px;
    color: var(--muted);
    margin-bottom: 14px;
    transition: background 0.15s ease;
  }
  .dropzone.over { background: rgba(0,113,227,0.06); border-color: var(--accent); color: var(--ink); }
  table { width: 100%; border-collapse: collapse; font-size: 13.5px; }
  th { text-align: left; font-size: 12px; color: var(--muted); font-weight: 500; padding: 6px 8px; border-bottom: 1px solid var(--line); }
  td { padding: 9px 8px; border-bottom: 1px solid var(--line); vertical-align: middle; }
  tr:last-child td { border-bottom: none; }
  .name-cell { display: flex; align-items: center; gap: 8px; font-weight: 500; cursor: pointer; }
  .name-cell:hover { color: var(--accent); }
  .muted { color: var(--muted); font-size: 12.5px; }
  .status-line { margin-top: 10px; font-size: 12.5px; color: var(--muted); min-height: 18px; }
</style>
</head>
<body>
<header>
  <h1>Casio fx-CG50</h1>
  <div class="storage" id="storageInfo">Cargando memoria...</div>
</header>
<main>
  <section class="panel">
    <div class="section-title">
      <span>Pantalla en vivo</span>
      <label style="font-weight:400;display:flex;align-items:center;gap:6px;cursor:pointer;">
        <input type="checkbox" id="liveToggle" checked> Auto
      </label>
    </div>
    <div class="lcd-wrap">
      <img id="lcd" alt="Pantalla fx-CG50" src="/api/screen.png">
    </div>
    <div class="row-actions">
      <button onclick="downloadScreen()">Guardar PNG</button>
      <button onclick="remountOs()">Aplicar en calculadora</button>
    </div>
    <div class="status-line" id="screenStatus"></div>
  </section>

  <section class="panel">
    <div class="toolbar">
      <div class="crumbs" id="crumbs">/</div>
      <div style="display:flex;gap:8px;">
        <input type="file" id="fileInput" multiple style="display:none" onchange="uploadFiles(this.files)">
        <button class="primary" onclick="document.getElementById('fileInput').click()">Subir archivo</button>
        <button onclick="createFolder()">Nueva carpeta</button>
        <button onclick="loadDir(currentDir)">Actualizar</button>
      </div>
    </div>
    <div class="dropzone" id="dropzone">Arrastra archivos aqui (.py, .g3a, .g3m, .g3e, .csv, .bmp)</div>
    <table>
      <thead>
        <tr><th>Nombre</th><th style="width:92px;">Tamano</th><th style="width:140px;text-align:right;">Acciones</th></tr>
      </thead>
      <tbody id="fileRows"></tbody>
    </table>
    <div class="status-line" id="fileStatus"></div>
  </section>
</main>
<script>
let currentDir = '/';
const fmtBytes = (n) => {
  if (n < 1024) return n + ' B';
  if (n < 1048576) return (n / 1024).toFixed(1) + ' KB';
  return (n / 1048576).toFixed(2) + ' MB';
};
function setStatus(msg) { document.getElementById('fileStatus').textContent = msg; }
function renderCrumbs() {
  const el = document.getElementById('crumbs');
  const parts = currentDir.split('/').filter(Boolean);
  let html = '<a onclick="loadDir(\'/\')">SMEM</a>';
  let acc = '';
  for (const p of parts) {
    acc += '/' + p;
    const target = acc;
    html += ' / <a onclick="loadDir(' + JSON.stringify(target) + ')">' + p + '</a>';
  }
  el.innerHTML = html;
}
async function loadDir(path) {
  currentDir = path || '/';
  renderCrumbs();
  const r = await fetch('/api/files?path=' + encodeURIComponent(currentDir));
  if (!r.ok) { setStatus('No se pudo leer la carpeta'); return; }
  const data = await r.json();
  document.getElementById('storageInfo').textContent =
    'Libre: ' + fmtBytes(data.freeBytes) + ' de ' + fmtBytes(data.totalBytes);
  const tbody = document.getElementById('fileRows');
  tbody.innerHTML = '';
  if (currentDir !== '/') {
    const parent = currentDir.split('/').slice(0, -1).join('/') || '/';
    const tr = document.createElement('tr');
    tr.innerHTML = '<td colspan="3"><div class="name-cell" onclick="loadDir(' + JSON.stringify(parent) + ')">.. (Subir nivel)</div></td>';
    tbody.appendChild(tr);
  }
  for (const e of data.entries) {
    const tr = document.createElement('tr');
    const icon = e.isDir
      ? '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="#0071E3" stroke-width="2"><path d="M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/></svg>'
      : '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="#6E6E73" stroke-width="2"><path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"/><path d="M14 2v6h6"/></svg>';
    const clickAction = e.isDir
      ? 'loadDir(' + JSON.stringify(e.path) + ')'
      : 'downloadFile(' + JSON.stringify(e.path) + ')';
    const actions = e.isDir
      ? '<button class="danger" onclick="deleteItem(' + JSON.stringify(e.path) + ', true)">Eliminar</button>'
      : '<button onclick="downloadFile(' + JSON.stringify(e.path) + ')">Descargar</button> ' +
        '<button class="danger" onclick="deleteItem(' + JSON.stringify(e.path) + ', false)">Eliminar</button>';
    tr.innerHTML =
      '<td><div class="name-cell" onclick=' + JSON.stringify(clickAction) + '>' + icon + '<span>' + e.name + '</span></div></td>' +
      '<td class="muted">' + (e.isDir ? 'Carpeta' : fmtBytes(e.size)) + '</td>' +
      '<td style="text-align:right;white-space:nowrap;">' + actions + '</td>';
    tbody.appendChild(tr);
  }
}
function downloadFile(p) {
  window.location.href = '/api/file?path=' + encodeURIComponent(p);
}
async function uploadFiles(fileList) {
  if (!fileList || !fileList.length) return;
  for (const f of fileList) {
    const dest = (currentDir === '/' ? '/' : currentDir + '/') + f.name;
    setStatus('Subiendo ' + f.name + '...');
    const buf = await f.arrayBuffer();
    const r = await fetch('/api/file?path=' + encodeURIComponent(dest) + '&overwrite=1', {
      method: 'POST',
      body: buf
    });
    if (!r.ok) { setStatus('Error al subir ' + f.name); return; }
  }
  await fetch('/api/remount', { method: 'POST' });
  setStatus('Transferencia lista y aplicada en la calculadora.');
  await loadDir(currentDir);
}
async function deleteItem(p, isDir) {
  if (!confirm('Eliminar ' + p + '?')) return;
  const r = await fetch('/api/file?path=' + encodeURIComponent(p) + (isDir ? '&recursive=1' : '') + '&remount=1', { method: 'DELETE' });
  setStatus(r.ok ? 'Eliminado: ' + p : 'No se pudo eliminar');
  await loadDir(currentDir);
}
async function createFolder() {
  const name = prompt('Nombre de la carpeta:');
  if (!name || !name.trim()) return;
  const dest = (currentDir === '/' ? '/' : currentDir + '/') + name.trim();
  const r = await fetch('/api/mkdir?path=' + encodeURIComponent(dest), { method: 'POST' });
  setStatus(r.ok ? 'Carpeta creada' : 'No se pudo crear la carpeta');
  await loadDir(currentDir);
}
async function remountOs() {
  const r = await fetch('/api/remount', { method: 'POST' });
  document.getElementById('screenStatus').textContent = r.ok ? 'Memoria sincronizada con Casio OS.' : 'Error al recargar.';
}
function downloadScreen() {
  const a = document.createElement('a');
  a.href = '/api/screen.png?t=' + Date.now();
  a.download = 'cg50_captura.png';
  a.click();
}
const dz = document.getElementById('dropzone');
dz.addEventListener('dragover', (e) => { e.preventDefault(); dz.classList.add('over'); });
dz.addEventListener('dragleave', () => dz.classList.remove('over'));
dz.addEventListener('drop', (e) => {
  e.preventDefault();
  dz.classList.remove('over');
  uploadFiles(e.dataTransfer.files);
});
setInterval(() => {
  if (document.getElementById('liveToggle').checked) {
    document.getElementById('lcd').src = '/api/screen.png?t=' + Date.now();
  }
}, 600);
loadDir('/');
</script>
</body>
</html>
''';
