#include "unidades.h"
#include <vector>
#include <cctype>
#include <unordered_map>

namespace tinta {

static std::unordered_map<std::string, Dimension> unidades_conocidas = {
    {"V", {2, 1, -3, -1}},    // Voltaje: L=2, M=1, T=-3, I=-1
    {"I", {0, 0, 0, 1}},      // Corriente: I=1
    {"A", {0, 0, 0, 1}},      // Amperio: I=1
    {"R", {2, 1, -3, -2}},    // Resistencia: L=2, M=1, T=-3, I=-2
    {"Ohm", {2, 1, -3, -2}},  // Ohmio: L=2, M=1, T=-3, I=-2
    {"W", {2, 1, -3, 0}},     // Vatio/Potencia: L=2, M=1, T=-3, I=0
    {"P", {2, 1, -3, 0}}      // Potencia
};

struct Token {
    enum Tipo { Variable, Multiplicacion, Division, Igual, Fin };
    Tipo tipo;
    std::string valor;
};

static std::vector<Token> Lexer(const std::string& expr) {
    std::vector<Token> tokens;
    size_t i = 0;
    while (i < expr.size()) {
        char c = expr[i];
        if (std::isspace(c)) {
            i++;
            continue;
        }
        if (c == '=') {
            tokens.push_back({Token::Igual, "="});
            i++;
        } else if (c == '*') {
            tokens.push_back({Token::Multiplicacion, "*"});
            i++;
        } else if (c == '/') {
            tokens.push_back({Token::Division, "/"});
            i++;
        } else if (std::isalpha(c)) {
            std::string var;
            while (i < expr.size() && std::isalpha(expr[i])) {
                var += expr[i];
                i++;
            }
            tokens.push_back({Token::Variable, var});
        } else {
            // Ignoramos caracteres desconocidos, se podría retornar error
            i++;
        }
    }
    tokens.push_back({Token::Fin, ""});
    return tokens;
}

static bool EvaluarLado(const std::vector<Token>& tokens, size_t inicio, size_t fin, Dimension& resultado, std::string& error) {
    if (inicio >= fin) {
        error = "Expresión vacía";
        return false;
    }

    if (tokens[inicio].tipo != Token::Variable) {
        error = "Se esperaba una variable";
        return false;
    }

    std::string var_inicial = tokens[inicio].valor;
    if (unidades_conocidas.find(var_inicial) == unidades_conocidas.end()) {
        error = "Unidad desconocida: " + var_inicial;
        return false;
    }

    resultado = unidades_conocidas.at(var_inicial);

    size_t i = inicio + 1;
    while (i < fin) {
        if (tokens[i].tipo == Token::Multiplicacion || tokens[i].tipo == Token::Division) {
            Token::Tipo op = tokens[i].tipo;
            i++;
            if (i >= fin || tokens[i].tipo != Token::Variable) {
                error = "Se esperaba una variable después del operador";
                return false;
            }
            std::string var_der = tokens[i].valor;
            if (unidades_conocidas.find(var_der) == unidades_conocidas.end()) {
                error = "Unidad desconocida: " + var_der;
                return false;
            }

            if (op == Token::Multiplicacion) {
                resultado = resultado * unidades_conocidas.at(var_der);
            } else {
                resultado = resultado / unidades_conocidas.at(var_der);
            }
            i++;
        } else {
            error = "Se esperaba un operador (* o /)";
            return false;
        }
    }
    return true;
}

ResultadoDimension AnalizarDimensiones(const std::string& expresion) {
    ResultadoDimension res;
    res.correcto = false;

    std::vector<Token> tokens = Lexer(expresion);
    
    size_t pos_igual = 0;
    bool encontrado_igual = false;
    for (size_t i = 0; i < tokens.size(); i++) {
        if (tokens[i].tipo == Token::Igual) {
            pos_igual = i;
            encontrado_igual = true;
            break;
        }
    }

    if (!encontrado_igual) {
        res.error = "Falta el signo '=' en la expresión";
        return res;
    }

    Dimension dim_izq;
    std::string err_izq;
    if (!EvaluarLado(tokens, 0, pos_igual, dim_izq, err_izq)) {
        res.error = "Lado izquierdo: " + err_izq;
        return res;
    }

    Dimension dim_der;
    std::string err_der;
    // tokens.size() - 1 because the last token is Fin
    if (!EvaluarLado(tokens, pos_igual + 1, tokens.size() - 1, dim_der, err_der)) {
        res.error = "Lado derecho: " + err_der;
        return res;
    }

    res.dimension_izq = dim_izq;
    res.dimension_der = dim_der;
    
    if (dim_izq == dim_der) {
        res.correcto = true;
    } else {
        res.correcto = false;
        res.error = "Las dimensiones no coinciden";
    }

    return res;
}

} // namespace tinta
