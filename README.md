# parsec++

A C++23 port of Haskell's [Parsec](https://github.com/haskell/parsec) parser combinator library. Header-only, zero dependencies at runtime, with the same semantics as the original: consumed/empty tracking, predictive parsing with single-token lookahead, error merging by furthest position, and ghost errors for precise diagnostics.

## Features

- **Monadic parser combinators** -- `bind`, `map`, `operator|` (choice), `operator>` (sequence), with full backtracking via `try_parse`
- **Character parsers** -- `char_`, `string_`, `digit`, `letter`, `satisfy`, `one_of`, `none_of`, and more
- **Repetition** -- `many`, `many1`, `skip_many`, `take_while`, `take_while1`
- **Combinators** -- `choice`, `between`, `sep_by`, `end_by`, `chainl1`, `chainr1`, `option`, `optional_`, `many_till`, `eof`
- **Expression parser** -- `build_expression_parser` with precedence table, infix/prefix/postfix operators, and left/right/none associativity
- **Token parser** -- `LanguageDef` and `TokenParser` for building lexers with whitespace handling, identifiers, reserved words, operators, string/char literals, and number parsing
- **Predefined language definitions** -- `haskell_style`, `java_style`, `haskell_def`, `haskell98_def`
- **Lazy evaluation** -- `lazy()` for recursive grammars, using `std::call_once` for thread-safe memoization
- **Error messages** -- Position-tracked errors with expected/unexpected reporting and automatic merging
- **Zero-copy parsing** -- Uses `std::string_view` with index-based advancement
- **User state** -- Thread user-defined state through the parse via the `UserState` template parameter

## Requirements

- C++23 compiler (GCC 13+, Clang 17+, MSVC 19.36+)
- CMake 3.20+

## Building

```bash
cmake -B build
cmake --build build
```

To disable tests or examples:

```bash
cmake -B build -DPARSECPP_BUILD_TESTS=OFF -DPARSECPP_BUILD_EXAMPLES=OFF
```

## Running Tests

Tests use [Catch2](https://github.com/catchorg/Catch2) and [RapidCheck](https://github.com/emil-e/rapidcheck) (property-based testing). Both are fetched automatically via CMake FetchContent.

```bash
cmake --build build
cd build && ctest --output-on-failure
```

## Running Examples

After building, two example programs are available in the `build/examples/` directory:

```bash
# Interactive calculator -- enter arithmetic expressions, get results
./build/examples/calculator

# JSON parser -- reads JSON from stdin and prints the parsed structure
echo '{"name": "parsec++", "version": 1}' | ./build/examples/json_parser
```

## Quick Start

```cpp
#include <parsecpp/parsecpp.hpp>
#include <iostream>

using namespace parsecpp;

int main() {
    // Parse a greeting
    auto greeting = string_("hello") > spaces() > take_while1([](char c) {
        return std::isalpha(static_cast<unsigned char>(c));
    });

    auto result = run_parser(greeting, "hello world");
    if (result) {
        std::cout << "Greeted: " << *result << "\n";  // "world"
    }
}
```

## Examples

### Calculator with Operator Precedence

```cpp
#include <parsecpp/parsecpp.hpp>

using namespace parsecpp;

Parser<double> expr();

Parser<double> number() {
    return take_while1([](char c) {
        return std::isdigit(static_cast<unsigned char>(c)) || c == '.';
    }).map([](std::string s) { return std::stod(s); });
}

Parser<double> factor() {
    return between(char_('('), char_(')'), lazy<double>([]() { return expr(); }))
         | number();
}

Parser<double> expr() {
    return build_expression_parser<double>(
        {
            // Higher precedence first (innermost)
            {
                Infix<double>{
                    char_('*').map([](char) -> std::function<double(double, double)> {
                        return [](double a, double b) { return a * b; };
                    }),
                    Assoc::Left
                },
            },
            // Lower precedence last (outermost)
            {
                Infix<double>{
                    char_('+').map([](char) -> std::function<double(double, double)> {
                        return [](double a, double b) { return a + b; };
                    }),
                    Assoc::Left
                },
            },
        },
        factor()
    );
}
```

### JSON Parser

```cpp
auto null_val  = string_("null").map([](auto) -> JsonValue { return nullptr; });
auto true_val  = string_("true").map([](auto) -> JsonValue { return true; });
auto false_val = string_("false").map([](auto) -> JsonValue { return false; });

auto json_string = between(char_('"'), char_('"'),
    many(satisfy([](char c) { return c != '"'; }))
        .map([](std::vector<char> cs) -> JsonValue {
            return std::string(cs.begin(), cs.end());
        })
);

auto value = spaces() > choice<JsonValue>({null_val, true_val, false_val, json_string});
```

### Backtracking with try_parse

```cpp
// Without try_parse, string_("ab") consumes 'a' then fails on 'c',
// producing a consumed error that prevents trying the alternative.
// try_parse converts consumed errors to empty errors, enabling backtracking.
auto p = try_parse(string_("ab")) | string_("ac");
auto result = run_parser(p, "ac");  // succeeds with "ac"
```

## Module Overview

| Header | Contents |
|---|---|
| `pos.hpp` | `SourcePos`, position update functions |
| `error.hpp` | `ParseError`, `Message`, error formatting and merging |
| `state.hpp` | `State<UserState>`, parse state with user-defined state |
| `result.hpp` | `Ok`, `Err`, `ParseResult` with consumed/empty tracking |
| `parser.hpp` | `Parser<T>` core class with `bind`, `map`, `label`, operators |
| `prim.hpp` | `pure`, `fail`, `try_parse`, `look_ahead`, `many`, `lazy`, `run_parser` |
| `char.hpp` | `satisfy`, `char_`, `string_`, `digit`, `letter`, `space`, `one_of`, ... |
| `combinator.hpp` | `choice`, `between`, `sep_by`, `chainl1`, `eof`, `not_followed_by`, ... |
| `expr.hpp` | `build_expression_parser`, `Infix`, `Prefix`, `Postfix`, `Assoc` |
| `token.hpp` | `LanguageDef`, `TokenParser` for building lexers |
| `language.hpp` | `haskell_style`, `java_style`, `haskell_def`, predefined language defs |
| `parsecpp.hpp` | Umbrella header -- includes everything |

## Installation

### CMake FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(parsecpp
    GIT_REPOSITORY https://github.com/yourusername/parsecpp.git
    GIT_TAG main
)
FetchContent_MakeAvailable(parsecpp)

target_link_libraries(your_target PRIVATE parsecpp)
```

### Manual

Copy the `include/parsecpp/` directory into your project and add it to your include path. No compilation needed -- it is header-only.

## License

MIT
