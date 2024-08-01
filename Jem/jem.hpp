#include <functional>
#include <string>
#include <fstream>
#include <unordered_map>
#include <utility>
#include <vector>
#include <variant>
#include <cstdint>
#include <optional>
#include <iostream>
#include <filesystem>

#pragma once
namespace {
enum TokenType {
    PUNC, STRING, NUMBER, BOOL, J_NULL, NONE, _NONE
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

        if (chr == 'n') {
            return {
                .type = J_NULL,
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
    Token next(bool modify_pCur = true) {
        Token cur_tk = readNextTok();

        // discard all the junk
        while (cur_tk.type == PUNC && (cur_tk.value == " " || cur_tk.value == "\t" || cur_tk.value == "\n")) {
            cur_tk = readNextTok();
        }

        while (cur_tk.type == PUNC && (cur_tk.value == " " || cur_tk.value == "\t" || cur_tk.value == "\n")) {
            cur_tk = readNextTok();
        }


        if (modify_pCur) p_CurTk = cur_tk;
        return cur_tk;
    }


    /* Return the next token from the m_Stream without consuming it. */
    Token peek() {
        setReturnPoint();

        if (m_Stream.eof()) {
            restoreCache();
            return {NONE, "NULL"};  // Return token with type NONE and empty value
        }
        p_PeekTk = next(false);
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


class JSON_t : std::variant<std::string, bool, JSObject, JSList, std::nullptr_t> {
    template<typename T>
    inline void checkSafety() const {
        if (CheckSafety) {
            if (std::holds_alternative<std::nullptr_t>(*this))
                throw std::runtime_error("the JSON value is null");
            if (!std::holds_alternative<T>(*this))
                throw std::runtime_error("call to an incorrect function for the type");
        }
    }

public:
    using variant::variant;
    static bool CheckSafety;

    [[nodiscard]] std::string toString() const {
        checkSafety<std::string>();
        return std::get<std::string>(*this);
    }

    [[nodiscard]] bool toBool() const {
        checkSafety<bool>();
        return std::get<bool>(*this);
    }

    [[nodiscard]] bool isNull() const {
        return std::holds_alternative<std::nullptr_t>(*this);
    }

    [[nodiscard]] JSObject toObject() const {
        checkSafety<JSObject>();
        return std::get<JSObject>(*this);
    }

    [[nodiscard]] JSList toList() const {
        checkSafety<JSList>();
        return std::get<JSList>(*this);
    }

    template <typename ItemType>
    [[nodiscard]] ItemType getAt(std::size_t index) const {
        auto item = this->toList().at(index);
        return std::get<ItemType>(item);
    }

    template <typename ItemType>
    [[nodiscard]] ItemType getFromKey(const std::string& key) {
        return std::get<ItemType>(this->toObject()[key]);
    }

    [[nodiscard]] std::string getStringAt(std::size_t index) const {
        return getAt<std::string>(index);
    }

    [[nodiscard]] bool getBoolAt(std::size_t index) const {
        return getAt<bool>(index);
    }
};

bool JSON_t::CheckSafety = true;

class Json {
    std::ifstream m_FStream;
    std::string   m_Src;

    InputStream   m_IS;
    TokenStream   m_Stream;
    JSON_t        m_JSON;

    JSObject parseObject() {
        // this method assumes the stream to be +1 after the opening brace
        std::string key = m_Stream.p_CurTk.value; m_Stream.next();
        JSObject ret;

        m_Stream.next();  // consume the colon
        while (!(m_Stream.p_CurTk.type == PUNC && m_Stream.p_CurTk.value == "}")) {
            switch (m_Stream.p_CurTk.type) {
                case PUNC:
                    if (m_Stream.p_CurTk.value == ",") {
                        key = m_Stream.next().value;
                        m_Stream.next();
                        continue;
                    }
                    else if (m_Stream.p_CurTk.value == ":") {
                        m_Stream.next();
                        continue;
                    }
                    else if (m_Stream.p_CurTk.value == "{") {
                        m_Stream.next();
                        ret[key] = parseObject();
                        continue;
                    }
                    else if (m_Stream.p_CurTk.value == "[") {
                        m_Stream.next();
                        ret[key] = parseList();
                        continue;
                    }
                    break;
                case BOOL:
                    ret[key] = m_Stream.p_CurTk.value[0] == 't';
                    m_Stream.next();
                    continue;
                    break;
                case J_NULL:
                    ret[key] = nullptr;
                    m_Stream.next();
                    continue;
                default:
                    ret[key] = m_Stream.p_CurTk.value;
                    m_Stream.next();
                    continue;
            }
        }
        m_Stream.next();
        return ret;
    }

    JSList parseList() {
        // this method assumes the stream to be +1 after the opening bracket
        JSList ret;

        while (!(m_Stream.p_CurTk.type == PUNC && m_Stream.p_CurTk.value == "]")) {
            switch (m_Stream.p_CurTk.type) {
                case PUNC:
                    switch (m_Stream.p_CurTk.value[0]) {
                        case '{':
                            m_Stream.next();
                            ret.emplace_back(parseObject());
                            break;
                        case '[':
                            m_Stream.next();
                            ret.emplace_back(parseList());
                            break;
                        case ',':
                            m_Stream.next();
                            continue;
                    } break;
                case BOOL:
                    ret.emplace_back(m_Stream.p_CurTk.value[0] == 't');
                    m_Stream.next();
                    break;
                case J_NULL:
                    ret.emplace_back(nullptr);
                    m_Stream.next();
                    break;
                default:
                    ret.emplace_back(std::move(m_Stream.p_CurTk.value));
                    m_Stream.next();
                    break;
            }
        }

        m_Stream.next();
        return ret;
    }

public:
    Json() = default;

    explicit Json(const std::filesystem::path& path) : m_FStream(path) {
        if (!m_FStream.is_open())
            throw std::runtime_error("Unable to read the file: " + path.string());

        m_Src = {std::istreambuf_iterator<char>(m_FStream), std::istreambuf_iterator<char>()};
        m_IS  = InputStream(std::move(m_Src));
        m_Stream = TokenStream(m_IS);
    }

    explicit Json(std::string source) noexcept : m_Src(std::move(source)) {}

    const JSON_t& dump(const std::optional<std::string>& key = std::nullopt) {
        Token ftk    = m_Stream.next();

        switch (ftk.type) {
            case PUNC:
                if (ftk.value == "{") {
                    m_Stream.next();
                    m_JSON = parseObject();
                } else {
                    m_Stream.next();
                    m_JSON = parseList();
                } break;
            case BOOL:
                m_JSON = ftk.value[0] == 't';
                return m_JSON;
            case J_NULL:
                m_JSON = nullptr;
                return m_JSON;
            default:
                m_JSON = std::move(ftk.value);
        }

        // uncomment to check the stream's output (for debugging purpose)
//        while (!m_Stream.eof()) {
//
//            std::cout << m_Stream.p_CurTk.type << " : " << m_Stream.p_CurTk.value << std::endl;
//            m_Stream.next();
//        }
        return m_JSON;
    }
};

}
