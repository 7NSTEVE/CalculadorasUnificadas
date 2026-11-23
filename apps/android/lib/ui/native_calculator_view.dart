import 'dart:async';
import 'dart:ui' as ui;

import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../native/calc_models.dart';
import '../native/casio_ffi_bindings.dart';
import '../theme/tema_interfaz.dart';

class CapturedHistoryEntry {
  final CasioCalcKind kind;
  final String calcTitle;
  final String keyName;
  final String keyLabel;
  final bool isEvalKey;
  final DateTime timestamp;
  final int indicatorsMask;
  final bool isCw2Indicators;
  final int pixelWidth;
  final int pixelHeight;
  final Uint8List? rgbaBytes;
  final Color lcdBg;
  final Color lcdFg;

  CapturedHistoryEntry({
    required this.kind,
    required this.calcTitle,
    required this.keyName,
    required this.keyLabel,
    required this.isEvalKey,
    required this.timestamp,
    required this.indicatorsMask,
    required this.isCw2Indicators,
    required this.pixelWidth,
    required this.pixelHeight,
    required this.rgbaBytes,
    required this.lcdBg,
    required this.lcdFg,
  });
}

class CasioTopIndicatorStrip extends StatelessWidget {
  final int mask;
  final bool isCw2Layout;
  final Color foreground;
  final Color background;
  final double height;

  const CasioTopIndicatorStrip({
    super.key,
    required this.mask,
    this.isCw2Layout = false,
    required this.foreground,
    required this.background,
    required this.height,
  });

  Widget _badge(String text, bool active, {bool boxed = false}) {
    if (!active) return const SizedBox.shrink();
    final fontSize = (height * 0.68).clamp(6.0, 13.0);
    if (boxed) {
      return Container(
        margin: const EdgeInsets.only(right: 3),
        padding: const EdgeInsets.symmetric(horizontal: 2.5, vertical: 0.5),
        decoration: BoxDecoration(
          color: foreground,
          borderRadius: BorderRadius.circular(1.5),
        ),
        child: Text(
          text,
          style: TextStyle(
            color: background,
            fontSize: fontSize,
            fontWeight: FontWeight.w800,
            height: 1.0,
            fontFamily: 'monospace',
          ),
        ),
      );
    }
    return Padding(
      padding: const EdgeInsets.only(right: 4),
      child: Text(
        text,
        style: TextStyle(
          color: foreground,
          fontSize: fontSize,
          fontWeight: FontWeight.w700,
          height: 1.0,
          fontFamily: 'monospace',
        ),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final List<Widget> left;
    final List<Widget> right;

    if (isCw2Layout) {
      left = [
        _badge('S', (mask & CasioCw2IndicatorBits.shift) != 0, boxed: true),
        _badge('Math', (mask & CasioCw2IndicatorBits.math) != 0),
        _badge('D', (mask & CasioCw2IndicatorBits.deg) != 0, boxed: true),
        _badge('R', (mask & CasioCw2IndicatorBits.rad) != 0, boxed: true),
        _badge('G', (mask & CasioCw2IndicatorBits.gra) != 0, boxed: true),
        _badge('FIX', (mask & CasioCw2IndicatorBits.fix) != 0),
        _badge('SCI', (mask & CasioCw2IndicatorBits.sci) != 0),
        _badge('f(x)', (mask & CasioCw2IndicatorBits.fx) != 0),
        _badge('E', (mask & CasioCw2IndicatorBits.eng) != 0),
        _badge('i', (mask & CasioCw2IndicatorBits.cmplx) != 0),
        _badge('∠', (mask & CasioCw2IndicatorBits.angle) != 0),
        _badge('⇵', (mask & CasioCw2IndicatorBits.wdown) != 0),
        _badge('VER', (mask & CasioCw2IndicatorBits.verify) != 0),
        _badge('g(x)', (mask & CasioCw2IndicatorBits.gx) != 0),
      ];
      right = [
        _badge('◀', (mask & CasioCw2IndicatorBits.left) != 0),
        _badge('▼', (mask & CasioCw2IndicatorBits.down) != 0),
        _badge('▲', (mask & CasioCw2IndicatorBits.up) != 0),
        _badge('▶', (mask & CasioCw2IndicatorBits.right) != 0),
        _badge('II', (mask & CasioCw2IndicatorBits.pause) != 0),
      ];
    } else {
      left = [
        _badge('S', (mask & CasioIndicatorBits.shift) != 0, boxed: true),
        _badge('A', (mask & CasioIndicatorBits.alpha) != 0, boxed: true),
        _badge('M', (mask & CasioIndicatorBits.m) != 0),
        _badge('STO', (mask & CasioIndicatorBits.sto) != 0),
        _badge('D', (mask & CasioIndicatorBits.deg) != 0, boxed: true),
        _badge('R', (mask & CasioIndicatorBits.rad) != 0, boxed: true),
        _badge('G', (mask & CasioIndicatorBits.gra) != 0, boxed: true),
        _badge('FIX', (mask & CasioIndicatorBits.fix) != 0),
        _badge('SCI', (mask & CasioIndicatorBits.sci) != 0),
        _badge('Math', (mask & CasioIndicatorBits.math) != 0),
      ];
      right = [
        _badge('▼', (mask & CasioIndicatorBits.down) != 0),
        _badge('▲', (mask & CasioIndicatorBits.up) != 0),
      ];
    }

    return Container(
      height: height,
      color: background,
      padding: const EdgeInsets.symmetric(horizontal: 4),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.center,
        children: [
          ...left,
          const Spacer(),
          ...right,
        ],
      ),
    );
  }
}

class NativeCalculatorFaceplate extends StatefulWidget {
  final CasioSkinGeometry skin;
  final CasioEngineController engine;
  final TokensInterfaz tokens;
  final void Function(CasioKeyDef key, CasioEngineController engine) onKeyLogged;
  final bool enableHardwareKeyboard;

