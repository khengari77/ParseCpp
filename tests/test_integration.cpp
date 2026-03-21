#include <catch2/catch_test_macros.hpp>
#include <rapidcheck.h>
#include <parsecpp/parsecpp.hpp>

#include <variant>
#include <vector>
#include <map>

using namespace parsecpp;

// ============================================================
// JSON Parser
// ============================================================

namespace json {

using JsonValue = std::variant<
    std::nullptr_t,
    bool,
    double,
    std::string,
    std::vector<std::variant<std::nullptr_t, bool, double, std::string>>,  // simplified array
    int  // placeholder for nested structures
>;

// Simplified JSON value parser (no deep nesting for test simplicity)
Parser<JsonValue> json_value() {
    auto ws = spaces();

    auto null_val = string_("null").map([](auto) -> JsonValue { return nullptr; });
    auto true_val = string_("true").map([](auto) -> JsonValue { return true; });
    auto false_val = string_("false").map([](auto) -> JsonValue { return false; });

    // Number: simplified
    auto number = take_while1([](char c) {
        return std::isdigit(static_cast<unsigned char>(c)) || c == '.' || c == '-' || c == '+' || c == 'e' || c == 'E';
    }).map([](std::string s) -> JsonValue {
        return std::stod(s);
    });

    // String
    auto escape_char = char_('\\') > choice<char>({
        char_('"'),
        char_('\\'),
        char_('/'),
        char_('n').map([](char) -> char { return '\n'; }),
        char_('t').map([](char) -> char { return '\t'; }),
        char_('r').map([](char) -> char { return '\r'; }),
    });
    auto string_char = satisfy([](char c) { return c != '"' && c != '\\'; }) | escape_char;
    auto json_string = between(char_('"'), char_('"'),
        many(string_char).map([](std::vector<char> cs) -> JsonValue {
            return std::string(cs.begin(), cs.end());
        })
    );

    return ws > choice<JsonValue>({null_val, true_val, false_val, number, json_string});
}

} // namespace json

TEST_CASE("JSON null", "[integration]") {
    auto result = run_parser(json::json_value(), "null");
    REQUIRE(result.has_value());
    REQUIRE(std::holds_alternative<std::nullptr_t>(*result));
}

TEST_CASE("JSON bool", "[integration]") {
    REQUIRE(std::get<bool>(*run_parser(json::json_value(), "true")) == true);
    REQUIRE(std::get<bool>(*run_parser(json::json_value(), "false")) == false);
}

TEST_CASE("JSON number", "[integration]") {
    auto result = run_parser(json::json_value(), "42.5");
    REQUIRE(result.has_value());
    REQUIRE(std::get<double>(*result) == 42.5);
}

TEST_CASE("JSON string", "[integration]") {
    auto result = run_parser(json::json_value(), "\"hello world\"");
    REQUIRE(result.has_value());
    REQUIRE(std::get<std::string>(*result) == "hello world");
}

TEST_CASE("JSON string with escapes", "[integration]") {
    auto result = run_parser(json::json_value(), "\"hello\\nworld\"");
    REQUIRE(result.has_value());
    REQUIRE(std::get<std::string>(*result) == "hello\nworld");
}

// ============================================================
// Calculator (expression parser integration)
// ============================================================

