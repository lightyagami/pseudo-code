#include "c_to_pseudo.h"

#include <cctype>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

enum class CTok {
    Eof, Ident, IntLit, RealLit, StrLit, CharLit,
    // Keywords
    Int, Long, Short, Double, Float, Char, Bool, Void,
    Struct, Typedef, Const, Static, Extern, Inline,
    If, Else, While, Do, For, Switch, Case, Default, Break, Continue, Return,
    // Symbols
    LParen, RParen, LBrace, RBrace, LBracket, RBracket,
    Semicolon, Comma, Dot, Arrow,
    Plus, Minus, Star, Slash, Percent,
    PlusPlus, MinusMinus,
    PlusEq, MinusEq, StarEq, SlashEq, PercentEq,
    Eq, EqEq, NotEq, Lt, Le, Gt, Ge,
    Amp, AmpAmp, Pipe, PipePipe, Bang,
    Question, Colon
};

struct CToken {
    CTok type = CTok::Eof;
    std::string text;
    int line = 1;
};

class CLexer {
public:
    explicit CLexer(const std::string& src) : src_(src) {}

    std::vector<CToken> tokenize() {
        std::vector<CToken> tokens;
        while (!atEnd()) {
            skipWhitespaceAndComments();
            if (atEnd()) break;

            char c = peek();
            if (c == '#') {
                // Preprocessor directive: skip until newline
                while (!atEnd() && peek() != '\n') advance();
                continue;
            }

            if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
                tokens.push_back(lexIdentOrKeyword());
            } else if (std::isdigit(static_cast<unsigned char>(c))) {
                tokens.push_back(lexNumber());
            } else if (c == '"') {
                tokens.push_back(lexString());
            } else if (c == '\'') {
                tokens.push_back(lexChar());
            } else {
                tokens.push_back(lexSymbol());
            }
        }
        tokens.push_back({CTok::Eof, "", line_});
        return tokens;
    }

private:
    const std::string& src_;
    size_t pos_ = 0;
    int line_ = 1;

    bool atEnd() const { return pos_ >= src_.size(); }
    char peek() const { return atEnd() ? '\0' : src_[pos_]; }
    char peekNext() const { return (pos_ + 1 >= src_.size()) ? '\0' : src_[pos_ + 1]; }
    char advance() {
        char c = src_[pos_++];
        if (c == '\n') line_++;
        return c;
    }

    void skipWhitespaceAndComments() {
        while (!atEnd()) {
            char c = peek();
            if (std::isspace(static_cast<unsigned char>(c))) {
                advance();
            } else if (c == '/' && peekNext() == '/') {
                while (!atEnd() && peek() != '\n') advance();
            } else if (c == '/' && peekNext() == '*') {
                advance(); advance();
                while (!atEnd() && !(peek() == '*' && peekNext() == '/')) advance();
                if (!atEnd()) { advance(); advance(); }
            } else {
                break;
            }
        }
    }

    CToken lexIdentOrKeyword() {
        int startLine = line_;
        size_t start = pos_;
        while (!atEnd() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_')) {
            advance();
        }
        std::string txt = src_.substr(start, pos_ - start);

        static const std::unordered_map<std::string, CTok> kw = {
            {"int", CTok::Int}, {"long", CTok::Long}, {"short", CTok::Short},
            {"double", CTok::Double}, {"float", CTok::Float}, {"char", CTok::Char},
            {"bool", CTok::Bool}, {"_Bool", CTok::Bool}, {"void", CTok::Void},
            {"struct", CTok::Struct}, {"typedef", CTok::Typedef}, {"const", CTok::Const},
            {"static", CTok::Static}, {"extern", CTok::Extern}, {"inline", CTok::Inline},
            {"if", CTok::If}, {"else", CTok::Else}, {"while", CTok::While}, {"do", CTok::Do},
            {"for", CTok::For}, {"switch", CTok::Switch}, {"case", CTok::Case},
            {"default", CTok::Default}, {"break", CTok::Break}, {"continue", CTok::Continue},
            {"return", CTok::Return}
        };

        auto it = kw.find(txt);
        if (it != kw.end()) return {it->second, txt, startLine};
        return {CTok::Ident, txt, startLine};
    }

    CToken lexNumber() {
        int startLine = line_;
        size_t start = pos_;
        bool isReal = false;
        if (peek() == '0' && (peekNext() == 'x' || peekNext() == 'X')) {
            advance(); advance();
            while (!atEnd() && std::isxdigit(static_cast<unsigned char>(peek()))) advance();
        } else {
            while (!atEnd() && std::isdigit(static_cast<unsigned char>(peek()))) advance();
            if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peekNext()))) {
                isReal = true;
                advance();
                while (!atEnd() && std::isdigit(static_cast<unsigned char>(peek()))) advance();
            }
        }
        // Skip suffixes like U, L, LL, f, etc.
        while (!atEnd() && (peek() == 'u' || peek() == 'U' || peek() == 'l' || peek() == 'L' ||
                            peek() == 'f' || peek() == 'F')) {
            advance();
        }
        std::string txt = src_.substr(start, pos_ - start);
        return {isReal ? CTok::RealLit : CTok::IntLit, txt, startLine};
    }

    CToken lexString() {
        int startLine = line_;
        advance(); // skip "
        std::string s;
        while (!atEnd() && peek() != '"') {
            if (peek() == '\\') {
                advance();
                if (atEnd()) break;
                char esc = advance();
                switch (esc) {
                    case 'n': s += '\n'; break;
                    case 't': s += '\t'; break;
                    case '\\': s += '\\'; break;
                    case '"': s += '"'; break;
                    default: s += esc; break;
                }
            } else {
                s += advance();
            }
        }
        if (!atEnd()) advance(); // skip closing "
        return {CTok::StrLit, s, startLine};
    }

    CToken lexChar() {
        int startLine = line_;
        advance(); // skip '
        std::string s;
        if (!atEnd() && peek() == '\\') {
            advance();
            char esc = advance();
            if (esc == 'n') s = "\n";
            else if (esc == 't') s = "\t";
            else s = std::string(1, esc);
        } else if (!atEnd()) {
            s = std::string(1, advance());
        }
        if (!atEnd() && peek() == '\'') advance();
        return {CTok::CharLit, s, startLine};
    }

    CToken lexSymbol() {
        int startLine = line_;
        char c = advance();
        switch (c) {
            case '(': return {CTok::LParen, "(", startLine};
            case ')': return {CTok::RParen, ")", startLine};
            case '{': return {CTok::LBrace, "{", startLine};
            case '}': return {CTok::RBrace, "}", startLine};
            case '[': return {CTok::LBracket, "[", startLine};
            case ']': return {CTok::RBracket, "]", startLine};
            case ';': return {CTok::Semicolon, ";", startLine};
            case ',': return {CTok::Comma, ",", startLine};
            case '.': return {CTok::Dot, ".", startLine};
            case '?': return {CTok::Question, "?", startLine};
            case ':': return {CTok::Colon, ":", startLine};
            case '+':
                if (peek() == '+') { advance(); return {CTok::PlusPlus, "++", startLine}; }
                if (peek() == '=') { advance(); return {CTok::PlusEq, "+=", startLine}; }
                return {CTok::Plus, "+", startLine};
            case '-':
                if (peek() == '-') { advance(); return {CTok::MinusMinus, "--", startLine}; }
                if (peek() == '=') { advance(); return {CTok::MinusEq, "-=", startLine}; }
                if (peek() == '>') { advance(); return {CTok::Arrow, "->", startLine}; }
                return {CTok::Minus, "-", startLine};
            case '*':
                if (peek() == '=') { advance(); return {CTok::StarEq, "*=", startLine}; }
                return {CTok::Star, "*", startLine};
            case '/':
                if (peek() == '=') { advance(); return {CTok::SlashEq, "/=", startLine}; }
                return {CTok::Slash, "/", startLine};
            case '%':
                if (peek() == '=') { advance(); return {CTok::PercentEq, "%=", startLine}; }
                return {CTok::Percent, "%", startLine};
            case '=':
                if (peek() == '=') { advance(); return {CTok::EqEq, "==", startLine}; }
                return {CTok::Eq, "=", startLine};
            case '!':
                if (peek() == '=') { advance(); return {CTok::NotEq, "!=", startLine}; }
                return {CTok::Bang, "!", startLine};
            case '<':
                if (peek() == '=') { advance(); return {CTok::Le, "<=", startLine}; }
                return {CTok::Lt, "<", startLine};
            case '>':
                if (peek() == '=') { advance(); return {CTok::Ge, ">=", startLine}; }
                return {CTok::Gt, ">", startLine};
            case '&':
                if (peek() == '&') { advance(); return {CTok::AmpAmp, "&&", startLine}; }
                return {CTok::Amp, "&", startLine};
            case '|':
                if (peek() == '|') { advance(); return {CTok::PipePipe, "||", startLine}; }
                return {CTok::Pipe, "|", startLine};
            default:
                return {CTok::Eof, std::string(1, c), startLine};
        }
    }
};