  const NativeCalculatorFaceplate({
    super.key,
    required this.skin,
    required this.engine,
    required this.tokens,
    required this.onKeyLogged,
    this.enableHardwareKeyboard = true,
  });

  @override
  State<NativeCalculatorFaceplate> createState() =>
      _NativeCalculatorFaceplateState();
}

class _NativeCalculatorFaceplateState extends State<NativeCalculatorFaceplate> {
  final GlobalKey _renderKey = GlobalKey();
  final FocusNode _focusNode = FocusNode();
  final Map<int, CasioKeyDef> _activePointers = {};
  Timer? _postPressCaptureTimer;

  CasioKeyDef? _findKeyByName(List<String> names) {
    for (final n in names) {
      for (final k in widget.skin.keys) {
        if (k.name == n) return k;
      }
    }
    return null;
  }

  CasioKeyDef? _mapLogicalKey(LogicalKeyboardKey key) {
    if (HardwareKeyboard.instance.isControlPressed &&
        key == LogicalKeyboardKey.keyM) {
      return _findKeyByName(['MENU', 'HOME', 'MODE']);
    }
    if (key == LogicalKeyboardKey.f1) return _findKeyByName(['F1']);
    if (key == LogicalKeyboardKey.f2) return _findKeyByName(['F2']);
    if (key == LogicalKeyboardKey.f3) return _findKeyByName(['F3']);
    if (key == LogicalKeyboardKey.f4) return _findKeyByName(['F4']);
    if (key == LogicalKeyboardKey.f5) return _findKeyByName(['F5']);
    if (key == LogicalKeyboardKey.f6) return _findKeyByName(['F6']);
    if (key == LogicalKeyboardKey.digit0 || key == LogicalKeyboardKey.numpad0) {
      return _findKeyByName(['0']);
    }
    if (key == LogicalKeyboardKey.digit1 || key == LogicalKeyboardKey.numpad1) {
      return _findKeyByName(['1']);
    }
    if (key == LogicalKeyboardKey.digit2 || key == LogicalKeyboardKey.numpad2) {
      return _findKeyByName(['2']);
    }
    if (key == LogicalKeyboardKey.digit3 || key == LogicalKeyboardKey.numpad3) {
      return _findKeyByName(['3']);
    }
    if (key == LogicalKeyboardKey.digit4 || key == LogicalKeyboardKey.numpad4) {
      return _findKeyByName(['4']);
    }
    if (key == LogicalKeyboardKey.digit5 || key == LogicalKeyboardKey.numpad5) {
      return _findKeyByName(['5']);
    }
    if (key == LogicalKeyboardKey.digit6 || key == LogicalKeyboardKey.numpad6) {
      return _findKeyByName(['6']);
    }
    if (key == LogicalKeyboardKey.digit7 || key == LogicalKeyboardKey.numpad7) {
      return _findKeyByName(['7']);
    }
    if (key == LogicalKeyboardKey.digit8 || key == LogicalKeyboardKey.numpad8) {
      return _findKeyByName(['8']);
    }
    if (key == LogicalKeyboardKey.digit9 || key == LogicalKeyboardKey.numpad9) {
      return _findKeyByName(['9']);
    }
    if (key == LogicalKeyboardKey.add || key == LogicalKeyboardKey.numpadAdd) {
      return _findKeyByName(['MAS']);
    }
    if (key == LogicalKeyboardKey.minus ||
        key == LogicalKeyboardKey.numpadSubtract) {
      return _findKeyByName(['MENOS']);
    }
    if (key == LogicalKeyboardKey.asterisk ||
        key == LogicalKeyboardKey.numpadMultiply) {
      return _findKeyByName(['POR']);
    }
    if (key == LogicalKeyboardKey.slash ||
        key == LogicalKeyboardKey.numpadDivide) {
      return _findKeyByName(['ENTRE']);
    }
    if (key == LogicalKeyboardKey.period ||
        key == LogicalKeyboardKey.numpadDecimal) {
      return _findKeyByName(['PUNTO', 'COMA']);
    }
    if (key == LogicalKeyboardKey.comma) {
      return _findKeyByName(['COMA']);
    }
    if (key == LogicalKeyboardKey.parenthesisLeft) {
      return _findKeyByName(['PAREN_A', 'PAREN_ABRE']);
    }
    if (key == LogicalKeyboardKey.parenthesisRight) {
      return _findKeyByName(['PAREN_C', 'PAREN_CIERRA']);
    }
    if (key == LogicalKeyboardKey.caret) {
      return _findKeyByName(['POT', 'POTENCIA']);
    }
    if (key == LogicalKeyboardKey.enter ||
        key == LogicalKeyboardKey.numpadEnter ||
        key == LogicalKeyboardKey.equal) {
      return _findKeyByName(['EXE', 'IGUAL', 'OK']);
    }
    if (key == LogicalKeyboardKey.backspace) {
      return _findKeyByName(['DEL']);
    }
    if (key == LogicalKeyboardKey.delete) {
      return _findKeyByName(['AC']);
    }
    if (key == LogicalKeyboardKey.escape) {
      return widget.skin.kind == CasioCalcKind.cg50
          ? _findKeyByName(['EXIT', 'AC'])
          : _findKeyByName(['AC', 'ATRAS']);
    }
    if (key == LogicalKeyboardKey.arrowUp) {
      return _findKeyByName(['ARRIBA']);
    }
    if (key == LogicalKeyboardKey.arrowDown) {
      return _findKeyByName(['ABAJO']);
    }
    if (key == LogicalKeyboardKey.arrowLeft) {
      return _findKeyByName(['IZQUIERDA']);
    }
    if (key == LogicalKeyboardKey.arrowRight) {
      return _findKeyByName(['DERECHA']);
    }
    return null;
  }