namespace calc {

Parser<double> expr();

Parser<double> number() {
    static Parser<double> p = take_while1([](char c) {
        return std::isdigit(static_cast<unsigned char>(c)) || c == '.';
    }).map([](std::string s) { return std::stod(s); });
    return p;
}

Parser<double> factor() {
    static Parser<double> p =
        between(char_('('), char_(')'), lazy<double>([]() { return expr(); }))
        | number();
    return p;
}

Parser<double> expr() {
    static Parser<double> p = build_expression_parser<double>(
        {
            // Higher precedence first (innermost)
            {
                Infix<double>{
                    char_('*').map([](char) -> std::function<double(double, double)> {
                        return [](double a, double b) { return a * b; };
                    }),
                    Assoc::Left
                },
                Infix<double>{
                    char_('/').map([](char) -> std::function<double(double, double)> {
                        return [](double a, double b) { return b != 0 ? a / b : 0; };
                    }),
                    Assoc::Left
                },
            },
            // Lower precedence last (outermost)
            {
                Infix<double>{
                    char_('+').map([](char) -> std::function<double(double, double)> {
                        return [](double a, double b) { return a + b; };
                    }),
                    Assoc::Left
                },
                Infix<double>{
                    char_('-').map([](char) -> std::function<double(double, double)> {
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

} // namespace calc

TEST_CASE("Calculator basic arithmetic", "[integration]") {
    REQUIRE(*run_parser(calc::expr(), "1+2") == 3.0);
    REQUIRE(*run_parser(calc::expr(), "10-3") == 7.0);
    REQUIRE(*run_parser(calc::expr(), "4*5") == 20.0);
    REQUIRE(*run_parser(calc::expr(), "10/2") == 5.0);
}

TEST_CASE("Calculator precedence", "[integration]") {
    REQUIRE(*run_parser(calc::expr(), "1+2*3") == 7.0);
    REQUIRE(*run_parser(calc::expr(), "2*3+4") == 10.0);
}

TEST_CASE("Calculator parentheses", "[integration]") {
    REQUIRE(*run_parser(calc::expr(), "(1+2)*3") == 9.0);
    REQUIRE(*run_parser(calc::expr(), "2*(3+4)") == 14.0);
}

TEST_CASE("Calculator nested parentheses", "[integration]") {
    REQUIRE(*run_parser(calc::expr(), "((1+2))") == 3.0);
}

// ============================================================
// CSV Parser
// ============================================================

namespace csv {

Parser<std::string> field() {
    return take_while([](char c) { return c != ',' && c != '\n'; });
}

Parser<std::vector<std::string>> row() {
    return sep_by(field(), char_(','));
}

Parser<std::vector<std::vector<std::string>>> csv_file() {
    return sep_by(row(), char_('\n'));
}

} // namespace csv

TEST_CASE("CSV single row", "[integration]") {
    auto result = run_parser(csv::row(), "a,b,c");
    REQUIRE(result.has_value());
    REQUIRE(result->size() == 3);
    REQUIRE((*result)[0] == "a");
    REQUIRE((*result)[1] == "b");
    REQUIRE((*result)[2] == "c");
}

TEST_CASE("CSV multiple rows", "[integration]") {
    auto result = run_parser(csv::csv_file(), "a,b\nc,d");
    REQUIRE(result.has_value());
    REQUIRE(result->size() == 2);
    REQUIRE((*result)[0].size() == 2);
    REQUIRE((*result)[1].size() == 2);
}

// ============================================================
// Monad law property tests
// ============================================================

TEST_CASE("Monad laws", "[integration][property]") {
    rc::check("Left identity: pure(x) >>= f == f(x)",
        [](int x) {
            auto f = [](int n) { return pure<int>(n * 2); };
            auto left = pure<int>(x).bind(f);
            auto right = f(x);
            auto result_l = run_parser(left, "");
            auto result_r = run_parser(right, "");
            RC_ASSERT(result_l.has_value() == result_r.has_value());
            if (result_l.has_value()) {
                RC_ASSERT(*result_l == *result_r);
            }
        });

    rc::check("Right identity: p >>= pure == p",
        [](int x) {
            auto p = pure<int>(x);
            auto bound = p.bind([](int v) { return pure<int>(v); });
            auto result_p = run_parser(p, "");
            auto result_b = run_parser(bound, "");
            RC_ASSERT(result_p.has_value() == result_b.has_value());
            if (result_p.has_value()) {
                RC_ASSERT(*result_p == *result_b);
            }
        });

    rc::check("Associativity: (p >>= f) >>= g == p >>= (x -> f(x) >>= g)",
        [](int x) {
            auto p = pure<int>(x);
            auto f = [](int n) { return pure<int>(n + 1); };
            auto g = [](int n) { return pure<int>(n * 2); };

            auto left = p.bind(f).bind(g);
            auto right = p.bind([f, g](int v) { return f(v).bind(g); });

            auto result_l = run_parser(left, "");
            auto result_r = run_parser(right, "");
            RC_ASSERT(result_l.has_value() == result_r.has_value());
            if (result_l.has_value()) {
                RC_ASSERT(*result_l == *result_r);
            }
        });
}