class CToPseudoConverter {
public:
    explicit CToPseudoConverter(const std::vector<CToken>& tokens) : toks_(tokens) {}

    std::string convert() {
        std::ostringstream out;

        while (!atEnd()) {
            if (isPseudocBoilerplate()) {
                skipPseudocBoilerplate();
                continue;
            }

            if (check(CTok::Typedef) || (check(CTok::Struct) && peekNext().type == CTok::Ident)) {
                std::string s = parseStructOrTypedef();
                if (!s.empty()) out << s << "\n";
                continue;
            }

            // Function definition or global variable
            if (isFunctionStart()) {
                std::string fn = parseFunction();
                if (!fn.empty()) out << fn << "\n";
            } else if (isTypeStart()) {
                std::string decl = parseGlobalDecl();
                if (!decl.empty()) out << decl << "\n";
            } else {
                advance();
            }
        }

        return out.str();
    }

private:
    const std::vector<CToken>& toks_;
    size_t cur_ = 0;
    std::unordered_set<std::string> byRefParams_;
    std::unordered_set<std::string> knownProcedures_;

    bool atEnd() const { return cur_ >= toks_.size() || toks_[cur_].type == CTok::Eof; }
    const CToken& peek() const { return toks_[cur_]; }
    const CToken& peekNext() const { return (cur_ + 1 < toks_.size()) ? toks_[cur_ + 1] : toks_.back(); }
    const CToken& previous() const { return toks_[cur_ - 1]; }
    const CToken& advance() { if (!atEnd()) ++cur_; return previous(); }
    bool check(CTok t) const { return !atEnd() && peek().type == t; }
    bool match(CTok t) { if (check(t)) { advance(); return true; } return false; }

    static std::string cleanIdent(const std::string& name) {
        if (name.rfind("pc_type_", 0) == 0) return name.substr(8);
        if (name.rfind("pc_", 0) == 0) return name.substr(3);
        return name;
    }

