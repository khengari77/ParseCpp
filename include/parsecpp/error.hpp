#pragma once

#include "pos.hpp"

#include <algorithm>
#include <compare>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

namespace parsecpp {

enum class MessageType : uint8_t {
    SysUnExpect = 1,
    UnExpect = 2,
    Expect = 3,
    Msg = 4,
};

struct Message {
    MessageType type;
    std::string text;

    auto operator<=>(const Message&) const = default;
    bool operator==(const Message&) const = default;
};

class ParseError {
public:
    SourcePos pos;
    std::string source_name;
    std::vector<Message> messages;

    ParseError() = default;
    ParseError(SourcePos p, std::vector<Message> msgs)
        : pos(std::move(p)), messages(std::move(msgs)) {}

    bool is_unknown() const { return messages.empty(); }

    ParseError add_message(Message msg) const {
        auto it = std::lower_bound(messages.begin(), messages.end(), msg);
        if (it != messages.end() && *it == msg) return *this;
        auto result = *this;
        auto pos_it = result.messages.begin() + (it - messages.begin());
        result.messages.insert(pos_it, std::move(msg));
        return result;
    }

    ParseError set_messages(std::vector<Message> msgs) const {
        std::sort(msgs.begin(), msgs.end());
        msgs.erase(std::unique(msgs.begin(), msgs.end()), msgs.end());
        return ParseError{pos, std::move(msgs)};
    }

    static ParseError unknown(SourcePos p) {
        return ParseError{std::move(p), {}};
    }

    static ParseError with_message(SourcePos p, MessageType type, std::string text) {
        return ParseError{std::move(p), {Message{type, std::move(text)}}};
    }

    static ParseError merge(const ParseError& e1, const ParseError& e2) {
        if (e1.is_unknown()) return e2;
        if (e2.is_unknown()) return e1;

        if (e1.pos > e2.pos) return e1;
        if (e2.pos > e1.pos) return e2;

        // Same position — linear merge (both vectors are sorted + deduplicated)
        std::vector<Message> combined;
        combined.reserve(e1.messages.size() + e2.messages.size());
        std::set_union(
            e1.messages.begin(), e1.messages.end(),
            e2.messages.begin(), e2.messages.end(),
            std::back_inserter(combined)
        );
        return ParseError{e1.pos, std::move(combined)};
    }

    std::string format() const {
        auto format_location = [this]() -> std::string {
            std::ostringstream os;
            if (!source_name.empty()) {
                os << '"' << source_name << "\" ";
            }
            os << pos;
            return os.str();
        };

        if (messages.empty()) {
            return "Unknown parse error at " + format_location();
        }

        std::vector<std::string> expects, unexpects, others;

        for (const auto& m : messages) {
            switch (m.type) {
                case MessageType::Expect:
                    expects.push_back(m.text);
                    break;
                case MessageType::SysUnExpect:
                case MessageType::UnExpect:
                    unexpects.push_back(m.text);
                    break;
                case MessageType::Msg:
                    others.push_back(m.text);
                    break;
            }
        }

        auto dedup = [](std::vector<std::string>& v) {
            std::sort(v.begin(), v.end());
            v.erase(std::unique(v.begin(), v.end()), v.end());
        };
        dedup(expects);
        dedup(unexpects);
        dedup(others);

        std::ostringstream os;
        os << "Parse error at " << format_location() << ": ";

        std::vector<std::string> parts;

        if (!unexpects.empty()) {
            if (unexpects.size() == 1 && unexpects.front().empty()) {
                parts.push_back("unexpected end of input");
            } else {
                std::ostringstream u;
                u << "unexpected ";
                bool first = true;
                for (const auto& s : unexpects) {
                    if (!first) u << ", or ";
                    u << s;
                    first = false;
                }
                parts.push_back(u.str());
            }
        }

        if (!expects.empty()) {
            std::ostringstream e;
            e << "expecting ";
            bool first = true;
            for (const auto& s : expects) {
                if (!first) e << ", or ";
                e << s;
                first = false;
            }
            parts.push_back(e.str());
        }

        for (const auto& s : others) {
            parts.push_back(s);
        }

        bool first = true;
        for (const auto& p : parts) {
            if (!first) os << "; ";
            os << p;
            first = false;
        }

        return os.str();
    }

    friend std::ostream& operator<<(std::ostream& os, const ParseError& err) {
        return os << err.format();
    }
};

} // namespace parsecpp
