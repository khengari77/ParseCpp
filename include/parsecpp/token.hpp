#pragma once

#include "char.hpp"
#include "combinator.hpp"

#include <map>
#include <stdexcept>
#include <string>
#include <variant>

namespace parsecpp {

struct LanguageDef {
    std::string comment_start;
    std::string comment_end;
    std::string comment_line;
    bool nested_comments = true;
    Parser<char> ident_start = satisfy([](char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; });
    Parser<char> ident_letter = satisfy([](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; });
    Parser<char> op_start = one_of(":!#$%&*+./<=>?@\\^|-~");
    Parser<char> op_letter = one_of(":!#$%&*+./<=>?@\\^|-~");
    std::vector<std::string> reserved_names;
    std::vector<std::string> reserved_op_names;
    bool case_sensitive = true;
};

class TokenParser {
public:
    explicit TokenParser(LanguageDef lang) : lang_(std::move(lang)) {
        if (lang_.comment_start.empty() != lang_.comment_end.empty()) {
            throw std::invalid_argument(
                "LanguageDef: comment_start and comment_end must both be set or both be empty"
            );
        }

        // Build whitespace parser
        white_space = make_white_space();

        // Symbols
        semi = symbol(";");
        comma = symbol(",");
        colon = symbol(":");
        dot = symbol(".");

        // Numbers
        auto decimal_p = take_while1([](char c) { return std::isdigit(static_cast<unsigned char>(c)); })
            .map([](std::string s) -> int64_t { return std::stoll(s); });
        decimal = lexeme(decimal_p);

        auto hex_p = (one_of("xX") > take_while1([](char c) { return std::isxdigit(static_cast<unsigned char>(c)); }))
            .map([](std::string s) -> int64_t { return std::stoll(s, nullptr, 16); });
        hexadecimal = lexeme(hex_p);

        auto oct_p = (one_of("oO") > take_while1([](char c) { return c >= '0' && c <= '7'; }))
            .map([](std::string s) -> int64_t { return std::stoll(s, nullptr, 8); });
        octal = lexeme(oct_p);

        natural = lexeme(
            (char_('0') > choice<int64_t>({hexadecimal, octal, decimal_p, pure<int64_t>(0)}))
            | decimal_p
        );

        integer = lexeme(
            (char_('-') > natural.map([](int64_t n) -> int64_t { return -n; }))
            | (char_('+') > natural)
            | natural
        );

        // Float
        auto fraction_p = char_('.') > take_while1([](char c) { return std::isdigit(static_cast<unsigned char>(c)); });

        auto exponent_p = one_of("eE") > option<std::string>(
            "", one_of("+-").map([](char c) { return std::string(1, c); })
        ).bind([decimal_p](std::string sign) {
            return decimal_p.map([sign = std::move(sign)](int64_t d) {
                return "e" + sign + std::to_string(d);
            });
        });

        auto float_val = take_while1([](char c) { return std::isdigit(static_cast<unsigned char>(c)); })
            .bind([fraction_p, exponent_p](std::string ds) {
                return choice<double>({
                    fraction_p.bind([ds, exponent_p](std::string frac) {
                        return option<std::string>("", exponent_p).map([ds, frac](std::string exp) {
                            return std::stod(ds + "." + frac + exp);
                        });
                    }),
                    exponent_p.map([ds](std::string exp) {
                        return std::stod(ds + exp);
                    })
                });
            });

        float_ = lexeme(
            (char_('-') > float_val.map([](double f) { return -f; }))
            | (char_('+') > float_val)
            | float_val
        );

        // String/char escape codes
        auto escape_code = char_('\\') > choice<char>({
            char_('n').map([](char) -> char { return '\n'; }),
            char_('r').map([](char) -> char { return '\r'; }),
            char_('t').map([](char) -> char { return '\t'; }),
            char_('\\').map([](char) -> char { return '\\'; }),
            char_('"').map([](char) -> char { return '"'; }),
            char_('\'').map([](char) -> char { return '\''; }),
            char_('b').map([](char) -> char { return '\b'; }),
            char_('f').map([](char) -> char { return '\f'; }),
            any_char()  // fallback
        });

        // Char literal
        auto char_content = satisfy([](char c) { return c != '\'' && c != '\\'; }) | escape_code;
        char_literal = lexeme(between(char_('\''), char_('\''), char_content));

        // String literal
        auto str_char = satisfy([](char c) { return c != '"' && c != '\\'; }) | escape_code;
        string_literal = lexeme(
            between(char_('"'), char_('"'),
                many(str_char).map([](std::vector<char> cs) {
                    return std::string(cs.begin(), cs.end());
                })
            )
        );

        // Identifier
        identifier = lexeme(make_identifier());

        // Operator
        operator_ = lexeme(make_operator());
    }

