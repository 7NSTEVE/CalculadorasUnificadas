import UIKit

public enum ToolType {
    case pen
    case eraser
    case highlighter
    case lasso
    case shapes
}

public class BarraHerramientas: UIView {
    public var onToolSelected: ((ToolType) -> Void)?
    
    private let stackView: UIStackView = {
        let stack = UIStackView()
        stack.axis = .horizontal
        stack.distribution = .equalSpacing
        stack.alignment = .center
        stack.spacing = 16
        stack.translatesAutoresizingMaskIntoConstraints = false
        return stack
    }()
    
    public override init(frame: CGRect) {
        super.init(frame: frame)
        setupView()
    }
    
    public required init?(coder: NSCoder) {
        super.init(coder: coder)
        setupView()
    }
    
    private func setupView() {
        // Blur background
        let blurEffect = UIBlurEffect(style: .systemMaterial)
        let blurView = UIVisualEffectView(effect: blurEffect)
        blurView.translatesAutoresizingMaskIntoConstraints = false
        blurView.layer.cornerRadius = 28
        blurView.layer.masksToBounds = true
        
        addSubview(blurView)
        
        // Shadow for the floating effect
        layer.shadowColor = UIColor.black.cgColor
        layer.shadowOpacity = 0.15
        layer.shadowOffset = CGSize(width: 0, height: 8)
        layer.shadowRadius = 16
        
        addSubview(stackView)
        
        NSLayoutConstraint.activate([
            blurView.topAnchor.constraint(equalTo: topAnchor),
            blurView.bottomAnchor.constraint(equalTo: bottomAnchor),
            blurView.leadingAnchor.constraint(equalTo: leadingAnchor),
            blurView.trailingAnchor.constraint(equalTo: trailingAnchor),
            
            stackView.topAnchor.constraint(equalTo: topAnchor, constant: 12),
            stackView.bottomAnchor.constraint(equalTo: bottomAnchor, constant: -12),
            stackView.leadingAnchor.constraint(equalTo: leadingAnchor, constant: 24),
            stackView.trailingAnchor.constraint(equalTo: trailingAnchor, constant: -24)
        ])
        
        setupButtons()
    }
    
    private func setupButtons() {
        let tools: [(ToolType, String)] = [
            (.pen, "pencil.tip"),
            (.eraser, "eraser"),
            (.highlighter, "highlighter"),
            (.lasso, "lasso"),
            (.shapes, "square.on.circle")
        ]
        
        for tool in tools {
            let button = createButton(for: tool.0, iconName: tool.1)
            stackView.addArrangedSubview(button)
        }
    }
    
    private func createButton(for type: ToolType, iconName: String) -> UIButton {
        var config = UIButton.Configuration.plain()
        let symbolConfig = UIImage.SymbolConfiguration(pointSize: 22, weight: .medium)
        config.image = UIImage(systemName: iconName, withConfiguration: symbolConfig)
        config.baseForegroundColor = .label
        config.contentInsets = NSDirectionalEdgeInsets(top: 8, leading: 8, bottom: 8, trailing: 8)
        
        let button = UIButton(type: .system)
        button.configuration = config
        button.addAction(UIAction { [weak self] _ in
            self?.onToolSelected?(type)
        }, for: .touchUpInside)
        
        return button
    }
}
