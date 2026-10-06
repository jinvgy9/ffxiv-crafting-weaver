#include "craft/engine.hpp"
#include "craft/csv.hpp"
#include <cstring>
#include <cmath>
#include <algorithm>
using namespace std;

Cfg C;
vector<Act> A;
vector<string> AKEY;
bool EXPECTED = false;
int ACT_OBSERVE = -1, ACT_STANDARD = -1;

// 注：技能表由 loadActions() 从 data/actions.csv 装载

St newState() {
    St s;
    s.prog = 0; s.dur = C.durMax; s.cp = C.cpMax; s.iq = 0; s.buffs = 0; s.flags = FB_FIRST; s.prev = -1;
    double q = (C.startQual >= 0) ? C.startQual : floor((double)C.qualMax * C.startQualPct / 100.0);
    s.qual = q; s.qualRaw = q;
    return s;
}

int cpCostOf(const St& s, int key) {
    const Act& a = A[key];
    if (AKEY[key] == "advancedTouch") return fget(s, FB_COMBO) ? a.cpCombo : a.cp;
    if (a.fromMask != 0 && s.prev >= 0 && ((a.fromMask >> s.prev) & 1)) return a.cpCombo ? a.cpCombo : a.cp;
    return a.cp;
}

bool canUse(const St& s, int key, bool done, bool failed) {
    const Act& a = A[key];
    if (done || failed) return false;
    if (a.lv > C.level) return false;
    if (a.firstOnly && !fget(s, FB_FIRST)) return false;
    if (a.once && fget(s, FB_USED_IMM)) return false;
    if (a.oncePerCraft && fget(s, FB_USED_TP)) return false;
    if (a.needIQ && s.iq < a.needIQ) return false;
    if (a.needExp && !(s.expedience > 0)) return false;
    if (a.noWasteNot && bget(s, BW) > 0) return false;
    if (cpCostOf(s, key) > s.cp) return false;
    return true;
}

// 返回 false = 无法执行（done/failed 由调用方从返回值判断）
bool applyAct(const St& s0, int key, St& s, double& gainProg, double& gainQual, bool& done, bool& failed) {
    const Act& a = A[key];
    s = s0;
    int cpCost = cpCostOf(s0, key);
    // 概率&期望模式：概率技能按成功率折算（失败＝扣耐久、扣 CP、无内静、无追加效果）；固定模式 succ=1
    const double succ = EXPECTED ? a.success : 1.0;

    // 2. 增量（固定模式 floor 取整，必须与 engine.mjs 的 Math.floor 一致；期望模式不取整、乘成功率）
    double iqMul = 1.0 + 0.1 * s.iq;
    gainProg = 0; gainQual = 0;
    if (a.type == 0 || a.type == 2) {
        double base = (a.type == 2) ? a.qualEff : a.eff;
        if (a.iqPer) base = min((double)a.effCap, a.eff + a.iqPer * s.iq);
        double mult = 1.0;
        if (bget(s, BI) > 0) mult += 0.5;
        if (bget(s, BG) > 0) mult += 1.0;
        double raw = (C.baseQual * base * mult * iqMul) / 100.0;
        gainQual = EXPECTED ? raw * succ : floor(raw);
    }
    if (a.type == 1 || a.type == 2) {
        double base = (a.type == 2) ? a.progEff : a.eff;
        double mult = 1.0;
        if (bget(s, BV) > 0) mult += 0.5;
        if (fget(s, FB_NEXT_PROG)) mult += 1.0;
        if (a.halfIfShort) {
            int realCost = (bget(s, BW) > 0 && a.dur > 0) ? (int)ceil(a.dur / 2.0) : a.dur;
            if (s.dur < realCost) base = floor(a.eff / 2.0);
        }
        double raw = (C.baseProg * base * mult) / 100.0;
        gainProg = EXPECTED ? raw * succ : floor(raw);
    }

    // 3. 耐久（工匠的绝技 → 0；俭约 → 减半向上取整）
    int durCost = fget(s, FB_NO_DUR) ? 0 : a.dur;
    if (bget(s, BW) > 0 && a.dur > 0 && durCost > 0) durCost = (int)ceil(a.dur / 2.0);
    s.dur -= durCost;
    // 4. CP
    s.cp -= cpCost;
    // 5. 入账
    s.prog += gainProg;
    if (s.qual < C.qualMax) s.qualRaw += gainQual;
    s.qual = min((double)C.qualMax, s.qual + gainQual);
    s.prev = (int8_t)key;
    fset(s, FB_FIRST, false);
    if (AKEY[key] == "muscleMemory") { fset(s, FB_NEXT_PROG, true); }
    else if (a.type == 1 || a.type == 2) fset(s, FB_NEXT_PROG, false);

    // 6. 内静
    if (a.type == 0 || a.type == 2) {
        if (a.clearIQ) s.iq = 0;
        else {
            bool combo = (a.fromMask != 0 && s0.prev >= 0 && ((a.fromMask >> s0.prev) & 1));
            double g = 1.0 + a.iqAdd + (combo ? a.iqComboAdd : 0);
            s.iq = min(10.0, s.iq + (EXPECTED ? g * succ : g));
        }
    }
    // 7. buff 计数
    if (a.type == 3) {
        if (a.restore) s.dur = min(C.durMax, s.dur + a.restore);
        if (a.restoreAll) { s.dur = C.durMax; fset(s, FB_USED_IMM, true); }
        if (a.buffId >= 0) bset(s, a.buffId, a.steps);
        for (int b = 0; b < 6; b++) {
            if (b == a.buffId) continue;
            int v = bget(s, b); if (v > 0) bset(s, b, v - 1);
        }
    } else {
        if ((a.type == 0 || a.type == 2) && bget(s, BG) > 0) bset(s, BG, 0);
        for (int b = 0; b < 6; b++) { int v = bget(s, b); if (v > 0) bset(s, b, v - 1); }
    }
    // 掌握：用【本步开始前】的计数判定
    if (AKEY[key] != "manipulation" && bget(s0, BM) > 0) s.dur = min(C.durMax, s.dur + 5);
    // 8. 连击链
    fset(s, FB_COMBO, (AKEY[key] == "standardTouch" && s0.prev == ACT_STANDARD) || AKEY[key] == "observe");
    // 9. 工匠的良机（概率&期望模式下为概率；固定模式为 1/0）
    s.expedience = a.grantExp ? succ : 0.0;
    // 10. 工匠的绝技：无步数倒计时 —— 一直保留到"第一个会消耗耐久的动作"才用掉
    if (a.noDurNext) { fset(s, FB_NO_DUR, true); fset(s, FB_USED_TP, true); }
    else if (a.dur > 0) fset(s, FB_NO_DUR, false);

    // 完成/失败判定
    done = false; failed = false;
    if (bget(s, BF) > 0 && s.prog >= C.progMax) s.prog = C.progMax - 1; // 最终确认
    if (s.dur <= 0 && s.prog < C.progMax) failed = true;
    if (s.prog >= C.progMax) { if (s.qual >= C.qualMax) done = true; else failed = true; }
    return true;
}