  KeyEventResult _handleKeyEvent(FocusNode node, KeyEvent event) {
    if (!widget.enableHardwareKeyboard) return KeyEventResult.ignored;
    final mapped = _mapLogicalKey(event.logicalKey);
    if (mapped == null) return KeyEventResult.ignored;
    const hwPointerId = -1000;
    if (event is KeyDownEvent) {
      setState(() => _activePointers[hwPointerId] = mapped);
      widget.engine.pressDown(mapped);
      return KeyEventResult.handled;
    } else if (event is KeyUpEvent) {
      final active = _activePointers.remove(hwPointerId);
      if (active != null) {
        widget.engine.releaseUp(active);
        setState(() {});
        _scheduleHistoryLog(active);
      }
      return KeyEventResult.handled;
    }
    return KeyEventResult.ignored;
  }

  void _handlePointerDown(PointerDownEvent ev) {
    final box = _renderKey.currentContext?.findRenderObject() as RenderBox?;
    if (box == null) return;
    final key =
        widget.skin.hitTest(ev.localPosition.dx, ev.localPosition.dy, box.size);
    if (key == null) return;
    unawaited(HapticFeedback.lightImpact());
    setState(() {
      _activePointers[ev.pointer] = key;
    });
    widget.engine.pressDown(key);
  }

