import Foundation
import AVFoundation
import Combine

public class MotorAudio: NSObject, ObservableObject, AVAudioRecorderDelegate {
    
    @Published public private(set) var isRecording = false
    private var audioRecorder: AVAudioRecorder?
    private var currentRecordingURL: URL?
    
    // Almacena las marcas de tiempo temporalmente durante la grabación
    private var timestamps: [[String: Any]] = []
    
    public override init() {
        super.init()
    }
    
    /// Solicita los permisos necesarios para grabar audio.
    public func solicitarPermisos(completion: @escaping (Bool) -> Void) {
        let session = AVAudioSession.sharedInstance()
        do {
            try session.setCategory(.playAndRecord, mode: .default)
            try session.setActive(true)
            
            if #available(iOS 17.0, *) {
                AVAudioApplication.requestRecordPermission { granted in
                    DispatchQueue.main.async {
                        completion(granted)
                    }
                }
            } else {
                session.requestRecordPermission { granted in
                    DispatchQueue.main.async {
                        completion(granted)
                    }
                }
            }
        } catch {
            print("Error configurando la sesión de audio: \(error.localizedDescription)")
            DispatchQueue.main.async {
                completion(false)
            }
        }
    }
    
    /// Inicia o detiene la grabación de audio de manera alternativa.
    public func toggleRecording() {
        if isRecording {
            stopRecording()
        } else {
            solicitarPermisos { [weak self] granted in
                if granted {
                    self?.startRecording()
                } else {
                    print("Permisos de micrófono denegados")
                }
            }
        }
    }
    
    /// Inicia la grabación de un nuevo archivo de audio.
    public func startRecording() {
        let session = AVAudioSession.sharedInstance()
        do {
            try session.setCategory(.playAndRecord, mode: .default)
            try session.setActive(true)
            
            let documentPath = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
            let formatter = DateFormatter()
            formatter.dateFormat = "yyyy-MM-dd_HH-mm-ss"
            let fileName = "Grabacion_\(formatter.string(from: Date())).m4a"
            let audioFilename = documentPath.appendingPathComponent(fileName)
            
            currentRecordingURL = audioFilename
            timestamps = [] // Reiniciar las marcas de tiempo
            
            let settings: [String: Any] = [
                AVFormatIDKey: Int(kAudioFormatMPEG4AAC),
                AVSampleRateKey: 44100.0,
                AVNumberOfChannelsKey: 2,
                AVEncoderAudioQualityKey: AVAudioQuality.high.rawValue
            ]
            
            audioRecorder = try AVAudioRecorder(url: audioFilename, settings: settings)
            audioRecorder?.delegate = self
            audioRecorder?.record()
            
            DispatchQueue.main.async {
                self.isRecording = true
            }
            print("Grabación iniciada en: \(audioFilename.path)")
            
        } catch {
            print("No se pudo iniciar la grabación: \(error.localizedDescription)")
            DispatchQueue.main.async {
                self.isRecording = false
            }
        }
    }
    
    /// Detiene la grabación actual y guarda el archivo JSON con las marcas de tiempo.
    public func stopRecording() {
        guard isRecording, let recorder = audioRecorder else { return }
        
        recorder.stop()
        audioRecorder = nil
        
        DispatchQueue.main.async {
            self.isRecording = false
        }
        
        print("Grabación detenida.")
        
        guard let fileURL = currentRecordingURL else { return }
        
        // Guardar timestamps en un JSON
        let jsonURL = fileURL.deletingPathExtension().appendingPathExtension("json")
        do {
            let data = try JSONSerialization.data(withJSONObject: timestamps, options: .prettyPrinted)
            try data.write(to: jsonURL)
            print("Marcas de tiempo guardadas en: \(jsonURL.path)")
        } catch {
            print("Error guardando el JSON de metadatos: \(error.localizedDescription)")
        }
    }
    
    /// Agrega un timestamp con una nota de texto durante la grabación.
    public func marcarTimestamp(nota: String) {
        guard isRecording, let recorder = audioRecorder else {
            print("No hay una grabación activa para marcar.")
            return
        }
        
        let currentTime = recorder.currentTime
        let marca: [String: Any] = [
            "tiempo": currentTime,
            "nota": nota
        ]
        
        timestamps.append(marca)
        print("Marca de tiempo agregada en \(currentTime)s: \(nota)")
    }
    
    // MARK: - AVAudioRecorderDelegate
    
    public func audioRecorderDidFinishRecording(_ recorder: AVAudioRecorder, successfully flag: Bool) {
        if !flag {
            print("La grabación finalizó con errores o fue interrumpida.")
            stopRecording()
        }
    }
}
