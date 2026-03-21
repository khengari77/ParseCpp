#include <parsecpp/parsecpp.hpp>
#include <iostream>
#include <string>

using namespace parsecpp;

// Simple calculator: supports +, -, *, /, parentheses, and unary minus.

Parser<double> expr();

Parser<double> number() {
    return take_while1([](char c) {
        return std::isdigit(static_cast<unsigned char>(c)) || c == '.';
    }).map([](std::string s) { return std::stod(s); });
}

Parser<double> factor() {
    static Parser<double> p = spaces() > (
        between(char_('('), char_(')'), lazy<double>([]() { return expr(); }))
        | number()
    );
    return p;
}

Parser<double> expr() {
    static Parser<double> p = build_expression_parser<double>(
        {
            // Higher precedence (innermost): * and /
            {
                Infix<double>{
                    (spaces() > char_('*')).map([](char) -> std::function<double(double, double)> {
                        return [](double a, double b) { return a * b; };
                    }),
                    Assoc::Left
                },
                Infix<double>{
                    (spaces() > char_('/')).map([](char) -> std::function<double(double, double)> {
                        return [](double a, double b) { return b != 0 ? a / b : 0; };
                    }),
                    Assoc::Left
                },
            },
            // Lower precedence (outermost): + and -
            {
                Infix<double>{
                    (spaces() > char_('+')).map([](char) -> std::function<double(double, double)> {
                        return [](double a, double b) { return a + b; };
                    }),
                    Assoc::Left
                },
                Infix<double>{
                    (spaces() > char_('-')).map([](char) -> std::function<double(double, double)> {
                        return [](double a, double b) { return a - b; };
                    }),
                    Assoc::Left
                },
            },
        },
        factor()
    );
    return p;
}

int main() {
    std::cout << "Parsec++ Calculator\n";
    std::cout << "Enter expressions (Ctrl+D to quit):\n\n";

    std::string line;
    while (std::cout << "> " && std::getline(std::cin, line)) {
        if (line.empty()) continue;

        auto result = run_parser(expr().skip(spaces()).skip(eof()), line);
        if (result) {
            std::cout << "= " << *result << "\n";
        } else {
            std::cout << result.error() << "\n";
        }
    }
}