    bool isPseudocBoilerplate() {
        const std::string& txt = peek().text;
        if (txt == "PC_Node" || txt == "PC_File" || txt == "pc_gc_head" || txt == "pc_files_head" ||
            txt == "pc_track" || txt == "pc_open_file" || txt == "pc_get_file" || txt == "pc_close_file" ||
            txt == "pc_read_file_line" || txt == "pc_write_file_line" || txt == "pc_eof" || txt == "pc_cleanup" ||
            txt == "pc_input_buf" || txt == "pc_read_line" || txt == "pc_read_int" || txt == "pc_read_real" ||
            txt == "pc_read_bool" || txt == "pc_add" || txt == "pc_sub" || txt == "pc_mul" ||
            txt == "pc_bounds_check" || txt == "pc_div" || txt == "pc_mod" || txt == "pc_concat" ||
            txt == "pc_substring" || txt == "pc_ucase" || txt == "pc_lcase" || txt == "pc_num_to_str_int" ||
            txt == "pc_num_to_str_real" || txt == "pc_str_to_num" || txt == "pc_left" || txt == "pc_right" ||
            txt == "pc_chr" || txt == "pc_asc" || txt == "pc_int" || txt == "pc_round" || txt == "pc_rnd") {
            return true;
        }
        return false;
    }

    void skipPseudocBoilerplate() {
        // Skip until the end of function or struct or global variable declaration
        while (!atEnd()) {
            if (check(CTok::LBrace)) {
                skipBlock();
                break;
            }
            if (check(CTok::Semicolon)) {
                advance();
                break;
            }
            advance();
        }
    }

    void skipBlock() {
        if (!match(CTok::LBrace)) return;
        int depth = 1;
        while (!atEnd() && depth > 0) {
            if (check(CTok::LBrace)) depth++;
            else if (check(CTok::RBrace)) depth--;
            advance();
        }
    }

    bool isTypeStart() const {
        CTok t = peek().type;
        return t == CTok::Int || t == CTok::Long || t == CTok::Short || t == CTok::Double ||
               t == CTok::Float || t == CTok::Char || t == CTok::Bool || t == CTok::Void ||
               t == CTok::Const || t == CTok::Static || t == CTok::Struct ||
               (t == CTok::Ident && (peek().text.rfind("pc_type_", 0) == 0 ||
                                     peekNext().type == CTok::Ident || peekNext().type == CTok::Star));
    }

    bool isFunctionStart() {
        size_t saved = cur_;
        while (!atEnd() && (check(CTok::Static) || check(CTok::Inline) || check(CTok::Extern) || check(CTok::Const))) {
            advance();
        }
        if (!isTypeStart()) { cur_ = saved; return false; }
        parseTypeString();
        if (check(CTok::Ident)) {
            advance();
            if (check(CTok::LParen)) {
                cur_ = saved;
                return true;
            }
        }
        cur_ = saved;
        return false;
    }

    std::string parseTypeString(bool* isPtr = nullptr) {
        bool isConst = false;
        while (check(CTok::Static) || check(CTok::Inline) || check(CTok::Extern) || check(CTok::Const)) {
            if (check(CTok::Const)) isConst = true;
            advance();
        }

        std::string t = "INTEGER";
        if (match(CTok::Void)) t = "VOID";
        else if (match(CTok::Int) || match(CTok::Short)) t = "INTEGER";
        else if (match(CTok::Long)) {
            match(CTok::Long);
            t = "INTEGER";
        } else if (match(CTok::Double) || match(CTok::Float)) t = "REAL";
        else if (match(CTok::Bool)) t = "BOOLEAN";
        else if (match(CTok::Char)) {
            if (match(CTok::Star)) t = "STRING";
            else t = "CHAR";
        } else if (match(CTok::Struct)) {
            t = cleanIdent(advance().text);
        } else if (check(CTok::Ident)) {
            t = cleanIdent(advance().text);
        }

        while (match(CTok::Star)) {
            if (t == "CHAR") t = "STRING";
            else if (isPtr) *isPtr = true;
        }

        (void)isConst;
        return t;
    }

    std::string parseStructOrTypedef() {
        if (match(CTok::Typedef)) {
            if (match(CTok::Struct)) {
                std::string tag = check(CTok::Ident) ? advance().text : "";
                if (!match(CTok::LBrace)) return "";
                std::ostringstream ss;
                std::vector<std::pair<std::string, std::string>> fields;
                while (!check(CTok::RBrace) && !atEnd()) {
                    std::string ftype = parseTypeString();
                    std::string fname = cleanIdent(advance().text);
                    match(CTok::Semicolon);
                    fields.emplace_back(fname, ftype);
                }
                match(CTok::RBrace);
                std::string typeName = cleanIdent(advance().text);
                match(CTok::Semicolon);

                if (typeName.empty()) typeName = cleanIdent(tag);
                if (typeName.empty() || typeName == "PC_Node" || typeName == "PC_File") return "";

                ss << "TYPE " << typeName << "\n";
                for (const auto& f : fields) {
                    ss << "    DECLARE " << f.first << " : " << f.second << "\n";
                }
                ss << "ENDTYPE\n";
                return ss.str();
            }
        } else if (match(CTok::Struct)) {
            std::string typeName = cleanIdent(advance().text);
            if (!match(CTok::LBrace)) return "";
            std::ostringstream ss;
            std::vector<std::pair<std::string, std::string>> fields;
            while (!check(CTok::RBrace) && !atEnd()) {
                std::string ftype = parseTypeString();
                std::string fname = cleanIdent(advance().text);
                match(CTok::Semicolon);
                fields.emplace_back(fname, ftype);
            }
            match(CTok::RBrace);
            match(CTok::Semicolon);

            if (typeName.empty() || typeName == "PC_Node" || typeName == "PC_File") return "";
            ss << "TYPE " << typeName << "\n";
            for (const auto& f : fields) {
                ss << "    DECLARE " << f.first << " : " << f.second << "\n";
            }
            ss << "ENDTYPE\n";
            return ss.str();
        }
        return "";
    }

