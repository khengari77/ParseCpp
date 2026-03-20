#pragma once

// Parsec++ — A C++ port of Haskell's Parsec parser combinator library
//
// Usage:
//   #include <parsecpp/parsecpp.hpp>
//   using namespace parsecpp;
//
//   auto p = char_('a') > digit();
//   auto result = run_parser(p, "a5");

#include "pos.hpp"
#include "error.hpp"
#include "state.hpp"
#include "result.hpp"
#include "parser.hpp"
#include "prim.hpp"
#include "char.hpp"
#include "combinator.hpp"
#include "expr.hpp"
#include "token.hpp"
#include "language.hpp"
