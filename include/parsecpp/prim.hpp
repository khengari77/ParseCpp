#pragma once

#include "parser.hpp"

#include <expected>
#include <functional>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <vector>

namespace parsecpp {

// --- pure / fail ---

template <typename T, typename UserState = NoUserState>
Parser<T, UserState> pure(T value) {
    return Parser<T, UserState>([value = std::move(value)](const State<UserState>& state) -> ParseResult<T, UserState> {
        return ParseResult<T, UserState>::ok_empty(value, state, ParseError::unknown(state.pos));
    });
}

template <typename T = std::monostate, typename UserState = NoUserState>
Parser<T, UserState> fail(std::string msg) {
    return Parser<T, UserState>([msg = std::move(msg)](const State<UserState>& state) -> ParseResult<T, UserState> {
        return ParseResult<T, UserState>::error_empty(
            ParseError::with_message(state.pos, MessageType::Msg, msg)
        );
    });
}

// --- token_prim ---

template <typename T, typename UserState = NoUserState>
Parser<T, UserState> token_prim(
    std::function<std::string(char)> show_tok,
    std::function<std::optional<T>(char)> test_tok,
    std::function<SourcePos(SourcePos, char)> next_pos = nullptr
) {
    return Parser<T, UserState>([show_tok = std::move(show_tok),
                                  test_tok = std::move(test_tok),
                                  next_pos = std::move(next_pos)](const State<UserState>& state) -> ParseResult<T, UserState> {
        if (state.index >= state.input.size()) {
            return ParseResult<T, UserState>::error_empty(
                ParseError::with_message(state.pos, MessageType::SysUnExpect, "")
            );
        }

        char tok = state.input[state.index];
        auto result = test_tok(tok);

        if (!result) {
            return ParseResult<T, UserState>::error_empty(
                ParseError::with_message(state.pos, MessageType::SysUnExpect, show_tok(tok))
            );
        }

        SourcePos new_pos = next_pos ? next_pos(state.pos, tok) : update_pos_char(state.pos, tok);
        auto ghost_err = ParseError::unknown(new_pos);
        State<UserState> new_state{state.input, std::move(new_pos), state.user, state.index + 1};
        return ParseResult<T, UserState>::ok_consumed(
            std::move(*result), std::move(new_state),
            std::move(ghost_err)
        );
    });
}

// --- try_parse ---

template <typename T, typename UserState = NoUserState>
Parser<T, UserState> try_parse(Parser<T, UserState> p) {
    return Parser<T, UserState>([p = std::move(p)](const State<UserState>& state) -> ParseResult<T, UserState> {
        auto res = p(state);
        if (res.is_error() && res.consumed) {
            return ParseResult<T, UserState>{std::get<Err>(res.reply), false};
        }
        return res;
    });
}

// --- look_ahead ---

template <typename T, typename UserState = NoUserState>
Parser<T, UserState> look_ahead(Parser<T, UserState> p) {
    return Parser<T, UserState>([p = std::move(p)](const State<UserState>& state) -> ParseResult<T, UserState> {
        auto res = p(state);
        if (res.is_ok()) {
            auto& ok = std::get<Ok<T, UserState>>(res.reply);
            return ParseResult<T, UserState>::ok_empty(
                std::move(ok.value), state, ParseError::unknown(state.pos)
            );
        }
        return res;
    });
}

// --- many / many1 / skip_many ---

namespace detail {

template <typename T, typename Acc, typename UserState>
Parser<Acc, UserState> many_accum(
    std::function<Acc(T, Acc)> acc_fn,
    Parser<T, UserState> p,
    Acc empty_acc
) {
    return Parser<Acc, UserState>([acc_fn = std::move(acc_fn),
                                    p = std::move(p),
                                    empty_acc = std::move(empty_acc)](const State<UserState>& state) -> ParseResult<Acc, UserState> {
        Acc current_acc = empty_acc;
        State<UserState> accum_state = state;
        bool consumed_overall = false;
        ParseError last_err = ParseError::unknown(state.pos);

        while (true) {
            auto res = p(accum_state);

            if (res.is_error()) {
                if (res.consumed) {
                    return ParseResult<Acc, UserState>{std::get<Err>(res.reply), true};
                }
                auto final_err = ParseError::merge(last_err, std::get<Err>(res.reply).error);
                if (consumed_overall) {
                    return ParseResult<Acc, UserState>::ok_consumed(
                        std::move(current_acc), std::move(accum_state), std::move(final_err)
                    );
                } else {
                    return ParseResult<Acc, UserState>::ok_empty(
                        std::move(current_acc), std::move(accum_state), std::move(final_err)
                    );
                }
            }

            auto& ok = std::get<Ok<T, UserState>>(res.reply);

            if (!res.consumed) {
                // Guard: parser accepts empty string
                return ParseResult<Acc, UserState>::error_consumed(
                    ParseError::with_message(
                        accum_state.pos, MessageType::Msg,
                        "many: combinator applied to a parser that accepts an empty string."
                    )
                );
            }

            consumed_overall = true;
            current_acc = acc_fn(std::move(ok.value), std::move(current_acc));
            accum_state = std::move(ok.state);
            last_err = std::move(ok.error);
        }
    });
}

} // namespace detail

template <typename T, typename UserState = NoUserState>
Parser<std::vector<T>, UserState> many(Parser<T, UserState> p) {
    return detail::many_accum<T, std::vector<T>, UserState>(
        [](T item, std::vector<T> lst) {
            lst.push_back(std::move(item));
            return lst;
        },
        std::move(p),
        std::vector<T>{}
    );
}

template <typename T, typename UserState = NoUserState>
Parser<std::vector<T>, UserState> many1(Parser<T, UserState> p) {
    return p.bind([p](T x) {
        return detail::many_accum<T, std::vector<T>, UserState>(
            [](T item, std::vector<T> lst) {
                lst.push_back(std::move(item));
                return lst;
            },
            p,
            std::vector<T>{std::move(x)}
        );
    });
}

template <typename T, typename UserState = NoUserState>
Parser<std::monostate, UserState> skip_many(Parser<T, UserState> p) {
    return detail::many_accum<T, std::monostate, UserState>(
        [](T, std::monostate m) { return m; },
        std::move(p),
        std::monostate{}
    );
}

template <typename T, typename UserState = NoUserState>
Parser<std::monostate, UserState> skip_many1(Parser<T, UserState> p) {
    return (p > skip_many(p)).map([](auto) { return std::monostate{}; });
}

// --- tokens ---

template <typename UserState = NoUserState>
Parser<std::string, UserState> tokens(std::string_view to_match) {
    std::string target(to_match);
    return Parser<std::string, UserState>([target = std::move(target)](const State<UserState>& state) -> ParseResult<std::string, UserState> {
        size_t idx = state.index;
        size_t len = target.size();

        if (state.input.substr(idx, len) == target) {
            auto new_pos = update_pos_string(state.pos, target);
            auto ghost_err = ParseError::unknown(new_pos);
            State<UserState> new_state{state.input, std::move(new_pos), state.user, idx + len};
            return ParseResult<std::string, UserState>::ok_consumed(
                target, std::move(new_state),
                std::move(ghost_err)
            );
        }

        // Failure
        std::string actual(state.input.substr(idx, len));
        std::string actual_msg = actual.empty() ? "" : "'" + actual + "'";
        ParseError err{state.pos, {
            Message{MessageType::Expect, "'" + target + "'"},
            Message{MessageType::SysUnExpect, actual_msg}
        }};
        return ParseResult<std::string, UserState>::error_empty(std::move(err));
    });
}

// --- take_while / take_while1 ---

namespace detail {

inline SourcePos scan_pos(SourcePos pos, std::string_view input, size_t from, size_t to) {
    int line = pos.line;
    int col = pos.column;
    for (size_t i = from; i < to; ++i) {
        char c = input[i];
        if (c == '\n') { ++line; col = 1; }
        else if (c == '\t') { col += 8 - ((col - 1) % 8); }
        else { ++col; }
    }
    return SourcePos{line, col};
}

} // namespace detail

template <typename UserState = NoUserState>
Parser<std::string, UserState> take_while(std::function<bool(char)> pred) {
    return Parser<std::string, UserState>([pred = std::move(pred)](const State<UserState>& state) -> ParseResult<std::string, UserState> {
        size_t idx = state.index;
        size_t end = idx;
        while (end < state.input.size() && pred(state.input[end])) {
            ++end;
        }
        if (end == idx) {
            return ParseResult<std::string, UserState>::ok_empty(
                "", state, ParseError::unknown(state.pos)
            );
        }
        std::string matched(state.input.substr(idx, end - idx));
        auto new_pos = detail::scan_pos(state.pos, state.input, idx, end);
        auto ghost_err = ParseError::unknown(new_pos);
        State<UserState> new_state{state.input, std::move(new_pos), state.user, end};
        return ParseResult<std::string, UserState>::ok_consumed(
            std::move(matched), std::move(new_state),
            std::move(ghost_err)
        );
    });
}

template <typename UserState = NoUserState>
Parser<std::string, UserState> take_while1(std::function<bool(char)> pred) {
    return Parser<std::string, UserState>([pred = std::move(pred)](const State<UserState>& state) -> ParseResult<std::string, UserState> {
        size_t idx = state.index;
        size_t end = idx;
        while (end < state.input.size() && pred(state.input[end])) {
            ++end;
        }
        if (end == idx) {
            auto tok_msg = idx < state.input.size()
                ? "'" + std::string(1, state.input[idx]) + "'"
                : std::string{};
            return ParseResult<std::string, UserState>::error_empty(
                ParseError::with_message(state.pos, MessageType::SysUnExpect, std::move(tok_msg))
            );
        }
        std::string matched(state.input.substr(idx, end - idx));
        auto new_pos = detail::scan_pos(state.pos, state.input, idx, end);
        auto ghost_err = ParseError::unknown(new_pos);
        State<UserState> new_state{state.input, std::move(new_pos), state.user, end};
        return ParseResult<std::string, UserState>::ok_consumed(
            std::move(matched), std::move(new_state),
            std::move(ghost_err)
        );
    });
}

// --- lazy ---

template <typename T, typename UserState = NoUserState, typename F>
Parser<T, UserState> lazy(F producer) {
    struct LazyState {
        std::once_flag flag;
        Parser<T, UserState> parser;
        std::function<Parser<T, UserState>()> producer;
    };

    auto shared = std::make_shared<LazyState>();
    shared->producer = std::function<Parser<T, UserState>()>(std::move(producer));

    return Parser<T, UserState>([shared](const State<UserState>& state) -> ParseResult<T, UserState> {
        std::call_once(shared->flag, [&]() {
            shared->parser = shared->producer();
            shared->producer = nullptr; // Release closure
        });
        return shared->parser(state);
    });
}

// --- run_parser ---

template <typename T, typename UserState = NoUserState>
std::expected<T, ParseError> run_parser(
    const Parser<T, UserState>& p,
    std::string_view input,
    UserState user_state = UserState{},
    std::string_view source_name = ""
) {
    State<UserState> initial{input, SourcePos{1, 1}, std::move(user_state), 0, source_name};
    auto res = p(initial);

    if (res.is_ok()) {
        return std::get<Ok<T, UserState>>(res.reply).value;
    }
    auto err = std::get<Err>(res.reply).error;
    err.source_name = std::string(source_name);
    return std::unexpected(std::move(err));
}

// --- parse_or_throw ---

class parse_exception : public std::runtime_error {
public:
    ParseError error;

    explicit parse_exception(ParseError err)
        : std::runtime_error(err.format()), error(std::move(err)) {}
};

template <typename T, typename UserState = NoUserState>
T parse_or_throw(
    const Parser<T, UserState>& p,
    std::string_view input,
    UserState user_state = UserState{},
    std::string_view source_name = ""
) {
    auto result = run_parser(p, input, std::move(user_state), source_name);
    if (result) {
        return std::move(*result);
    }
    throw parse_exception(std::move(result.error()));
}

} // namespace parsecpp
