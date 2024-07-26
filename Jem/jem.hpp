#include <functional>
#include <memory>
#include <string>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <variant>
#include <cstdint>
#include <iterator>
#include <stack>
#include <optional>
#include <iostream>


#pragma once
namespace {
enum TokenType : uint8_t {
    PUNC, STRING, NUMBER, BOOL, NONE, _NONE
};

struct Token {
    TokenType type;
    std::string value;
};


class InputStream {
    std::tuple<size_t, size_t, size_t> cache;
    std::string m_CurrentLine{};
    std::string m_Source{};
    size_t prev_col_state = 0;

public:
    std::size_t Pos = 0, Line = 1, Col = 0;
    std::unordered_map<std::size_t, std::string> LineMap{};

    InputStream() = default;
    explicit InputStream(std::string&& _source): m_Source(_source) {}

    /** @brief Returns the next value without discarding it */
    char peek() {
        return m_Source.at(Pos);
    }

    /** @brief returns the next value and discards it */
    char next() {
        char chr = m_Source.at(Pos++);
        if (chr == '\n') {
            prev_col_state = Col;
            LineMap[Line] = m_CurrentLine;
            m_CurrentLine.clear();
            Line++;
            Col = 0;
        } else {
            Col++;
            m_CurrentLine += chr;
        }
        return chr;
    }

    /** @brief back off by one char **/
    void backoff() {
        char chr = m_Source.at(--Pos);
        if (chr == '\n') {
            Line--;
            Col = prev_col_state;
        } else {
            Col--;
        }
    }

    /** @brief saves the current state */
    void setReturnPoint() {
        cache = {Pos, Line, Col};
    }

    /** @brief restores cache */
    void restoreCache() {
        std::tie(Pos, Line, Col) = cache;
    }

    /** @brief resets the state of the m_Stream */
    void reset() {
        Col = Pos = 0; Line = 1;
    }

    /** @brief returns true if no more chars are left in the m_Stream */
    bool eof() {
        return Pos == m_Source.size();
    }

    std::string getCurrentLine() const {
        return m_CurrentLine;
    }
};

using namespace std::string_view_literals;

class TokenStream {
private:
    struct StreamState {
        std::size_t Line, Pos, Col;
    };

    std::string                 m_Ret;             // temporary cache
    std::string                 m_Rax;             // temporary cache
    Token                       m_lastTok{};
    Token                       m_Cur{};
    StreamState                 m_Cache{};
    InputStream                 m_Stream;          // InputStream instance


    static bool isDigit(char chr) {
        return ('0' <= chr && chr <= '9');
    }

    static bool isId(char chr) {
        return ('A' <= chr && chr <= 'Z') || ('a' <= chr && chr <= 'z');
    }

    static bool isPunctuation(char chr) {
        return "();,{}[]"sv.find(chr) >= 0;
    }

    static bool isOpChar(char _chr) {
        return "+-/*><="sv.find(_chr) != std::string::npos;
    }

    static bool isWhiteSpace(char _chr) {
        return " \t\n"sv.find(_chr) != std::string::npos;
    }

    /* Read and return the characters until the given boolean function evaluates to true. */
    std::string readWhile(const std::function<bool (char)>& delimiter) {
        std::string ret;
        while (!m_Stream.eof()) {
            if (delimiter(m_Stream.peek())) {
                ret += m_Stream.next();
            } else { break; }
        }
        return ret;
    }

    /* Used to start reading a m_Stream of characters till the `_end` char is hit. */
    std::string readEscaped(char _end) {
        uint8_t is_escaped = false;
        std::string ret;

        m_Stream.next();
        while (!m_Stream.eof()) {
            char chr = m_Stream.next();
            if (is_escaped) {
                ret += chr;
                is_escaped = false;
            } else if (chr == '\\') {
                ret += '\\';
                is_escaped = true;
            }
            else if (chr == _end)
                break;
            else
                ret += chr;
        }

        return ret;
    }

    Token readString(char del = '"') {
        m_Ret = readEscaped(del);
        return {STRING, m_Ret};
    }

    Token readNumber(const char* neg = "") {
        static uint8_t has_decim = false;
        std::string number = readWhile([](char ch) -> bool {
            if (ch == '.') {
                if (has_decim) return false;
                has_decim = true;
                return true;
            } return isDigit(ch);
        });
        has_decim = false;
        m_Ret = number;
        return {NUMBER, neg + m_Ret};
    }

    /* Consume the next token from the m_Stream. */
    Token readNextTok() {
        setReturnPoint();

        if (m_Stream.eof()) {return {NONE, "null"};}
        auto chr = m_Stream.peek();
        if (chr == '"') return readString();
        if (chr == '\'') return readString('\'');
        if (isDigit(chr)) return readNumber();
        if (chr == 't') {
            return {
                .type  = BOOL,
                .value = readWhile(isId)
            };
        }

        if (chr == 'f') {
            return {
                .type = BOOL,
                .value = readWhile(isId)
            };
        }

        m_Ret = std::string(1, m_Stream.next());

        if (isPunctuation(chr) ) return {
                    PUNC,
                    m_Ret,
            };


        throw std::runtime_error("[FATAL]: No valid token found");
    }


public:
    Token p_CurTk{_NONE, ""};
    Token p_PeekTk{_NONE, ""};

