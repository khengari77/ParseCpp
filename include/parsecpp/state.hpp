#pragma once

#include "pos.hpp"

#include <cstddef>
#include <string_view>

namespace parsecpp {

struct NoUserState {};

template <typename UserState = NoUserState>
struct State {
    std::string_view input;
    SourcePos pos;
    UserState user;
    size_t index = 0;

    State() = default;
    State(std::string_view inp, SourcePos p, UserState u, size_t idx = 0)
        : input(inp), pos(std::move(p)), user(std::move(u)), index(idx) {}

    std::string_view remaining() const {
        return input.substr(index);
    }
};

// Specialization for NoUserState — easier construction
template <>
struct State<NoUserState> {
    std::string_view input;
    SourcePos pos;
    NoUserState user{};
    size_t index = 0;

    State() = default;
    State(std::string_view inp, SourcePos p, NoUserState u = {}, size_t idx = 0)
        : input(inp), pos(std::move(p)), user(u), index(idx) {}

    std::string_view remaining() const {
        return input.substr(index);
    }
};

} // namespace parsecpp
