#include "formatter.h"
#include "token.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <unordered_set>
#include <vector>

namespace {

std::string toUpper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

std::string trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\t' || s[start] == '\r')) start++;
    size_t end = s.size();
    while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r')) end--;
    return s.substr(start, end - start);
}

// Format tokens on a single line while preserving string literals and comments
std::string formatLineTokens(const std::string& line) {
    std::string result;
    size_t i = 0;
    size_t n = line.size();

    while (i < n) {
        // Comment: preserve rest of the line
        if (i + 1 < n && line[i] == '/' && line[i + 1] == '/') {
            result += line.substr(i);
            break;
        }

        // String literal: preserve verbatim
        if (line[i] == '"') {
            result += '"';
            i++;
            while (i < n) {
                result += line[i];
                if (line[i] == '\\' && i + 1 < n) {
                    result += line[i + 1];
                    i += 2;
                    continue;
                }
                if (line[i] == '"') {
                    i++;
                    break;
                }
                i++;
            }
            continue;
        }

        // Character literal: preserve verbatim
        if (line[i] == '\'') {
            result += '\'';
            i++;
            while (i < n) {
                result += line[i];
                if (line[i] == '\\' && i + 1 < n) {
                    result += line[i + 1];
                    i += 2;
                    continue;
                }
                if (line[i] == '\'') {
                    i++;
                    break;
                }
                i++;
            }
            continue;
        }

        // Identifier or Keyword
        if (std::isalpha(static_cast<unsigned char>(line[i])) || line[i] == '_') {
            size_t start = i;
            while (i < n && (std::isalnum(static_cast<unsigned char>(line[i])) || line[i] == '_')) {
                i++;
            }
            std::string word = line.substr(start, i - start);
            std::string upperWord = toUpper(word);
            if (kKeywords.find(upperWord) != kKeywords.end()) {
                result += upperWord;
            } else {
                result += word;
            }
            continue;
        }

        // Colon: format as ' : ' outside brackets (for declarations/CASE), compact ':' inside brackets (for array bounds 1:5)
        if (line[i] == ':') {
            size_t depth = 0;
            for (size_t k = 0; k < i; ++k) {
                if (line[k] == '[') depth++;
                else if (line[k] == ']') depth = (depth > 0 ? depth - 1 : 0);
            }
            if (depth > 0) {
                result += ':';
                i++;
            } else {
                if (!result.empty() && result.back() != ' ') result += ' ';
                result += ": ";
                i++;
                while (i < n && line[i] == ' ') i++;
            }
            continue;
        }

        // Operator: <-
        if (i + 1 < n && line[i] == '<' && line[i + 1] == '-') {
            // Ensure spacing around <-
            if (!result.empty() && result.back() != ' ') result += ' ';
            result += "<-";
            i += 2;
            if (i < n && line[i] != ' ') result += ' ';
            continue;
        }

        // Operator: <>
        if (i + 1 < n && line[i] == '<' && line[i + 1] == '>') {
            if (!result.empty() && result.back() != ' ') result += ' ';
            result += "<>";
            i += 2;
            if (i < n && line[i] != ' ') result += ' ';
            continue;
        }

        // Operator: <= or >=
        if (i + 1 < n && (line[i] == '<' || line[i] == '>') && line[i + 1] == '=') {
            if (!result.empty() && result.back() != ' ') result += ' ';
            result += line.substr(i, 2);
            i += 2;
            if (i < n && line[i] != ' ') result += ' ';
            continue;
        }

        // Other character
        result += line[i++];
    }

    return result;
}

// Get first uppercase keyword on a line
std::string getFirstWord(const std::string& line) {
    size_t i = 0;
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) i++;
    size_t start = i;
    while (i < line.size() && (std::isalnum(static_cast<unsigned char>(line[i])) || line[i] == '_')) i++;
    return toUpper(line.substr(start, i - start));
}

} // namespace

std::string Formatter::format(const std::string& source) {
    std::istringstream stream(source);
    std::string line;
    std::vector<std::string> rawLines;
    while (std::getline(stream, line)) {
        rawLines.push_back(line);
    }

    std::ostringstream out;
    int indent = 0;
    bool lastWasBlank = false;

    for (const auto& raw : rawLines) {
        std::string trimmed = trim(raw);
        if (trimmed.empty()) {
            if (!lastWasBlank) {
                out << "\n";
                lastWasBlank = true;
            }
            continue;
        }
        lastWasBlank = false;

        // Comment-only line: preserve comment with current indent
        if (trimmed.size() >= 2 && trimmed[0] == '/' && trimmed[1] == '/') {
            out << std::string(indent * 4, ' ') << trimmed << "\n";
            continue;
        }

        std::string firstWord = getFirstWord(trimmed);

        // Dedent before line if closing block keyword
        if (firstWord == "ENDIF" || firstWord == "ELSE" ||
            firstWord == "ENDWHILE" || firstWord == "NEXT" ||
            firstWord == "UNTIL" || firstWord == "ENDCASE" ||
            firstWord == "OTHERWISE" || firstWord == "ENDPROCEDURE" ||
            firstWord == "ENDFUNCTION" || firstWord == "ENDTYPE" ||
            firstWord == "ENDCLASS") {
            indent = std::max(0, indent - 1);
        }

        std::string formattedLine = formatLineTokens(trimmed);
        out << std::string(indent * 4, ' ') << formattedLine << "\n";

        // Indent after line if opening block keyword
        if (firstWord == "IF" || firstWord == "ELSE" ||
            firstWord == "WHILE" || firstWord == "FOR" ||
            firstWord == "REPEAT" || firstWord == "CASE" ||
            firstWord == "OTHERWISE" || firstWord == "PROCEDURE" ||
            firstWord == "FUNCTION" || firstWord == "TYPE" ||
            firstWord == "CLASS") {
            indent++;
        }
    }

    return out.str();
}
