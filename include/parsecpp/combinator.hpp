#pragma once

#include "prim.hpp"

#include <functional>
#include <optional>
#include <vector>

namespace parsecpp {

// --- choice ---

template <typename T, typename UserState = NoUserState>
Parser<T, UserState> choice(std::vector<Parser<T, UserState>> parsers) {
    if (parsers.empty()) {
        return fail<T, UserState>("no alternatives");
    }

    return Parser<T, UserState>([parsers = std::move(parsers)](const State<UserState>& state) -> ParseResult<T, UserState> {
        ParseError max_error = ParseError::unknown(state.pos);

        for (const auto& p : parsers) {
            auto res = p(state);

            if (res.is_ok()) {
                auto& ok = std::get<Ok<T, UserState>>(res.reply);
                auto final_err = ParseError::merge(max_error, ok.error);
                if (res.consumed) {
                    return ParseResult<T, UserState>::ok_consumed(
                        std::move(ok.value), std::move(ok.state), std::move(final_err)
                    );
                } else {
                    return ParseResult<T, UserState>::ok_empty(
                        std::move(ok.value), std::move(ok.state), std::move(final_err)
                    );
                }
            }

            if (res.consumed) {
                return res;
            }

            max_error = ParseError::merge(max_error, std::get<Err>(res.reply).error);
        }

        return ParseResult<T, UserState>::error_empty(std::move(max_error));
    });
}

// --- count ---

template <typename T, typename UserState = NoUserState>
Parser<std::vector<T>, UserState> count(int n, Parser<T, UserState> p) {
    if (n <= 0) {
        return pure<std::vector<T>, UserState>(std::vector<T>{});
    }

    return Parser<std::vector<T>, UserState>([n, p = std::move(p)](const State<UserState>& state) -> ParseResult<std::vector<T>, UserState> {
        std::vector<T> results;
        results.reserve(n);
        State<UserState> current_state = state;
        bool consumed_overall = false;
        ParseError last_error = ParseError::unknown(state.pos);

        for (int i = 0; i < n; ++i) {
            auto res = p(current_state);
            consumed_overall = consumed_overall || res.consumed;

            if (res.is_error()) {
                if (res.consumed) {
                    return ParseResult<std::vector<T>, UserState>{std::get<Err>(res.reply), true};
                }
                auto final_err = ParseError::merge(last_error, std::get<Err>(res.reply).error);
                if (consumed_overall) {
                    return ParseResult<std::vector<T>, UserState>::error_consumed(std::move(final_err));
                }
                return ParseResult<std::vector<T>, UserState>::error_empty(std::move(final_err));
            }

            auto& ok = std::get<Ok<T, UserState>>(res.reply);
            results.push_back(std::move(ok.value));
            current_state = std::move(ok.state);
            last_error = std::move(ok.error);
        }

        if (consumed_overall) {
            return ParseResult<std::vector<T>, UserState>::ok_consumed(
                std::move(results), std::move(current_state), std::move(last_error)
            );
        }
        return ParseResult<std::vector<T>, UserState>::ok_empty(
            std::move(results), std::move(current_state), std::move(last_error)
        );
    });
}

// --- between ---

template <typename T, typename Open, typename Close, typename UserState = NoUserState>
Parser<T, UserState> between(
    Parser<Open, UserState> open,
    Parser<Close, UserState> close,
    Parser<T, UserState> p
) {
    return (open > p).skip(close);
}

// --- option ---

template <typename T, typename UserState = NoUserState>
Parser<T, UserState> option(T default_val, Parser<T, UserState> p) {
    return p | pure<T, UserState>(std::move(default_val));
}

// --- option_maybe ---

template <typename T, typename UserState = NoUserState>
Parser<std::optional<T>, UserState> option_maybe(Parser<T, UserState> p) {
    return p.map([](T val) -> std::optional<T> { return std::move(val); })
         | pure<std::optional<T>, UserState>(std::nullopt);
}

// --- optional_ ---

template <typename T, typename UserState = NoUserState>
Parser<std::monostate, UserState> optional_(Parser<T, UserState> p) {
    return p.map([](T) { return std::monostate{}; })
         | pure<std::monostate, UserState>(std::monostate{});
}

// --- sep_by ---

template <typename T, typename Sep, typename UserState = NoUserState>
Parser<std::vector<T>, UserState> sep_by1(Parser<T, UserState> p, Parser<Sep, UserState> sep) {
    return p.bind([p, sep](T x) {
        return many(sep > p).map([x = std::move(x)](std::vector<T> xs) {
            xs.insert(xs.begin(), std::move(x));
            return xs;
        });
    });
}

template <typename T, typename Sep, typename UserState = NoUserState>
Parser<std::vector<T>, UserState> sep_by(Parser<T, UserState> p, Parser<Sep, UserState> sep) {
    return sep_by1(p, sep) | pure<std::vector<T>, UserState>(std::vector<T>{});
}

// --- end_by ---

template <typename T, typename Sep, typename UserState = NoUserState>
Parser<std::vector<T>, UserState> end_by1(Parser<T, UserState> p, Parser<Sep, UserState> sep) {
    return many1(p.skip(sep));
}

template <typename T, typename Sep, typename UserState = NoUserState>
Parser<std::vector<T>, UserState> end_by(Parser<T, UserState> p, Parser<Sep, UserState> sep) {
    return many(p.skip(sep));
}

// --- sep_end_by ---

template <typename T, typename Sep, typename UserState = NoUserState>
Parser<std::vector<T>, UserState> sep_end_by1(Parser<T, UserState> p, Parser<Sep, UserState> sep) {
    return Parser<std::vector<T>, UserState>([p, sep](const State<UserState>& state) -> ParseResult<std::vector<T>, UserState> {
        auto res_p = p(state);
        if (res_p.is_error()) {
            return ParseResult<std::vector<T>, UserState>{std::get<Err>(res_p.reply), res_p.consumed};
        }

        auto& ok_first = std::get<Ok<T, UserState>>(res_p.reply);
        std::vector<T> results;
        results.push_back(std::move(ok_first.value));
        State<UserState> current_state = std::move(ok_first.state);
        bool overall_consumed = res_p.consumed;
        ParseError last_err = std::move(ok_first.error);

        while (true) {
            auto res_sep = sep(current_state);

            if (res_sep.is_error()) {
                if (res_sep.consumed) {
                    return ParseResult<std::vector<T>, UserState>{std::get<Err>(res_sep.reply), true};
                }
                auto final_err = ParseError::merge(last_err, std::get<Err>(res_sep.reply).error);
                if (overall_consumed) {
                    return ParseResult<std::vector<T>, UserState>::ok_consumed(
                        std::move(results), std::move(current_state), std::move(final_err)
                    );
                }
                return ParseResult<std::vector<T>, UserState>::ok_empty(
                    std::move(results), std::move(current_state), std::move(final_err)
                );
            }

            auto& sep_ok = std::get<Ok<Sep, UserState>>(res_sep.reply);
            auto state_after_sep = std::move(sep_ok.state);
            bool consumed_sep = res_sep.consumed;

            auto res_next = p(state_after_sep);

            if (res_next.is_error()) {
                if (res_next.consumed) {
                    return ParseResult<std::vector<T>, UserState>{std::get<Err>(res_next.reply), true};
                }
                auto final_err = ParseError::merge(sep_ok.error, std::get<Err>(res_next.reply).error);
                overall_consumed = overall_consumed || consumed_sep;
                if (overall_consumed) {
                    return ParseResult<std::vector<T>, UserState>::ok_consumed(
                        std::move(results), std::move(state_after_sep), std::move(final_err)
                    );
                }
                return ParseResult<std::vector<T>, UserState>::ok_empty(
                    std::move(results), std::move(state_after_sep), std::move(final_err)
                );
            }

            auto& p_ok = std::get<Ok<T, UserState>>(res_next.reply);
            results.push_back(std::move(p_ok.value));
            current_state = std::move(p_ok.state);
            overall_consumed = overall_consumed || consumed_sep || res_next.consumed;
            last_err = std::move(p_ok.error);
        }
    });
}

template <typename T, typename Sep, typename UserState = NoUserState>
Parser<std::vector<T>, UserState> sep_end_by(Parser<T, UserState> p, Parser<Sep, UserState> sep) {
    return sep_end_by1(p, sep) | pure<std::vector<T>, UserState>(std::vector<T>{});
}

// --- chainl1 / chainl ---

template <typename T, typename UserState = NoUserState>
Parser<T, UserState> chainl1(
    Parser<T, UserState> p,
    Parser<std::function<T(T, T)>, UserState> op
) {
    return Parser<T, UserState>([p, op](const State<UserState>& state) -> ParseResult<T, UserState> {
        auto res_p = p(state);
        if (res_p.is_error()) {
            return res_p;
        }

        auto& ok_p = std::get<Ok<T, UserState>>(res_p.reply);
        T acc = std::move(ok_p.value);
        State<UserState> curr_state = std::move(ok_p.state);
        bool consumed = res_p.consumed;
        ParseError err = std::move(ok_p.error);

        while (true) {
            auto res_op = op(curr_state);
            if (res_op.is_error()) {
                if (res_op.consumed) {
                    return ParseResult<T, UserState>{std::get<Err>(res_op.reply), true};
                }
                auto final_err = ParseError::merge(err, std::get<Err>(res_op.reply).error);
                if (consumed) {
                    return ParseResult<T, UserState>::ok_consumed(std::move(acc), std::move(curr_state), std::move(final_err));
                }
                return ParseResult<T, UserState>::ok_empty(std::move(acc), std::move(curr_state), std::move(final_err));
            }

            auto& ok_op = std::get<Ok<std::function<T(T, T)>, UserState>>(res_op.reply);
            auto op_func = std::move(ok_op.value);

            auto res_right = p(ok_op.state);
            if (res_right.is_error()) {
                bool final_consumed = consumed || res_op.consumed || res_right.consumed;
                auto merged_err = ParseError::merge(ok_op.error, std::get<Err>(res_right.reply).error);
                if (res_right.consumed) {
                    return ParseResult<T, UserState>{std::get<Err>(res_right.reply), true};
                }
                return ParseResult<T, UserState>{Err{std::move(merged_err)}, final_consumed};
            }

            auto& ok_right = std::get<Ok<T, UserState>>(res_right.reply);
            acc = op_func(std::move(acc), std::move(ok_right.value));
            curr_state = std::move(ok_right.state);
            consumed = consumed || res_op.consumed || res_right.consumed;
            err = std::move(ok_right.error);
        }
    });
}

template <typename T, typename UserState = NoUserState>
Parser<T, UserState> chainl(
    Parser<T, UserState> p,
    Parser<std::function<T(T, T)>, UserState> op,
    T default_val
) {
    return chainl1(std::move(p), std::move(op)) | pure<T, UserState>(std::move(default_val));
}

// --- chainr1 / chainr ---

namespace detail {

template <typename T>
struct OpChain {
    std::vector<T> values;
    std::vector<std::function<T(T, T)>> ops;
};

template <typename T, typename UserState>
Parser<OpChain<T>, UserState>
scan_op_chain(Parser<T, UserState> term_parser, Parser<std::function<T(T, T)>, UserState> op_parser) {
    return Parser<OpChain<T>, UserState>(
        [term_parser, op_parser](const State<UserState>& state) -> ParseResult<OpChain<T>, UserState> {
            auto res_first = term_parser(state);
            if (res_first.is_error()) {
                return ParseResult<OpChain<T>, UserState>{std::get<Err>(res_first.reply), res_first.consumed};
            }

            auto& ok_first = std::get<Ok<T, UserState>>(res_first.reply);
            OpChain<T> chain;
            chain.values.reserve(4);
            chain.ops.reserve(4);
            chain.values.push_back(std::move(ok_first.value));
            State<UserState> curr_state = std::move(ok_first.state);
            bool consumed = res_first.consumed;
            ParseError err = std::move(ok_first.error);

            while (true) {
                auto res_op = op_parser(curr_state);
                if (res_op.is_error()) {
                    if (res_op.consumed) {
                        return ParseResult<OpChain<T>, UserState>{std::get<Err>(res_op.reply), true};
                    }
                    auto final_err = ParseError::merge(err, std::get<Err>(res_op.reply).error);
                    if (consumed) {
                        return ParseResult<OpChain<T>, UserState>::ok_consumed(
                            std::move(chain), std::move(curr_state), std::move(final_err)
                        );
                    }
                    return ParseResult<OpChain<T>, UserState>::ok_empty(
                        std::move(chain), std::move(curr_state), std::move(final_err)
                    );
                }

                auto& ok_op = std::get<Ok<std::function<T(T, T)>, UserState>>(res_op.reply);
                auto res_next = term_parser(ok_op.state);

                if (res_next.is_error()) {
                    bool final_consumed = consumed || res_op.consumed || res_next.consumed;
                    auto merged_err = ParseError::merge(ok_op.error, std::get<Err>(res_next.reply).error);
                    if (res_next.consumed) {
                        return ParseResult<OpChain<T>, UserState>{std::get<Err>(res_next.reply), true};
                    }
                    return ParseResult<OpChain<T>, UserState>{Err{std::move(merged_err)}, final_consumed};
                }

                auto& ok_next = std::get<Ok<T, UserState>>(res_next.reply);
                chain.ops.push_back(std::move(ok_op.value));
                chain.values.push_back(std::move(ok_next.value));
                curr_state = std::move(ok_next.state);
                consumed = consumed || res_op.consumed || res_next.consumed;
                err = std::move(ok_next.error);
            }
        }
    );
}

} // namespace detail

template <typename T, typename UserState = NoUserState>
Parser<T, UserState> chainr1(
    Parser<T, UserState> p,
    Parser<std::function<T(T, T)>, UserState> op
) {
    return detail::scan_op_chain(std::move(p), std::move(op)).map([](detail::OpChain<T> chain) -> T {
        if (chain.values.empty()) {
            throw std::logic_error("chainr1: empty chain");
        }

        T acc = std::move(chain.values.back());
        for (int i = static_cast<int>(chain.ops.size()) - 1; i >= 0; --i) {
            acc = chain.ops[i](std::move(chain.values[i]), std::move(acc));
        }
        return acc;
    });
}

template <typename T, typename UserState = NoUserState>
Parser<T, UserState> chainr(
    Parser<T, UserState> p,
    Parser<std::function<T(T, T)>, UserState> op,
    T default_val
) {
    return chainr1(std::move(p), std::move(op)) | pure<T, UserState>(std::move(default_val));
}

// --- any_token ---

template <typename UserState = NoUserState>
Parser<char, UserState> any_token() {
    return token_prim<char, UserState>(
        [](char c) -> std::string { return std::string(1, c); },
        [](char c) -> std::optional<char> { return c; }
    );
}

// --- not_followed_by ---

template <typename T, typename UserState = NoUserState>
Parser<std::monostate, UserState> not_followed_by(Parser<T, UserState> p) {
    return Parser<std::monostate, UserState>(
        [p = std::move(p)](const State<UserState>& state) -> ParseResult<std::monostate, UserState> {
            auto tried = try_parse(look_ahead(std::move(p)));
            auto res = tried(state);
            if (res.is_error()) {
                return ParseResult<std::monostate, UserState>::ok_empty(
                    std::monostate{}, state, ParseError::unknown(state.pos)
                );
            }
            // Parser succeeded — we want failure
            auto& ok = std::get<Ok<T, UserState>>(res.reply);
            // We can't easily convert T to string generically, so use a generic message
            return ParseResult<std::monostate, UserState>::error_empty(
                ParseError::with_message(state.pos, MessageType::UnExpect, "token")
            );
        }
    );
}

// --- eof ---

template <typename UserState = NoUserState>
Parser<std::monostate, UserState> eof() {
    return not_followed_by(any_token<UserState>()).label("end of input");
}

// --- many_till ---

template <typename T, typename End, typename UserState = NoUserState>
Parser<std::vector<T>, UserState> many_till(Parser<T, UserState> p, Parser<End, UserState> end) {
    return Parser<std::vector<T>, UserState>(
        [p, end](const State<UserState>& state) -> ParseResult<std::vector<T>, UserState> {
            std::vector<T> results;
            State<UserState> curr = state;
            bool consumed = false;
            ParseError err = ParseError::unknown(state.pos);

            while (true) {
                auto res_end = end(curr);
                if (res_end.is_ok()) {
                    consumed = consumed || res_end.consumed;
                    auto& ok_end = std::get<Ok<End, UserState>>(res_end.reply);
                    auto final_err = ParseError::merge(err, ok_end.error);
                    if (consumed) {
                        return ParseResult<std::vector<T>, UserState>::ok_consumed(
                            std::move(results), std::move(ok_end.state), std::move(final_err)
                        );
                    }
                    return ParseResult<std::vector<T>, UserState>::ok_empty(
                        std::move(results), std::move(ok_end.state), std::move(final_err)
                    );
                }

                if (res_end.consumed) {
                    return ParseResult<std::vector<T>, UserState>{std::get<Err>(res_end.reply), true};
                }

                err = ParseError::merge(err, std::get<Err>(res_end.reply).error);

                auto res_p = p(curr);
                if (res_p.is_error()) {
                    if (res_p.consumed) {
                        return ParseResult<std::vector<T>, UserState>{std::get<Err>(res_p.reply), true};
                    }
                    return ParseResult<std::vector<T>, UserState>{
                        Err{ParseError::merge(err, std::get<Err>(res_p.reply).error)},
                        consumed
                    };
                }

                auto& ok_p = std::get<Ok<T, UserState>>(res_p.reply);
                results.push_back(std::move(ok_p.value));
                curr = std::move(ok_p.state);
                consumed = consumed || res_p.consumed;
                err = std::move(ok_p.error);
            }
        }
    );
}

} // namespace parsecpp