    // Lexeme: parse p then skip trailing whitespace
    template <typename T>
    Parser<T> lexeme(Parser<T> p) const {
        return p.skip(white_space);
    }

    // Symbol: parse exact string then skip whitespace
    Parser<std::string> symbol(std::string_view s) const {
        return lexeme(string_(s));
    }

    // Bracket helpers
    template <typename T>
    Parser<T> parens(Parser<T> p) const {
        return between(symbol("("), symbol(")"), std::move(p));
    }

    template <typename T>
    Parser<T> braces(Parser<T> p) const {
        return between(symbol("{"), symbol("}"), std::move(p));
    }

    template <typename T>
    Parser<T> angles(Parser<T> p) const {
        return between(symbol("<"), symbol(">"), std::move(p));
    }

    template <typename T>
    Parser<T> brackets(Parser<T> p) const {
        return between(symbol("["), symbol("]"), std::move(p));
    }

    // Separated lists
    template <typename T>
    Parser<std::vector<T>> semi_sep(Parser<T> p) const {
        return sep_by(std::move(p), semi);
    }

    template <typename T>
    Parser<std::vector<T>> semi_sep1(Parser<T> p) const {
        return sep_by1(std::move(p), semi);
    }

    template <typename T>
    Parser<std::vector<T>> comma_sep(Parser<T> p) const {
        return sep_by(std::move(p), comma);
    }

    template <typename T>
    Parser<std::vector<T>> comma_sep1(Parser<T> p) const {
        return sep_by1(std::move(p), comma);
    }

    // Reserved word: match name, ensure not prefix of longer identifier
    Parser<std::monostate> reserved(std::string_view name) const {
        return lexeme(make_reserved(name));
    }

    // Reserved operator: match name, ensure not prefix of longer operator
    Parser<std::monostate> reserved_op(std::string_view name) const {
        return lexeme(make_reserved_op(name));
    }

    // Public parsers
    Parser<std::monostate> white_space;
    Parser<std::string> semi;
    Parser<std::string> comma;
    Parser<std::string> colon;
    Parser<std::string> dot;
    Parser<int64_t> decimal;
    Parser<int64_t> hexadecimal;
    Parser<int64_t> octal;
    Parser<int64_t> natural;
    Parser<int64_t> integer;
    Parser<double> float_;
    Parser<char> char_literal;
    Parser<std::string> string_literal;
    Parser<std::string> identifier;
    Parser<std::string> operator_;

private:
    LanguageDef lang_;

