import 'dart:async';
import 'dart:io';
import 'dart:typed_data';

import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';
import 'package:path_provider/path_provider.dart';

import '../connectivity/cg50_local_bridge_server.dart';
import '../native/casio_ffi_bindings.dart';
import '../theme/tema_interfaz.dart';

// Nearest-neighbor scaler + optional Casio dark outer bezel frame for
// Manager PLUS-parity screen captures (x1/x2/x3, PNG/BMP, with/without frame).
({Uint8List rgba, int width, int height}) composeScaledCapture({
  required Uint8List srcRgba,
  required int srcW,
  required int srcH,
  required int scale,
  required bool includeFrame,
}) {
  final s = scale.clamp(1, 3);
  final innerW = srcW * s;
  final innerH = srcH * s;
  final pad = includeFrame ? (10 * s) : 0;
  final outW = innerW + pad * 2;
  final outH = innerH + pad * 2;
  final out = Uint8List(outW * outH * 4);

  if (includeFrame) {
    // Matte charcoal frame (#1D1D1F) with 1px inner hairline (#3A3A3C)
    for (int y = 0; y < outH; y++) {
      for (int x = 0; x < outW; x++) {
        final isBorder = x == pad - 1 ||
            x == outW - pad ||
            y == pad - 1 ||
            y == outH - pad;
        final off = (y * outW + x) * 4;
        out[off] = isBorder ? 0x48 : 0x1D;
        out[off + 1] = isBorder ? 0x48 : 0x1D;
        out[off + 2] = isBorder ? 0x4A : 0x1F;
        out[off + 3] = 0xFF;
      }
    }
  }

  for (int y = 0; y < innerH; y++) {
    final sy = y ~/ s;
    final dstRow = (y + pad) * outW;
    final srcRow = sy * srcW;
    for (int x = 0; x < innerW; x++) {
      final sx = x ~/ s;
      final sOff = (srcRow + sx) * 4;
      final dOff = (dstRow + x + pad) * 4;
      out[dOff] = srcRgba[sOff];
      out[dOff + 1] = srcRgba[sOff + 1];
      out[dOff + 2] = srcRgba[sOff + 2];
      out[dOff + 3] = srcRgba[sOff + 3];
    }
  }
  return (rgba: out, width: outW, height: outH);
}

class Cg50CaptureSheet extends StatefulWidget {
  final CasioEngineController engine;
  final TokensInterfaz tokens;
  final Future<bool> Function({
    required String suggestedName,
    required String mimeType,
    required String sourcePath,
  }) onSaveFile;
  final Future<bool> Function({
    required String sourcePath,
    required String fileName,
  }) onCopyImageToClipboard;
  final void Function(String message) onNotify;

  const Cg50CaptureSheet({
    super.key,
    required this.engine,
    required this.tokens,
    required this.onSaveFile,
    required this.onCopyImageToClipboard,
    required this.onNotify,
  });

  @override
  State<Cg50CaptureSheet> createState() => _Cg50CaptureSheetState();
}

class _Cg50CaptureSheetState extends State<Cg50CaptureSheet> {
  int _scale = 2; // 1, 2, 3
  String _format = 'png'; // 'png' | 'bmp'
  bool _includeFrame = true;
  bool _busy = false;

  Future<({File file, String name, String mime})?> _buildTempImage() async {
    final rgba = widget.engine.lastRgbaBytes;
    final w = widget.engine.frameWidth;
    final h = widget.engine.frameHeight;
    if (rgba == null || w <= 0 || h <= 0) return null;

    final composed = composeScaledCapture(
      srcRgba: rgba,
      srcW: w,
      srcH: h,
      scale: _scale,
      includeFrame: _includeFrame,
    );
    final bytes = _format == 'bmp'
        ? encodeRgbaToBmp(composed.rgba, composed.width, composed.height)
        : encodeRgbaToPng(composed.rgba, composed.width, composed.height);
    final now = DateTime.now();
    final tag =
        '${now.year}${now.month.toString().padLeft(2, '0')}${now.day.toString().padLeft(2, '0')}_'
        '${now.hour.toString().padLeft(2, '0')}${now.minute.toString().padLeft(2, '0')}${now.second.toString().padLeft(2, '0')}';
    final name = 'captura_${widget.engine.kind.name}_${_scale}x_$tag.$_format';
    final tmpDir = await getTemporaryDirectory();
    final file = File('${tmpDir.path}/$name');
    await file.writeAsBytes(bytes, flush: true);
    return (
      file: file,
      name: name,
      mime: _format == 'bmp' ? 'image/bmp' : 'image/png',
    );
  }