// key：与 bfs.mjs 的整数编码等价（离散状态）
uint64_t keyOf(const St& s) {
    uint64_t k = (uint64_t)s.iq;
    for (int b = 0; b < 6; b++) k = k * 9 + (uint64_t)min(8, bget(s, b));
    k = k * 2 + (fget(s, FB_COMBO) ? 1 : 0);
    k = k * 2 + (fget(s, FB_FIRST) ? 1 : 0);
    k = k * 2 + (fget(s, FB_USED_IMM) ? 1 : 0);
    k = k * 2 + (fget(s, FB_USED_TP) ? 1 : 0);
    k = k * 2 + (fget(s, FB_NO_DUR) ? 1 : 0);
    k = k * 2 + (s.expedience > 0 ? 1 : 0);
    k = k * 2 + (fget(s, FB_NEXT_PROG) ? 1 : 0);
    k = k * (A.size() + 1) + (uint64_t)(s.prev + 1);
    return k;
}

// ================================================================ 由 CSV 构建技能表
//   数据与代码分离：技能表是 data/actions.csv 的镜像，改数值改数据、不改代码
//   注意 CNPOOL：Act::cn 是 const char*，必须指向稳定存储，否则出作用域即悬空
static vector<string> CNPOOL;

int findActByKey(const char* key) {
    for (size_t i = 0; i < AKEY.size(); i++) if (AKEY[i] == key) return (int)i;
    return -1;
}

bool loadActions(const std::filesystem::path& csvPath) {
    vector<vector<string> > rows;
    if (!readCsv(csvPath, rows)) {
        fprintf(stderr, "缺数据文件：%s\n", csvPath.u8string().c_str());
        return false;
    }
    A.clear(); AKEY.clear(); CNPOOL.clear();
    CNPOOL.reserve(rows.size());
    for (size_t i = 0; i < rows.size(); i++) {
        const vector<string>& c = rows[i];
        Act a;
        CNPOOL.push_back(scol(c, 1));
        a.cn = CNPOOL.back().c_str();
        a.lv = icol(c, 3); a.cp = icol(c, 4); a.cpCombo = icol(c, 5); a.dur = icol(c, 6);
        a.eff = icol(c, 7); a.progEff = icol(c, 8); a.qualEff = icol(c, 9);
        const string ty = scol(c, 10);
        a.type = (ty == "quality") ? 0 : (ty == "progress") ? 1 : (ty == "both") ? 2 : 3;
        a.success = dcol(c, 11); if (a.success <= 0.0) a.success = 1.0;
        a.iqAdd = icol(c, 12); a.iqComboAdd = icol(c, 13); a.iqPer = icol(c, 14);
        a.effCap = icol(c, 15); a.needIQ = icol(c, 16);
        a.buffId = icol(c, 17); a.steps = icol(c, 18); a.restore = icol(c, 19);
        a.restoreAll = (uint8_t)icol(c, 20); a.once = (uint8_t)icol(c, 21); a.noDurNext = (uint8_t)icol(c, 22);
        a.halfIfShort = (uint8_t)icol(c, 23); a.firstOnly = (uint8_t)icol(c, 24); a.clearIQ = (uint8_t)icol(c, 25);
        a.noWasteNot = (uint8_t)icol(c, 26); a.grantExp = (uint8_t)icol(c, 27); a.needExp = (uint8_t)icol(c, 28);
        A.push_back(a);
        AKEY.push_back(scol(c, 0));
    }
    // 第二遍：连击来源（要等 key→索引 建好才能解析）
    for (size_t i = 0; i < rows.size(); i++) {
        const string from = scol(rows[i], 29);
        if (from.empty()) continue;
        const int idx = findActByKey(from.c_str());
        if (idx < 0) {
            fprintf(stderr, "actions.csv: 第 %zu 行的 combo_from 指向未知技能「%s」\n", i + 2, from.c_str());
            return false;
        }
        A[i].fromMask = (uint8_t)(1u << idx);
    }
    ACT_OBSERVE = findActByKey("observe");
    ACT_STANDARD = findActByKey("standardTouch");
    if (A.empty()) { fprintf(stderr, "actions.csv 里没有技能\n"); return false; }
    return true;
}
