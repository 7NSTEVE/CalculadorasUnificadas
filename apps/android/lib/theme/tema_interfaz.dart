import 'dart:ui';

import 'package:flutter/material.dart';

enum AppThemePreset {
  porcelainLight,
  calibratedDark,
  highContrast,
}

@immutable
class TokensInterfaz {
  final AppThemePreset preset;
  final Brightness brightness;
  final Color canvas;
  final Color surface;
  final Color surfaceTranslucent;
  final Color controlTrack;
  final Color controlThumb;
  final Color border;
  final Color textPrimary;
  final Color textSecondary;
  final Color accent;
  final Color keyHighlightFill;
  final Color keyHighlightBorder;
  final Color destructive;
  final double borderWidth;

  const TokensInterfaz({
    required this.preset,
    required this.brightness,
    required this.canvas,
    required this.surface,
    required this.surfaceTranslucent,
    required this.controlTrack,
    required this.controlThumb,
    required this.border,
    required this.textPrimary,
    required this.textSecondary,
    required this.accent,
    required this.keyHighlightFill,
    required this.keyHighlightBorder,
    required this.destructive,
    required this.borderWidth,
  });

  static TokensInterfaz of(AppThemePreset preset) {
    switch (preset) {
      case AppThemePreset.porcelainLight:
        return const TokensInterfaz(
          preset: AppThemePreset.porcelainLight,
          brightness: Brightness.light,
          canvas: Color(0xFFF5F5F7),
          surface: Color(0xFFFFFFFF),
          surfaceTranslucent: Color(0xCCFFFFFF),
          controlTrack: Color(0x14000000),
          controlThumb: Color(0xFFFFFFFF),
          border: Color(0x18000000),
          textPrimary: Color(0xFF1D1D1F),
          textSecondary: Color(0xFF6E6E73),
          accent: Color(0xFF0071E3),
          keyHighlightFill: Color(0x2E0071E3),
          keyHighlightBorder: Color(0xB80071E3),
          destructive: Color(0xFFD70015),
          borderWidth: 0.75,
        );
      case AppThemePreset.calibratedDark:
        return const TokensInterfaz(
          preset: AppThemePreset.calibratedDark,
          brightness: Brightness.dark,
          canvas: Color(0xFF121417),
          surface: Color(0xFF1C1F24),
          surfaceTranslucent: Color(0xCC1C1F24),
          controlTrack: Color(0x1FFFFFFF),
          controlThumb: Color(0xFF2C313A),
          border: Color(0x1FFFFFFF),
          textPrimary: Color(0xFFF5F5F7),
          textSecondary: Color(0xFF98989D),
          accent: Color(0xFF0A84FF),
          keyHighlightFill: Color(0x330A84FF),
          keyHighlightBorder: Color(0xC00A84FF),
          destructive: Color(0xFFFF453A),
          borderWidth: 0.75,
        );
      case AppThemePreset.highContrast:
        return const TokensInterfaz(
          preset: AppThemePreset.highContrast,
          brightness: Brightness.light,
          canvas: Color(0xFFFFFFFF),
          surface: Color(0xFFFFFFFF),
          surfaceTranslucent: Color(0xFFFFFFFF),
          controlTrack: Color(0xFFE5E5E5),
          controlThumb: Color(0xFF000000),
          border: Color(0xFF000000),
          textPrimary: Color(0xFF000000),
          textSecondary: Color(0xFF1A1A1A),
          accent: Color(0xFF0033CC),
          keyHighlightFill: Color(0x380033CC),
          keyHighlightBorder: Color(0xFF0033CC),
          destructive: Color(0xFFB00020),
          borderWidth: 1.5,
        );
    }
  }

  ThemeData toThemeData() {
    final isDark = brightness == Brightness.dark;
    return ThemeData(
      useMaterial3: true,
      brightness: brightness,
      scaffoldBackgroundColor: canvas,
      colorScheme: ColorScheme(
        brightness: brightness,
        primary: accent,
        onPrimary: Colors.white,
        secondary: accent,
        onSecondary: Colors.white,
        error: destructive,
        onError: Colors.white,
        surface: surface,
        onSurface: textPrimary,
      ),
      dividerColor: border,
      popupMenuTheme: PopupMenuThemeData(
        color: surface,
        elevation: 6,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(12),
          side: BorderSide(color: border, width: borderWidth),
        ),
        textStyle: TextStyle(color: textPrimary, fontSize: 14),
      ),
      snackBarTheme: SnackBarThemeData(
        backgroundColor: isDark ? const Color(0xFF2C313A) : const Color(0xFF1D1D1F),
        contentTextStyle: const TextStyle(color: Colors.white, fontSize: 13.5),
        behavior: SnackBarBehavior.floating,
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
      ),
    );
  }
}

