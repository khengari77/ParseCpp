#include <catch2/catch_test_macros.hpp>
#include <rapidcheck.h>
#include <parsecpp/parsecpp.hpp>

using namespace parsecpp;

TEST_CASE("choice tries alternatives", "[combinator]") {
    auto p = choice<char>({char_('a'), char_('b'), char_('c')});
    REQUIRE(*run_parser(p, "a") == 'a');
    REQUIRE(*run_parser(p, "b") == 'b');
    REQUIRE(*run_parser(p, "c") == 'c');
    REQUIRE(!run_parser(p, "x").has_value());
}

TEST_CASE("count parses exactly n times", "[combinator]") {
    auto p = count(3, char_('a'));
    REQUIRE(run_parser(p, "aaab").has_value());
    REQUIRE(run_parser(p, "aaab")->size() == 3);
    REQUIRE(!run_parser(p, "aab").has_value());
}

TEST_CASE("between parses delimited content", "[combinator]") {
    auto p = between(char_('('), char_(')'), char_('x'));
    REQUIRE(*run_parser(p, "(x)") == 'x');
    REQUIRE(!run_parser(p, "(y)").has_value());
}

TEST_CASE("option returns default on failure", "[combinator]") {
    auto p = option('z', char_('a'));
    REQUIRE(*run_parser(p, "a") == 'a');
    REQUIRE(*run_parser(p, "b") == 'z');
}

TEST_CASE("option_maybe returns nullopt on failure", "[combinator]") {
    auto p = option_maybe(char_('a'));
    REQUIRE(run_parser(p, "a").has_value());
    REQUIRE(*run_parser(p, "a") == 'a');
    auto result = run_parser(p, "b");
    REQUIRE(result.has_value());
    REQUIRE(!result->has_value());
}

TEST_CASE("sep_by parses separated items", "[combinator]") {
    auto p = sep_by(digit(), char_(','));
    auto result = run_parser(p, "1,2,3");
    REQUIRE(result.has_value());
    REQUIRE(result->size() == 3);
    REQUIRE((*result)[0] == '1');
    REQUIRE((*result)[1] == '2');
    REQUIRE((*result)[2] == '3');
}

TEST_CASE("sep_by on empty input", "[combinator]") {
    auto p = sep_by(digit(), char_(','));
    auto result = run_parser(p, "abc");
    REQUIRE(result.has_value());
    REQUIRE(result->empty());
}

TEST_CASE("sep_by1 requires at least one", "[combinator]") {
    auto p = sep_by1(digit(), char_(','));
    REQUIRE(!run_parser(p, "abc").has_value());
    auto result = run_parser(p, "1,2");
    REQUIRE(result.has_value());
    REQUIRE(result->size() == 2);
}

TEST_CASE("end_by parses terminated items", "[combinator]") {
    auto p = end_by(char_('a'), char_(';'));
    auto result = run_parser(p, "a;a;");
    REQUIRE(result.has_value());
    REQUIRE(result->size() == 2);
}

TEST_CASE("chainl1 left associative", "[combinator]") {
    auto num = digit().map([](char c) -> int { return c - '0'; });
    auto add = char_('+').map([](char) -> std::function<int(int, int)> {
        return [](int a, int b) { return a + b; };
    });
    auto p = chainl1(num, add);
    REQUIRE(*run_parser(p, "1+2+3") == 6);
}

TEST_CASE("chainr1 right associative", "[combinator]") {
    auto num = digit().map([](char c) -> int { return c - '0'; });
    auto exp_op = char_('^').map([](char) -> std::function<int(int, int)> {
        return [](int a, int b) {
            int result = 1;
            for (int i = 0; i < b; ++i) result *= a;
            return result;
        };
    });
    auto p = chainr1(num, exp_op);
    // 2^3^2 = 2^(3^2) = 2^9 = 512
    REQUIRE(*run_parser(p, "2^3^2") == 512);
}

TEST_CASE("eof succeeds at end of input", "[combinator]") {
    auto p = string_("hi") > eof();
    REQUIRE(run_parser(p, "hi").has_value());
    REQUIRE(!run_parser(p, "hi!").has_value());
}

TEST_CASE("not_followed_by succeeds when parser fails", "[combinator]") {
    auto p = string_("let").skip(not_followed_by(alpha_num()));
    REQUIRE(run_parser(p, "let ").has_value());
    REQUIRE(!run_parser(p, "letter").has_value());
}

TEST_CASE("many_till collects until terminator", "[combinator]") {
    auto p = many_till(any_char(), char_('.'));
    auto result = run_parser(p, "abc.");
    REQUIRE(result.has_value());
    REQUIRE(result->size() == 3);
    REQUIRE((*result)[0] == 'a');
    REQUIRE((*result)[1] == 'b');
    REQUIRE((*result)[2] == 'c');
}

// Property-based tests
TEST_CASE("Combinator properties", "[combinator][property]") {
    rc::check("sep_by result count matches separators + 1",
        []() {
            auto n = *rc::gen::inRange(1, 10);
            std::string input;
            for (int i = 0; i < n; ++i) {
                if (i > 0) input += ',';
                input += 'a';
            }
            auto result = run_parser(sep_by(char_('a'), char_(',')), input);
            RC_ASSERT(result.has_value());
            RC_ASSERT(static_cast<int>(result->size()) == n);
        });

    rc::check("chainl1 is left-associative (subtraction test)",
        []() {
            auto num = digit().map([](char c) -> int { return c - '0'; });
            auto sub = char_('-').map([](char) -> std::function<int(int, int)> {
                return [](int a, int b) { return a - b; };
            });
            // 5-3-1 left-assoc = (5-3)-1 = 1
            auto result = run_parser(chainl1(num, sub), "5-3-1");
            RC_ASSERT(result.has_value());
            RC_ASSERT(*result == 1);
        });

    rc::check("count(n, p) returns exactly n results",
        []() {
            auto n = *rc::gen::inRange(0, 10);
            std::string input(n, 'x');
            auto result = run_parser(count(n, char_('x')), input);
            RC_ASSERT(result.has_value());
            RC_ASSERT(static_cast<int>(result->size()) == n);
        });
}