  Future<void> _copyToClipboard() async {
    if (_busy) return;
    setState(() => _busy = true);
    try {
      final res = await _buildTempImage();
      if (res == null) {
        widget.onNotify('Sin fotograma disponible');
        return;
      }
      try {
        final ok = await widget.onCopyImageToClipboard(
          sourcePath: res.file.path,
          fileName: res.name,
        );
        if (mounted) Navigator.of(context).pop();
        widget.onNotify(ok
            ? 'Captura copiada al portapapeles'
            : 'No se pudo copiar la imagen');
      } finally {
        if (await res.file.exists()) await res.file.delete();
      }
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  Future<void> _saveImageFile() async {
    if (_busy) return;
    setState(() => _busy = true);
    try {
      final res = await _buildTempImage();
      if (res == null) {
        widget.onNotify('Sin fotograma disponible');
        return;
      }
      try {
        final ok = await widget.onSaveFile(
          suggestedName: res.name,
          mimeType: res.mime,
          sourcePath: res.file.path,
        );
        if (mounted && ok) {
          Navigator.of(context).pop();
          widget.onNotify('Captura guardada: ${res.name}');
        }
      } finally {
        if (await res.file.exists()) await res.file.delete();
      }
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    final t = widget.tokens;
    final img = widget.engine.lcdImage;
    final outW = widget.engine.frameWidth * _scale + (_includeFrame ? 20 * _scale : 0);
    final outH = widget.engine.frameHeight * _scale + (_includeFrame ? 20 * _scale : 0);

    return Container(
      decoration: BoxDecoration(
        color: t.surface,
        borderRadius: const BorderRadius.vertical(top: Radius.circular(20)),
        border: Border.all(color: t.border, width: t.borderWidth),
      ),
      padding: const EdgeInsets.fromLTRB(18, 12, 18, 20),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Center(
            child: Container(
              width: 36,
              height: 4,
              decoration: BoxDecoration(
                color: t.border,
                borderRadius: BorderRadius.circular(2),
              ),
            ),
          ),
          const SizedBox(height: 12),
          Row(
            children: [
              Text(
                'Capturar pantalla',
                style: TextStyle(
                  color: t.textPrimary,
                  fontSize: 17,
                  fontWeight: FontWeight.w600,
                ),
              ),
              const Spacer(),
              Text(
                '$outW×$outH px',
                style: TextStyle(color: t.textSecondary, fontSize: 12.5),
              ),
            ],
          ),
          const SizedBox(height: 14),
          Center(
            child: Container(
              padding: EdgeInsets.all(_includeFrame ? 8 : 0),
              decoration: BoxDecoration(
                color: _includeFrame ? const Color(0xFF1D1D1F) : Colors.transparent,
                borderRadius: BorderRadius.circular(_includeFrame ? 10 : 4),
                border: Border.all(color: t.border, width: t.borderWidth),
              ),
              child: SizedBox(
                height: 132,
                child: AspectRatio(
                  aspectRatio: widget.engine.frameWidth > 0 &&
                          widget.engine.frameHeight > 0
                      ? widget.engine.frameWidth / widget.engine.frameHeight
                      : 384 / 216,
                  child: img != null
                      ? RawImage(
                          image: img,
                          fit: BoxFit.fill,
                          filterQuality: FilterQuality.none,
                        )
                      : Container(color: t.canvas),
                ),
              ),
            ),
          ),
          const SizedBox(height: 16),
          Row(
            children: [
              Text('Tamaño',
                  style: TextStyle(color: t.textSecondary, fontSize: 13.5)),
              const Spacer(),
              SelectorSegmentado<int>(
                tokens: t,
                value: _scale,
                segments: const [(1, '×1'), (2, '×2'), (3, '×3')],
                onChanged: (v) => setState(() => _scale = v),
              ),
            ],
          ),
          const SizedBox(height: 10),
          Row(
            children: [
              Text('Formato',
                  style: TextStyle(color: t.textSecondary, fontSize: 13.5)),
              const Spacer(),
              SelectorSegmentado<String>(
                tokens: t,
                value: _format,
                segments: const [('png', 'PNG'), ('bmp', 'BMP')],
                onChanged: (v) => setState(() => _format = v),
              ),
            ],
          ),
          const SizedBox(height: 6),
          SwitchListTile.adaptive(
            dense: true,
            contentPadding: EdgeInsets.zero,
            activeTrackColor: t.accent,
            title: Text('Incluir cuadro exterior',
                style: TextStyle(color: t.textPrimary, fontSize: 14)),
            value: _includeFrame,
            onChanged: (v) => setState(() => _includeFrame = v),
          ),
          const SizedBox(height: 12),
          Row(
            children: [
              Expanded(
                child: FilledButton.icon(
                  onPressed: _busy ? null : _copyToClipboard,
                  style: FilledButton.styleFrom(
                    backgroundColor: t.accent,
                    foregroundColor: Colors.white,
                    shape: RoundedRectangleBorder(
                        borderRadius: BorderRadius.circular(10)),
                    padding: const EdgeInsets.symmetric(vertical: 11),
                  ),
                  icon: const Icon(CupertinoIcons.doc_on_clipboard, size: 17),
                  label: const Text('Copiar'),
                ),
              ),
              const SizedBox(width: 10),
              Expanded(
                child: OutlinedButton.icon(
                  onPressed: _busy ? null : _saveImageFile,
                  style: OutlinedButton.styleFrom(
                    foregroundColor: t.textPrimary,
                    side: BorderSide(color: t.border, width: t.borderWidth),
                    shape: RoundedRectangleBorder(
                        borderRadius: BorderRadius.circular(10)),
                    padding: const EdgeInsets.symmetric(vertical: 11),
                  ),
                  icon: const Icon(CupertinoIcons.square_arrow_down, size: 17),
                  label: const Text('Guardar'),
                ),
              ),
            ],
          ),
        ],
      ),
    );
  }
}

class Cg50EnlargedLcdSheet extends StatefulWidget {
  final CasioEngineController engine;
  final TokensInterfaz tokens;
  final VoidCallback onOpenCapture;
  final Future<void> Function()? onScanQr;

