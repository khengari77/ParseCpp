#pragma once

#include <compare>
#include <ostream>
#include <string>
#include <string_view>

namespace parsecpp {

struct SourcePos {
    int line = 1;
    int column = 1;
    std::string name;

    auto operator<=>(const SourcePos& other) const {
        if (auto cmp = line <=> other.line; cmp != 0) return cmp;
        return column <=> other.column;
    }

    bool operator==(const SourcePos& other) const {
        return line == other.line && column == other.column;
    }

    friend std::ostream& operator<<(std::ostream& os, const SourcePos& pos) {
        if (!pos.name.empty()) {
            os << '"' << pos.name << "\" ";
        }
        os << "(line " << pos.line << ", column " << pos.column << ')';
        return os;
    }
};

inline SourcePos initial_pos(std::string_view name = "") {
    return SourcePos{1, 1, std::string(name)};
}

inline SourcePos update_pos_char(SourcePos pos, char c) {
    if (c == '\n') {
        return SourcePos{pos.line + 1, 1, std::move(pos.name)};
    } else if (c == '\t') {
        int tab_width = 8;
        int new_column = pos.column + tab_width - ((pos.column - 1) % tab_width);
        return SourcePos{pos.line, new_column, std::move(pos.name)};
    } else {
        return SourcePos{pos.line, pos.column + 1, std::move(pos.name)};
    }
}

inline SourcePos update_pos_string(SourcePos pos, std::string_view s) {
    if (s.empty()) return pos;

    // Check for tabs — must process char by char
    if (s.find('\t') != std::string_view::npos) {
        for (char c : s) {
            pos = update_pos_char(std::move(pos), c);
        }
        return pos;
    }

    // Count newlines
    size_t newlines = 0;
    size_t last_newline = std::string_view::npos;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\n') {
            ++newlines;
            last_newline = i;
        }
    }

    if (newlines == 0) {
        return SourcePos{pos.line, pos.column + static_cast<int>(s.size()), std::move(pos.name)};
    }

    int new_column = static_cast<int>(s.size() - last_newline);
    return SourcePos{pos.line + static_cast<int>(newlines), new_column, std::move(pos.name)};
}

} // namespace parsecpp