    std::string parseArrayDeclAndInit(const std::string& name, const std::string& type, int lvl, bool isGlobal = false) {
        std::string n1;
        if (!check(CTok::RBracket)) {
            n1 = parseExprString();
        }
        match(CTok::RBracket);

        std::string n2;
        if (match(CTok::LBracket)) {
            if (!check(CTok::RBracket)) {
                n2 = parseExprString();
            }
            match(CTok::RBracket);
        }

        std::vector<std::string> inits1D;
        std::vector<std::vector<std::string>> inits2D;
        bool hasInit = false;

        if (match(CTok::Eq)) {
            hasInit = true;
            if (match(CTok::LBrace)) {
                if (n2.empty()) {
                    // 1D array
                    while (!check(CTok::RBrace) && !atEnd()) {
                        inits1D.push_back(parseExprString());
                        if (!match(CTok::Comma)) break;
                    }
                    match(CTok::RBrace);
                } else {
                    // 2D array
                    while (!check(CTok::RBrace) && !atEnd()) {
                        if (match(CTok::LBrace)) {
                            std::vector<std::string> row;
                            while (!check(CTok::RBrace) && !atEnd()) {
                                row.push_back(parseExprString());
                                if (!match(CTok::Comma)) break;
                            }
                            match(CTok::RBrace);
                            inits2D.push_back(row);
                        } else {
                            inits1D.push_back(parseExprString());
                        }
                        if (!match(CTok::Comma)) break;
                    }
                    match(CTok::RBrace);
                }
            } else {
                std::string sVal = parseExprString();
                match(CTok::Semicolon);
                std::string ind = isGlobal ? "" : indent(lvl);
                return ind + "DECLARE " + name + " : STRING\n" + ind + name + " <- " + sVal;
            }
        }
        match(CTok::Semicolon);

        int span1 = 1;
        if (!n1.empty()) {
            try { span1 = std::stoi(n1); } catch (...) {}
        } else if (!inits1D.empty()) {
            span1 = static_cast<int>(inits1D.size());
        } else if (!inits2D.empty()) {
            span1 = static_cast<int>(inits2D.size());
        }

        int span2 = 0;
        if (!n2.empty()) {
            try { span2 = std::stoi(n2); } catch (...) {}
        } else if (!inits2D.empty() && !inits2D[0].empty()) {
            span2 = static_cast<int>(inits2D[0].size());
        }

        std::string ind = isGlobal ? "" : indent(lvl);
        std::ostringstream ss;
        if (span2 == 0) {
            ss << ind << "DECLARE " << name << " : ARRAY[0:" << (span1 > 0 ? span1 - 1 : 0) << "] OF " << type;
            if (hasInit) {
                bool isZeroInit = (inits1D.size() == 1 && (inits1D[0] == "0" || inits1D[0] == "0.0") && span1 > 1);
                if (!isZeroInit) {
                    for (size_t i = 0; i < inits1D.size(); ++i) {
                        ss << "\n" << ind << name << "[" << i << "] <- " << inits1D[i];
                    }
                }
            }
        } else {
            ss << ind << "DECLARE " << name << " : ARRAY[0:" << (span1 > 0 ? span1 - 1 : 0) << ", 0:" << (span2 > 0 ? span2 - 1 : 0) << "] OF " << type;
            if (hasInit) {
                if (!inits2D.empty()) {
                    for (size_t r = 0; r < inits2D.size(); ++r) {
                        for (size_t c = 0; c < inits2D[r].size(); ++c) {
                            ss << "\n" << ind << name << "[" << r << ", " << c << "] <- " << inits2D[r][c];
                        }
                    }
                } else if (!inits1D.empty()) {
                    bool isZeroInit = (inits1D.size() == 1 && (inits1D[0] == "0" || inits1D[0] == "0.0"));
                    if (!isZeroInit) {
                        for (size_t i = 0; i < inits1D.size(); ++i) {
                            int r = static_cast<int>(i / span2);
                            int c = static_cast<int>(i % span2);
                            ss << "\n" << ind << name << "[" << r << ", " << c << "] <- " << inits1D[i];
                        }
                    }
                }
            }
        }
        return ss.str();
    }

    std::string parseGlobalDecl() {
        bool isConst = false;
        if (check(CTok::Const)) { isConst = true; advance(); }
        while (check(CTok::Static) || check(CTok::Extern)) advance();
        if (check(CTok::Const)) { isConst = true; advance(); }

        std::string type = parseTypeString();
        std::string name = cleanIdent(advance().text);

        // Check if array
        if (match(CTok::LBracket)) {
            return parseArrayDeclAndInit(name, type, 0, true);
        }

        if (match(CTok::Eq)) {
            std::string val = parseExprString();
            match(CTok::Semicolon);
            if (isConst) {
                return "CONSTANT " + name + " = " + val;
            }
            return "DECLARE " + name + " : " + type + "\n" + name + " <- " + val;
        }

        match(CTok::Semicolon);
        return "DECLARE " + name + " : " + type;
    }

