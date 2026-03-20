#pragma once

#include "error.hpp"
#include "state.hpp"

#include <variant>

namespace parsecpp {

template <typename T, typename UserState = NoUserState>
struct Ok {
    T value;
    State<UserState> state;
    ParseError error; // ghost errors from tried alternatives
};

struct Err {
    ParseError error;
};

template <typename T, typename UserState = NoUserState>
using Reply = std::variant<Ok<T, UserState>, Err>;

template <typename T, typename UserState = NoUserState>
struct ParseResult {
    Reply<T, UserState> reply;
    bool consumed;

    bool is_ok() const {
        return std::holds_alternative<Ok<T, UserState>>(reply);
    }

    bool is_error() const {
        return std::holds_alternative<Err>(reply);
    }

    const Ok<T, UserState>& ok() const {
        return std::get<Ok<T, UserState>>(reply);
    }

    const Err& err() const {
        return std::get<Err>(reply);
    }

    const ParseError& error() const {
        if (auto* e = std::get_if<Err>(&reply)) {
            return e->error;
        }
        return std::get<Ok<T, UserState>>(reply).error;
    }

    static ParseResult ok_consumed(T value, State<UserState> state, ParseError err) {
        return ParseResult{
            Ok<T, UserState>{std::move(value), std::move(state), std::move(err)},
            true
        };
    }

    static ParseResult ok_empty(T value, State<UserState> state, ParseError err) {
        return ParseResult{
            Ok<T, UserState>{std::move(value), std::move(state), std::move(err)},
            false
        };
    }

    static ParseResult error_consumed(ParseError err) {
        return ParseResult{Err{std::move(err)}, true};
    }

    static ParseResult error_empty(ParseError err) {
        return ParseResult{Err{std::move(err)}, false};
    }
};

} // namespace parsecpp