class TranslucentSurface extends StatelessWidget {
  final TokensInterfaz tokens;
  final bool reduceTransparency;
  final Widget child;
  final EdgeInsetsGeometry padding;
  final BorderRadius borderRadius;
  final bool bottomBorderOnly;

  const TranslucentSurface({
    super.key,
    required this.tokens,
    required this.reduceTransparency,
    required this.child,
    this.padding = EdgeInsets.zero,
    this.borderRadius = BorderRadius.zero,
    this.bottomBorderOnly = false,
  });

  @override
  Widget build(BuildContext context) {
    final bool solid =
        reduceTransparency || tokens.preset == AppThemePreset.highContrast;
    final BoxDecoration deco = BoxDecoration(
      color: solid ? tokens.surface : tokens.surfaceTranslucent,
      borderRadius: borderRadius,
      border: bottomBorderOnly
          ? Border(
              bottom: BorderSide(
                color: tokens.border,
                width: tokens.borderWidth,
              ),
            )
          : Border.all(color: tokens.border, width: tokens.borderWidth),
    );

    final inner = Container(
      padding: padding,
      decoration: deco,
      child: child,
    );

    if (solid) return inner;

    return ClipRRect(
      borderRadius: borderRadius,
      child: BackdropFilter(
        filter: ImageFilter.blur(sigmaX: 18, sigmaY: 18),
        child: inner,
      ),
    );
  }
}

class SelectorSegmentado<T> extends StatelessWidget {
  final TokensInterfaz tokens;
  final T value;
  final List<(T, String)> segments;
  final ValueChanged<T> onChanged;

  const SelectorSegmentado({
    super.key,
    required this.tokens,
    required this.value,
    required this.segments,
    required this.onChanged,
  });

  @override
  Widget build(BuildContext context) {
    final isHighContrast = tokens.preset == AppThemePreset.highContrast;
    return Container(
      padding: const EdgeInsets.all(3),
      decoration: BoxDecoration(
        color: tokens.controlTrack,
        borderRadius: BorderRadius.circular(10),
        border: isHighContrast
            ? Border.all(color: tokens.border, width: tokens.borderWidth)
            : null,
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          for (final seg in segments)
            _SegmentItem<T>(
              tokens: tokens,
              label: seg.$2,
              selected: seg.$1 == value,
              onTap: () => onChanged(seg.$1),
            ),
        ],
      ),
    );
  }
}

class _SegmentItem<T> extends StatelessWidget {
  final TokensInterfaz tokens;
  final String label;
  final bool selected;
  final VoidCallback onTap;

  const _SegmentItem({
    required this.tokens,
    required this.label,
    required this.selected,
    required this.onTap,
  });

  @override
  Widget build(BuildContext context) {
    final isHighContrast = tokens.preset == AppThemePreset.highContrast;
    final Color fg = selected
        ? (isHighContrast ? Colors.white : tokens.textPrimary)
        : tokens.textSecondary;

    return GestureDetector(
      behavior: HitTestBehavior.opaque,
      onTap: onTap,
      child: AnimatedContainer(
        duration: const Duration(milliseconds: 160),
        curve: Curves.easeOutCubic,
        padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
        decoration: BoxDecoration(
          color: selected ? tokens.controlThumb : Colors.transparent,
          borderRadius: BorderRadius.circular(7.5),
          boxShadow: selected && !isHighContrast
              ? const [
                  BoxShadow(
                    color: Color(0x14000000),
                    blurRadius: 3,
                    offset: Offset(0, 1),
                  ),
                ]
              : null,
        ),
        child: Text(
          label,
          maxLines: 1,
          softWrap: false,
          style: TextStyle(
            color: fg,
            fontSize: 12.5,
            fontWeight: selected ? FontWeight.w600 : FontWeight.w500,
            letterSpacing: -0.1,
          ),
        ),
      ),
    );
  }
}
