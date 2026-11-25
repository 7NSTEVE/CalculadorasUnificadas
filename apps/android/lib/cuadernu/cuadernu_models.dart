import 'dart:ui';

/// Top-level sections in the unified CuadernU app (Android / iOS / Desktop).
enum CuadernuWorkspaceTab {
  notebook,
  calculators,
  stem,
  study,
}

/// Interactive canvas / notebook editing tools.
enum CuadernuToolMode {
  pen,
  highlighter,
  eraser,
  pan,
  text,
  shape,
  math,
  sticky,
  tape,
}

/// Metadata for a `.cudoc` notebook file managed inside the local CuadernU vault.
class CuadernuVaultEntry {
  final String id;
  final String title;
  final String path;
  final DateTime updatedAt;
  final bool infiniteCanvas;
  final int pageCount;

  const CuadernuVaultEntry({
    required this.id,
    required this.title,
    required this.path,
    required this.updatedAt,
    required this.infiniteCanvas,
    required this.pageCount,
  });

  Map<String, dynamic> toJson() => {
        'id': id,
        'title': title,
        'path': path,
        'updatedAt': updatedAt.toIso8601String(),
        'infiniteCanvas': infiniteCanvas,
        'pageCount': pageCount,
      };

  static CuadernuVaultEntry fromJson(Map<String, dynamic> m) =>
      CuadernuVaultEntry(
        id: m['id']?.toString() ?? '',
        title: m['title']?.toString() ?? 'Cuaderno sin título',
        path: m['path']?.toString() ?? '',
        updatedAt: DateTime.tryParse(m['updatedAt']?.toString() ?? '') ??
            DateTime.now(),
        infiniteCanvas: m['infiniteCanvas'] == true,
        pageCount: (m['pageCount'] as num?)?.toInt() ?? 1,
      );
}

/// Row/card item in the structured academic database (Table & Kanban views).
class CuadernuDbItem {
  String id;
  String title;
  String course;
  String status; // 'Pendiente' | 'En curso' | 'Completado'
  String priority; // 'Alta' | 'Media' | 'Baja'
  String dueDate;

  CuadernuDbItem({
    required this.id,
    required this.title,
    required this.course,
    required this.status,
    required this.priority,
    required this.dueDate,
  });

  Map<String, dynamic> toJson() => {
        'id': id,
        'title': title,
        'course': course,
        'status': status,
        'priority': priority,
        'dueDate': dueDate,
      };

  static CuadernuDbItem fromJson(Map<String, dynamic> m) => CuadernuDbItem(
        id: m['id']?.toString() ?? '',
        title: m['title']?.toString() ?? '',
        course: m['course']?.toString() ?? '',
        status: m['status']?.toString() ?? 'Pendiente',
        priority: m['priority']?.toString() ?? 'Media',
        dueDate: m['dueDate']?.toString() ?? '',
      );
}

/// Knowledge note supporting `[[wikilinks]]` for the C++17 force-directed graph.
class CuadernuKnowledgeNote {
  String id;
  String title;
  String folder;
  String body;
  List<String> tags;

  CuadernuKnowledgeNote({
    required this.id,
    required this.title,
    required this.folder,
    required this.body,
    required this.tags,
  });

  Map<String, dynamic> toJson() => {
        'id': id,
        'title': title,
        'folder': folder,
        'body': body,
        'tags': tags,
      };

  static CuadernuKnowledgeNote fromJson(Map<String, dynamic> m) =>
      CuadernuKnowledgeNote(
        id: m['id']?.toString() ?? '',
        title: m['title']?.toString() ?? '',
        folder: m['folder']?.toString() ?? 'General',
        body: m['body']?.toString() ?? '',
        tags: (m['tags'] as List?)?.map((e) => e.toString()).toList() ?? [],
      );
}

/// Flashcard with SM-2 spaced repetition state computed by native C++17 engine.
class CuadernuFlashcard {
  String id;
  String front;
  String back;
  String source;
  int repetitions;
  int intervalDays;
  double easeFactor;
  int nextReviewEpoch;

  CuadernuFlashcard({
    required this.id,
    required this.front,
    required this.back,
    this.source = 'Manual',
    this.repetitions = 0,
    this.intervalDays = 1,
    this.easeFactor = 2.5,
    this.nextReviewEpoch = 0,
  });

  Map<String, dynamic> toJson() => {
        'id': id,
        'front': front,
        'back': back,
        'source': source,
        'repetitions': repetitions,
        'intervalDays': intervalDays,
        'easeFactor': easeFactor,
        'nextReviewEpoch': nextReviewEpoch,
      };

  static CuadernuFlashcard fromJson(Map<String, dynamic> m) =>
      CuadernuFlashcard(
        id: m['id']?.toString() ?? '',
        front: m['front']?.toString() ?? '',
        back: m['back']?.toString() ?? '',
        source: m['source']?.toString() ?? 'Manual',
        repetitions: (m['repetitions'] as num?)?.toInt() ?? 0,
        intervalDays: (m['intervalDays'] as num?)?.toInt() ?? 1,
        easeFactor: (m['easeFactor'] as num?)?.toDouble() ?? 2.5,
        nextReviewEpoch: (m['nextReviewEpoch'] as num?)?.toInt() ?? 0,
      );
}

/// Converts a Flutter [Color] to CuadernU C++ `0xRRGGBBAA` packed uint32.
int colorToStudioRgba(Color c) {
  final r = (c.r * 255.0).round().clamp(0, 255);
  final g = (c.g * 255.0).round().clamp(0, 255);
  final b = (c.b * 255.0).round().clamp(0, 255);
  final a = (c.a * 255.0).round().clamp(0, 255);
  return ((r & 0xFF) << 24) |
      ((g & 0xFF) << 16) |
      ((b & 0xFF) << 8) |
      (a & 0xFF);
}

/// Converts a CuadernU C++ `0xRRGGBBAA` uint32 to Flutter [Color].
Color studioRgbaToColor(int rgba) {
  final r = (rgba >> 24) & 0xFF;
  final g = (rgba >> 16) & 0xFF;
  final b = (rgba >> 8) & 0xFF;
  final a = rgba & 0xFF;
  return Color.fromARGB(a, r, g, b);
}
