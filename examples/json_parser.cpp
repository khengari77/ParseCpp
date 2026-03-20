#include <parsecpp/parsecpp.hpp>
#include <iostream>
#include <map>
#include <variant>
#include <memory>

using namespace parsecpp;

// Forward declaration for recursive JSON type
struct JsonValue;
using JsonPtr = std::shared_ptr<JsonValue>;
using JsonArray = std::vector<JsonPtr>;
using JsonObject = std::vector<std::pair<std::string, JsonPtr>>;

struct JsonValue {
    std::variant<
        std::nullptr_t,
        bool,
        double,
        std::string,
        JsonArray,
        JsonObject
    > data;
};

JsonPtr make_json(auto val) {
    return std::make_shared<JsonValue>(JsonValue{std::move(val)});
}

void print_json(const JsonPtr& val, int indent = 0);

// Parser
Parser<JsonPtr> json_value();

Parser<JsonPtr> json_null() {
    return string_("null").map([](auto) { return make_json(nullptr); });
}

Parser<JsonPtr> json_bool() {
    return string_("true").map([](auto) { return make_json(true); })
         | string_("false").map([](auto) { return make_json(false); });
}

Parser<JsonPtr> json_number() {
    auto sign = option<std::string>("", char_('-').map([](char c) { return std::string(1, c); }));
    auto digits = take_while1([](char c) { return std::isdigit(static_cast<unsigned char>(c)); });
    auto frac = (char_('.') > digits).map([](std::string d) { return "." + d; });
    auto exp_part = (one_of("eE") > option<std::string>("", one_of("+-").map([](char c) { return std::string(1, c); })))
        .bind([](std::string s) {
            return take_while1([](char c) { return std::isdigit(static_cast<unsigned char>(c)); })
                .map([s](std::string d) { return "e" + s + d; });
        });

    return sign.bind([digits, frac, exp_part](std::string s) {
        return digits.bind([s, frac, exp_part](std::string d) {
            return option<std::string>("", frac).bind([s, d, exp_part](std::string f) {
                return option<std::string>("", exp_part).map([s, d, f](std::string e) {
                    return make_json(std::stod(s + d + f + e));
                });
            });
        });
    });
}

Parser<JsonPtr> json_string_val() {
    auto escape = char_('\\') > choice<char>({
        char_('"'), char_('\\'), char_('/'),
        char_('n').map([](char) -> char { return '\n'; }),
        char_('t').map([](char) -> char { return '\t'; }),
        char_('r').map([](char) -> char { return '\r'; }),
        char_('b').map([](char) -> char { return '\b'; }),
        char_('f').map([](char) -> char { return '\f'; }),
    });
    auto str_char = satisfy([](char c) { return c != '"' && c != '\\'; }) | escape;

    return between(char_('"'), char_('"'),
        many(str_char).map([](std::vector<char> cs) {
            return make_json(std::string(cs.begin(), cs.end()));
        })
    );
}

Parser<JsonPtr> json_array() {
    auto ws = spaces();
    return between(
        char_('[') > ws,
        ws > char_(']'),
        sep_by(
            ws > lazy<JsonPtr>([]() { return json_value(); }).skip(spaces()),
            char_(',')
        )
    ).map([](std::vector<JsonPtr> items) {
        return make_json(JsonArray(std::move(items)));
    });
}

Parser<JsonPtr> json_object() {
    auto ws = spaces();
    auto key = between(char_('"'), char_('"'),
        many(satisfy([](char c) { return c != '"'; })).map([](std::vector<char> cs) {
            return std::string(cs.begin(), cs.end());
        })
    );

    auto pair = (ws > key).skip(ws).skip(char_(':')).bind([](std::string k) {
        return (spaces() > lazy<JsonPtr>([]() { return json_value(); })).map([k = std::move(k)](JsonPtr v) {
            return std::make_pair(k, v);
        });
    });

    return between(
        char_('{') > ws,
        ws > char_('}'),
        sep_by(pair, char_(','))
    ).map([](std::vector<std::pair<std::string, JsonPtr>> pairs) {
        return make_json(JsonObject(std::move(pairs)));
    });
}

Parser<JsonPtr> json_value() {
    auto ws = spaces();
    return ws > choice<JsonPtr>({
        json_null(),
        json_bool(),
        json_number(),
        json_string_val(),
        json_array(),
        json_object()
    });
}

void print_json(const JsonPtr& val, int indent) {
    std::string pad(indent, ' ');
    std::visit([&](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::nullptr_t>) {
            std::cout << "null";
        } else if constexpr (std::is_same_v<T, bool>) {
            std::cout << (arg ? "true" : "false");
        } else if constexpr (std::is_same_v<T, double>) {
            std::cout << arg;
        } else if constexpr (std::is_same_v<T, std::string>) {
            std::cout << '"' << arg << '"';
        } else if constexpr (std::is_same_v<T, JsonArray>) {
            std::cout << "[\n";
            for (size_t i = 0; i < arg.size(); ++i) {
                std::cout << pad << "  ";
                print_json(arg[i], indent + 2);
                if (i + 1 < arg.size()) std::cout << ",";
                std::cout << "\n";
            }
            std::cout << pad << "]";
        } else if constexpr (std::is_same_v<T, JsonObject>) {
            std::cout << "{\n";
            for (size_t i = 0; i < arg.size(); ++i) {
                std::cout << pad << "  \"" << arg[i].first << "\": ";
                print_json(arg[i].second, indent + 2);
                if (i + 1 < arg.size()) std::cout << ",";
                std::cout << "\n";
            }
            std::cout << pad << "}";
        }
    }, val->data);
}

int main() {
    std::string input = R"({
        "name": "parsec++",
        "version": 0.1,
        "features": ["parser combinators", "backtracking", "error messages"],
        "is_cool": true,
        "bugs": null
    })";

    std::cout << "Parsing JSON:\n" << input << "\n\n";

    auto result = run_parser(json_value().skip(spaces()).skip(eof()), input);
    if (result) {
        std::cout << "Parsed successfully:\n";
        print_json(*result);
        std::cout << "\n";
    } else {
        std::cout << "Parse error: " << result.error() << "\n";
    }
}
