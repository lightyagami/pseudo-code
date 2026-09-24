#ifndef PSEUDOC_DIAGNOSTICS_H
#define PSEUDOC_DIAGNOSTICS_H

#include <string>
#include <vector>

class Diagnostics {
public:
    Diagnostics(std::string file, const std::string& source, bool quiet = false);

    void error(int line, int col, const std::string& msg);
    int errorCount() const { return errors_; }
    void reset();

private:
    std::string file_;
    std::vector<std::string> lines_;
    int errors_ = 0;
    bool quiet_ = false;
};

#endif // PSEUDOC_DIAGNOSTICS_H
