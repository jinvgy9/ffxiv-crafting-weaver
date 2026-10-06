#pragma once
// 数据模型：配方配置 / 技能定义 / 搜索状态 —— 只有结构，没有逻辑（笑
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iterator>
#include <filesystem>

using namespace std;

// ---------------------------------------------------------------- 配方配置
// ---------------------------------------------------------------- 配置
struct Cfg {
    int level = 100;
    int progMax = 0, qualMax = 0, durMax = 0, cpMax = 0;
    int baseProg = 0, baseQual = 0;
    double startQual = -1;   // 绝对值；<0 表示用百分比
    double startQualPct = 0; // 百分比
    int maxDepth = 22;
    int perLayer = 150000;
    bool quiet = false;      // --quiet：不打印进度（脚本 / 对拍用）
};

// ---------------------------------------------------------------- 技能定义
struct Act {
    const char* cn;
    int lv = 1, cp = 0, cpCombo = 0, dur = 0, eff = 0, progEff = 0, qualEff = 0;
    int type = 0;
    int iqAdd = 0, iqComboAdd = 0, iqPer = 0, effCap = 0, needIQ = 0;
    int restore = 0;
    int steps = 0;                 // buff 持续
    int buffId = -1;               // 0..5
    uint8_t fromMask = 0;          // 连击来源（按动作 id 位）
    uint8_t needExp = 0, grantExp = 0;
    uint8_t firstOnly = 0, clearIQ = 0, noWasteNot = 0, once = 0, oncePerCraft = 0;
    uint8_t noDurNext = 0, halfIfShort = 0, restoreAll = 0;
    double success = 1.0;
};

// ---------------------------------------------------------------- buff 编号
// buff 编号：0 innovation 1 veneration 2 greatStrides 3 wasteNot 4 manipulation 5 finalAppraisal
enum { BI = 0, BV, BG, BW, BM, BF };

// ---------------------------------------------------------------- 搜索状态
struct St {
    double prog = 0, qual = 0, qualRaw = 0;
    double iq = 0;
    double expedience = 0;   // 工匠的良机：固定模式为 1/0；概率&期望模式为概率
    int32_t dur = 0, cp = 0;
    uint32_t buffs = 0;      // 6×4bit
    uint8_t flags = 0;       // 见 FB_*
    int8_t prev = -1;
};
enum { FB_COMBO = 1, FB_FIRST = 2, FB_USED_IMM = 4, FB_USED_TP = 8, FB_NO_DUR = 16, FB_NEXT_PROG = 64 };

static inline int bget(const St& s, int b) { return (s.buffs >> (4 * b)) & 0xF; }
static inline void bset(St& s, int b, int v) { s.buffs = (s.buffs & ~(0xFu << (4 * b))) | ((uint32_t)(v & 0xF) << (4 * b)); }
static inline bool fget(const St& s, int f) { return (s.flags & f) != 0; }
static inline void fset(St& s, int f, bool v) { if (v) s.flags |= f; else s.flags &= ~f; }
