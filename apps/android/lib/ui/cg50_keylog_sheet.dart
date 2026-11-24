import 'dart:async';
import 'dart:io';

import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:path_provider/path_provider.dart';

import '../keylog/cg50_keylog.dart';
import '../native/casio_ffi_bindings.dart';
import '../theme/tema_interfaz.dart';

class Cg50KeyLogSheet extends StatefulWidget {
  final CasioCg50Engine engine;
  final Cg50KeyLogController controller;
  final TokensInterfaz tokens;
  final Future<Map<String, String>?> Function({String mimeType, String prefix})
      onPickFileWithMeta;
  final Future<bool> Function({
    required String suggestedName,
    required String mimeType,
    required String sourcePath,
  }) onSaveFile;
  final void Function(String message) onNotify;

  const Cg50KeyLogSheet({
    super.key,
    required this.engine,
    required this.controller,
    required this.tokens,
    required this.onPickFileWithMeta,
    required this.onSaveFile,
    required this.onNotify,
  });

  @override
  State<Cg50KeyLogSheet> createState() => _Cg50KeyLogSheetState();
}

class _Cg50KeyLogSheetState extends State<Cg50KeyLogSheet> {
  bool _showOptions = false;

  Future<void> _addNoteDialog() async {
    final ctrl = TextEditingController();
    final text = await showDialog<String>(
      context: context,
      builder: (ctx) => AlertDialog(
        backgroundColor: widget.tokens.surface,
        title: Text('Agregar nota',
            style: TextStyle(color: widget.tokens.textPrimary, fontSize: 17)),
        content: TextField(
          controller: ctrl,
          autofocus: true,
          decoration: const InputDecoration(
            hintText: 'Explicación o paso guía',
            border: OutlineInputBorder(),
            isDense: true,
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(ctx).pop(null),
            child: const Text('Cancelar'),
          ),
          TextButton(
            onPressed: () => Navigator.of(ctx).pop(ctrl.text.trim()),
            child: const Text('Agregar'),
          ),
        ],
      ),
    );
    ctrl.dispose();
    if (text != null && text.isNotEmpty) {
      widget.controller.addNote(text);
    }
  }

  Future<void> _renameRecordDialog() async {
    final ctrl =
        TextEditingController(text: widget.controller.activeRecord.name);
    final name = await showDialog<String>(
      context: context,
      builder: (ctx) => AlertDialog(
        backgroundColor: widget.tokens.surface,
        title: Text('Renombrar registro',
            style: TextStyle(color: widget.tokens.textPrimary, fontSize: 17)),
        content: TextField(
          controller: ctrl,
          autofocus: true,
          decoration: const InputDecoration(
            border: OutlineInputBorder(),
            isDense: true,
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(ctx).pop(null),
            child: const Text('Cancelar'),
          ),
          TextButton(
            onPressed: () => Navigator.of(ctx).pop(ctrl.text.trim()),
            child: const Text('Guardar'),
          ),
        ],
      ),
    );
    ctrl.dispose();
    if (name != null && name.isNotEmpty) {
      widget.controller.renameActiveRecord(name);
    }
  }

  Future<void> _saveKeyLogFile() async {
    final tmpDir = await getTemporaryDirectory();
    final baseName =
        widget.controller.activeRecord.name.replaceAll(RegExp(r'[^A-Za-z0-9_-]'), '_');
    final fileName = '${baseName.isEmpty ? "secuencia" : baseName}.cg50log.json';
    final tmpFile = File('${tmpDir.path}/$fileName');
    await tmpFile.writeAsString(widget.controller.exportJsonString(), flush: true);
    try {
      final saved = await widget.onSaveFile(
        suggestedName: fileName,
        mimeType: 'application/json',
        sourcePath: tmpFile.path,
      );
      if (saved) widget.onNotify('Key-Log guardado: $fileName');
    } finally {
      if (await tmpFile.exists()) await tmpFile.delete();
    }
  }

  Future<void> _openKeyLogFile() async {
    final picked = await widget.onPickFileWithMeta(
      mimeType: '*/*',
      prefix: 'keylog',
    );
    if (picked == null) return;
    final p = picked['path'] ?? '';
    if (p.isEmpty) return;
    final file = File(p);
    if (!await file.exists()) return;
    try {
      final raw = await file.readAsString();
      final ok = widget.controller.importJsonString(raw);
      widget.onNotify(ok ? 'Key-Log cargado' : 'Archivo Key-Log no válido');
    } finally {
      if (await file.exists()) await file.delete();
    }
  }

