import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter/foundation.dart';

import '../native/calc_models.dart';
import '../native/casio_ffi_bindings.dart';

enum KeyLogMode {
  idle,
  recording,
  playingAuto,
  playingPaused,
  stepping,
}

class KeyLogStep {
  final int? keyCode;
  final String label;
  final bool isPause;
  final int pauseMs;
  final String note;

  const KeyLogStep({
    this.keyCode,
    required this.label,
    this.isPause = false,
    this.pauseMs = 0,
    this.note = '',
  });

  factory KeyLogStep.key(int code, String label) =>
      KeyLogStep(keyCode: code, label: label);

  factory KeyLogStep.pause([int ms = 1000]) =>
      KeyLogStep(label: 'Pausa', isPause: true, pauseMs: ms);

  factory KeyLogStep.note(String text) =>
      KeyLogStep(label: 'Nota', note: text);

  KeyLogStep copyWith({
    int? keyCode,
    String? label,
    bool? isPause,
    int? pauseMs,
    String? note,
  }) {
    return KeyLogStep(
      keyCode: keyCode ?? this.keyCode,
      label: label ?? this.label,
      isPause: isPause ?? this.isPause,
      pauseMs: pauseMs ?? this.pauseMs,
      note: note ?? this.note,
    );
  }

  Map<String, dynamic> toJson() => {
        if (keyCode != null) 'keyCode': keyCode,
        'label': label,
        if (isPause) 'isPause': true,
        if (pauseMs > 0) 'pauseMs': pauseMs,
        if (note.isNotEmpty) 'note': note,
      };

  factory KeyLogStep.fromJson(Map<String, dynamic> j) {
    return KeyLogStep(
      keyCode: (j['keyCode'] as num?)?.toInt(),
      label: j['label'] as String? ?? '',
      isPause: j['isPause'] as bool? ?? false,
      pauseMs: (j['pauseMs'] as num?)?.toInt() ?? 0,
      note: j['note'] as String? ?? '',
    );
  }
}

class KeyLogRecord {
  String name;
  final List<KeyLogStep> steps;
  bool includeMemoryState;
  String? stateGzipBase64;

  KeyLogRecord({
    required this.name,
    List<KeyLogStep>? steps,
    this.includeMemoryState = false,
    this.stateGzipBase64,
  }) : steps = steps ?? [];

  Map<String, dynamic> toJson() => {
        'name': name,
        'includeMemoryState': includeMemoryState,
        if (stateGzipBase64 != null) 'stateGzipBase64': stateGzipBase64,
        'steps': steps.map((s) => s.toJson()).toList(),
      };

  factory KeyLogRecord.fromJson(Map<String, dynamic> j) {
    final rawSteps = (j['steps'] as List<dynamic>?) ?? const [];
    return KeyLogRecord(
      name: j['name'] as String? ?? 'Datos1',
      includeMemoryState: j['includeMemoryState'] as bool? ?? false,
      stateGzipBase64: j['stateGzipBase64'] as String?,
      steps: rawSteps
          .whereType<Map<String, dynamic>>()
          .map(KeyLogStep.fromJson)
          .toList(),
    );
  }
}

class KeyLogDocument {
  static const int currentVersion = 1;
  final int version;
  final List<KeyLogRecord> records;

  KeyLogDocument({
    this.version = currentVersion,
    List<KeyLogRecord>? records,
  }) : records = records ?? [KeyLogRecord(name: 'Datos1')];

  String toJsonString() {
    return const JsonEncoder.withIndent('  ').convert({
      'format': 'cg50_keylog',
      'version': version,
      'records': records.map((r) => r.toJson()).toList(),
    });
  }

  static KeyLogDocument? fromJsonString(String raw) {
    try {
      final decoded = jsonDecode(raw);
      if (decoded is! Map<String, dynamic>) return null;
      final rawRecs = decoded['records'] as List<dynamic>?;
      if (rawRecs == null || rawRecs.isEmpty) return null;
      final recs = rawRecs
          .whereType<Map<String, dynamic>>()
          .map(KeyLogRecord.fromJson)
          .toList();
      if (recs.isEmpty) return null;
      return KeyLogDocument(
        version: (decoded['version'] as num?)?.toInt() ?? currentVersion,
        records: recs,
      );
    } catch (_) {
      return null;
    }
  }
}

class Cg50KeyLogController extends ChangeNotifier {
  KeyLogDocument _doc = KeyLogDocument();
  int _selectedRecordIndex = 0;
  KeyLogMode _mode = KeyLogMode.idle;
  int _playCursor = -1;
  int _playbackGeneration = 0;

  bool _showCurrentKey = true;
  bool _showNextKey = true;
  int _autoPlayIntervalMs = 550;
  bool _includeMemoryState = false;
  Uint8List? _prePlayBackupState;

