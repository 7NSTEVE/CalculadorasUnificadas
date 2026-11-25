import XCTest

final class BibliotecaInicioUITests: XCTestCase {
    override func setUpWithError() throws {
        continueAfterFailure = false
    }

    func testArranqueCrearAbrirYVolverALaBiblioteca() throws {
        let app = XCUIApplication()
        let tituloCuaderno = "Cuaderno UI \(UUID().uuidString.prefix(8))"
        app.launchArguments.append("--pruebas-ui")
        app.launch()

        let biblioteca = app.collectionViews["biblioteca.cuadernos"]
        XCTAssertTrue(biblioteca.waitForExistence(timeout: 12),
                      "La biblioteca no apareció al iniciar la app.")
        adjuntarCaptura(nombre: "01-biblioteca-inicial")

        let crear = app.buttons["Crear cuaderno nuevo"]
        XCTAssertTrue(crear.waitForExistence(timeout: 4),
                      "La biblioteca apareció sin una acción para crear cuadernos.")
        crear.tap()

        let alerta = app.alerts["Nuevo cuaderno"]
        XCTAssertTrue(alerta.waitForExistence(timeout: 4))
        let nombre = alerta.textFields["Nombre del cuaderno"]
        XCTAssertTrue(nombre.exists)
        nombre.tap()
        nombre.typeText(tituloCuaderno)
        alerta.buttons["Crear"].tap()

        let nuevaPestana = app.buttons["editor.pestana.nueva"]
        XCTAssertTrue(nuevaPestana.waitForExistence(timeout: 12),
                      "Crear un cuaderno no abrió un editor utilizable.")
        XCTAssertTrue(nuevaPestana.isHittable,
                      "La acción de nueva pestaña existe, pero no se puede tocar.")
        let canvas = app.otherElements["editor.canvas.contenedor"]
        XCTAssertTrue(canvas.waitForExistence(timeout: 8),
                      "La shell apareció, pero no existe el contenedor real del lienzo.")
        XCTAssertGreaterThan(canvas.frame.width, 100)
        XCTAssertGreaterThan(canvas.frame.height, 100)
        adjuntarCaptura(nombre: "02-editor-cuaderno-creado")

        let volver = app.buttons["editor.volver.biblioteca"]
        XCTAssertTrue(volver.waitForExistence(timeout: 4),
                      "El editor no ofreció volver a la biblioteca.")
        volver.tap()

        XCTAssertTrue(biblioteca.waitForExistence(timeout: 8))
        let creadoEnBiblioteca = app.cells.matching(
            NSPredicate(format: "label CONTAINS %@", tituloCuaderno))
            .firstMatch
        XCTAssertTrue(creadoEnBiblioteca.waitForExistence(timeout: 4),
            "El cuaderno creado no quedó visible en la biblioteca.")
        adjuntarCaptura(nombre: "03-biblioteca-con-cuaderno")
    }

    private func adjuntarCaptura(nombre: String) {
        let captura = XCTAttachment(screenshot: XCUIScreen.main.screenshot())
        captura.name = nombre
        captura.lifetime = .keepAlways
        add(captura)
    }
}