    std::string parseFunction() {
        byRefParams_.clear();
        while (check(CTok::Static) || check(CTok::Inline) || check(CTok::Extern)) advance();
        std::string retType = parseTypeString();
        std::string name = cleanIdent(advance().text);

        match(CTok::LParen);
        std::vector<std::string> paramDecls;
        if (!check(CTok::RParen)) {
            while (!atEnd()) {
                if (match(CTok::Void) && check(CTok::RParen)) break;
                bool isPtr = false;
                std::string ptype = parseTypeString(&isPtr);
                if (match(CTok::Star)) {
                    isPtr = true;
                }
                bool isByRef = isPtr && (ptype != "STRING");
                std::string pname = cleanIdent(advance().text);
                if (isByRef) {
                    byRefParams_.insert(pname);
                    paramDecls.push_back("BYREF " + pname + " : " + ptype);
                } else {
                    paramDecls.push_back(pname + " : " + ptype);
                }
                if (!match(CTok::Comma)) break;
            }
        }
        match(CTok::RParen);

        if (name == "main") {
            // Unwrap main() into top-level statements
            if (!match(CTok::LBrace)) return "";
            std::ostringstream ss;
            while (!check(CTok::RBrace) && !atEnd()) {
                std::string st = parseStatement(0);
                if (!st.empty()) ss << st << "\n";
            }
            match(CTok::RBrace);
            return ss.str();
        }

        if (retType == "VOID") {
            knownProcedures_.insert(name);
        }

        std::ostringstream ss;
        if (retType == "VOID") {
            ss << "PROCEDURE " << name << "(";
            for (size_t i = 0; i < paramDecls.size(); ++i) {
                if (i > 0) ss << ", ";
                ss << paramDecls[i];
            }
            ss << ")\n";
        } else {
            ss << "FUNCTION " << name << "(";
            for (size_t i = 0; i < paramDecls.size(); ++i) {
                if (i > 0) ss << ", ";
                ss << paramDecls[i];
            }
            ss << ") RETURNS " << retType << "\n";
        }

        if (match(CTok::LBrace)) {
            while (!check(CTok::RBrace) && !atEnd()) {
                std::string st = parseStatement(1);
                if (!st.empty()) ss << st << "\n";
            }
            match(CTok::RBrace);
        }

        if (retType == "VOID") {
            ss << "ENDPROCEDURE\n";
        } else {
            ss << "ENDFUNCTION\n";
        }
        return ss.str();
    }

    std::string indent(int lvl) {
        return std::string(lvl * 4, ' ');
    }

