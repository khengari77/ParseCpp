#pragma once

#include "prim.hpp"

#include <cctype>
#include <string>
#include <string_view>

namespace parsecpp {

// --- satisfy ---

template <typename UserState = NoUserState>
Parser<char, UserState> satisfy(std::function<bool(char)> pred) {
    return token_prim<char, UserState>(
        [](char c) -> std::string {
            if (c == '\0') return "EOF";
            return std::string(1, c);
        },
        [pred = std::move(pred)](char c) -> std::optional<char> {
            if (pred(c)) return c;
            return std::nullopt;
        }
    );
}

// --- char_ ---

template <typename UserState = NoUserState>
Parser<char, UserState> char_(char c) {
    return satisfy<UserState>([c](char x) { return x == c; })
        .label("'" + std::string(1, c) + "'");
}

// --- any_char ---

template <typename UserState = NoUserState>
Parser<char, UserState> any_char() {
    return satisfy<UserState>([](char) { return true; });
}

// --- string_ ---

template <typename UserState = NoUserState>
Parser<std::string, UserState> string_(std::string_view s) {
    return tokens<UserState>(s).label("'" + std::string(s) + "'");
}

// --- Character classes ---

template <typename UserState = NoUserState>
Parser<char, UserState> digit() {
    return satisfy<UserState>([](char c) { return std::isdigit(static_cast<unsigned char>(c)); })
        .label("digit");
}

template <typename UserState = NoUserState>
Parser<char, UserState> hex_digit() {
    return satisfy<UserState>([](char c) { return std::isxdigit(static_cast<unsigned char>(c)); })
        .label("hexadecimal digit");
}

template <typename UserState = NoUserState>
Parser<char, UserState> oct_digit() {
    return satisfy<UserState>([](char c) { return c >= '0' && c <= '7'; })
        .label("octal digit");
}

template <typename UserState = NoUserState>
Parser<char, UserState> letter() {
    return satisfy<UserState>([](char c) { return std::isalpha(static_cast<unsigned char>(c)); })
        .label("letter");
}

template <typename UserState = NoUserState>
Parser<char, UserState> alpha_num() {
    return satisfy<UserState>([](char c) { return std::isalnum(static_cast<unsigned char>(c)); })
        .label("letter or digit");
}

template <typename UserState = NoUserState>
Parser<char, UserState> upper() {
    return satisfy<UserState>([](char c) { return std::isupper(static_cast<unsigned char>(c)); })
        .label("uppercase letter");
}

template <typename UserState = NoUserState>
Parser<char, UserState> lower() {
    return satisfy<UserState>([](char c) { return std::islower(static_cast<unsigned char>(c)); })
        .label("lowercase letter");
}

template <typename UserState = NoUserState>
Parser<char, UserState> space() {
    return satisfy<UserState>([](char c) { return std::isspace(static_cast<unsigned char>(c)); })
        .label("space");
}

template <typename UserState = NoUserState>
Parser<std::monostate, UserState> spaces() {
    return skip_many(space<UserState>()).label("white space");
}

template <typename UserState = NoUserState>
Parser<char, UserState> newline() {
    return char_<UserState>('\n').label("lf new-line");
}

template <typename UserState = NoUserState>
Parser<char, UserState> crlf() {
    return (char_<UserState>('\r') > char_<UserState>('\n')).label("crlf new-line");
}

template <typename UserState = NoUserState>
Parser<char, UserState> end_of_line() {
    return (newline<UserState>() | crlf<UserState>()).label("new-line");
}

template <typename UserState = NoUserState>
Parser<char, UserState> tab() {
    return char_<UserState>('\t').label("tab");
}

// --- one_of / none_of ---

template <typename UserState = NoUserState>
Parser<char, UserState> one_of(std::string_view chars) {
    std::string chars_str(chars);
    return satisfy<UserState>([chars_str](char c) {
        return chars_str.find(c) != std::string::npos;
    }).label("one of " + chars_str);
}

template <typename UserState = NoUserState>
Parser<char, UserState> none_of(std::string_view chars) {
    std::string chars_str(chars);
    return satisfy<UserState>([chars_str](char c) {
        return chars_str.find(c) == std::string::npos;
    }).label("none of " + chars_str);
}

} // namespace parsecpp
