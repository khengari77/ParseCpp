#include <catch2/catch_test_macros.hpp>
#include <rapidcheck.h>
#include <parsecpp/error.hpp>

using namespace parsecpp;

TEST_CASE("ParseError unknown", "[error]") {
    auto err = ParseError::unknown(SourcePos{1, 1, ""});
    REQUIRE(err.is_unknown());
    REQUIRE(err.format().find("Unknown") != std::string::npos);
}

TEST_CASE("ParseError with message", "[error]") {
    auto err = ParseError::with_message(SourcePos{1, 1, ""}, MessageType::Expect, "'a'");
    REQUIRE(!err.is_unknown());
    REQUIRE(err.format().find("expecting 'a'") != std::string::npos);
}

TEST_CASE("ParseError merge unknown prefers known", "[error]") {
    auto unknown = ParseError::unknown(SourcePos{1, 1, ""});
    auto known = ParseError::with_message(SourcePos{1, 1, ""}, MessageType::Expect, "'a'");

    auto m1 = ParseError::merge(unknown, known);
    REQUIRE(!m1.is_unknown());

    auto m2 = ParseError::merge(known, unknown);
    REQUIRE(!m2.is_unknown());
}

TEST_CASE("ParseError merge furthest position wins", "[error]") {
    auto e1 = ParseError::with_message(SourcePos{1, 1, ""}, MessageType::Expect, "'a'");
    auto e2 = ParseError::with_message(SourcePos{1, 5, ""}, MessageType::Expect, "'b'");

    auto merged = ParseError::merge(e1, e2);
    REQUIRE(merged.pos.column == 5);
    REQUIRE(merged.format().find("'b'") != std::string::npos);
}

TEST_CASE("ParseError merge same position combines messages", "[error]") {
    auto e1 = ParseError::with_message(SourcePos{1, 1, ""}, MessageType::Expect, "'a'");
    auto e2 = ParseError::with_message(SourcePos{1, 1, ""}, MessageType::Expect, "'b'");

    auto merged = ParseError::merge(e1, e2);
    REQUIRE(merged.messages.size() == 2);
    auto fmt = merged.format();
    REQUIRE(fmt.find("'a'") != std::string::npos);
    REQUIRE(fmt.find("'b'") != std::string::npos);
}

TEST_CASE("ParseError unexpected formatting", "[error]") {
    auto err = ParseError::with_message(SourcePos{1, 1, ""}, MessageType::SysUnExpect, "x");
    REQUIRE(err.format().find("unexpected x") != std::string::npos);
}

TEST_CASE("ParseError unexpected end of input", "[error]") {
    auto err = ParseError::with_message(SourcePos{1, 1, ""}, MessageType::SysUnExpect, "");
    REQUIRE(err.format().find("unexpected end of input") != std::string::npos);
}

// Property-based tests
TEST_CASE("ParseError merge properties", "[error][property]") {
    rc::check("merge with unknown is identity",
        []() {
            auto pos = SourcePos{*rc::gen::inRange(1, 100), *rc::gen::inRange(1, 100), ""};
            auto err = ParseError::with_message(pos, MessageType::Expect, "x");
            auto unknown = ParseError::unknown(pos);

            auto m1 = ParseError::merge(err, unknown);
            auto m2 = ParseError::merge(unknown, err);
            RC_ASSERT(!m1.is_unknown());
            RC_ASSERT(!m2.is_unknown());
        });

    rc::check("merge at same position combines messages",
        []() {
            auto pos = SourcePos{1, 1, ""};
            auto n = *rc::gen::inRange(1, 5);
            ParseError acc = ParseError::unknown(pos);
            for (int i = 0; i < n; ++i) {
                auto msg = ParseError::with_message(pos, MessageType::Expect, "msg" + std::to_string(i));
                acc = ParseError::merge(acc, msg);
            }
            RC_ASSERT(static_cast<int>(acc.messages.size()) == n);
        });
}
