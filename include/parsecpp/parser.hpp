#pragma once

#include "result.hpp"

#include <functional>
#include <memory>
#include <tuple>
#include <type_traits>

namespace parsecpp {

template <typename T, typename UserState = NoUserState>
class Parser {
public:
    using state_type = State<UserState>;
    using result_type = ParseResult<T, UserState>;
    using fn_type = std::function<result_type(const state_type&)>;

    Parser() = default;
    explicit Parser(fn_type fn) : fn_(std::make_shared<fn_type>(std::move(fn))) {}

    result_type operator()(const state_type& state) const {
        return (*fn_)(state);
    }

    // Monadic bind: Parser<T> -> (T -> Parser<U>) -> Parser<U>
    template <typename F>
    auto bind(F f) const -> Parser<
        typename std::invoke_result_t<F, T>::value_type,
        UserState
    > {
        using U = typename std::invoke_result_t<F, T>::value_type;
        auto self_fn = fn_;
        return Parser<U, UserState>([self_fn, f = std::move(f)](const state_type& state) -> ParseResult<U, UserState> {
            auto res_self = (*self_fn)(state);

            if (auto* e = std::get_if<Err>(&res_self.reply)) {
                return ParseResult<U, UserState>{Err{e->error}, res_self.consumed};
            }

            auto& ok_self = std::get<Ok<T, UserState>>(res_self.reply);
            auto next_parser = f(ok_self.value);
            auto res_next = next_parser(ok_self.state);

            bool consumed_overall = res_self.consumed || res_next.consumed;

            // Inline merge short-circuit
            ParseError merged_err;
            if (ok_self.error.is_unknown()) {
                merged_err = res_next.error();
            } else if (res_next.error().is_unknown()) {
                merged_err = ok_self.error;
            } else {
                merged_err = ParseError::merge(ok_self.error, res_next.error());
            }

            if (auto* e2 = std::get_if<Err>(&res_next.reply)) {
                return ParseResult<U, UserState>{Err{std::move(merged_err)}, consumed_overall};
            }

            auto& ok_next = std::get<Ok<typename std::invoke_result_t<F, T>::value_type, UserState>>(res_next.reply);
            return ParseResult<U, UserState>{
                Ok<U, UserState>{std::move(ok_next.value), std::move(ok_next.state), std::move(merged_err)},
                consumed_overall
            };
        });
    }

    // Functor map: Parser<T> -> (T -> U) -> Parser<U>
    template <typename F>
    auto map(F f) const -> Parser<std::invoke_result_t<F, T>, UserState> {
        using U = std::invoke_result_t<F, T>;
        auto self_fn = fn_;
        return Parser<U, UserState>([self_fn, f = std::move(f)](const state_type& state) -> ParseResult<U, UserState> {
            auto res = (*self_fn)(state);
            if (auto* ok = std::get_if<Ok<T, UserState>>(&res.reply)) {
                return ParseResult<U, UserState>{
                    Ok<U, UserState>{f(ok->value), std::move(ok->state), std::move(ok->error)},
                    res.consumed
                };
            }
            auto& e = std::get<Err>(res.reply);
            return ParseResult<U, UserState>{Err{std::move(e.error)}, res.consumed};
        });
    }

    // Label for error messages (Haskell's <?>)
    Parser<T, UserState> label(std::string msg) const {
        auto self_fn = fn_;
        Message expect_msg{MessageType::Expect, std::move(msg)};
        return Parser<T, UserState>([self_fn, expect_msg = std::move(expect_msg)](const state_type& state) -> result_type {
            auto res = (*self_fn)(state);
            if (res.consumed || res.is_ok()) {
                return res;
            }
            // Empty Error — rewrite expect messages
            auto& err = std::get<Err>(res.reply);
            std::vector<Message> non_expect;
            for (const auto& m : err.error.messages) {
                if (m.type != MessageType::Expect) {
                    non_expect.push_back(m);
                }
            }
            non_expect.push_back(expect_msg);
            return ParseResult<T, UserState>::error_empty(ParseError{state.pos, std::move(non_expect)});
        });
    }

