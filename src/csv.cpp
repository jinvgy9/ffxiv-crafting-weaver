#include "craft/csv.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
using namespace std;

// 只在本文件使用，不导出
static vector<string> splitCsvLine(const string& line) {
    vector<string> out; string cur; bool inQ = false;
    for (size_t i = 0; i < line.size(); i++) {
        char c = line[i];
        if (inQ) {
            if (c == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') { cur += '"'; i++; }
                else inQ = false;
            } else cur += c;
        } else {
            if (c == '"') inQ = true;
            else if (c == ',') { out.push_back(cur); cur.clear(); }
            else cur += c;
        }
    }
    out.push_back(cur);
    return out;
}

bool readCsv(const std::filesystem::path& p, vector<vector<string> >& rows) {
    std::error_code ec;
    if (!std::filesystem::exists(p, ec)) return false;
    std::ifstream f(p);
    if (!f) return false;
    string line; bool first = true;
    while (std::getline(f, line)) {
        if (!line.empty() && line[line.size() - 1] == '\r') line.erase(line.size() - 1);
        if (line.empty()) continue;
        if (first) { first = false; continue; }   // 跳表头
        rows.push_back(splitCsvLine(line));
    }
    return true;
}

int icol(const vector<string>& c, size_t i) { return i < c.size() ? atoi(c[i].c_str()) : 0; }
double dcol(const vector<string>& c, size_t i) { return i < c.size() ? atof(c[i].c_str()) : 0.0; }

string scol(const vector<string>& c, size_t i) {
    if (i >= c.size()) return string();
    const string& s = c[i];
    size_t a = s.find_first_not_of(" \t\r\n"); if (a == string::npos) return string();
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}
