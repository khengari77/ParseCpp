#include <catch2/catch_test_macros.hpp>
#include <rapidcheck.h>
#include <parsecpp/parsecpp.hpp>

using namespace parsecpp;

TEST_CASE("pure always succeeds", "[prim]") {
    auto p = pure<int>(42);
    auto result = run_parser(p, "anything");
    REQUIRE(result.has_value());
    REQUIRE(*result == 42);
}

TEST_CASE("fail always fails", "[prim]") {
    auto p = fail<int>("oops");
    auto result = run_parser(p, "anything");
    REQUIRE(!result.has_value());
    REQUIRE(result.error().format().find("oops") != std::string::npos);
}

TEST_CASE("try_parse backtracks on consumed error", "[prim]") {
    // string_("ab") consumes 'a' then fails on 'c'
    // Without try_parse: consumed error, no backtracking
    // With try_parse: empty error, can try alternative
    auto p = try_parse(string_("ab")) | string_("ac");
    auto result = run_parser(p, "ac");
    REQUIRE(result.has_value());
    REQUIRE(*result == "ac");
}

TEST_CASE("look_ahead does not consume input", "[prim]") {
    auto p = look_ahead(char_('a'));
    State<> state{"abc", SourcePos{1, 1}, {}, 0};
    auto res = p(state);
    REQUIRE(res.is_ok());
    REQUIRE(!res.consumed); // Must not consume
    auto& ok = std::get<Ok<char>>(res.reply);
    REQUIRE(ok.value == 'a');
    REQUIRE(ok.state.index == 0); // Position unchanged
}

TEST_CASE("many zero matches", "[prim]") {
    auto p = many(char_('x'));
    auto result = run_parser(p, "abc");
    REQUIRE(result.has_value());
    REQUIRE(result->empty());
}

TEST_CASE("many multiple matches", "[prim]") {
    auto p = many(char_('a'));
    auto result = run_parser(p, "aaab");
    REQUIRE(result.has_value());
    REQUIRE(result->size() == 3);
}

TEST_CASE("many1 requires at least one", "[prim]") {
    auto p = many1(char_('a'));
    REQUIRE(!run_parser(p, "bbb").has_value());
    auto result = run_parser(p, "aab");
    REQUIRE(result.has_value());
    REQUIRE(result->size() == 2);
}

TEST_CASE("skip_many discards results", "[prim]") {
    auto p = skip_many(char_(' ')) > char_('a');
    auto result = run_parser(p, "   a");
    REQUIRE(result.has_value());
    REQUIRE(*result == 'a');
}

TEST_CASE("tokens matches exact string", "[prim]") {
    auto p = tokens("hello");
    auto result = run_parser(p, "hello world");
    REQUIRE(result.has_value());
    REQUIRE(*result == "hello");
}

TEST_CASE("tokens fails on mismatch", "[prim]") {
    auto p = tokens("hello");
    REQUIRE(!run_parser(p, "world").has_value());
}

TEST_CASE("take_while scans matching chars", "[prim]") {
    auto p = take_while([](char c) { return c != ' '; });
    auto result = run_parser(p, "hello world");
    REQUIRE(result.has_value());
    REQUIRE(*result == "hello");
}

TEST_CASE("take_while1 requires at least one", "[prim]") {
    auto p = take_while1([](char c) { return std::isdigit(static_cast<unsigned char>(c)); });
    REQUIRE(!run_parser(p, "abc").has_value());
    auto result = run_parser(p, "123abc");
    REQUIRE(result.has_value());
    REQUIRE(*result == "123");
}

TEST_CASE("lazy defers construction", "[prim]") {
    auto p = lazy<char>([]() { return char_('a'); });
    auto result = run_parser(p, "a");
    REQUIRE(result.has_value());
    REQUIRE(*result == 'a');
}

TEST_CASE("lazy enables recursive parsers", "[prim]") {
    // Parse balanced parentheses: ( [expr] )
    // expr = '(' optional(expr) ')'
    Parser<std::monostate> expr;
    expr = lazy<std::monostate>(
        [&expr]() -> Parser<std::monostate> {
            return char_('(').bind([&expr](char) {
                return optional_(expr).bind([](std::monostate) {
                    return char_(')').map([](char) { return std::monostate{}; });
                });
            });
        }
    );
    REQUIRE(run_parser(expr, "()").has_value());
    REQUIRE(run_parser(expr, "(())").has_value());
    REQUIRE(run_parser(expr, "((()))").has_value());
}

// Property-based tests
TEST_CASE("Prim properties", "[prim][property]") {
    rc::check("pure(x) always returns x",
        [](int x) {
            auto result = run_parser(pure<int>(x), "");
            RC_ASSERT(result.has_value());
            RC_ASSERT(*result == x);
        });

    rc::check("fail always fails regardless of input",
        [](const std::string& input) {
            auto result = run_parser(fail<int>("err"), input);
            RC_ASSERT(!result.has_value());
        });

    rc::check("try_parse never produces consumed error",
        []() {
            auto s = *rc::gen::string<std::string>();
            auto p = try_parse(string_("xyz_impossible_match"));
            State<> state{s, SourcePos{1, 1}, {}, 0};
            auto res = p(state);
            RC_ASSERT(!res.consumed || res.is_ok());
        });

    rc::check("look_ahead never consumes on success",
        []() {
            auto c = *rc::gen::inRange(static_cast<char>('a'), static_cast<char>('z'));
            std::string input(1, c);
            auto p = look_ahead(satisfy(std::function<bool(char)>([c](char x) { return x == c; })));
            State<> state{input, SourcePos{1, 1}, {}, 0};
            auto res = p(state);
            if (res.is_ok()) {
                RC_ASSERT(!res.consumed);
            }
        });
}
