import UIKit

/// Protocolo para delegar las acciones de los gestos al controlador
protocol GestosLienzoDelegate: AnyObject {
    func didStartDrawing(at point: CGPoint)
    func didContinueDrawing(to point: CGPoint)
    func didFinishDrawing(at point: CGPoint)
    
    func didPanLienzo(translation: CGPoint, state: UIGestureRecognizer.State)
    func didZoomLienzo(scale: CGFloat, state: UIGestureRecognizer.State)
}

/// Clase encargada de configurar y manejar los gestos sobre el lienzo
class GestosLienzo: NSObject {
    private weak var view: UIView?
    weak var delegate: GestosLienzoDelegate?
    
    private var drawGesture: UIPanGestureRecognizer!
    private var panGesture: UIPanGestureRecognizer!
    private var pinchGesture: UIPinchGestureRecognizer!
    
    init(view: UIView) {
        self.view = view
        super.init()
        setupGestures()
    }
    
    private func setupGestures() {
        guard let view = view else { return }
        
        // Gesto para dibujar (exclusivo para lápiz del iPad)
        drawGesture = UIPanGestureRecognizer(target: self, action: #selector(handleDraw(_:)))
        drawGesture.allowedTouchTypes = [NSNumber(value: UITouch.TouchType.pencil.rawValue)]
        drawGesture.maximumNumberOfTouches = 1
        view.addGestureRecognizer(drawGesture)
        
        // Gesto para paneo (desplazamiento del lienzo con el dedo)
        panGesture = UIPanGestureRecognizer(target: self, action: #selector(handlePan(_:)))
        panGesture.allowedTouchTypes = [NSNumber(value: UITouch.TouchType.direct.rawValue)]
        panGesture.minimumNumberOfTouches = 1
        panGesture.delegate = self
        view.addGestureRecognizer(panGesture)
        
        // Gesto para zoom (pellizco con el dedo)
        pinchGesture = UIPinchGestureRecognizer(target: self, action: #selector(handlePinch(_:)))
        pinchGesture.allowedTouchTypes = [NSNumber(value: UITouch.TouchType.direct.rawValue)]
        pinchGesture.delegate = self
        view.addGestureRecognizer(pinchGesture)
    }
    
    @objc private func handleDraw(_ gesture: UIPanGestureRecognizer) {
        guard let view = view else { return }
        let location = gesture.location(in: view)
        
        switch gesture.state {
        case .began:
            delegate?.didStartDrawing(at: location)
        case .changed:
            delegate?.didContinueDrawing(to: location)
        case .ended, .cancelled, .failed:
            delegate?.didFinishDrawing(at: location)
        default:
            break
        }
    }
    
    @objc private func handlePan(_ gesture: UIPanGestureRecognizer) {
        guard let view = view else { return }
        let translation = gesture.translation(in: view)
        
        delegate?.didPanLienzo(translation: translation, state: gesture.state)
        
        // Reiniciar la traslación para reportar solo el incremento
        if gesture.state == .changed {
            gesture.setTranslation(.zero, in: view)
        }
    }
    
    @objc private func handlePinch(_ gesture: UIPinchGestureRecognizer) {
        delegate?.didZoomLienzo(scale: gesture.scale, state: gesture.state)
        
        // Reiniciar la escala para reportar solo el incremento
        if gesture.state == .changed {
            gesture.scale = 1.0
        }
    }
}

// MARK: - UIGestureRecognizerDelegate
extension GestosLienzo: UIGestureRecognizerDelegate {
    func gestureRecognizer(_ gestureRecognizer: UIGestureRecognizer, shouldRecognizeSimultaneouslyWith otherGestureRecognizer: UIGestureRecognizer) -> Bool {
        // Permitir que el paneo y el zoom funcionen simultáneamente
        if (gestureRecognizer == panGesture && otherGestureRecognizer == pinchGesture) ||
           (gestureRecognizer == pinchGesture && otherGestureRecognizer == panGesture) {
            return true
        }
        return false
    }
}