    std::string parseStatement(int lvl) {
        // Skip empty statement
        if (match(CTok::Semicolon)) return "";

        // Skip pc_cleanup()
        if (check(CTok::Ident) && peek().text == "pc_cleanup") {
            advance(); match(CTok::LParen); match(CTok::RParen); match(CTok::Semicolon);
            return "";
        }

        // printf / puts / scanf
        if (check(CTok::Ident)) {
            const std::string& id = peek().text;
            if (id == "printf") {
                return parsePrintf(lvl);
            } else if (id == "puts") {
                advance(); match(CTok::LParen);
                std::string arg = parseExprString();
                match(CTok::RParen); match(CTok::Semicolon);
                return indent(lvl) + "OUTPUT " + arg;
            } else if (id == "scanf") {
                return parseScanf(lvl);
            }
        }

        // Variable declaration inside block
        if (isTypeStart()) {
            bool isConst = false;
            if (check(CTok::Const)) { isConst = true; advance(); }
            std::string type = parseTypeString();
            std::string name = cleanIdent(advance().text);

            if (name.rfind("tmp_end_", 0) == 0 || name.rfind("tmp_step_", 0) == 0) {
                // Generated temporary variable: skip
                if (match(CTok::Eq)) { parseExprString(); }
                match(CTok::Semicolon);
                return "";
            }

            if (match(CTok::LBracket)) {
                return parseArrayDeclAndInit(name, type, lvl, false);
            }

            if (match(CTok::Eq)) {
                std::string val = parseExprString();
                match(CTok::Semicolon);
                if (isConst) {
                    return indent(lvl) + "CONSTANT " + name + " = " + val;
                }
                return indent(lvl) + "DECLARE " + name + " : " + type + "\n" +
                       indent(lvl) + name + " <- " + val;
            }

            match(CTok::Semicolon);
            return indent(lvl) + "DECLARE " + name + " : " + type;
        }

        // If statement
        if (match(CTok::If)) {
            match(CTok::LParen);
            std::string cond = parseExprString();
            match(CTok::RParen);
            std::ostringstream ss;
            ss << indent(lvl) << "IF " << cond << " THEN\n";
            ss << parseBlockOrStmt(lvl + 1);
            if (match(CTok::Else)) {
                ss << "\n" << indent(lvl) << "ELSE\n";
                ss << parseBlockOrStmt(lvl + 1);
            }
            ss << "\n" << indent(lvl) << "ENDIF";
            return ss.str();
        }

        // While statement
        if (match(CTok::While)) {
            match(CTok::LParen);
            std::string cond = parseExprString();
            match(CTok::RParen);
            std::ostringstream ss;
            ss << indent(lvl) << "WHILE " << cond << " DO\n";
            ss << parseBlockOrStmt(lvl + 1);
            ss << "\n" << indent(lvl) << "ENDWHILE";
            return ss.str();
        }

        // Do-while (REPEAT UNTIL)
        if (match(CTok::Do)) {
            std::string body = parseBlockOrStmt(lvl + 1);
            match(CTok::While);
            match(CTok::LParen);
            std::string cond = parseExprString();
            match(CTok::RParen);
            match(CTok::Semicolon);

            // In C: do { ... } while (!cond); -> REPEAT ... UNTIL cond
            if (cond.rfind("NOT (", 0) == 0 && cond.back() == ')') {
                cond = cond.substr(5, cond.size() - 6);
            } else if (cond.rfind("!", 0) == 0) {
                cond = cond.substr(1);
            } else {
                cond = "NOT (" + cond + ")";
            }

            std::ostringstream ss;
            ss << indent(lvl) << "REPEAT\n";
            ss << body << "\n";
            ss << indent(lvl) << "UNTIL " << cond;
            return ss.str();
        }

        // For loop
        if (match(CTok::For)) {
            match(CTok::LParen);

            // Check for for (;;) or for (; cond; step)
            if (match(CTok::Semicolon)) {
                std::string cond = "TRUE";
                if (!check(CTok::Semicolon)) {
                    cond = parseExprString();
                }
                match(CTok::Semicolon);
                std::string stepStmt;
                if (!check(CTok::RParen)) {
                    stepStmt = parseExprString();
                }
                match(CTok::RParen);

                std::ostringstream ss;
                ss << indent(lvl) << "WHILE " << cond << " DO\n";
                ss << parseBlockOrStmt(lvl + 1);
                if (!stepStmt.empty()) {
                    ss << "\n" << indent(lvl + 1) << stepStmt;
                }
                ss << "\n" << indent(lvl) << "ENDWHILE";
                return ss.str();
            }

            std::string declPrefix;
            if (isTypeStart()) {
                std::string type = parseTypeString();
                if (check(CTok::Ident)) {
                    std::string v = cleanIdent(peek().text);
                    declPrefix = indent(lvl) + "DECLARE " + v + " : " + type + "\n";
                }
            }
            std::string varName;
            std::string startVal;
            if (check(CTok::Ident)) {
                varName = cleanIdent(advance().text);
                if (match(CTok::Eq)) {
                    startVal = parseExprString();
                }
            }
            match(CTok::Semicolon);

            std::string endVal;
            std::string compOp;
            if (check(CTok::Ident) && cleanIdent(peek().text) == varName) {
                advance();
                if (match(CTok::Le)) { compOp = "<="; endVal = parseExprString(); }
                else if (match(CTok::Lt)) { compOp = "<"; endVal = parseExprString(); }
                else if (match(CTok::Ge)) { compOp = ">="; endVal = parseExprString(); }
                else if (match(CTok::Gt)) { compOp = ">"; endVal = parseExprString(); }
            } else {
                endVal = parseExprString();
            }
            match(CTok::Semicolon);

            std::string stepStr = "1";
            if (match(CTok::PlusPlus)) {
                if (check(CTok::Ident) && cleanIdent(peek().text) == varName) { advance(); stepStr = "1"; }
            } else if (match(CTok::MinusMinus)) {
                if (check(CTok::Ident) && cleanIdent(peek().text) == varName) { advance(); stepStr = "-1"; }
            } else if (check(CTok::Ident) && cleanIdent(peek().text) == varName) {
                advance();
                if (match(CTok::PlusPlus)) stepStr = "1";
                else if (match(CTok::MinusMinus)) stepStr = "-1";
                else if (match(CTok::PlusEq)) stepStr = parseExprString();
                else if (match(CTok::MinusEq)) stepStr = "-" + parseExprString();
                else if (match(CTok::Eq)) {
                    std::string rhs = parseExprString();
                    if (rhs.rfind(varName + " - ", 0) == 0) {
                        stepStr = "-" + rhs.substr(varName.size() + 3);
                    } else if (rhs.rfind(varName + " + ", 0) == 0) {
                        stepStr = rhs.substr(varName.size() + 3);
                    } else {
                        stepStr = rhs;
                    }
                }
            }
            match(CTok::RParen);

            // Adjust bound if strict comparison (< or >)
            if (compOp == "<") {
                bool isNum = true;
                for (char c : endVal) {
                    if (!std::isdigit(static_cast<unsigned char>(c)) && c != '-') { isNum = false; break; }
                }
                if (isNum && !endVal.empty() && endVal != "-") {
                    try {
                        long long v = std::stoll(endVal);
                        endVal = std::to_string(v - 1);
                    } catch (...) {
                        endVal = "(" + endVal + " - 1)";
                    }
                } else {
                    endVal = "(" + endVal + " - 1)";
                }
            } else if (compOp == ">") {
                bool isNum = true;
                for (char c : endVal) {
                    if (!std::isdigit(static_cast<unsigned char>(c)) && c != '-') { isNum = false; break; }
                }
                if (isNum && !endVal.empty() && endVal != "-") {
                    try {
                        long long v = std::stoll(endVal);
                        endVal = std::to_string(v + 1);
                    } catch (...) {
                        endVal = "(" + endVal + " + 1)";
                    }
                } else {
                    endVal = "(" + endVal + " + 1)";
                }
            }

            std::ostringstream ss;
            if (!declPrefix.empty()) ss << declPrefix;
            ss << indent(lvl) << "FOR " << varName << " <- " << startVal << " TO " << endVal;
            if (stepStr != "1") ss << " STEP " << stepStr;
            ss << "\n";
            ss << parseBlockOrStmt(lvl + 1);
            ss << "\n" << indent(lvl) << "NEXT " << varName;
            return ss.str();
        }

        // Switch (CASE OF)
        if (match(CTok::Switch)) {
            match(CTok::LParen);
            std::string sel = parseExprString();
            match(CTok::RParen);
            match(CTok::LBrace);
            std::ostringstream ss;
            ss << indent(lvl) << "CASE OF " << sel << "\n";
            while (!check(CTok::RBrace) && !atEnd()) {
                if (match(CTok::Case)) {
                    std::string val = parseExprString();
                    match(CTok::Colon);
                    ss << indent(lvl + 1) << val << ":\n";
                    while (!check(CTok::Case) && !check(CTok::Default) && !check(CTok::RBrace) && !atEnd()) {
                        if (match(CTok::Break)) { match(CTok::Semicolon); continue; }
                        std::string st = parseStatement(lvl + 2);
                        if (!st.empty()) ss << st << "\n";
                    }
                } else if (match(CTok::Default)) {
                    match(CTok::Colon);
                    ss << indent(lvl + 1) << "OTHERWISE:\n";
                    while (!check(CTok::Case) && !check(CTok::Default) && !check(CTok::RBrace) && !atEnd()) {
                        if (match(CTok::Break)) { match(CTok::Semicolon); continue; }
                        std::string st = parseStatement(lvl + 2);
                        if (!st.empty()) ss << st << "\n";
                    }
                } else {
                    advance();
                }
            }
            match(CTok::RBrace);
            ss << indent(lvl) << "ENDCASE";
            return ss.str();
        }

        // Return
        if (match(CTok::Return)) {
            if (match(CTok::Semicolon)) {
                return indent(lvl) + "RETURN";
            }
            std::string expr = parseExprString();
            match(CTok::Semicolon);
            if (expr == "0" && lvl == 0) return ""; // return 0; in main
            return indent(lvl) + "RETURN " + expr;
        }

        // Expression statement or assignment
        std::string s = parseExprString();
        match(CTok::Semicolon);

        // Check if function call to a procedure
        if (s.find("(") != std::string::npos && s.find("<-") == std::string::npos) {
            std::string fname = s.substr(0, s.find("("));
            if (knownProcedures_.find(fname) != knownProcedures_.end()) {
                return indent(lvl) + "CALL " + s;
            }
        }

        return indent(lvl) + s;
    }

