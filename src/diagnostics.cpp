#include "diagnostics.h"

#include <iostream>
#include <sstream>
#include <utility>

Diagnostics::Diagnostics(std::string file, const std::string& source, bool quiet)
    : file_(std::move(file)), quiet_(quiet) {
    std::istringstream in(source);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines_.push_back(line);
    }
}

void Diagnostics::error(int line, int col, const std::string& msg) {
    ++errors_;
    if (quiet_) return;

    std::cerr << file_ << ":" << line << ":" << col << ": error: " << msg << "\n";
    if (line >= 1 && line <= static_cast<int>(lines_.size())) {
        const std::string& text = lines_[line - 1];
        std::string pad;
        for (int i = 0; i < col - 1; ++i) {
            pad += (i < static_cast<int>(text.size()) && text[i] == '\t') ? '\t' : ' ';
        }
        std::cerr << "  " << text << "\n  " << pad << "^\n";
    }
}

void Diagnostics::reset() {
    errors_ = 0;
    lines_.clear();
}