  void _finishPointer(int pointerId, bool commitHistory) {
    final active = _activePointers.remove(pointerId);
    if (active == null) return;
    widget.engine.releaseUp(active);
    setState(() {});
    if (commitHistory) {
      _scheduleHistoryLog(active);
    }
  }

  void _scheduleHistoryLog(CasioKeyDef key) {
    _postPressCaptureTimer?.cancel();
    final delayMs = widget.skin.kind == CasioCalcKind.cg50 ? 220 : 120;
    _postPressCaptureTimer = Timer(Duration(milliseconds: delayMs), () {
      if (!mounted) return;
      widget.onKeyLogged(key, widget.engine);
    });
  }

  @override
  void dispose() {
    _postPressCaptureTimer?.cancel();
    _focusNode.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Focus(
      focusNode: _focusNode,
      autofocus: widget.enableHardwareKeyboard,
      onKeyEvent: _handleKeyEvent,
      child: AnimatedBuilder(
        animation: widget.engine,
        builder: (context, _) {
          final aspect = widget.skin.skinWidth / widget.skin.skinHeight;
          return Center(
            child: AspectRatio(
              aspectRatio: aspect,
              child: LayoutBuilder(
                builder: (context, constraints) {
                  final rw = constraints.maxWidth;
                  final rh = constraints.maxHeight;
                  final scaleX = rw / widget.skin.skinWidth;
                  final scaleY = rh / widget.skin.skinHeight;
                  final lcd = widget.skin.lcdRect;
                  final lcdLeft = lcd.left * scaleX;
                  final lcdTop = lcd.top * scaleY;
                  final lcdWidth = lcd.width * scaleX;
                  final lcdHeight = lcd.height * scaleY;

                  final img = widget.engine.lcdImage;
                  final stripH = widget.skin.hasTopIndicatorStrip
                      ? (lcdHeight * 0.145).clamp(10.0, 22.0)
                      : 0.0;

                  return Listener(
                    key: _renderKey,
                    behavior: HitTestBehavior.opaque,
                    onPointerDown: _handlePointerDown,
                    onPointerUp: (e) => _finishPointer(e.pointer, true),
                    onPointerCancel: (e) => _finishPointer(e.pointer, false),
                    child: Stack(
                      fit: StackFit.expand,
                      children: [
                        Image.asset(
                          widget.skin.assetPath,
                          fit: BoxFit.fill,
                          filterQuality: FilterQuality.medium,
                        ),
                        Positioned(
                          left: lcdLeft,
                          top: lcdTop,
                          width: lcdWidth,
                          height: lcdHeight,
                          child: ClipRect(
                            child: Container(
                              color: widget.skin.lcdBackground,
                              child: Column(
                                crossAxisAlignment: CrossAxisAlignment.stretch,
                                children: [
                                  if (widget.skin.hasTopIndicatorStrip)
                                    CasioTopIndicatorStrip(
                                      mask: widget.engine.indicatorsMask,
                                      isCw2Layout:
                                          widget.skin.isCw2IndicatorLayout,
                                      foreground: widget.skin.lcdForeground,
                                      background: widget.skin.lcdBackground,
                                      height: stripH,
                                    ),
                                  Expanded(
                                    child: img != null
                                        ? RawImage(
                                            image: img,
                                            fit: BoxFit.fill,
                                            filterQuality: FilterQuality.none,
                                          )
                                        : Center(
                                            child: SizedBox(
                                              width: 18,
                                              height: 18,
                                              child: CircularProgressIndicator(
                                                strokeWidth: 2,
                                                color: widget.skin.lcdForeground
                                                    .withValues(alpha: 0.5),
                                              ),
                                            ),
                                          ),
                                  ),
                                ],
                              ),
                            ),
                          ),
                        ),
                        for (final pressed in _activePointers.values)
                          Positioned(
                            left: (pressed.cx - pressed.w * 0.5) * scaleX,
                            top: (pressed.cy - pressed.h * 0.5) * scaleY,
                            width: pressed.w * scaleX,
                            height: pressed.h * scaleY,
                            child: IgnorePointer(
                              child: Container(
                                decoration: BoxDecoration(
                                  color: widget.tokens.keyHighlightFill,
                                  borderRadius: BorderRadius.circular(6),
                                  border: Border.all(
                                    color: widget.tokens.keyHighlightBorder,
                                    width: 1.4,
                                  ),
                                ),
                              ),
                            ),
                          ),
                      ],
                    ),
                  );
                },
              ),
            ),
          );
        },
      ),
    );
  }
}

class RgbaThumbnailWidget extends StatefulWidget {
  final Uint8List rgbaBytes;
  final int width;
  final int height;
  final int indicatorsMask;
  final bool hasTopStrip;
  final bool isCw2Indicators;
  final Color lcdBg;
  final Color lcdFg;
  final Color borderColor;

