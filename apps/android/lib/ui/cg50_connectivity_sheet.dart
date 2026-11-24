import 'dart:async';
import 'dart:io';

import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:path_provider/path_provider.dart';

import '../connectivity/cg50_local_bridge_server.dart';
import '../native/casio_ffi_bindings.dart';
import '../theme/tema_interfaz.dart';

class Cg50ConnectivitySheet extends StatefulWidget {
  final CasioCg50Engine engine;
  final Cg50LocalBridgeServer bridgeServer;
  final Cg50LinkPeerClient linkClient;
  final TokensInterfaz tokens;
  final Future<Map<String, String>?> Function({String mimeType, String prefix})
      onPickFileWithMeta;
  final Future<bool> Function({
    required String suggestedName,
    required String mimeType,
    required String sourcePath,
  }) onSaveFile;
  final Future<void> Function() onExportFullMemory;
  final Future<void> Function() onImportFullMemory;
  final void Function(String message) onNotify;

  const Cg50ConnectivitySheet({
    super.key,
    required this.engine,
    required this.bridgeServer,
    required this.linkClient,
    required this.tokens,
    required this.onPickFileWithMeta,
    required this.onSaveFile,
    required this.onExportFullMemory,
    required this.onImportFullMemory,
    required this.onNotify,
  });

  @override
  State<Cg50ConnectivitySheet> createState() => _Cg50ConnectivitySheetState();
}

class _Cg50ConnectivitySheetState extends State<Cg50ConnectivitySheet> {
  int _tab = 0; // 0 = Archivos, 1 = USB / PC, 2 = Enlace
  String _currentPath = '/';
  Cg50SmemListing? _listing;
  Cg50UsbStatus? _usbStatus;
  bool _loading = true;
  bool _busy = false;
  String? _statusMsg;
  final TextEditingController _peerController =
      TextEditingController(text: '192.168.1.100:8750');

  @override
  void initState() {
    super.initState();
    unawaited(_refreshAll());
  }

  @override
  void dispose() {
    _peerController.dispose();
    super.dispose();
  }

  Future<void> _refreshAll() async {
    if (!mounted) return;
    setState(() => _loading = true);
    final listing = await widget.engine.listSmem(_currentPath);
    final usb = await widget.engine.getUsbStatus();
    if (!mounted) return;
    setState(() {
      _listing = listing;
      _usbStatus = usb;
      _loading = false;
    });
  }

  String _formatBytes(int bytes) {
    if (bytes < 1024) return '$bytes B';
    if (bytes < 1024 * 1024) return '${(bytes / 1024).toStringAsFixed(1)} KB';
    return '${(bytes / (1024 * 1024)).toStringAsFixed(2)} MB';
  }

  String _joinRemote(String dir, String name) {
    if (dir == '/' || dir.isEmpty) return '/$name';
    return '$dir/$name';
  }

