#pragma once

#include "token.hpp"

namespace parsecpp {
namespace language {

inline LanguageDef empty_def() {
    return LanguageDef{
        .comment_start = "",
        .comment_end = "",
        .comment_line = "",
        .nested_comments = true,
        .ident_start = letter() | char_('_'),
        .ident_letter = alpha_num() | one_of("_'"),
        .op_start = one_of(":!#$%&*+./<=>?@\\^|-~"),
        .op_letter = one_of(":!#$%&*+./<=>?@\\^|-~"),
        .reserved_names = {},
        .reserved_op_names = {},
        .case_sensitive = true,
    };
}

inline LanguageDef haskell_style() {
    auto def = empty_def();
    def.comment_start = "{-";
    def.comment_end = "-}";
    def.comment_line = "--";
    def.nested_comments = true;
    def.ident_start = letter();
    def.ident_letter = alpha_num() | one_of("_'");
    return def;
}

inline LanguageDef java_style() {
    auto def = empty_def();
    def.comment_start = "/*";
    def.comment_end = "*/";
    def.comment_line = "//";
    def.nested_comments = true;
    def.ident_start = letter();
    def.ident_letter = alpha_num() | one_of("_'");
    def.case_sensitive = false;
    return def;
}

inline LanguageDef python_style() {
    auto def = empty_def();
    def.comment_line = "#";
    def.nested_comments = false;
    def.ident_start = letter() | char_('_');
    def.ident_letter = alpha_num() | char_('_');
    def.reserved_names = {
        "def", "class", "if", "else", "elif", "while", "for", "return",
        "import", "from", "try", "except", "raise", "pass", "with", "as",
        "lambda", "yield", "None", "True", "False", "await", "async"
    };
    def.reserved_op_names = {
        "+", "-", "*", "/", "%", "**", "//", "==", "!=",
        "<", ">", "<=", ">=", "=", "+=", "-=", "*=", "/="
    };
    return def;
}

inline LanguageDef haskell98_def() {
    auto def = haskell_style();
    def.reserved_op_names = {"::", "..", "=", "\\", "|", "<-", "->", "@", "~", "=>"};
    def.reserved_names = {
        "let", "in", "case", "of", "if", "then", "else", "data", "type",
        "class", "default", "deriving", "do", "import", "infix", "infixl",
        "infixr", "instance", "module", "newtype", "where", "primitive"
    };
    return def;
}

inline LanguageDef haskell_def() {
    auto def = haskell98_def();
    def.ident_letter = def.ident_letter | char_('#');
    auto names = def.reserved_names;
    names.insert(names.end(), {"foreign", "export", "_ccall_", "_casm_", "forall"});
    def.reserved_names = std::move(names);
    return def;
}

} // namespace language
} // namespace parsecpp
