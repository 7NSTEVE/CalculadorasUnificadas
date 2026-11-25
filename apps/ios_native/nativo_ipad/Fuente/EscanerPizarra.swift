import SwiftUI
import VisionKit

public struct EscanerPizarra: UIViewControllerRepresentable {
    public var onScanCompleted: (([UIImage]) -> Void)?
    public var onScanCanceled: (() -> Void)?
    public var onScanFailed: ((Error) -> Void)?
    
    public init(
        onScanCompleted: (([UIImage]) -> Void)? = nil,
        onScanCanceled: (() -> Void)? = nil,
        onScanFailed: ((Error) -> Void)? = nil
    ) {
        self.onScanCompleted = onScanCompleted
        self.onScanCanceled = onScanCanceled
        self.onScanFailed = onScanFailed
    }
    
    public func makeUIViewController(context: Context) -> VNDocumentCameraViewController {
        let scannerViewController = VNDocumentCameraViewController()
        scannerViewController.delegate = context.coordinator
        return scannerViewController
    }
    
    public func updateUIViewController(_ uiViewController: VNDocumentCameraViewController, context: Context) {
        // No updates needed
    }
    
    public func makeCoordinator() -> Coordinator {
        Coordinator(self)
    }
    
    public class Coordinator: NSObject, VNDocumentCameraViewControllerDelegate {
        var parent: EscanerPizarra
        
        init(_ parent: EscanerPizarra) {
            self.parent = parent
        }
        
        public func documentCameraViewController(_ controller: VNDocumentCameraViewController, didFinishWith scan: VNDocumentCameraScan) {
            var scannedImages: [UIImage] = []
            for pageIndex in 0..<scan.pageCount {
                // imageOfPage(at:) returns the cropped and processed image
                let image = scan.imageOfPage(at: pageIndex)
                scannedImages.append(image)
            }
            parent.onScanCompleted?(scannedImages)
        }
        
        public func documentCameraViewControllerDidCancel(_ controller: VNDocumentCameraViewController) {
            parent.onScanCanceled?()
        }
        
        public func documentCameraViewController(_ controller: VNDocumentCameraViewController, didFailWithError error: Error) {
            parent.onScanFailed?(error)
        }
    }
}