    std::string parseBlockOrStmt(int lvl) {
        if (match(CTok::LBrace)) {
            std::ostringstream ss;
            bool first = true;
            while (!check(CTok::RBrace) && !atEnd()) {
                std::string st = parseStatement(lvl);
                if (!st.empty()) {
                    if (!first) ss << "\n";
                    ss << st;
                    first = false;
                }
            }
            match(CTok::RBrace);
            return ss.str();
        }
        return parseStatement(lvl);
    }

    std::string parsePrintf(int lvl) {
        advance(); // skip printf
        match(CTok::LParen);
        std::string fmt = advance().text; // StrLit
        std::vector<std::string> args;
        while (match(CTok::Comma)) {
            args.push_back(parseExprString());
        }
        match(CTok::RParen);
        match(CTok::Semicolon);

        // Parse format string into literal pieces and arguments
        std::vector<std::string> outputItems;
        size_t argIdx = 0;
        std::string curText;

        for (size_t i = 0; i < fmt.size(); ++i) {
            if (fmt[i] == '%' && i + 1 < fmt.size()) {
                if (!curText.empty()) {
                    outputItems.push_back("\"" + curText + "\"");
                    curText.clear();
                }
                i++;
                while (i < fmt.size() && (fmt[i] == 'l' || fmt[i] == 'h')) i++;
                // Specifier: %d, %lld, %s, %f, %g, %c
                if (argIdx < args.size()) {
                    outputItems.push_back(args[argIdx++]);
                }
            } else if (fmt[i] == '\n' && i == fmt.size() - 1) {
                // Trailing newline in printf is implicit in OUTPUT
                break;
            } else {
                curText += fmt[i];
            }
        }
        if (!curText.empty()) {
            outputItems.push_back("\"" + curText + "\"");
        }

        // Add any remaining args
        while (argIdx < args.size()) {
            outputItems.push_back(args[argIdx++]);
        }

        std::ostringstream ss;
        ss << indent(lvl) << "OUTPUT ";
        for (size_t i = 0; i < outputItems.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << outputItems[i];
        }
        return ss.str();
    }

    std::string parseScanf(int lvl) {
        advance(); // skip scanf
        match(CTok::LParen);
        (void)advance(); // fmt
        match(CTok::Comma);
        std::string target = parseExprString();
        if (target.rfind("&", 0) == 0) target = target.substr(1);
        match(CTok::RParen);
        match(CTok::Semicolon);
        return indent(lvl) + "INPUT " + target;
    }

    std::string parseExprString() {
        return parseAssignExpr();
    }

    std::string parseAssignExpr() {
        std::string lhs = parseLogicalOr();
        if (match(CTok::Eq)) {
            std::string rhs = parseAssignExpr();
            return lhs + " <- " + rhs;
        }
        if (match(CTok::PlusEq)) {
            std::string rhs = parseAssignExpr();
            return lhs + " <- " + lhs + " + " + rhs;
        }
        if (match(CTok::MinusEq)) {
            std::string rhs = parseAssignExpr();
            return lhs + " <- " + lhs + " - " + rhs;
        }
        if (match(CTok::StarEq)) {
            std::string rhs = parseAssignExpr();
            return lhs + " <- " + lhs + " * " + rhs;
        }
        if (match(CTok::SlashEq)) {
            std::string rhs = parseAssignExpr();
            return lhs + " <- " + lhs + " / " + rhs;
        }
        if (match(CTok::PercentEq)) {
            std::string rhs = parseAssignExpr();
            return lhs + " <- " + lhs + " MOD " + rhs;
        }
        return lhs;
    }

    std::string parseLogicalOr() {
        std::string l = parseLogicalAnd();
        while (match(CTok::PipePipe)) {
            l += " OR " + parseLogicalAnd();
        }
        return l;
    }

    std::string parseLogicalAnd() {
        std::string l = parseEquality();
        while (match(CTok::AmpAmp)) {
            l += " AND " + parseEquality();
        }
        return l;
    }

    std::string parseEquality() {
        std::string l = parseComparison();
        while (check(CTok::EqEq) || check(CTok::NotEq)) {
            bool eq = match(CTok::EqEq);
            if (!eq) match(CTok::NotEq);
            l += (eq ? " = " : " <> ") + parseComparison();
        }
        return l;
    }

