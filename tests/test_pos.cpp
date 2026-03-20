#include <catch2/catch_test_macros.hpp>
#include <rapidcheck.h>
#include <parsecpp/pos.hpp>

using namespace parsecpp;

TEST_CASE("SourcePos initial position", "[pos]") {
    SourcePos pos;
    REQUIRE(pos.line == 1);
    REQUIRE(pos.column == 1);
}

TEST_CASE("SourcePos comparison", "[pos]") {
    SourcePos a{1, 5};
    SourcePos b{1, 10};
    SourcePos c{2, 1};

    REQUIRE(a < b);
    REQUIRE(b < c);
    REQUIRE(a < c);
    REQUIRE(a == a);
    REQUIRE(!(a == b));
}

TEST_CASE("update_pos_char regular character", "[pos]") {
    auto pos = update_pos_char(SourcePos{1, 1}, 'a');
    REQUIRE(pos.line == 1);
    REQUIRE(pos.column == 2);
}

TEST_CASE("update_pos_char newline", "[pos]") {
    auto pos = update_pos_char(SourcePos{1, 5}, '\n');
    REQUIRE(pos.line == 2);
    REQUIRE(pos.column == 1);
}

TEST_CASE("update_pos_char tab", "[pos]") {
    auto pos = update_pos_char(SourcePos{1, 1}, '\t');
    REQUIRE(pos.line == 1);
    REQUIRE(pos.column == 9); // tab to next multiple of 8 + 1

    pos = update_pos_char(SourcePos{1, 5}, '\t');
    REQUIRE(pos.column == 9);
}

TEST_CASE("update_pos_string simple", "[pos]") {
    auto pos = update_pos_string(SourcePos{1, 1}, "hello");
    REQUIRE(pos.line == 1);
    REQUIRE(pos.column == 6);
}

TEST_CASE("update_pos_string with newlines", "[pos]") {
    auto pos = update_pos_string(SourcePos{1, 1}, "ab\ncd\nef");
    REQUIRE(pos.line == 3);
    REQUIRE(pos.column == 3);
}

TEST_CASE("update_pos_string empty", "[pos]") {
    auto pos = update_pos_string(SourcePos{3, 7}, "");
    REQUIRE(pos.line == 3);
    REQUIRE(pos.column == 7);
}

// Property-based tests
TEST_CASE("SourcePos properties", "[pos][property]") {
    rc::check("column advances by string length (no newlines/tabs)",
        [](const std::string& s) {
            // Filter out newlines and tabs
            std::string filtered;
            for (char c : s) {
                if (c != '\n' && c != '\t' && c != '\r') {
                    filtered += c;
                }
            }
            auto pos = update_pos_string(SourcePos{1, 1}, filtered);
            RC_ASSERT(pos.line == 1);
            RC_ASSERT(pos.column == 1 + static_cast<int>(filtered.size()));
        });

    rc::check("newlines increment line count",
        []() {
            auto n = *rc::gen::inRange(0, 20);
            std::string s(n, '\n');
            auto pos = update_pos_string(SourcePos{1, 1}, s);
            RC_ASSERT(pos.line == 1 + n);
            RC_ASSERT(pos.column == 1);
        });
}
