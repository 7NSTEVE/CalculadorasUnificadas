import UIKit

public class HerramientasCircuito: UIView {
    
    // MARK: - Callbacks
    public var onTheveninPressed: (() -> Void)?
    public var onSuperposicionPressed: (() -> Void)?
    public var onACSweepPressed: (() -> Void)?
    public var onCalcularPotenciaPressed: (() -> Void)?
    
    // MARK: - UI Components
    private let blurEffectView: UIVisualEffectView = {
        let blurEffect = UIBlurEffect(style: .systemMaterial)
        let view = UIVisualEffectView(effect: blurEffect)
        view.translatesAutoresizingMaskIntoConstraints = false
        view.layer.cornerRadius = 12
        view.clipsToBounds = true
        return view
    }()
    
    private let stackView: UIStackView = {
        let stack = UIStackView()
        stack.axis = .vertical
        stack.spacing = 10
        stack.distribution = .fillEqually
        stack.translatesAutoresizingMaskIntoConstraints = false
        return stack
    }()
    
    // MARK: - Initialization
    override public init(frame: CGRect) {
        super.init(frame: frame)
        setupView()
    }
    
    required public init?(coder: NSCoder) {
        super.init(coder: coder)
        setupView()
    }
    
    // MARK: - Setup
    private func setupView() {
        backgroundColor = .clear
        
        // Add Blur Effect
        addSubview(blurEffectView)
        
        // Add StackView to Blur Effect
        blurEffectView.contentView.addSubview(stackView)
        
        // Constraints
        NSLayoutConstraint.activate([
            blurEffectView.topAnchor.constraint(equalTo: topAnchor),
            blurEffectView.bottomAnchor.constraint(equalTo: bottomAnchor),
            blurEffectView.leadingAnchor.constraint(equalTo: leadingAnchor),
            blurEffectView.trailingAnchor.constraint(equalTo: trailingAnchor),
            
            stackView.topAnchor.constraint(equalTo: blurEffectView.contentView.topAnchor, constant: 16),
            stackView.bottomAnchor.constraint(equalTo: blurEffectView.contentView.bottomAnchor, constant: -16),
            stackView.leadingAnchor.constraint(equalTo: blurEffectView.contentView.leadingAnchor, constant: 16),
            stackView.trailingAnchor.constraint(equalTo: blurEffectView.contentView.trailingAnchor, constant: -16)
        ])
        
        setupButtons()
    }
    
    private func setupButtons() {
        let theveninButton = createButton(title: "Thévenin", action: #selector(theveninTapped))
        let superposicionButton = createButton(title: "Superposición", action: #selector(superposicionTapped))
        let acSweepButton = createButton(title: "AC Sweep", action: #selector(acSweepTapped))
        let calcularPotenciaButton = createButton(title: "Calcular Potencia", action: #selector(calcularPotenciaTapped))
        
        stackView.addArrangedSubview(theveninButton)
        stackView.addArrangedSubview(superposicionButton)
        stackView.addArrangedSubview(acSweepButton)
        stackView.addArrangedSubview(calcularPotenciaButton)
    }
    
    private func createButton(title: String, action: Selector) -> UIButton {
        let button = UIButton(type: .system)
        button.setTitle(title, for: .normal)
        button.backgroundColor = UIColor.systemBlue.withAlphaComponent(0.8)
        button.setTitleColor(.white, for: .normal)
        button.titleLabel?.font = UIFont.systemFont(ofSize: 16, weight: .semibold)
        button.layer.cornerRadius = 8
        button.addTarget(self, action: action, for: .touchUpInside)
        return button
    }
    
    // MARK: - Actions
    @objc private func theveninTapped() {
        onTheveninPressed?()
    }
    
    @objc private func superposicionTapped() {
        onSuperposicionPressed?()
    }
    
    @objc private func acSweepTapped() {
        onACSweepPressed?()
    }
    
    @objc private func calcularPotenciaTapped() {
        onCalcularPotenciaPressed?()
    }
}