  KeyLogDocument get document => _doc;
  List<KeyLogRecord> get records => _doc.records;
  int get selectedRecordIndex => _selectedRecordIndex;
  KeyLogRecord get activeRecord => _doc.records[_selectedRecordIndex];
  KeyLogMode get mode => _mode;
  int get playCursor => _playCursor;

  bool get isRecording => _mode == KeyLogMode.recording;
  bool get isPlaying =>
      _mode == KeyLogMode.playingAuto ||
      _mode == KeyLogMode.playingPaused ||
      _mode == KeyLogMode.stepping;
  bool get showCurrentKey => _showCurrentKey;
  bool get showNextKey => _showNextKey;
  int get autoPlayIntervalMs => _autoPlayIntervalMs;
  bool get includeMemoryState => _includeMemoryState;
  bool get hasBackupState => _prePlayBackupState != null;

  int? get highlightedCurrentKeyCode {
    if (!_showCurrentKey) return null;
    if (_playCursor < 0 || _playCursor >= activeRecord.steps.length) return null;
    return activeRecord.steps[_playCursor].keyCode;
  }

  int? get highlightedNextKeyCode {
    if (!_showNextKey) return null;
    final steps = activeRecord.steps;
    final start = _playCursor + 1;
    if (start < 0 || start >= steps.length) return null;
    for (int i = start; i < steps.length; i++) {
      if (steps[i].keyCode != null) return steps[i].keyCode;
    }
    return null;
  }

  void setShowCurrentKey(bool v) {
    _showCurrentKey = v;
    notifyListeners();
  }

  void setShowNextKey(bool v) {
    _showNextKey = v;
    notifyListeners();
  }

  void setAutoPlayIntervalMs(int ms) {
    _autoPlayIntervalMs = ms.clamp(200, 2500);
    notifyListeners();
  }

  void setIncludeMemoryState(bool v) {
    _includeMemoryState = v;
    activeRecord.includeMemoryState = v;
    notifyListeners();
  }

  void selectRecord(int index) {
    if (index < 0 || index >= _doc.records.length) return;
    stop();
    _selectedRecordIndex = index;
    _includeMemoryState = activeRecord.includeMemoryState;
    notifyListeners();
  }

  void addRecord() {
    stop();
    final idx = _doc.records.length + 1;
    _doc.records.add(KeyLogRecord(
      name: 'Datos$idx',
      includeMemoryState: _includeMemoryState,
    ));
    _selectedRecordIndex = _doc.records.length - 1;
    notifyListeners();
  }

  void renameActiveRecord(String name) {
    final trimmed = name.trim();
    if (trimmed.isEmpty) return;
    activeRecord.name = trimmed;
    notifyListeners();
  }

  void deleteActiveRecord() {
    stop();
    if (_doc.records.length <= 1) {
      activeRecord.steps.clear();
      activeRecord.stateGzipBase64 = null;
    } else {
      _doc.records.removeAt(_selectedRecordIndex);
      if (_selectedRecordIndex >= _doc.records.length) {
        _selectedRecordIndex = _doc.records.length - 1;
      }
    }
    notifyListeners();
  }

  Future<void> startRecording(CasioCg50Engine? engine,
      {bool clearExisting = false}) async {
    stop();
    if (clearExisting) {
      activeRecord.steps.clear();
    }
    activeRecord.includeMemoryState = _includeMemoryState;
    if (_includeMemoryState && engine != null && engine.ready) {
      final rawState = await engine.exportStateBytes();
      if (rawState != null && rawState.isNotEmpty) {
        final gz = gzip.encode(rawState);
        activeRecord.stateGzipBase64 = base64Encode(gz);
      }
    } else if (!_includeMemoryState) {
      activeRecord.stateGzipBase64 = null;
    }
    _mode = KeyLogMode.recording;
    notifyListeners();
  }

  void recordKeyPress(CasioKeyDef key) {
    if (_mode != KeyLogMode.recording) return;
    final label = key.displayLabel.isNotEmpty ? key.displayLabel : key.name;
    activeRecord.steps.add(KeyLogStep.key(key.matrixCode, label));
    notifyListeners();
  }

  void insertPause([int ms = 1000]) {
    activeRecord.steps.add(KeyLogStep.pause(ms));
    notifyListeners();
  }

  void addNote(String text) {
    final clean = text.trim();
    if (clean.isEmpty) return;
    activeRecord.steps.add(KeyLogStep.note(clean));
    notifyListeners();
  }

  void updateStepNote(int index, String note) {
    if (index < 0 || index >= activeRecord.steps.length) return;
    activeRecord.steps[index] = activeRecord.steps[index].copyWith(note: note);
    notifyListeners();
  }

  void deleteStep(int index) {
    if (index < 0 || index >= activeRecord.steps.length) return;
    activeRecord.steps.removeAt(index);
    if (_playCursor >= activeRecord.steps.length) {
      _playCursor = activeRecord.steps.length - 1;
    }
    notifyListeners();
  }

