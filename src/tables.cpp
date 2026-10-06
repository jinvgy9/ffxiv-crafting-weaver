#include "craft/tables.hpp"
#include "craft/csv.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <fstream>
#include <iterator>
#include <algorithm>
using namespace std;

// ---- HQ 概率表 ----
int HQ[100];
bool HQok = false;

int hqProb(double pct) {
    if (pct <= 0) return 1;
    if (pct >= 100) return 100;
    if (!HQok) return -1;
    int idx = (int)floor(pct);
    if (idx > 99) idx = 99;
    return HQ[idx];
}

// ---------------------------------------------------------------- rlv 表 / 等级压制表
unordered_map<int, RlvRow> RTAB;
unordered_map<int, int> EQUIV;
bool RTABok = false;

bool loadTables(const std::filesystem::path& dataDir) {
    RTAB.clear(); EQUIV.clear(); RTABok = false; HQok = false;
    vector<vector<string> > rows;

    rows.clear();
    if (!readCsv(dataDir / "recipe_level_table.csv", rows)) {
        fprintf(stderr, "缺数据文件：%s\n", (dataDir / "recipe_level_table.csv").u8string().c_str());
        return false;
    }
    for (size_t i = 0; i < rows.size(); i++) {
        const vector<string>& c = rows[i];
        RlvRow& r = RTAB[icol(c, 0)];
        r.pd = dcol(c, 1); r.qd = dcol(c, 2); r.pm = dcol(c, 3); r.qm = dcol(c, 4);
        r.mask = icol(c, 5); r.stars = icol(c, 6);
    }

    rows.clear();
    if (!readCsv(dataDir / "recipe_caps.csv", rows)) {
        fprintf(stderr, "缺数据文件：%s\n", (dataDir / "recipe_caps.csv").u8string().c_str());
        return false;
    }
    for (size_t i = 0; i < rows.size(); i++) {
        const vector<string>& c = rows[i];
        RlvRow& r = RTAB[icol(c, 0)];
        r.cjl = icol(c, 1); r.diff = icol(c, 2); r.qual = icol(c, 3);
        r.dur = icol(c, 4); r.stars = icol(c, 5); r.mask = icol(c, 6);
    }

    rows.clear();
    if (!readCsv(dataDir / "level_suppression.csv", rows)) {
        fprintf(stderr, "缺数据文件：%s\n", (dataDir / "level_suppression.csv").u8string().c_str());
        return false;
    }
    for (size_t i = 0; i < rows.size(); i++) EQUIV[icol(rows[i], 0)] = icol(rows[i], 1);

    rows.clear();
    if (!readCsv(dataDir / "hq_table.csv", rows)) {
        fprintf(stderr, "缺数据文件：%s\n", (dataDir / "hq_table.csv").u8string().c_str());
        return false;
    }
    int n = 0;
    for (size_t i = 0; i < rows.size(); i++) {
        int idx = icol(rows[i], 0);
        if (idx >= 0 && idx < 100) { HQ[idx] = icol(rows[i], 1); n++; }
    }
    HQok = (n >= 100);
    if (!HQok) fprintf(stderr, "HQ 概率表不完整（只有 %d/100 项）\n", n);

    RTABok = !RTAB.empty();
    if (!RTABok) fprintf(stderr, "rlv 表为空\n");
    return RTABok;
}