  Future<void> _importSingleFile() async {
    if (_busy) return;
    setState(() => _busy = true);
    try {
      final picked = await widget.onPickFileWithMeta(
        mimeType: '*/*',
        prefix: 'smem_in',
      );
      if (picked == null) return;
      final localPath = picked['path'] ?? '';
      final origName = picked['name'] ?? 'archivo.bin';
      if (localPath.isEmpty) return;
      final file = File(localPath);
      if (!await file.exists()) return;
      final bytes = await file.readAsBytes();
      await file.delete();

      final remotePath = _joinRemote(_currentPath, origName);
      final ok = await widget.engine.writeSmemFile(
        remotePath,
        bytes,
        overwrite: true,
        remountOs: true,
      );
      if (!mounted) return;
      setState(() {
        _statusMsg = ok
            ? 'Importado "$origName" y aplicado en Casio OS'
            : 'No se pudo escribir "$origName" en SMEM';
      });
      if (ok) {
        widget.onNotify('Archivo importado: $origName');
        await _refreshAll();
      }
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  Future<void> _exportEntry(Cg50SmemEntry entry) async {
    if (_busy || entry.isDir) return;
    setState(() => _busy = true);
    try {
      final bytes = await widget.engine.readSmemFile(entry.path);
      if (bytes == null) {
        if (mounted) {
          setState(() => _statusMsg = 'No se pudo leer ${entry.name}');
        }
        return;
      }
      final tmpDir = await getTemporaryDirectory();
      final tmpFile = File('${tmpDir.path}/export_${entry.name}');
      await tmpFile.writeAsBytes(bytes, flush: true);
      try {
        final saved = await widget.onSaveFile(
          suggestedName: entry.name,
          mimeType: 'application/octet-stream',
          sourcePath: tmpFile.path,
        );
        if (mounted && saved) {
          setState(() => _statusMsg = 'Exportado ${entry.name}');
          widget.onNotify('Exportado: ${entry.name}');
        }
      } finally {
        if (await tmpFile.exists()) await tmpFile.delete();
      }
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  Future<void> _deleteEntry(Cg50SmemEntry entry) async {
    if (_busy) return;
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        backgroundColor: widget.tokens.surface,
        title: Text(
          'Eliminar ${entry.isDir ? "carpeta" : "archivo"}',
          style: TextStyle(color: widget.tokens.textPrimary, fontSize: 17),
        ),
        content: Text(
          '¿Eliminar "${entry.name}" de la memoria de almacenamiento?',
          style: TextStyle(color: widget.tokens.textSecondary, fontSize: 14),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(ctx).pop(false),
            child: const Text('Cancelar'),
          ),
          TextButton(
            onPressed: () => Navigator.of(ctx).pop(true),
            child: Text('Eliminar',
                style: TextStyle(color: widget.tokens.destructive)),
          ),
        ],
      ),
    );
    if (confirmed != true) return;
    setState(() => _busy = true);
    try {
      final ok = await widget.engine.deleteSmemPath(
        entry.path,
        recursive: entry.isDir,
        remountOs: true,
      );
      if (!mounted) return;
      setState(() {
        _statusMsg = ok
            ? 'Eliminado: ${entry.name}'
            : 'No se pudo eliminar ${entry.name}';
      });
      await _refreshAll();
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  Future<void> _createFolder() async {
    final ctrl = TextEditingController();
    final name = await showDialog<String>(
      context: context,
      builder: (ctx) => AlertDialog(
        backgroundColor: widget.tokens.surface,
        title: Text(
          'Nueva carpeta',
          style: TextStyle(color: widget.tokens.textPrimary, fontSize: 17),
        ),
        content: TextField(
          controller: ctrl,
          autofocus: true,
          decoration: const InputDecoration(
            hintText: 'Nombre de la carpeta',
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
            child: const Text('Crear'),
          ),
        ],
      ),
    );
    ctrl.dispose();
    if (name == null || name.isEmpty) return;
    final ok = await widget.engine.mkdirSmem(
      _joinRemote(_currentPath, name),
      remountOs: false,
    );
    if (!mounted) return;
    setState(() {
      _statusMsg = ok ? 'Carpeta "$name" creada' : 'Nombre de carpeta no válido';
    });
    await _refreshAll();
  }

  Future<void> _copyText(String text, String label) async {
    await Clipboard.setData(ClipboardData(text: text));
    if (!mounted) return;
    setState(() => _statusMsg = 'Copiado: $label');
  }

  Widget _buildFilesTab(TokensInterfaz t) {
    final listing = _listing;
    final canUp = _currentPath != '/';

    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        if (listing != null) ...[
          Row(
            children: [
              Expanded(
                child: Text(
                  'Libre: ${_formatBytes(listing.freeBytes)} de ${_formatBytes(listing.totalBytes)}',
                  style: TextStyle(color: t.textSecondary, fontSize: 12.5),
                ),
              ),
              Text(
                _currentPath,
                style: TextStyle(
                  color: t.textPrimary,
                  fontSize: 12.5,
                  fontWeight: FontWeight.w600,
                ),
              ),
            ],
          ),
          const SizedBox(height: 6),
          ClipRRect(
            borderRadius: BorderRadius.circular(4),
            child: LinearProgressIndicator(
              value: listing.totalBytes > 0
                  ? (listing.usedBytes / listing.totalBytes).clamp(0.0, 1.0)
                  : 0.0,
              minHeight: 5,
              backgroundColor: t.controlTrack,
              valueColor: AlwaysStoppedAnimation<Color>(t.accent),
            ),
          ),
          const SizedBox(height: 12),
        ],
        Row(
          children: [
            Expanded(
              child: FilledButton.icon(
                onPressed: _busy ? null : _importSingleFile,
                style: FilledButton.styleFrom(
                  backgroundColor: t.accent,
                  foregroundColor: Colors.white,
                  shape: RoundedRectangleBorder(
                      borderRadius: BorderRadius.circular(10)),
                  padding: const EdgeInsets.symmetric(vertical: 10),
                ),
                icon: const Icon(CupertinoIcons.arrow_down_doc, size: 17),
                label: const Text('Importar archivo',
                    style: TextStyle(fontSize: 13.5)),
              ),
            ),
            const SizedBox(width: 8),
            OutlinedButton.icon(
              onPressed: _busy ? null : _createFolder,
              style: OutlinedButton.styleFrom(
                foregroundColor: t.textPrimary,
                side: BorderSide(color: t.border, width: t.borderWidth),
                shape: RoundedRectangleBorder(
                    borderRadius: BorderRadius.circular(10)),
                padding:
                    const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
              ),
              icon: const Icon(CupertinoIcons.folder_badge_plus, size: 17),
              label: const Text('Carpeta', style: TextStyle(fontSize: 13.5)),
            ),
          ],
        ),
        const SizedBox(height: 10),
        Expanded(
          child: _loading
              ? Center(
                  child: CircularProgressIndicator(
                      strokeWidth: 2, color: t.accent),
                )
              : listing == null
                  ? Center(
                      child: Text(
                        'Memoria de almacenamiento no disponible',
                        style: TextStyle(color: t.textSecondary),
                      ),
                    )
                  : ListView.separated(
                      itemCount: listing.entries.length + (canUp ? 1 : 0),
                      separatorBuilder: (_, _) => Divider(
                        height: 1,
                        thickness: t.borderWidth,
                        color: t.border,
                      ),
                      itemBuilder: (context, index) {
                        if (canUp && index == 0) {
                          return ListTile(
                            dense: true,
                            contentPadding:
                                const EdgeInsets.symmetric(horizontal: 4),
                            leading: Icon(CupertinoIcons.arrow_turn_up_left,
                                size: 18, color: t.accent),
                            title: Text('Subir nivel',
                                style: TextStyle(
                                    color: t.accent, fontSize: 14)),
                            onTap: () {
                              final parts = _currentPath
                                  .split('/')
                                  .where((s) => s.isNotEmpty)
                                  .toList();
                              parts.removeLast();
                              _currentPath =
                                  parts.isEmpty ? '/' : '/${parts.join('/')}';
                              unawaited(_refreshAll());
                            },
                          );
                        }
                        final entry = listing.entries[index - (canUp ? 1 : 0)];
                        return ListTile(
                          dense: true,
                          contentPadding:
                              const EdgeInsets.symmetric(horizontal: 4),
                          leading: Icon(
                            entry.isDir
                                ? CupertinoIcons.folder_fill
                                : CupertinoIcons.doc_text,
                            size: 19,
                            color: entry.isDir ? t.accent : t.textSecondary,
                          ),
                          title: Text(
                            entry.name,
                            maxLines: 1,
                            overflow: TextOverflow.ellipsis,
                            style: TextStyle(
                              color: t.textPrimary,
                              fontSize: 14,
                              fontWeight: FontWeight.w500,
                            ),
                          ),
                          subtitle: Text(
                            entry.isDir ? 'Carpeta' : _formatBytes(entry.size),
                            style: TextStyle(
                                color: t.textSecondary, fontSize: 12),
                          ),
                          trailing: Row(
                            mainAxisSize: MainAxisSize.min,
                            children: [
                              if (!entry.isDir)
                                IconButton(
                                  tooltip: 'Exportar',
                                  icon: Icon(CupertinoIcons.share,
                                      size: 18, color: t.accent),
                                  onPressed:
                                      _busy ? null : () => _exportEntry(entry),
                                ),
                              IconButton(
                                tooltip: 'Eliminar',
                                icon: Icon(CupertinoIcons.trash,
                                    size: 18, color: t.destructive),
                                onPressed:
                                    _busy ? null : () => _deleteEntry(entry),
                              ),
                            ],
                          ),
                          onTap: () {
                            if (entry.isDir) {
                              _currentPath = entry.path;
                              unawaited(_refreshAll());
                            } else {
                              unawaited(_exportEntry(entry));
                            }
                          },
                        );
                      },
                    ),
        ),
        Divider(height: 16, thickness: t.borderWidth, color: t.border),
        Row(
          children: [
            Expanded(
              child: TextButton.icon(
                onPressed: () {
                  Navigator.of(context).pop();
                  unawaited(widget.onExportFullMemory());
                },
                icon: Icon(CupertinoIcons.archivebox,
                    size: 16, color: t.textSecondary),
                label: Text('Respaldar memoria',
                    style: TextStyle(color: t.textPrimary, fontSize: 12.5)),
              ),
            ),
            Expanded(
              child: TextButton.icon(
                onPressed: () {
                  Navigator.of(context).pop();
                  unawaited(widget.onImportFullMemory());
                },
                icon: Icon(CupertinoIcons.arrow_counterclockwise,
                    size: 16, color: t.textSecondary),
                label: Text('Restaurar memoria',
                    style: TextStyle(color: t.textPrimary, fontSize: 12.5)),
              ),
            ),
          ],
        ),
      ],
    );
  }

  Widget _buildUsbPcTab(TokensInterfaz t) {
    return AnimatedBuilder(
      animation: widget.bridgeServer,
      builder: (context, _) {
        final srv = widget.bridgeServer;
        final usb = _usbStatus;
        final primaryIp = srv.addresses.isNotEmpty
            ? srv.addresses.firstWhere((a) => a != '127.0.0.1',
                orElse: () => '127.0.0.1')
            : '127.0.0.1';
        final webUrl = 'http://$primaryIp:${srv.port}';
        const adbCmd = 'adb forward tcp:8750 tcp:8750';

        return ListView(
          children: [
            SwitchListTile.adaptive(
              contentPadding: EdgeInsets.zero,
              activeTrackColor: t.accent,
              title: Text(
                'Puente directo con PC',
                style: TextStyle(
                    color: t.textPrimary,
                    fontSize: 15,
                    fontWeight: FontWeight.w600),
              ),
              subtitle: Text(
                srv.isRunning
                    ? 'Activo en puerto ${srv.port} · Acceso directo sin clave'
                    : 'Apagado · Enciéndelo para abrir archivos y pantalla en la PC',
                style: TextStyle(color: t.textSecondary, fontSize: 12.5),
              ),
              value: srv.isRunning,
              onChanged: (v) async {
                if (v) {
                  await srv.start();
                } else {
                  await srv.stop();
                }
              },
            ),
            if (srv.isRunning) ...[
              const SizedBox(height: 8),
              Container(
                padding: const EdgeInsets.all(12),
                decoration: BoxDecoration(
                  color: t.canvas,
                  borderRadius: BorderRadius.circular(10),
                  border: Border.all(color: t.border, width: t.borderWidth),
                ),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text('Por Wi-Fi (navegador de tu PC)',
                        style: TextStyle(
                            color: t.textSecondary,
                            fontSize: 12,
                            fontWeight: FontWeight.w600)),
                    const SizedBox(height: 4),
                    Row(
                      children: [
                        Expanded(
                          child: SelectableText(
                            webUrl,
                            style: TextStyle(
                              color: t.accent,
                              fontSize: 14,
                              fontWeight: FontWeight.w600,
                              fontFamily: 'monospace',
                            ),
                          ),
                        ),
                        IconButton(
                          tooltip: 'Copiar URL',
                          icon: Icon(CupertinoIcons.doc_on_doc,
                              size: 18, color: t.accent),
                          onPressed: () => _copyText(webUrl, webUrl),
                        ),
                      ],
                    ),
                    Divider(height: 16, thickness: t.borderWidth, color: t.border),
                    Text('Por cable USB del teléfono',
                        style: TextStyle(
                            color: t.textSecondary,
                            fontSize: 12,
                            fontWeight: FontWeight.w600)),
                    const SizedBox(height: 4),
                    Row(
                      children: [
                        Expanded(
                          child: SelectableText(
                            adbCmd,
                            style: TextStyle(
                              color: t.textPrimary,
                              fontSize: 12.5,
                              fontFamily: 'monospace',
                            ),
                          ),
                        ),
                        IconButton(
                          tooltip: 'Copiar comando',
                          icon: Icon(CupertinoIcons.doc_on_doc,
                              size: 18, color: t.textSecondary),
                          onPressed: () => _copyText(adbCmd, 'comando USB'),
                        ),
                      ],
                    ),
                    Text(
                      'Luego abre http://127.0.0.1:8750 o usa tools/cg50_pc_bridge.py --adb',
                      style: TextStyle(color: t.textSecondary, fontSize: 12),
                    ),
                  ],
                ),
              ),
            ],
            if (srv.lastError != null) ...[
              const SizedBox(height: 8),
              Text(srv.lastError!,
                  style: TextStyle(color: t.destructive, fontSize: 12.5)),
            ],
            Divider(height: 28, thickness: t.borderWidth, color: t.border),
            SwitchListTile.adaptive(
              contentPadding: EdgeInsets.zero,
              activeTrackColor: t.accent,
              title: Text(
                'Cable USB virtual',
                style: TextStyle(
                    color: t.textPrimary,
                    fontSize: 15,
                    fontWeight: FontWeight.w600),
              ),
              subtitle: Text(
                (usb?.connected ?? false)
                    ? 'Conectado · Puerto USB activo'
                    : 'Desconectado · Simula la conexión física del cable USB',
                style: TextStyle(color: t.textSecondary, fontSize: 12.5),
              ),
              value: usb?.connected ?? false,
              onChanged: (v) async {
                await widget.engine.setUsbConnected(v);
                final st = await widget.engine.getUsbStatus();
                if (mounted) {
                  setState(() {
                    _usbStatus = st;
                    _statusMsg = v
                        ? 'Cable USB virtual conectado'
                        : 'Cable USB virtual desconectado';
                  });
                }
              },
            ),
          ],
        );
      },
    );
  }

  Widget _buildLinkTab(TokensInterfaz t) {
    return AnimatedBuilder(
      animation: Listenable.merge([widget.bridgeServer, widget.linkClient]),
      builder: (context, _) {
        final srv = widget.bridgeServer;
        final cli = widget.linkClient;
        final ip = srv.addresses.isNotEmpty
            ? srv.addresses.firstWhere((a) => a != '127.0.0.1',
                orElse: () => '127.0.0.1')
            : '127.0.0.1';

        return ListView(
          children: [
            Text(
              'Conecta dos fx-CG50 en la misma red local para intercambiar datos desde la aplicación Link.',
              style: TextStyle(color: t.textSecondary, fontSize: 13),
            ),
            const SizedBox(height: 14),
            SwitchListTile.adaptive(
              contentPadding: EdgeInsets.zero,
              activeTrackColor: t.accent,
              title: Text(
                'Recibir enlace en este dispositivo',
                style: TextStyle(
                    color: t.textPrimary,
                    fontSize: 14.5,
                    fontWeight: FontWeight.w600),
              ),
              subtitle: Text(
                srv.isRunning
                    ? 'Dirección: $ip:${srv.port} · Conectados: ${srv.linkPeerCount} (TX ${srv.linkTxBytes} B · RX ${srv.linkRxBytes} B)'
                    : 'Activa el puerto local para esperar otra calculadora',
                style: TextStyle(color: t.textSecondary, fontSize: 12.5),
              ),
              value: srv.isRunning,
              onChanged: (v) async {
                if (v) {
                  await srv.start();
                } else {
                  await srv.stop();
                }
              },
            ),
            Divider(height: 24, thickness: t.borderWidth, color: t.border),
            Text(
              'Conectar a otra calculadora',
              style: TextStyle(
                  color: t.textPrimary,
                  fontSize: 14.5,
                  fontWeight: FontWeight.w600),
            ),
            const SizedBox(height: 8),
            Row(
              children: [
                Expanded(
                  child: TextField(
                    controller: _peerController,
                    enabled: !cli.isConnected,
                    style: TextStyle(color: t.textPrimary, fontSize: 14),
                    decoration: InputDecoration(
                      hintText: '192.168.1.42:8750',
                      isDense: true,
                      border: OutlineInputBorder(
                          borderRadius: BorderRadius.circular(10)),
                    ),
                  ),
                ),
                const SizedBox(width: 8),
                FilledButton(
                  style: FilledButton.styleFrom(
                    backgroundColor:
                        cli.isConnected ? t.destructive : t.accent,
                    foregroundColor: Colors.white,
                    shape: RoundedRectangleBorder(
                        borderRadius: BorderRadius.circular(10)),
                  ),
                  onPressed: () async {
                    if (cli.isConnected) {
                      await cli.disconnect();
                    } else {
                      final ok = await cli.connect(_peerController.text);
                      if (mounted) {
                        setState(() {
                          _statusMsg = ok
                              ? 'Enlace activo con ${_peerController.text}'
                              : (cli.lastError ?? 'No se pudo conectar');
                        });
                      }
                    }
                  },
                  child: Text(cli.isConnected ? 'Desconectar' : 'Conectar'),
                ),
              ],
            ),
            if (cli.isConnected) ...[
              const SizedBox(height: 8),
              Text(
                'Conectado · TX ${cli.txBytes} B · RX ${cli.rxBytes} B',
                style: TextStyle(color: t.accent, fontSize: 12.5),
              ),
            ] else if (cli.lastError != null) ...[
              const SizedBox(height: 8),
              Text(cli.lastError!,
                  style: TextStyle(color: t.destructive, fontSize: 12.5)),
            ],
          ],
        );
      },
    );
  }

  @override
  Widget build(BuildContext context) {
    final t = widget.tokens;
    return Container(
      decoration: BoxDecoration(
        color: t.surface,
        borderRadius: const BorderRadius.vertical(top: Radius.circular(20)),
        border: Border.all(color: t.border, width: t.borderWidth),
      ),
      padding: EdgeInsets.fromLTRB(
        18,
        12,
        18,
        MediaQuery.of(context).viewInsets.bottom + 16,
      ),
      child: SizedBox(
        height: MediaQuery.of(context).size.height * 0.72,
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
            const SizedBox(height: 12),
            Row(
              children: [
                Text(
                  'Conectar y archivos',
                  style: TextStyle(
                    color: t.textPrimary,
                    fontSize: 17,
                    fontWeight: FontWeight.w600,
                  ),
                ),
                const Spacer(),
                IconButton(
                  tooltip: 'Actualizar',
                  icon: Icon(CupertinoIcons.refresh,
                      size: 18, color: t.textSecondary),
                  onPressed: _refreshAll,
                ),
                IconButton(
                  tooltip: 'Cerrar',
                  icon: Icon(CupertinoIcons.xmark_circle_fill,
                      size: 21, color: t.textSecondary),
                  onPressed: () => Navigator.of(context).pop(),
                ),
              ],
            ),
            const SizedBox(height: 8),
            SelectorSegmentado<int>(
              tokens: t,
              value: _tab,
              segments: const [
                (0, 'Archivos'),
                (1, 'USB / PC'),
                (2, 'Enlace'),
              ],
              onChanged: (v) => setState(() => _tab = v),
            ),
            const SizedBox(height: 14),
            Expanded(
              child: _tab == 0
                  ? _buildFilesTab(t)
                  : _tab == 1
                      ? _buildUsbPcTab(t)
                      : _buildLinkTab(t),
            ),
            if (_statusMsg != null) ...[
              const SizedBox(height: 6),
              Text(
                _statusMsg!,
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
                style: TextStyle(color: t.textSecondary, fontSize: 12),
              ),
            ],
          ],
        ),
      ),
    );
  }
}
