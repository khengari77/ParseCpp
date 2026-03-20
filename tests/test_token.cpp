#include <catch2/catch_test_macros.hpp>
#include <parsecpp/parsecpp.hpp>

using namespace parsecpp;

TEST_CASE("TokenParser integer", "[token]") {
    TokenParser tp(language::empty_def());
    REQUIRE(*run_parser(tp.integer, "42") == 42);
    REQUIRE(*run_parser(tp.integer, "-7") == -7);
    REQUIRE(*run_parser(tp.integer, "+10") == 10);
}

TEST_CASE("TokenParser natural with hex/octal", "[token]") {
    TokenParser tp(language::empty_def());
    REQUIRE(*run_parser(tp.natural, "0xff") == 255);
    REQUIRE(*run_parser(tp.natural, "0o17") == 15);
    REQUIRE(*run_parser(tp.natural, "42") == 42);
    REQUIRE(*run_parser(tp.natural, "0") == 0);
}

TEST_CASE("TokenParser identifier", "[token]") {
    TokenParser tp(language::empty_def());
    auto result = run_parser(tp.identifier, "foo ");
    REQUIRE(result.has_value());
    REQUIRE(*result == "foo");
}

TEST_CASE("TokenParser reserved word rejected as identifier", "[token]") {
    auto def = language::haskell_def();
    TokenParser tp(def);
    REQUIRE(!run_parser(tp.identifier, "let ").has_value());
}

TEST_CASE("TokenParser reserved word matches", "[token]") {
    auto def = language::haskell_def();
    TokenParser tp(def);
    REQUIRE(run_parser(tp.reserved("let"), "let ").has_value());
    // "letter" should not match "let" reserved
    REQUIRE(!run_parser(tp.reserved("let"), "letter").has_value());
}

TEST_CASE("TokenParser string literal", "[token]") {
    TokenParser tp(language::empty_def());
    auto result = run_parser(tp.string_literal, "\"hello world\"");
    REQUIRE(result.has_value());
    REQUIRE(*result == "hello world");
}

TEST_CASE("TokenParser string literal with escapes", "[token]") {
    TokenParser tp(language::empty_def());
    auto result = run_parser(tp.string_literal, "\"hello\\nworld\"");
    REQUIRE(result.has_value());
    REQUIRE(*result == "hello\nworld");
}

TEST_CASE("TokenParser char literal", "[token]") {
    TokenParser tp(language::empty_def());
    auto result = run_parser(tp.char_literal, "'a'");
    REQUIRE(result.has_value());
    REQUIRE(*result == 'a');
}

TEST_CASE("TokenParser whitespace skipping", "[token]") {
    TokenParser tp(language::empty_def());
    auto p = tp.integer.skip(tp.integer);
    auto result = run_parser(p, "1   2");
    REQUIRE(result.has_value());
    REQUIRE(*result == 1);
}

TEST_CASE("TokenParser parens/braces/brackets", "[token]") {
    TokenParser tp(language::empty_def());
    REQUIRE(*run_parser(tp.parens(tp.integer), "( 42 )") == 42);
    REQUIRE(*run_parser(tp.braces(tp.integer), "{ 42 }") == 42);
    REQUIRE(*run_parser(tp.brackets(tp.integer), "[ 42 ]") == 42);
}

TEST_CASE("TokenParser comma_sep", "[token]") {
    TokenParser tp(language::empty_def());
    auto result = run_parser(tp.comma_sep(tp.integer), "1 , 2 , 3");
    REQUIRE(result.has_value());
    REQUIRE(result->size() == 3);
    REQUIRE((*result)[0] == 1);
    REQUIRE((*result)[1] == 2);
    REQUIRE((*result)[2] == 3);
}

TEST_CASE("TokenParser line comments", "[token]") {
    auto def = language::haskell_style();
    TokenParser tp(def);
    auto p = tp.symbol("x") > tp.symbol("y");
    auto result = run_parser(p, "x -- comment\ny");
    REQUIRE(result.has_value());
    REQUIRE(*result == "y");
}