  @override
  Widget build(BuildContext context) {
    final t = widget.tokens;
    return AnimatedBuilder(
      animation: widget.controller,
      builder: (context, _) {
        final kl = widget.controller;
        final rec = kl.activeRecord;

        return Container(
          decoration: BoxDecoration(
            color: t.surface,
            borderRadius: const BorderRadius.vertical(top: Radius.circular(20)),
            border: Border.all(color: t.border, width: t.borderWidth),
          ),
          padding: const EdgeInsets.fromLTRB(18, 12, 18, 16),
          child: SizedBox(
            height: MediaQuery.of(context).size.height * 0.70,
            child: Column(
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
                const SizedBox(height: 10),
                Row(
                  children: [
                    Text(
                      'Key-Log',
                      style: TextStyle(
                        color: t.textPrimary,
                        fontSize: 17,
                        fontWeight: FontWeight.w600,
                      ),
                    ),
                    const SizedBox(width: 8),
                    if (kl.isRecording)
                      Container(
                        padding: const EdgeInsets.symmetric(
                            horizontal: 8, vertical: 2),
                        decoration: BoxDecoration(
                          color: t.destructive.withValues(alpha: 0.14),
                          borderRadius: BorderRadius.circular(6),
                        ),
                        child: Text(
                          'Grabando',
                          style: TextStyle(
                              color: t.destructive,
                              fontSize: 11.5,
                              fontWeight: FontWeight.w600),
                        ),
                      ),
                    const Spacer(),
                    IconButton(
                      tooltip: 'Opciones',
                      icon: Icon(
                        CupertinoIcons.slider_horizontal_3,
                        size: 19,
                        color: _showOptions ? t.accent : t.textSecondary,
                      ),
                      onPressed: () =>
                          setState(() => _showOptions = !_showOptions),
                    ),
                    IconButton(
                      tooltip: 'Cerrar',
                      icon: Icon(CupertinoIcons.xmark_circle_fill,
                          size: 21, color: t.textSecondary),
                      onPressed: () => Navigator.of(context).pop(),
                    ),
                  ],
                ),
                const SizedBox(height: 6),
                // Record switcher
                Row(
                  children: [
                    Expanded(
                      child: SingleChildScrollView(
                        scrollDirection: Axis.horizontal,
                        child: Row(
                          children: [
                            for (int i = 0; i < kl.records.length; i++)
                              Padding(
                                padding: const EdgeInsets.only(right: 6),
                                child: ChoiceChip(
                                  label: Text(kl.records[i].name,
                                      style: const TextStyle(fontSize: 12.5)),
                                  selected: i == kl.selectedRecordIndex,
                                  onSelected: (_) => kl.selectRecord(i),
                                ),
                              ),
                          ],
                        ),
                      ),
                    ),
                    IconButton(
                      tooltip: 'Nuevo registro',
                      icon: Icon(CupertinoIcons.add_circled,
                          size: 20, color: t.accent),
                      onPressed: kl.addRecord,
                    ),
                    IconButton(
                      tooltip: 'Renombrar',
                      icon: Icon(CupertinoIcons.pencil,
                          size: 18, color: t.textSecondary),
                      onPressed: _renameRecordDialog,
                    ),
                  ],
                ),
                const SizedBox(height: 8),
                // Transport controls
                Wrap(
                  spacing: 8,
                  runSpacing: 8,
                  children: [
                    FilledButton.icon(
                      style: FilledButton.styleFrom(
                        backgroundColor:
                            kl.isRecording ? t.destructive : t.accent,
                        foregroundColor: Colors.white,
                        shape: RoundedRectangleBorder(
                            borderRadius: BorderRadius.circular(10)),
                        padding: const EdgeInsets.symmetric(
                            horizontal: 12, vertical: 8),
                      ),
                      onPressed: () async {
                        if (kl.isRecording) {
                          kl.stop();
                        } else {
                          await kl.startRecording(widget.engine);
                          if (context.mounted) {
                            Navigator.of(context).pop();
                            widget.onNotify(
                                'Grabación Key-Log activa; pulsa teclas en la calculadora');
                          }
                        }
                      },
                      icon: Icon(
                        kl.isRecording
                            ? CupertinoIcons.stop_fill
                            : CupertinoIcons.circle_fill,
                        size: 14,
                      ),
                      label: Text(kl.isRecording ? 'Detener' : 'Grabar',
                          style: const TextStyle(fontSize: 13)),
                    ),
                    OutlinedButton.icon(
                      style: OutlinedButton.styleFrom(
                        foregroundColor: t.textPrimary,
                        side: BorderSide(color: t.border, width: t.borderWidth),
                        shape: RoundedRectangleBorder(
                            borderRadius: BorderRadius.circular(10)),
                        padding: const EdgeInsets.symmetric(
                            horizontal: 10, vertical: 8),
                      ),
                      onPressed: rec.steps.isEmpty
                          ? null
                          : () {
                              if (kl.mode == KeyLogMode.playingAuto) {
                                kl.pausePlayback();
                              } else {
                                Navigator.of(context).pop();
                                unawaited(kl.startAutoPlay(widget.engine));
                              }
                            },
                      icon: Icon(
                        kl.mode == KeyLogMode.playingAuto
                            ? CupertinoIcons.pause_fill
                            : CupertinoIcons.play_fill,
                        size: 15,
                      ),
                      label: Text(
                        kl.mode == KeyLogMode.playingAuto
                            ? 'Pausar'
                            : 'Reproducir',
                        style: const TextStyle(fontSize: 13),
                      ),
                    ),
                    OutlinedButton.icon(
                      style: OutlinedButton.styleFrom(
                        foregroundColor: t.textPrimary,
                        side: BorderSide(color: t.border, width: t.borderWidth),
                        shape: RoundedRectangleBorder(
                            borderRadius: BorderRadius.circular(10)),
                        padding: const EdgeInsets.symmetric(
                            horizontal: 10, vertical: 8),
                      ),
                      onPressed: rec.steps.isEmpty
                          ? null
                          : () => unawaited(kl.stepNext(widget.engine)),
                      icon: const Icon(CupertinoIcons.forward_end_fill,
                          size: 14),
                      label: const Text('Paso', style: TextStyle(fontSize: 13)),
                    ),
                    OutlinedButton.icon(
                      style: OutlinedButton.styleFrom(
                        foregroundColor: t.textPrimary,
                        side: BorderSide(color: t.border, width: t.borderWidth),
                        shape: RoundedRectangleBorder(
                            borderRadius: BorderRadius.circular(10)),
                        padding: const EdgeInsets.symmetric(
                            horizontal: 10, vertical: 8),
                      ),
                      onPressed: () => kl.insertPause(1000),
                      icon: const Icon(CupertinoIcons.timer, size: 15),
                      label:
                          const Text('Pausa', style: TextStyle(fontSize: 13)),
                    ),
                    OutlinedButton.icon(
                      style: OutlinedButton.styleFrom(
                        foregroundColor: t.textPrimary,
                        side: BorderSide(color: t.border, width: t.borderWidth),
                        shape: RoundedRectangleBorder(
                            borderRadius: BorderRadius.circular(10)),
                        padding: const EdgeInsets.symmetric(
                            horizontal: 10, vertical: 8),
                      ),
                      onPressed: _addNoteDialog,
                      icon: const Icon(CupertinoIcons.text_bubble, size: 15),
                      label: const Text('Nota', style: TextStyle(fontSize: 13)),
                    ),
                  ],
                ),
                if (_showOptions) ...[
                  const SizedBox(height: 10),
                  Container(
                    padding: const EdgeInsets.all(10),
                    decoration: BoxDecoration(
                      color: t.canvas,
                      borderRadius: BorderRadius.circular(10),
                      border: Border.all(color: t.border, width: t.borderWidth),
                    ),
                    child: Column(
                      children: [
                        SwitchListTile.adaptive(
                          dense: true,
                          contentPadding: EdgeInsets.zero,
                          activeTrackColor: t.accent,
                          title: Text('Mostrar tecla actual',
                              style: TextStyle(
                                  color: t.textPrimary, fontSize: 13.5)),
                          value: kl.showCurrentKey,
                          onChanged: kl.setShowCurrentKey,
                        ),
                        SwitchListTile.adaptive(
                          dense: true,
                          contentPadding: EdgeInsets.zero,
                          activeTrackColor: t.accent,
                          title: Text('Mostrar tecla siguiente',
                              style: TextStyle(
                                  color: t.textPrimary, fontSize: 13.5)),
                          value: kl.showNextKey,
                          onChanged: kl.setShowNextKey,
                        ),
                        SwitchListTile.adaptive(
                          dense: true,
                          contentPadding: EdgeInsets.zero,
                          activeTrackColor: t.accent,
                          title: Text('Incluir estado de memoria (L/E)',
                              style: TextStyle(
                                  color: t.textPrimary, fontSize: 13.5)),
                          value: kl.includeMemoryState,
                          onChanged: kl.setIncludeMemoryState,
                        ),
                        Row(
                          children: [
                            Text('Intervalo: ${kl.autoPlayIntervalMs} ms',
                                style: TextStyle(
                                    color: t.textSecondary, fontSize: 12.5)),
                            Expanded(
                              child: Slider(
                                value: kl.autoPlayIntervalMs.toDouble(),
                                min: 200,
                                max: 1500,
                                divisions: 13,
                                activeColor: t.accent,
                                onChanged: (v) =>
                                    kl.setAutoPlayIntervalMs(v.round()),
                              ),
                            ),
                          ],
                        ),
                      ],
                    ),
                  ),
                ],
                const SizedBox(height: 10),
                Expanded(
                  child: rec.steps.isEmpty
                      ? Center(
                          child: Text(
                            'Pulsa Grabar y luego usa las teclas de la calculadora.',
                            textAlign: TextAlign.center,
                            style: TextStyle(
                                color: t.textSecondary, fontSize: 13.5),
                          ),
                        )
                      : ReorderableListView.builder(
                          itemCount: rec.steps.length,
                          onReorderItem: (oldIdx, newIdx) =>
                              kl.moveStep(oldIdx, newIdx, alreadyAdjusted: true),
                          itemBuilder: (context, idx) {
                            final step = rec.steps[idx];
                            final isCur = idx == kl.playCursor;
                            return ListTile(
                              key: ValueKey('step_${idx}_${step.label}_${step.note}'),
                              dense: true,
                              selected: isCur,
                              selectedTileColor:
                                  t.accent.withValues(alpha: 0.10),
                              leading: CircleAvatar(
                                radius: 12,
                                backgroundColor:
                                    isCur ? t.accent : t.controlTrack,
                                child: Text(
                                  '${idx + 1}',
                                  style: TextStyle(
                                    fontSize: 11,
                                    color:
                                        isCur ? Colors.white : t.textSecondary,
                                  ),
                                ),
                              ),
                              title: Text(
                                step.isPause
                                    ? 'Pausa (${step.pauseMs} ms)'
                                    : step.keyCode != null
                                        ? '[${step.label}]'
                                        : step.note,
                                style: TextStyle(
                                  color: t.textPrimary,
                                  fontSize: 14,
                                  fontWeight: FontWeight.w500,
                                ),
                              ),
                              subtitle: (step.keyCode != null &&
                                      step.note.isNotEmpty)
                                  ? Text(step.note,
                                      style: TextStyle(
                                          color: t.textSecondary, fontSize: 12))
                                  : null,
                              trailing: IconButton(
                                icon: Icon(CupertinoIcons.minus_circle,
                                    size: 18, color: t.destructive),
                                onPressed: () => kl.deleteStep(idx),
                              ),
                            );
                          },
                        ),
                ),
                if (kl.hasBackupState) ...[
                  const SizedBox(height: 6),
                  OutlinedButton.icon(
                    onPressed: () async {
                      final ok = await kl.restoreBackupState(widget.engine);
                      widget.onNotify(ok
                          ? 'Memoria anterior restaurada'
                          : 'No se pudo restaurar');
                    },
                    icon: const Icon(CupertinoIcons.arrow_counterclockwise,
                        size: 16),
                    label: const Text('Restaurar memoria anterior'),
                  ),
                ],
                Divider(height: 16, thickness: t.borderWidth, color: t.border),
                Row(
                  mainAxisAlignment: MainAxisAlignment.spaceBetween,
                  children: [
                    TextButton.icon(
                      onPressed: rec.steps.isEmpty
                          ? null
                          : () async {
                              await Clipboard.setData(
                                  ClipboardData(text: kl.formatStepsAsText()));
                              widget.onNotify('Secuencia copiada');
                            },
                      icon: const Icon(CupertinoIcons.doc_on_doc, size: 16),
                      label: const Text('Copiar', style: TextStyle(fontSize: 12.5)),
                    ),
                    TextButton.icon(
                      onPressed: rec.steps.isEmpty ? null : _saveKeyLogFile,
                      icon: const Icon(CupertinoIcons.floppy_disk, size: 16),
                      label:
                          const Text('Guardar', style: TextStyle(fontSize: 12.5)),
                    ),
                    TextButton.icon(
                      onPressed: _openKeyLogFile,
                      icon: const Icon(CupertinoIcons.folder_open, size: 16),
                      label: const Text('Abrir', style: TextStyle(fontSize: 12.5)),
                    ),
                    TextButton.icon(
                      onPressed: rec.steps.isEmpty ? null : kl.clearActiveSteps,
                      icon: Icon(CupertinoIcons.trash,
                          size: 16, color: t.destructive),
                      label: Text('Limpiar',
                          style: TextStyle(
                              fontSize: 12.5, color: t.destructive)),
                    ),
                  ],
                ),
              ],
            ),
          ),
        );
      },
    );
  }
}