  void moveStep(int oldIndex, int newIndex, {bool alreadyAdjusted = false}) {
    final list = activeRecord.steps;
    if (oldIndex < 0 || oldIndex >= list.length) return;
    int target = newIndex;
    if (!alreadyAdjusted && target > oldIndex) target -= 1;
    target = target.clamp(0, list.length - 1);
    final item = list.removeAt(oldIndex);
    list.insert(target, item);
    notifyListeners();
  }

  void clearActiveSteps() {
    stop();
    activeRecord.steps.clear();
    notifyListeners();
  }

  String formatStepsAsText() {
    final buf = StringBuffer();
    for (int i = 0; i < activeRecord.steps.length; i++) {
      final s = activeRecord.steps[i];
      if (i > 0) buf.write(' ');
      if (s.isPause) {
        buf.write('[PAUSA]');
      } else if (s.keyCode != null) {
        buf.write('[${s.label}]');
      } else if (s.note.isNotEmpty) {
        buf.write('(${s.note})');
      }
    }
    return buf.toString();
  }

  String exportJsonString() => _doc.toJsonString();

  bool importJsonString(String raw) {
    final parsed = KeyLogDocument.fromJsonString(raw);
    if (parsed == null) return false;
    stop();
    _doc = parsed;
    _selectedRecordIndex = 0;
    _includeMemoryState = activeRecord.includeMemoryState;
    notifyListeners();
    return true;
  }

  Future<void> _prepareStateForPlayback(CasioCg50Engine engine) async {
    final b64 = activeRecord.stateGzipBase64;
    if (b64 == null || b64.isEmpty) return;
    _prePlayBackupState ??= await engine.exportStateBytes();
    try {
      final gz = base64Decode(b64);
      final raw = Uint8List.fromList(gzip.decode(gz));
      await engine.importStateBytes(raw);
    } catch (_) {}
  }

  Future<bool> restoreBackupState(CasioCg50Engine engine) async {
    final backup = _prePlayBackupState;
    if (backup == null) return false;
    stop();
    final ok = await engine.importStateBytes(backup);
    if (ok) {
      _prePlayBackupState = null;
      notifyListeners();
    }
    return ok;
  }

  Future<void> _executeSingleStep(CasioCg50Engine engine, KeyLogStep step) async {
    final code = step.keyCode;
    if (code != null) {
      engine.pressCodeDown(code);
      await Future<void>.delayed(const Duration(milliseconds: 145));
      engine.releaseCodeUp(code);
    }
  }

  Future<void> startAutoPlay(CasioCg50Engine engine) async {
    if (activeRecord.steps.isEmpty) return;
    final gen = ++_playbackGeneration;
    if (_playCursor < 0 || _playCursor >= activeRecord.steps.length - 1) {
      _playCursor = -1;
      await _prepareStateForPlayback(engine);
    }
    _mode = KeyLogMode.playingAuto;
    notifyListeners();

    while (gen == _playbackGeneration && _mode == KeyLogMode.playingAuto) {
      final next = _playCursor + 1;
      if (next >= activeRecord.steps.length) {
        _mode = KeyLogMode.idle;
        _playCursor = -1;
        notifyListeners();
        return;
      }
      _playCursor = next;
      notifyListeners();
      final step = activeRecord.steps[_playCursor];
      if (step.isPause) {
        _mode = KeyLogMode.playingPaused;
        notifyListeners();
        return;
      }
      await _executeSingleStep(engine, step);
      if (gen != _playbackGeneration || _mode != KeyLogMode.playingAuto) return;
      await Future<void>.delayed(Duration(milliseconds: _autoPlayIntervalMs));
    }
  }

  Future<void> stepNext(CasioCg50Engine engine) async {
    if (activeRecord.steps.isEmpty) return;
    ++_playbackGeneration;
    if (_playCursor < 0) {
      await _prepareStateForPlayback(engine);
    }
    final next = _playCursor + 1;
    if (next >= activeRecord.steps.length) {
      _mode = KeyLogMode.idle;
      _playCursor = -1;
      notifyListeners();
      return;
    }
    _mode = KeyLogMode.stepping;
    _playCursor = next;
    notifyListeners();
    final step = activeRecord.steps[_playCursor];
    if (!step.isPause) {
      await _executeSingleStep(engine, step);
    }
    if (_playCursor >= activeRecord.steps.length - 1) {
      _mode = KeyLogMode.idle;
      _playCursor = -1;
      notifyListeners();
    }
  }

  void pausePlayback() {
    if (_mode == KeyLogMode.playingAuto) {
      ++_playbackGeneration;
      _mode = KeyLogMode.playingPaused;
      notifyListeners();
    }
  }

  void stop() {
    ++_playbackGeneration;
    _mode = KeyLogMode.idle;
    _playCursor = -1;
    notifyListeners();
  }
}
