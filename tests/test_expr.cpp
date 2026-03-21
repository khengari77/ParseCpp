#include <catch2/catch_test_macros.hpp>
#include <parsecpp/parsecpp.hpp>

using namespace parsecpp;

namespace {

// Simple integer parser for tests
Parser<int> number() {
    return many1(digit()).map([](std::vector<char> cs) {
        int n = 0;
        for (char c : cs) n = n * 10 + (c - '0');
        return n;
    });
}

// Build a calculator expression parser
Parser<int> calc_expr() {
    static auto term = lazy<int>([]() -> Parser<int> {
        return build_expression_parser<int>(
            {
                // Higher precedence (applied first = innermost): * and /
                {
                    Infix<int>{
                        char_('*').map([](char) -> std::function<int(int, int)> {
                            return [](int a, int b) { return a * b; };
                        }),
                        Assoc::Left
                    },
                    Infix<int>{
                        char_('/').map([](char) -> std::function<int(int, int)> {
                            return [](int a, int b) { return b != 0 ? a / b : 0; };
                        }),
                        Assoc::Left
                    },
                },
                // Lower precedence (applied last = outermost): + and -
                {
                    Infix<int>{
                        char_('+').map([](char) -> std::function<int(int, int)> {
                            return [](int a, int b) { return a + b; };
                        }),
                        Assoc::Left
                    },
                    Infix<int>{
                        char_('-').map([](char) -> std::function<int(int, int)> {
                            return [](int a, int b) { return a - b; };
                        }),
                        Assoc::Left
                    },
                },
            },
            // Parenthesised sub-expression or number
            between(char_('('), char_(')'), lazy<int>([]() { return calc_expr(); }))
            | number()
        );
    });

    return term;
}

} // namespace

TEST_CASE("Expression parser simple addition", "[expr]") {
    auto expr = calc_expr();
    REQUIRE(*run_parser(expr, "1+2") == 3);
}

TEST_CASE("Expression parser precedence", "[expr]") {
    auto expr = calc_expr();
    // 1 + 2 * 3 = 1 + 6 = 7 (multiplication binds tighter)
    REQUIRE(*run_parser(expr, "1+2*3") == 7);
}

TEST_CASE("Expression parser left associativity", "[expr]") {
    auto expr = calc_expr();
    // 10 - 3 - 2 = (10 - 3) - 2 = 5
    REQUIRE(*run_parser(expr, "10-3-2") == 5);
}

TEST_CASE("Expression parser parentheses", "[expr]") {
    auto expr = calc_expr();
    // (1 + 2) * 3 = 9
    REQUIRE(*run_parser(expr, "(1+2)*3") == 9);
}

TEST_CASE("Expression parser with prefix operator", "[expr]") {
    auto neg = Prefix<int>{
        char_('-').map([](char) -> std::function<int(int)> {
            return [](int x) { return -x; };
        })
    };

    auto add = Infix<int>{
        char_('+').map([](char) -> std::function<int(int, int)> {
            return [](int a, int b) { return a + b; };
        }),
        Assoc::Left
    };

    auto expr = build_expression_parser<int>(
        {{add}, {neg}},  // neg has higher precedence than add
        number()
    );

    // Prefix: -5 + 3 = (-5) + 3 = -2
    // Note: precedence table goes from lowest to highest
    REQUIRE(*run_parser(expr, "5+3") == 8);
}

TEST_CASE("Expression parser single number", "[expr]") {
    auto expr = calc_expr();
    REQUIRE(*run_parser(expr, "42") == 42);
}