  const Cg50EnlargedLcdSheet({
    super.key,
    required this.engine,
    required this.tokens,
    required this.onOpenCapture,
    this.onScanQr,
  });

  @override
  State<Cg50EnlargedLcdSheet> createState() => _Cg50EnlargedLcdSheetState();
}

class _Cg50EnlargedLcdSheetState extends State<Cg50EnlargedLcdSheet> {
  int _zoom = 2; // 1, 2, 3

  @override
  Widget build(BuildContext context) {
    final t = widget.tokens;
    return AnimatedBuilder(
      animation: widget.engine,
      builder: (context, _) {
        final img = widget.engine.lcdImage;
        final aspect = widget.engine.frameWidth > 0 &&
                widget.engine.frameHeight > 0
            ? widget.engine.frameWidth / widget.engine.frameHeight
            : 384 / 216;
        final maxW = MediaQuery.of(context).size.width - 36;
        final desiredW = (_zoom == 1)
            ? (maxW * 0.65)
            : (_zoom == 2 ? (maxW * 0.88) : maxW);

        return Container(
          decoration: BoxDecoration(
            color: t.surface,
            borderRadius: const BorderRadius.vertical(top: Radius.circular(20)),
            border: Border.all(color: t.border, width: t.borderWidth),
          ),
          padding: const EdgeInsets.fromLTRB(18, 12, 18, 22),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              Center(
                child: Container(
                  width: 36,
                  height: 4,
                  decoration: BoxDecoration(
                    color: t.border,
                    borderRadius: BorderRadius.circular(2),
                  ),
                ),
              ),
              const SizedBox(height: 12),
              Row(
                children: [
                  Text(
                    'Pantalla ampliada',
                    style: TextStyle(
                      color: t.textPrimary,
                      fontSize: 17,
                      fontWeight: FontWeight.w600,
                    ),
                  ),
                  const Spacer(),
                  SelectorSegmentado<int>(
                    tokens: t,
                    value: _zoom,
                    segments: const [(1, '×1'), (2, '×2'), (3, '×3')],
                    onChanged: (v) => setState(() => _zoom = v),
                  ),
                ],
              ),
              const SizedBox(height: 14),
              Center(
                child: GestureDetector(
                  onTap: widget.onScanQr == null
                      ? null
                      : () => unawaited(widget.onScanQr!()),
                  child: Container(
                    width: desiredW,
                    padding: const EdgeInsets.all(10),
                    decoration: BoxDecoration(
                      color: const Color(0xFF1D1D1F),
                      borderRadius: BorderRadius.circular(12),
                    ),
                    child: AspectRatio(
                      aspectRatio: aspect,
                      child: img != null
                          ? RawImage(
                              image: img,
                              fit: BoxFit.fill,
                              filterQuality: FilterQuality.none,
                            )
                          : Container(color: Colors.white),
                    ),
                  ),
                ),
              ),
              const SizedBox(height: 14),
              Row(
                mainAxisAlignment: MainAxisAlignment.end,
                children: [
                  if (widget.onScanQr != null) ...[
                    OutlinedButton.icon(
                      onPressed: () => unawaited(widget.onScanQr!()),
                      style: OutlinedButton.styleFrom(
                        foregroundColor: t.textPrimary,
                        side: BorderSide(color: t.border, width: t.borderWidth),
                        shape: RoundedRectangleBorder(
                            borderRadius: BorderRadius.circular(10)),
                      ),
                      icon: const Icon(CupertinoIcons.qrcode, size: 17),
                      label: const Text('Leer QR'),
                    ),
                    const SizedBox(width: 8),
                  ],
                  FilledButton.icon(
                    onPressed: () {
                      Navigator.of(context).pop();
                      widget.onOpenCapture();
                    },
                    style: FilledButton.styleFrom(
                      backgroundColor: t.accent,
                      foregroundColor: Colors.white,
                      shape: RoundedRectangleBorder(
                          borderRadius: BorderRadius.circular(10)),
                    ),
                    icon: const Icon(CupertinoIcons.camera, size: 17),
                    label: const Text('Capturar'),
                  ),
                ],
              ),
            ],
          ),
        );
      },
    );
  }
}