    // Choice: try self, if empty-fail try other (Haskell's <|>)
    Parser<T, UserState> operator|(const Parser<T, UserState>& other) const {
        auto self_fn = fn_;
        auto other_fn = other.fn_;
        return Parser<T, UserState>([self_fn, other_fn](const state_type& state) -> result_type {
            auto res1 = (*self_fn)(state);

            // If Ok OR Consumed Error -> res1 wins
            if (res1.is_ok() || res1.consumed) {
                return res1;
            }

            // res1 is Empty Error
            auto res2 = (*other_fn)(state);

            if (!res2.consumed && res2.is_error()) {
                auto merged = ParseError::merge(
                    std::get<Err>(res1.reply).error,
                    std::get<Err>(res2.reply).error
                );
                return result_type::error_empty(std::move(merged));
            }

            return res2;
        });
    }

    // Sequence keep-right: self >> other (Haskell's *> / >>)
    template <typename U>
    Parser<U, UserState> operator>(const Parser<U, UserState>& other) const {
        auto self_fn = fn_;
        auto other_fn = other.fn_;
        return Parser<U, UserState>([self_fn, other_fn](const state_type& state) -> ParseResult<U, UserState> {
            auto res_self = (*self_fn)(state);
            if (auto* e = std::get_if<Err>(&res_self.reply)) {
                return ParseResult<U, UserState>{Err{e->error}, res_self.consumed};
            }

            auto& ok_self = std::get<Ok<T, UserState>>(res_self.reply);
            auto res_other = (*other_fn)(ok_self.state);
            bool consumed = res_self.consumed || res_other.consumed;

            // Merge ghost errors
            ParseError merged;
            if (ok_self.error.is_unknown()) {
                merged = res_other.error();
            } else if (res_other.error().is_unknown()) {
                merged = ok_self.error;
            } else {
                merged = ParseError::merge(ok_self.error, res_other.error());
            }

            if (auto* e2 = std::get_if<Err>(&res_other.reply)) {
                return ParseResult<U, UserState>{Err{std::move(merged)}, consumed};
            }

            auto& ok_other = std::get<Ok<U, UserState>>(res_other.reply);
            return ParseResult<U, UserState>{
                Ok<U, UserState>{std::move(ok_other.value), std::move(ok_other.state), std::move(merged)},
                consumed
            };
        });
    }

    // Sequence keep-left (Haskell's <*)
    template <typename U>
    Parser<T, UserState> skip(const Parser<U, UserState>& other) const {
        auto self_fn = fn_;
        auto other_fn = other.fn_;
        return Parser<T, UserState>([self_fn, other_fn](const state_type& state) -> result_type {
            auto res_self = (*self_fn)(state);
            if (auto* e = std::get_if<Err>(&res_self.reply)) {
                return result_type{Err{e->error}, res_self.consumed};
            }

            auto& ok_self = std::get<Ok<T, UserState>>(res_self.reply);
            auto res_other = (*other_fn)(ok_self.state);
            bool consumed = res_self.consumed || res_other.consumed;

            ParseError merged;
            if (ok_self.error.is_unknown()) {
                merged = res_other.error();
            } else if (res_other.error().is_unknown()) {
                merged = ok_self.error;
            } else {
                merged = ParseError::merge(ok_self.error, res_other.error());
            }

            if (auto* e2 = std::get_if<Err>(&res_other.reply)) {
                return result_type{Err{std::move(merged)}, consumed};
            }

            auto& ok_other = std::get<Ok<U, UserState>>(res_other.reply);
            return result_type{
                Ok<T, UserState>{std::move(ok_self.value), std::move(ok_other.state), std::move(merged)},
                consumed
            };
        });
    }

    // Sequence both into tuple
    template <typename U>
    Parser<std::tuple<T, U>, UserState> operator&(const Parser<U, UserState>& other) const {
        return this->bind([other](T val_t) {
            return other.map([val_t = std::move(val_t)](U val_u) {
                return std::make_tuple(std::move(val_t), std::move(val_u));
            });
        });
    }

    // Monadic bind operator
    template <typename F>
    auto operator>>=(F f) const {
        return this->bind(std::move(f));
    }

    // Type alias for extracting value type
    using value_type = T;

private:
    std::shared_ptr<fn_type> fn_;

    // Allow other Parser specializations to access fn_
    template <typename, typename>
    friend class Parser;
};

} // namespace parsecpp