    std::string parseComparison() {
        std::string l = parseAddSub();
        while (check(CTok::Lt) || check(CTok::Le) || check(CTok::Gt) || check(CTok::Ge)) {
            std::string op;
            if (match(CTok::Lt)) op = " < ";
            else if (match(CTok::Le)) op = " <= ";
            else if (match(CTok::Gt)) op = " > ";
            else if (match(CTok::Ge)) op = " >= ";
            l += op + parseAddSub();
        }
        return l;
    }

    std::string parseAddSub() {
        std::string l = parseMulDiv();
        while (check(CTok::Plus) || check(CTok::Minus)) {
            bool plus = match(CTok::Plus);
            if (!plus) match(CTok::Minus);
            l += (plus ? " + " : " - ") + parseMulDiv();
        }
        return l;
    }

    std::string parseMulDiv() {
        std::string l = parseUnary();
        while (check(CTok::Star) || check(CTok::Slash) || check(CTok::Percent)) {
            std::string op;
            if (match(CTok::Star)) op = " * ";
            else if (match(CTok::Slash)) op = " / ";
            else if (match(CTok::Percent)) op = " MOD ";
            l += op + parseUnary();
        }
        return l;
    }

    std::string parseUnary() {
        if (match(CTok::Bang)) {
            return "NOT (" + parseUnary() + ")";
        }
        if (match(CTok::Minus)) {
            return "-" + parseUnary();
        }
        if (match(CTok::Star)) {
            // Pointer deref *x -> if x is BYREF param, just x
            std::string operand = parseUnary();
            if (byRefParams_.find(operand) != byRefParams_.end()) return operand;
            return operand;
        }
        if (match(CTok::Amp)) {
            // Address of &x -> if passing to BYREF, just x
            return parseUnary();
        }
        if (match(CTok::PlusPlus)) {
            std::string operand = parseUnary();
            return operand + " <- " + operand + " + 1";
        }
        if (match(CTok::MinusMinus)) {
            std::string operand = parseUnary();
            return operand + " <- " + operand + " - 1";
        }

        std::string prim = parsePrimary();
        if (match(CTok::PlusPlus)) return prim + " <- " + prim + " + 1";
        if (match(CTok::MinusMinus)) return prim + " <- " + prim + " - 1";
        return prim;
    }

    std::string parsePrimary() {
        if (match(CTok::LParen)) {
            std::string e = parseExprString();
            match(CTok::RParen);
            return "(" + e + ")";
        }
        if (check(CTok::IntLit)) {
            return advance().text;
        }
        if (check(CTok::RealLit)) {
            return advance().text;
        }
        if (check(CTok::StrLit)) {
            return "\"" + advance().text + "\"";
        }
        if (check(CTok::CharLit)) {
            return "'" + advance().text + "'";
        }
        if (check(CTok::Ident)) {
            std::string name = cleanIdent(advance().text);

            // Postfix chaining: arr[i], rec.field, func(args)
            while (check(CTok::LBracket) || check(CTok::Dot) || check(CTok::Arrow) || check(CTok::LParen)) {
                if (match(CTok::LBracket)) {
                    std::string idx1 = parseExprString();
                    std::string idx2;
                    if (match(CTok::Comma)) idx2 = parseExprString();
                    match(CTok::RBracket);
                    if (idx2.empty()) name += "[" + idx1 + "]";
                    else name += "[" + idx1 + ", " + idx2 + "]";
                } else if (match(CTok::Dot) || match(CTok::Arrow)) {
                    name += "." + cleanIdent(advance().text);
                } else if (match(CTok::LParen)) {
                    std::vector<std::string> args;
                    if (!check(CTok::RParen)) {
                        while (!atEnd()) {
                            args.push_back(parseExprString());
                            if (!match(CTok::Comma)) break;
                        }
                    }
                    match(CTok::RParen);

                    // Map common C builtins back to Cambridge pseudocode built-ins
                    if (name == "strlen" && args.size() == 1) return "LENGTH(" + args[0] + ")";
                    if (name == "toupper" && args.size() == 1) return "UCASE(" + args[0] + ")";
                    if (name == "tolower" && args.size() == 1) return "LCASE(" + args[0] + ")";
                    if (name == "pc_left" && args.size() == 2) return "LEFT(" + args[0] + ", " + args[1] + ")";
                    if (name == "pc_right" && args.size() == 2) return "RIGHT(" + args[0] + ", " + args[1] + ")";
                    if (name == "pc_substring" && args.size() == 3) return "MID(" + args[0] + ", " + args[1] + ", " + args[2] + ")";
                    if (name == "pc_chr" && args.size() == 1) return "CHR(" + args[0] + ")";
                    if (name == "pc_asc" && args.size() == 1) return "ASC(" + args[0] + ")";
                    if (name == "pc_int" && args.size() == 1) return "INT(" + args[0] + ")";
                    if (name == "pc_round" && args.size() == 2) return "ROUND(" + args[0] + ", " + args[1] + ")";
                    if (name == "pc_rnd" && args.empty()) return "RND()";
                    if (name == "pc_div" && args.size() == 2) return "DIV(" + args[0] + ", " + args[1] + ")";
                    if (name == "pc_mod" && args.size() == 2) return "MOD(" + args[0] + ", " + args[1] + ")";

                    name += "(";
                    for (size_t i = 0; i < args.size(); ++i) {
                        if (i > 0) name += ", ";
                        name += args[i];
                    }
                    name += ")";
                }
            }
            return name;
        }

        if (!atEnd()) return advance().text;
        return "";
    }
};

} // namespace

std::string translateCToPseudocode(const std::string& cSource) {
    CLexer lexer(cSource);
    std::vector<CToken> tokens = lexer.tokenize();
    CToPseudoConverter converter(tokens);
    return converter.convert();
}