    TokenStream() = default;
    explicit TokenStream(InputStream& _stream) : m_Stream(_stream) {}

    void setReturnPoint() {
        m_Cache.Line = m_Stream.Line;
        m_Cache.Pos  = m_Stream.Pos;
        m_Cache.Col  = m_Stream.Col;
    }

    void restoreCache() {
        m_Stream.Pos  = m_Cache.Pos;
        m_Stream.Line = m_Cache.Line;
        m_Stream.Col  = m_Cache.Col;
    }

    /* An abstraction over readNextTok. */
    Token next(bool _readNewLines = false, bool _readWhitespaces = false, bool _modifyCurTk = true) {
        Token cur_tk = readNextTok();

        // discard whitespaces
        if (!_readWhitespaces)
            while ((cur_tk.type == PUNC && cur_tk.value == " ")) {
                cur_tk = readNextTok();
            }

        // discard newlines
        if (!_readNewLines)
            while (cur_tk.type == PUNC && cur_tk.value == "\n") {
                cur_tk = readNextTok();
            }

        if (_modifyCurTk) { p_CurTk = cur_tk; }
        return cur_tk;
    }


    /* Return the next token from the m_Stream without consuming it. */
    Token peek() {
        setReturnPoint();

        if (m_Stream.eof()) {
            restoreCache();
            return {NONE, "NULL"};  // Return token with type NONE and empty value
        }
        p_PeekTk = next(false, false, false);
        restoreCache();
        return p_PeekTk;
    }

    bool eof() const {
        return p_CurTk.type == NONE;
    }
};
}

namespace jem {

struct JSON_t;

using JSObject = std::unordered_map<std::string, JSON_t>;
using JSList   = std::vector<JSON_t>;

struct JSON_t : std::variant<std::string, bool, JSObject, JSList> {
    using variant::variant;
};


class Json {
    using variant_t = std::variant<JSObject, std::vector<JSON_t>>;
    std::stack<std::shared_ptr<variant_t>> m_Scopes;

    std::ifstream m_FStream;
    std::string   m_Src;

    InputStream   m_IS;
    TokenStream   m_Stream;
    JSON_t        m_JSON;

    // this method assumes the stream to be +1 after the opening brace
    void parseObject() {
        std::string cur_key;

        if (m_Stream.p_CurTk.type == STRING) {
            cur_key = m_Stream.p_CurTk.value;

            switch (m_Stream.next().type) {
                case PUNC:
                    break;
                case STRING:
                    break;
                case NUMBER:
                    break;
                case BOOL:
                    break;
                default:
                    break;
            }
        } else if (m_Stream.p_CurTk.type == PUNC && m_Stream.p_CurTk.value == "}") {
            *m_Scopes.top() = JSObject();
            m_Scopes.pop();
        }
    }

    void parseList() {

    }

    template <typename T>
    void parseLiteral(const std::optional<std::string>& key = std::nullopt) {
        static_assert(std::is_same_v<T, bool> || std::is_same_v<T, std::string>);

        // Assuming the current token to be the literal itself
        if (key.has_value()) {
            auto obj = std::get<JSObject>(*m_Scopes.top());
            if constexpr (std::is_same_v<T, bool>)
                 obj[key.value()] = m_Stream.p_CurTk.value[0] == 't';
            else obj[key.value()] = m_Stream.p_CurTk.value;
        } else if (!m_Scopes.empty() && std::holds_alternative<JSList>(*m_Scopes.top())) {
            auto obj = std::get<JSList>(*m_Scopes.top());
            if constexpr (std::is_same_v<T, bool>)
                 obj.emplace_back(m_Stream.p_CurTk.value[0] == 't');
            else obj.emplace_back(m_Stream.p_CurTk.value);
        } else {
            if constexpr (std::is_same_v<T, bool>)
                 m_JSON = m_Stream.p_CurTk.value[0] == 't';
            else m_JSON = m_Stream.p_CurTk.value;
        }
    }

public:
    Json() = default;

    explicit Json(const std::string& path) : m_FStream(path) {
        if (!m_FStream.is_open())
            throw std::runtime_error("Unable to read the file: " + path);

        m_Src = {std::istreambuf_iterator<char>(m_FStream), std::istreambuf_iterator<char>()};
        m_IS  = InputStream(std::move(m_Src));
        m_Stream = TokenStream(m_IS);
    }

    void constructWithString(const std::string& source) noexcept { m_Src = source; }

    const JSON_t& dump(const std::optional<std::string>& key = std::nullopt) {
        Token ftk     = m_Stream.next();
        bool  is_obj  = false;

        switch (ftk.type) {
            case PUNC:
                if (ftk.value == "{") {
                    auto o = std::make_shared<variant_t>(JSObject());
                    m_Scopes.push(o);
                    m_JSON = std::get<JSObject>(*o);
                    is_obj = true;
                } else {
                    auto o = std::make_shared<variant_t>(JSList());
                    m_Scopes.push(o);
                    m_JSON = std::get<JSList>(*o);
                } break;
            case BOOL:
                parseLiteral<bool>();
                return m_JSON;
            default:
                parseLiteral<std::string>();
                return m_JSON;
        }

        while (!m_Stream.eof()) {
            Token e = m_Stream.next();
            if (is_obj) {
                m_Stream.next();
            }
        }
        return m_JSON;
    }
};

}