  const RgbaThumbnailWidget({
    super.key,
    required this.rgbaBytes,
    required this.width,
    required this.height,
    required this.indicatorsMask,
    required this.hasTopStrip,
    required this.isCw2Indicators,
    required this.lcdBg,
    required this.lcdFg,
    required this.borderColor,
  });

  @override
  State<RgbaThumbnailWidget> createState() => _RgbaThumbnailWidgetState();
}

class _RgbaThumbnailWidgetState extends State<RgbaThumbnailWidget> {
  ui.Image? _img;

  @override
  void initState() {
    super.initState();
    _decode();
  }

  @override
  void didUpdateWidget(covariant RgbaThumbnailWidget oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (!identical(oldWidget.rgbaBytes, widget.rgbaBytes)) {
      _decode();
    }
  }

  void _decode() {
    ui.decodeImageFromPixels(
      widget.rgbaBytes,
      widget.width,
      widget.height,
      ui.PixelFormat.rgba8888,
      (img) {
        if (!mounted) {
          img.dispose();
          return;
        }
        setState(() {
          _img?.dispose();
          _img = img;
        });
      },
    );
  }

  @override
  void dispose() {
    _img?.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final stripH = widget.hasTopStrip ? 14.0 : 0.0;
    return AspectRatio(
      aspectRatio: widget.width / widget.height,
      child: Container(
        decoration: BoxDecoration(
          color: widget.lcdBg,
          borderRadius: BorderRadius.circular(6),
          border: Border.all(color: widget.borderColor),
        ),
        child: ClipRRect(
          borderRadius: BorderRadius.circular(5),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              if (widget.hasTopStrip)
                CasioTopIndicatorStrip(
                  mask: widget.indicatorsMask,
                  isCw2Layout: widget.isCw2Indicators,
                  foreground: widget.lcdFg,
                  background: widget.lcdBg,
                  height: stripH,
                ),
              Expanded(
                child: _img == null
                    ? const SizedBox.shrink()
                    : RawImage(
                        image: _img,
                        fit: BoxFit.fill,
                        filterQuality: FilterQuality.none,
                      ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class CasioHistorySheet extends StatelessWidget {
  final List<CapturedHistoryEntry> entries;
  final TokensInterfaz tokens;
  final bool reduceTransparency;
  final VoidCallback onClear;

  const CasioHistorySheet({
    super.key,
    required this.entries,
    required this.tokens,
    required this.reduceTransparency,
    required this.onClear,
  });

  @override
  Widget build(BuildContext context) {
    final reversed = entries.reversed.toList();
    return SizedBox(
      height: MediaQuery.of(context).size.height * 0.70,
      child: TranslucentSurface(
        tokens: tokens,
        reduceTransparency: reduceTransparency,
        borderRadius: const BorderRadius.vertical(top: Radius.circular(18)),
        child: Column(
          children: [
            const SizedBox(height: 8),
            Container(
              width: 36,
              height: 4,
              decoration: BoxDecoration(
                color: tokens.border,
                borderRadius: BorderRadius.circular(2),
              ),
            ),
            Padding(
              padding: const EdgeInsets.fromLTRB(16, 10, 12, 8),
              child: Row(
                children: [
                  Icon(CupertinoIcons.clock, color: tokens.accent, size: 19),
                  const SizedBox(width: 8),
                  Text(
                    'Historial (${entries.length})',
                    style: TextStyle(
                      color: tokens.textPrimary,
                      fontWeight: FontWeight.w600,
                      fontSize: 16,
                    ),
                  ),
                  const Spacer(),
                  TextButton(
                    onPressed: entries.isEmpty ? null : onClear,
                    child: Text(
                      'Limpiar',
                      style: TextStyle(
                        color: entries.isEmpty
                            ? tokens.textSecondary
                            : tokens.destructive,
                        fontWeight: FontWeight.w500,
                      ),
                    ),
                  ),
                ],
              ),
            ),
            Divider(height: 1, color: tokens.border),
            Expanded(
              child: reversed.isEmpty
                  ? Center(
                      child: Padding(
                        padding: const EdgeInsets.all(24),
                        child: Text(
                          'Las operaciones registradas aparecerán aquí.',
                          textAlign: TextAlign.center,
                          style: TextStyle(
                            color: tokens.textSecondary,
                            fontSize: 14,
                          ),
                        ),
                      ),
                    )
                  : ListView.separated(
                      padding: const EdgeInsets.all(14),
                      itemCount: reversed.length,
                      separatorBuilder: (_, _) =>
                          Divider(height: 20, color: tokens.border),
                      itemBuilder: (_, idx) {
                        final e = reversed[idx];
                        final hh = e.timestamp.hour.toString().padLeft(2, '0');
                        final mm =
                            e.timestamp.minute.toString().padLeft(2, '0');
                        final ss =
                            e.timestamp.second.toString().padLeft(2, '0');
                        return Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            Row(
                              children: [
                                Text(
                                  '${e.calcTitle} · ${e.keyLabel}',
                                  style: TextStyle(
                                    color: e.isEvalKey
                                        ? tokens.accent
                                        : tokens.textPrimary,
                                    fontSize: 13,
                                    fontWeight: FontWeight.w600,
                                  ),
                                ),
                                const Spacer(),
                                Text(
                                  '$hh:$mm:$ss',
                                  style: TextStyle(
                                    color: tokens.textSecondary,
                                    fontSize: 11.5,
                                  ),
                                ),
                              ],
                            ),
                            if (e.rgbaBytes != null &&
                                e.pixelWidth > 0 &&
                                e.pixelHeight > 0) ...[
                              const SizedBox(height: 8),
                              SizedBox(
                                height:
                                    e.kind == CasioCalcKind.cg50 ? 112 : 76,
                                child: RgbaThumbnailWidget(
                                  rgbaBytes: e.rgbaBytes!,
                                  width: e.pixelWidth,
                                  height: e.pixelHeight,
                                  indicatorsMask: e.indicatorsMask,
                                  hasTopStrip: e.kind != CasioCalcKind.cg50,
                                  isCw2Indicators: e.isCw2Indicators,
                                  lcdBg: e.lcdBg,
                                  lcdFg: e.lcdFg,
                                  borderColor: tokens.border,
                                ),
                              ),
                            ],
                          ],
                        );
                      },
                    ),
            ),
          ],
        ),
      ),
    );
  }
}
