#include <catch2/catch_test_macros.hpp>
#include <rapidcheck.h>
#include <parsecpp/parsecpp.hpp>

using namespace parsecpp;

TEST_CASE("char_ matches specific character", "[char]") {
    REQUIRE(run_parser(char_('a'), "abc").has_value());
    REQUIRE(*run_parser(char_('a'), "abc") == 'a');
    REQUIRE(!run_parser(char_('a'), "xyz").has_value());
}

TEST_CASE("any_char matches any character", "[char]") {
    REQUIRE(run_parser(any_char(), "x").has_value());
    REQUIRE(*run_parser(any_char(), "x") == 'x');
    REQUIRE(!run_parser(any_char(), "").has_value());
}

TEST_CASE("satisfy with predicate", "[char]") {
    auto p = satisfy([](char c) { return c >= 'A' && c <= 'Z'; });
    REQUIRE(run_parser(p, "Hello").has_value());
    REQUIRE(*run_parser(p, "Hello") == 'H');
    REQUIRE(!run_parser(p, "hello").has_value());
}

TEST_CASE("string_ matches exact string", "[char]") {
    REQUIRE(run_parser(string_("hello"), "hello world").has_value());
    REQUIRE(*run_parser(string_("hello"), "hello world") == "hello");
    REQUIRE(!run_parser(string_("hello"), "world").has_value());
}

TEST_CASE("digit matches digits", "[char]") {
    REQUIRE(run_parser(digit(), "5x").has_value());
    REQUIRE(*run_parser(digit(), "5x") == '5');
    REQUIRE(!run_parser(digit(), "abc").has_value());
}

TEST_CASE("letter matches letters", "[char]") {
    REQUIRE(run_parser(letter(), "abc").has_value());
    REQUIRE(!run_parser(letter(), "123").has_value());
}

TEST_CASE("alpha_num matches letters and digits", "[char]") {
    REQUIRE(run_parser(alpha_num(), "a").has_value());
    REQUIRE(run_parser(alpha_num(), "5").has_value());
    REQUIRE(!run_parser(alpha_num(), "!").has_value());
}

TEST_CASE("upper/lower", "[char]") {
    REQUIRE(run_parser(upper(), "A").has_value());
    REQUIRE(!run_parser(upper(), "a").has_value());
    REQUIRE(run_parser(lower(), "a").has_value());
    REQUIRE(!run_parser(lower(), "A").has_value());
}

TEST_CASE("space/spaces", "[char]") {
    REQUIRE(run_parser(space(), " x").has_value());
    REQUIRE(!run_parser(space(), "x").has_value());

    auto p = spaces() > char_('x');
    REQUIRE(run_parser(p, "   x").has_value());
    REQUIRE(*run_parser(p, "   x") == 'x');
    // spaces succeeds on empty too
    REQUIRE(run_parser(p, "x").has_value());
}

TEST_CASE("one_of/none_of", "[char]") {
    REQUIRE(run_parser(one_of("aeiou"), "echo").has_value());
    REQUIRE(*run_parser(one_of("aeiou"), "echo") == 'e');
    REQUIRE(!run_parser(one_of("aeiou"), "xyz").has_value());

    REQUIRE(run_parser(none_of("aeiou"), "hello").has_value());
    REQUIRE(*run_parser(none_of("aeiou"), "hello") == 'h');
    REQUIRE(!run_parser(none_of("aeiou"), "echo").has_value());
}

TEST_CASE("newline/tab/end_of_line", "[char]") {
    REQUIRE(run_parser(newline(), "\nabc").has_value());
    REQUIRE(run_parser(tab(), "\tabc").has_value());
    REQUIRE(run_parser(end_of_line(), "\n").has_value());
    REQUIRE(run_parser(end_of_line(), "\r\n").has_value());
}

// Property-based tests
TEST_CASE("Char parser properties", "[char][property]") {
    rc::check("char_ matches only the specified character",
        []() {
            auto c = *rc::gen::inRange(static_cast<char>('a'), static_cast<char>('z'));
            auto other = *rc::gen::inRange(static_cast<char>('a'), static_cast<char>('z'));
            std::string input(1, other);
            auto result = run_parser(char_(c), input);
            RC_ASSERT(result.has_value() == (c == other));
        });

    rc::check("digit succeeds only on digit chars",
        []() {
            auto c = *rc::gen::inRange(static_cast<char>(32), static_cast<char>(126));
            std::string input(1, c);
            auto result = run_parser(digit(), input);
            RC_ASSERT(result.has_value() == (std::isdigit(static_cast<unsigned char>(c)) != 0));
        });

    rc::check("letter succeeds only on alpha chars",
        []() {
            auto c = *rc::gen::inRange(static_cast<char>(32), static_cast<char>(126));
            std::string input(1, c);
            auto result = run_parser(letter(), input);
            RC_ASSERT(result.has_value() == (std::isalpha(static_cast<unsigned char>(c)) != 0));
        });
}
