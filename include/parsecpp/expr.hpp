#pragma once

#include "combinator.hpp"

#include <functional>
#include <variant>
#include <vector>

namespace parsecpp {

enum class Assoc { None, Left, Right };

template <typename T>
struct Infix {
    Parser<std::function<T(T, T)>> parser;
    Assoc assoc;
};

template <typename T>
struct Prefix {
    Parser<std::function<T(T)>> parser;
};

template <typename T>
struct Postfix {
    Parser<std::function<T(T)>> parser;
};

template <typename T>
using Operator = std::variant<Infix<T>, Prefix<T>, Postfix<T>>;

namespace detail {

template <typename T>
Parser<T> make_level_parser(const std::vector<Operator<T>>& ops, Parser<T> term) {
    std::vector<Parser<std::function<T(T, T)>>> infix_r, infix_l, infix_n;
    std::vector<Parser<std::function<T(T)>>> prefix, postfix;

    for (const auto& op : ops) {
        if (auto* inf = std::get_if<Infix<T>>(&op)) {
            switch (inf->assoc) {
                case Assoc::Right: infix_r.push_back(inf->parser); break;
                case Assoc::Left:  infix_l.push_back(inf->parser); break;
                case Assoc::None:  infix_n.push_back(inf->parser); break;
            }
        } else if (auto* pre = std::get_if<Prefix<T>>(&op)) {
            prefix.push_back(pre->parser);
        } else if (auto* post = std::get_if<Postfix<T>>(&op)) {
            postfix.push_back(post->parser);
        }
    }

    // Build prefix parser: choice(prefix) | identity
    Parser<std::function<T(T)>> pre_parser;
    if (!prefix.empty()) {
        pre_parser = choice(std::move(prefix))
            | pure<std::function<T(T)>>(std::function<T(T)>([](T x) { return x; }));
    } else {
        pre_parser = pure<std::function<T(T)>>(std::function<T(T)>([](T x) { return x; }));
    }

    // Build postfix parser: choice(postfix) | identity
    Parser<std::function<T(T)>> post_parser;
    if (!postfix.empty()) {
        post_parser = choice(std::move(postfix))
            | pure<std::function<T(T)>>(std::function<T(T)>([](T x) { return x; }));
    } else {
        post_parser = pure<std::function<T(T)>>(std::function<T(T)>([](T x) { return x; }));
    }

    // term_parser = pre >>= \f -> term >>= \x -> post >>= \g -> pure(g(f(x)))
    auto term_parser = pre_parser.bind([term, post_parser](std::function<T(T)> f) {
        return term.bind([f = std::move(f), post_parser](T x) {
            return post_parser.bind([f = std::move(f), x = std::move(x)](std::function<T(T)> g) {
                return pure<T>(g(f(x)));
            });
        });
    });

    // Build ambiguity checker from all infix ops before any are moved
    Parser<std::function<T(T, T)>> ambig_op = fail<std::function<T(T, T)>>("no ops");
    if (!infix_n.empty()) {
        std::vector<Parser<std::function<T(T, T)>>> all_infix;
        all_infix.insert(all_infix.end(), infix_r.begin(), infix_r.end());
        all_infix.insert(all_infix.end(), infix_l.begin(), infix_l.end());
        all_infix.insert(all_infix.end(), infix_n.begin(), infix_n.end());
        ambig_op = try_parse(choice(std::move(all_infix)));
    }

    // Apply infix operators
    Parser<T> result_parser = term_parser;

    if (!infix_l.empty()) {
        auto op_l = choice(std::move(infix_l));
        result_parser = chainl1(result_parser, op_l);
    }

    if (!infix_r.empty()) {
        auto op_r = choice(std::move(infix_r));
        result_parser = chainr1(result_parser, op_r);
    }

    if (!infix_n.empty()) {
        auto op_n = choice(std::move(infix_n));
        result_parser = result_parser.bind([op_n, result_parser, ambig_op](T x) {
            return (op_n.bind([x, result_parser, ambig_op](std::function<T(T, T)> f) {
                return result_parser.bind([x, f, ambig_op](T y) {
                    // Reject ambiguous chaining after x op y
                    return (ambig_op.bind([](auto) {
                        return fail<T>("ambiguous use of a non-associative operator");
                    })) | pure<T>(f(x, y));
                });
            })) | pure<T>(x);
        });
    }

    return result_parser;
}

} // namespace detail

template <typename T>
Parser<T> build_expression_parser(
    std::vector<std::vector<Operator<T>>> table,
    Parser<T> simple_term
) {
    Parser<T> term = std::move(simple_term);
    for (const auto& ops : table) {
        term = detail::make_level_parser(ops, std::move(term));
    }
    return term;
}

} // namespace parsecpp