    Parser<std::monostate> make_white_space() const {
        if (lang_.comment_start.empty() && lang_.comment_line.empty()) {
            return skip_many(space());
        }

        std::vector<Parser<std::monostate>> parsers;
        parsers.push_back(skip_many1(space()));

        if (!lang_.comment_line.empty()) {
            auto line_comment = try_parse(string_(lang_.comment_line))
                > skip_many(satisfy([](char c) { return c != '\n'; }));
            parsers.push_back(line_comment.map([](auto) { return std::monostate{}; }));
        }

        if (!lang_.comment_start.empty() && !lang_.comment_end.empty()) {
            std::string start_str = lang_.comment_start;
            std::string end_str = lang_.comment_end;
            bool nested = lang_.nested_comments;

            // Collect first chars of start/end markers
            std::string marker_chars;
            if (!start_str.empty()) marker_chars += start_str[0];
            if (!end_str.empty() && marker_chars.find(end_str[0]) == std::string::npos) {
                marker_chars += end_str[0];
            }

            auto start_p = try_parse(string_(start_str));
            auto end_p = try_parse(string_(end_str));
            auto scan_non_markers = skip_many1(none_of(marker_chars));
            auto scan_one_marker = one_of(marker_chars);

            auto block_comment = Parser<std::monostate>(
                [start_p, end_p, scan_non_markers, scan_one_marker, start_str, nested]
                (const State<>& state) -> ParseResult<std::monostate> {
                    auto res = start_p(state);
                    if (res.is_error()) {
                        return ParseResult<std::monostate>{std::get<Err>(res.reply), res.consumed};
                    }

                    auto curr_state = std::get<Ok<std::string>>(res.reply).state;
                    int nesting = 1;

                    while (nesting > 0) {
                        auto res_end = end_p(curr_state);
                        if (res_end.is_ok()) {
                            --nesting;
                            curr_state = std::get<Ok<std::string>>(res_end.reply).state;
                            continue;
                        }

                        if (nested) {
                            auto res_start = start_p(curr_state);
                            if (res_start.is_ok()) {
                                ++nesting;
                                curr_state = std::get<Ok<std::string>>(res_start.reply).state;
                                continue;
                            }
                        }

                        if (curr_state.index >= curr_state.input.size()) {
                            return ParseResult<std::monostate>::error_consumed(
                                ParseError::with_message(curr_state.pos, MessageType::UnExpect, "end of input in comment")
                            );
                        }

                        auto res_skip = scan_non_markers(curr_state);
                        if (res_skip.is_ok()) {
                            curr_state = std::get<Ok<std::monostate>>(res_skip.reply).state;
                            continue;
                        }

                        auto res_mk = scan_one_marker(curr_state);
                        if (res_mk.is_ok()) {
                            curr_state = std::get<Ok<char>>(res_mk.reply).state;
                            continue;
                        }

                        return ParseResult<std::monostate>::error_consumed(
                            ParseError::unknown(curr_state.pos)
                        );
                    }

                    return ParseResult<std::monostate>::ok_consumed(
                        std::monostate{}, curr_state, ParseError::unknown(curr_state.pos)
                    );
                }
            );
            parsers.push_back(block_comment);
        }

        return skip_many(choice(std::move(parsers)));
    }

    Parser<std::string> make_identifier() const {
        auto reserved_names = lang_.reserved_names;
        return lang_.ident_start.bind([ident_letter = lang_.ident_letter, reserved_names](char c) {
            return many(ident_letter).bind([c, reserved_names](std::vector<char> cs) {
                std::string name(1, c);
                name.append(cs.begin(), cs.end());
                if (std::find(reserved_names.begin(), reserved_names.end(), name) != reserved_names.end()) {
                    return fail<std::string>("unexpected reserved name '" + name + "'");
                }
                return pure<std::string>(std::move(name));
            });
        });
    }

    Parser<std::monostate> make_reserved(std::string_view name) const {
        return try_parse(
            string_(name) > not_followed_by(lang_.ident_letter)
        ).map([](auto) { return std::monostate{}; })
        .label("reserved word '" + std::string(name) + "'");
    }

    Parser<std::string> make_operator() const {
        auto reserved_ops = lang_.reserved_op_names;
        return lang_.op_start.bind([op_letter = lang_.op_letter, reserved_ops](char c) {
            return many(op_letter).bind([c, reserved_ops](std::vector<char> cs) {
                std::string op(1, c);
                op.append(cs.begin(), cs.end());
                if (std::find(reserved_ops.begin(), reserved_ops.end(), op) != reserved_ops.end()) {
                    return fail<std::string>("unexpected reserved op '" + op + "'");
                }
                return pure<std::string>(std::move(op));
            });
        });
    }

    Parser<std::monostate> make_reserved_op(std::string_view name) const {
        return try_parse(
            string_(name) > not_followed_by(lang_.op_letter)
        ).map([](auto) { return std::monostate{}; })
        .label("reserved operator '" + std::string(name) + "'");
    }
};

} // namespace parsecpp
