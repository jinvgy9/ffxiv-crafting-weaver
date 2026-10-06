#include "craft/model.hpp"
#include "craft/engine.hpp"
#include "craft/tables.hpp"
#include "craft/hqreport.hpp"
#include "craft/solver.hpp"
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
#ifdef _WIN32
  #define NOMINMAX            // 防止 windows.h 定义 min/max 宏
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
#endif

using namespace std;

// 已知参数名（用于识别误拼）。注意：--seq 在另一处单独解析，但同样算已知。
static bool isKnownArg(const string& a) {
    if (a == "--help" || a == "-h" || a == "--quiet" || a == "--no-progress" || a == "--strict") return true;
    if (a.rfind("--", 0) != 0) return true;            // 不以 -- 开头的留给位置参数，不检查
    size_t eq = a.find('=');
    const string k = a.substr(2, eq == string::npos ? string::npos : eq - 2);
    static const char* K[] = {
        "datadir", "rlv", "cm", "ctrl", "baseProg", "baseQual",
        "level", "progMax", "prog", "qualMax", "qual", "durMax", "dur", "cpMax", "cp",
        "startQual", "qabs", "startQualPct", "q0", "depth", "mode", "perlayer", "ban", "seq"
    };
    for (size_t i = 0; i < sizeof(K) / sizeof(K[0]); i++) if (k == K[i]) return true;
    return false;
}

// 给误拼的参数找一个最接近的已知名（编辑距离，够用就行）
static string suggestArg(const string& a) {
    if (a.rfind("--", 0) != 0) return string();
    size_t eq = a.find('=');
    const string k = a.substr(2, eq == string::npos ? string::npos : eq - 2);
    static const char* K[] = {
        "datadir", "rlv", "cm", "ctrl", "level", "prog", "qual", "dur", "cp",
        "qabs", "q0", "depth", "mode", "perlayer", "ban", "seq", "quiet", "strict", "help"
    };
    const char* best = 0; int bestD = 3;   // 只报距离 <= 2 的（K 是 const char*[]）
    for (size_t i = 0; i < sizeof(K) / sizeof(K[0]); i++) {
        const string s = K[i];
        vector<vector<int>> d(k.size() + 1, vector<int>(s.size() + 1, 0));
        for (size_t x = 0; x <= k.size(); x++) d[x][0] = (int)x;
        for (size_t y = 0; y <= s.size(); y++) d[0][y] = (int)y;
        for (size_t x = 1; x <= k.size(); x++)
            for (size_t y = 1; y <= s.size(); y++)
                d[x][y] = min(min(d[x - 1][y] + 1, d[x][y - 1] + 1),
                              d[x - 1][y - 1] + (k[x - 1] == s[y - 1] ? 0 : 1));
        if (d[k.size()][s.size()] < bestD) { bestD = d[k.size()][s.size()]; best = K[i]; }
    }
    if (best) return string("（是否想写 --") + best + " ？）";
    return string();
}

// --help（以及不带任何参数时）打印的说明。内容与 README 的「参数表」保持一致。
static void printHelp() {
    printf(
        "ffxiv-crafting-weaver —— FF14 生产工序求解器\n"
        "  给定配方与三维属性，求最少步数把品质拉满并完成制作的工序，\n"
        "  并给出该步数层的容错前沿（品质溢出 / CP 余量）与 HQ 效益评估。\n"
        "\n"
        "用法：\n"
        "  craftweave.exe --rlv=<配方品级> --cm=<作业> --ctrl=<加工> --cp=<制作力> [其他]\n"
        "\n"
        "必给：\n"
        "  --rlv      配方品级。难度/品质/耐久上限与修正系数、压制系数都由它查 data/ 带出\n"
        "  --cm       作业精度    --ctrl  加工精度    --cp  制作力\n"
        "\n"
        "可选：\n"
        "  --lv       职业等级（默认 100），用于等级压制判定\n"
        "  --prog     配方难度（不给则查表）\n"
        "  --qual     配方上限品质（不给则查表）\n"
        "  --dur      配方耐久（不给则查表）\n"
        "  --qabs     品质条初始值（绝对值）\n"
        "  --q0       品质条初始值（百分比，默认 0%%）\n"
        "  --mode     fixed=固定模式（默认）｜expected=概率&期望模式\n"
        "  --depth    最大探索步数（默认 22），给不够会直接报无解\n"
        "  --perlayer 每层探索量级（默认 150000），调小会丢解\n"
        "  --ban      禁用某技能，多参数以逗号分隔\n"
        "             例 --ban=rapidSynthesis,hastyTouch,daringTouch\n"
        "  --seq      跳过搜索，直接评估一条序列（支持 key*N），并输出它的 HQ 效益\n"
        "  --datadir  数据目录（默认：可执行文件同级的 data/）\n"
        "  --quiet    不打印进度行（进度只走 stderr，不影响 stdout 的结果）\n"
        "  --strict   未知参数时直接报错退出（默认只警告并继续执行）\n"
        "  --help     显示本说明\n"
        "\n"
        "技能名见 data/actions.csv 第一列（basicTouch 加工、preparatoryTouch 坯料加工、\n"
        "groundwork 坯料制作、trainedPerfection 工匠的绝技…）。\n"
        "\n"
        "示例：\n"
        "  craftweave.exe --rlv=770 --cm=5878 --ctrl=5623 --cp=749 --q0=50 --depth=20 --perlayer=2000000\n"
        "  craftweave.exe --seq=muscleMemory,preparatoryTouch*2,byregot --rlv=770 --cm=5878 --ctrl=5623 --cp=749 --qabs=10600\n"
        "\n"
        "注意：--perlayer 是启发式裁剪，不是有保证的剪枝。报「最少 N 步」时请连同\n"
        "      探索量级一起报；报「无解」时请先调大探索量级复核。\n");
}

int main(int argc, char** argv) {
#ifdef _WIN32
    // 控制台按 UTF-8 解释输出：否则在代码页 936(GBK) 的终端里中文会整片乱码。
    // 只影响控制台显示；重定向到文件时不受影响（文件里始终是 UTF-8 字节）。
    // 注：PowerShell 用管道捕获本程序输出时按它自己的 [Console]::OutputEncoding 解码，
    //     子进程改不了父进程的设置 —— 那是用户侧需要 chcp 65001 的场景（见 README）。
    SetConsoleOutputCP(CP_UTF8);
#endif
    if (argc < 2) { printHelp(); return 0; }   // 不带参数时给说明，而不是跑一次全 0 的搜索

    // 未知参数：默认【只警告、继续执行】（与以前的静默忽略相比不改变行为，但你能看见）；
    // 加 --strict 才升级为报错退出（适合脚本/CI）。
    {
        bool strict = false;
        for (int i = 1; i < argc; i++) if (string(argv[i]) == "--strict") strict = true;
        for (int i = 1; i < argc; i++) {
            const string a = argv[i];
            if (isKnownArg(a)) continue;
            fprintf(stderr, "⚠️ 未知参数，已忽略：%s %s\n", a.c_str(), suggestArg(a).c_str());
            if (strict) { fprintf(stderr, "（--strict 已启用，故直接退出）\n"); return 2; }
        }
    }

    // 数据目录：默认与 exe 同级的 data\（项目自带）；可用 --datadir= 覆盖
    std::filesystem::path dataDir = std::filesystem::absolute(argv[0]).parent_path() / "data";
    string dataDirArg;
    int rlvArg = -1;
    double cmArg = -1, ctrlArg = -1;
    bool baseGiven = false;
    for (int i = 1; i < argc; i++) {
        string a = argv[i];
        if (a == "--help" || a == "-h") { printHelp(); return 0; }
        if (a.rfind("--datadir=", 0) == 0) dataDirArg = a.substr(10);
        else if (a == "--quiet" || a == "--no-progress") C.quiet = true;
        else if (a.rfind("--rlv=", 0) == 0) rlvArg = atoi(a.c_str() + 6);
        else if (a.rfind("--cm=", 0) == 0) cmArg = atof(a.c_str() + 5);
        else if (a.rfind("--ctrl=", 0) == 0) ctrlArg = atof(a.c_str() + 7);
        else if (a.rfind("--baseProg=", 0) == 0 || a.rfind("--baseQual=", 0) == 0) baseGiven = true;
    }
    if (!dataDirArg.empty()) dataDir = std::filesystem::u8path(dataDirArg);
    // 载入项目自带数据（缺文件会明确报错，不静默按 0 算）
    if (!loadActions(dataDir / "actions.csv")) return 2;
    if (!loadTables(dataDir)) return 2;

    // 若给了 --rlv 而没给基准值，就自己查表算
    // 必须放在 --seq 直评之前 —— 否则直评路径拿不到 baseProg/baseQual（表现：进展与品质恒为 0，但 CP/耐久照扣）
    if (rlvArg >= 0 && !baseGiven) {
        (void)0; // 表已在启动时载入
        auto it = RTAB.find(rlvArg);
        if (!RTABok || it == RTAB.end() || it->second.pd <= 0) {
            fprintf(stderr, "rlv %d 在 rlv 表里查不到（可用 --datadir= 指定数据目录；当前：%s）\n",
                    rlvArg, dataDir.u8string().c_str());
            return 2;
        }
        const RlvRow& r = it->second;
        if (C.progMax <= 0) C.progMax = r.diff;
        if (C.qualMax <= 0) C.qualMax = r.qual;
        if (C.durMax <= 0) C.durMax = r.dur;
        auto eq = EQUIV.find(C.level);
        bool suppressed = (eq != EQUIV.end() && eq->second <= rlvArg);
        C.baseProg = (int)floor(((cmArg * 10) / r.pd + 2) * (suppressed ? r.pm / 100.0 : 1.0));
        C.baseQual = (int)floor(((ctrlArg * 10) / r.qd + 35) * (suppressed ? r.qm / 100.0 : 1.0));
        fprintf(stderr, "【rlv 表】rlv %d → 配方等级 %d ｜ 难度 %d ｜ 品质 %d ｜ 耐久 %d ｜ 掩码 %d ｜ PD %.0f QD %.0f PM %.0f%% QM %.0f%%%s\n",
                rlvArg, r.cjl, r.diff, r.qual, r.dur, r.mask, r.pd, r.qd, r.pm, r.qm,
                suppressed ? "  ｜ 等级压制生效" : "");
    }

    ACT_OBSERVE = findActByKey("observe");
    ACT_STANDARD = findActByKey("standardTouch");
    vector<int> banIdx;
    for (int i = 1; i < argc; i++) {
        string a = argv[i]; if (a.rfind("--", 0) != 0) continue;
        size_t eq = a.find('='); string k = a.substr(2, eq - 2), v = a.substr(eq + 1);
        auto iv = [&]() { return atoi(v.c_str()); };
        if (k == "level") C.level = iv();
        else if (k == "progMax" || k == "prog") C.progMax = iv();
        else if (k == "qualMax" || k == "qual") C.qualMax = iv();
        else if (k == "durMax" || k == "dur") C.durMax = iv();
        else if (k == "cpMax" || k == "cp") C.cpMax = iv();
        else if (k == "baseProg") C.baseProg = iv();
        else if (k == "baseQual") C.baseQual = iv();
        else if (k == "startQual" || k == "qabs") C.startQual = atof(v.c_str());
        else if (k == "startQualPct" || k == "q0") C.startQualPct = atof(v.c_str());
        else if (k == "depth") C.maxDepth = iv();
        else if (k == "mode") EXPECTED = (v == "expected"); // fixed（默认，固定模式）｜ expected（概率&期望模式）
        else if (k == "perlayer") C.perLayer = iv();
        else if (k == "ban") { size_t p = 0; while (p <= v.size()) { size_t q = v.find(',', p); string nm = v.substr(p, q == string::npos ? string::npos : q - p); if (!nm.empty()) { int idx = -1; for (size_t j = 0; j < AKEY.size(); j++) if (AKEY[j] == nm) idx = (int)j; if (idx >= 0) banIdx.push_back(idx); } if (q == string::npos) break; p = q + 1; } }
    }
    // --seq=<序列>：跳过搜索，直接评估该序列的 HQ 效益（支持 key*N 简写，便于复算）
    {
        string seqSpec;
        for (int i = 1; i < argc; i++) {
            string a = argv[i];
            if (a.rfind("--seq=", 0) == 0) seqSpec = a.substr(6);
        }
        if (!seqSpec.empty()) {
            vector<int> sq;
            size_t p0 = 0;
            while (p0 <= seqSpec.size()) {
                size_t q0 = seqSpec.find(',', p0);
                string nm = seqSpec.substr(p0, q0 == string::npos ? string::npos : q0 - p0);
                if (!nm.empty()) {
                    int repN = 1; string rep = nm;
                    size_t star = nm.find('*');
                    if (star != string::npos) { rep = nm.substr(0, star); repN = atoi(nm.c_str() + star + 1); }
                    int idx = -1;
                    for (size_t j = 0; j < AKEY.size(); j++) if (AKEY[j] == rep) { idx = (int)j; break; }
                    if (idx < 0) { fprintf(stderr, "未知动作: %s\n", rep.c_str()); return 2; }
                    for (int r = 0; r < repN; r++) sq.push_back(idx);
                }
                if (q0 == string::npos) break;
                p0 = q0 + 1;
            }
            St v = newState(); bool okAll = true; bool vDone = false;
            for (size_t i = 0; i < sq.size(); i++) {
                if (!canUse(v, sq[i], false, false)) { fprintf(stderr, "第 %zu 手「%s」不可用\n", i + 1, A[sq[i]].cn); okAll = false; break; }
                St nx; double gp, gq; bool dn, fl;
                applyAct(v, sq[i], nx, gp, gq, dn, fl);
                v = nx;
                if (fl) { fprintf(stderr, "第 %zu 手后失败\n", i + 1); okAll = false; break; }
                if (dn) { vDone = true; break; }
            }
            fprintf(stderr, "SEQ_MODE %zu 手 ｜ %s ｜ 进展 %.2f/%.0f ｜ 品质 %.2f/%.0f ｜ 耐久 %d ｜ CP %d\n",
                    sq.size(), vDone ? "完成" : (okAll ? "未收尾" : "不可行"), v.prog, (double)C.progMax, v.qual, (double)C.qualMax, v.dur, v.cp);
            // 结构化输出（与搜索模式同格式，供 check.ps1 做秒级回放对拍）
            printf("STEPS %zu\n", sq.size());
            if (EXPECTED) printf("FINAL %.2f %.2f %.2f %d %d\n", v.prog, v.qual, v.qualRaw, v.dur, v.cp);
            else printf("FINAL %d %d %d %d %d\n", (int)llround(v.prog), (int)llround(v.qual), (int)llround(v.qualRaw), v.dur, v.cp);
            {
                string s2;
                for (size_t i = 0; i < sq.size(); i++) { if (i) s2 += ","; s2 += AKEY[sq[i]]; }
                printf("SEQ %s\n", s2.c_str());
            }
            reportHQ(sq);
            return 0;
        }
    }

    vector<int> ALL;
    for (size_t i = 0; i < A.size(); i++) {
        if (A[i].lv > C.level) continue;
        // ★ 固定模式（确定性）排除概率技能：它们的成败本就是概率的，放进确定性搜索会得到
        //    "看起来更短、实战成功率极低"的假解（例：4 个高速制作连成，成功率仅 6.25%）。
        //    概率&期望模式（expected）保留它们并按成功率折算。
        //    注意：这只影响搜索；--seq 直评仍可评估含概率技能的序列。
        if (!EXPECTED && A[i].success < 1.0) continue;
        bool b = false; for (int x : banIdx) if (x == (int)i) b = true;
        if (!b) ALL.push_back((int)i);
    }

    fprintf(stderr, "【C++】模式 %s ｜ 目标 进度 %d ＋ 品质 %d（初始 %.2f）｜ 耐久 %d ｜ CP %d ｜ 动作 %zu ｜ 深度 %d\n",
            EXPECTED ? "概率&期望模式" : "固定模式", C.progMax, C.qualMax, newState().qual, C.durMax, C.cpMax, ALL.size(), C.maxDepth);

    // ---- 搜索（已拆到 solver.cpp；输出格式保持不变）----
    SolveResult R = solve(ALL);

    if (!R.found) { printf("NO_SOLUTION\n"); return 1; }
    printf("STEPS %d\n", R.steps);
    printf("FRONT %zu\n", R.front.size());
    for (size_t i = 0; i < R.front.size(); i++) {
        const St& g = R.front[i];
        if (EXPECTED) printf("F %.2f %d %d %.2f %.2f\n", g.qualRaw - C.qualMax, g.cp, g.dur, g.prog, g.qualRaw);
        else printf("F %d %d %d %d %d\n", (int)llround(g.qualRaw - C.qualMax), g.cp, g.dur, (int)llround(g.prog), (int)llround(g.qualRaw));
    }

    // front[0] 即"溢出最大"那条解（求解器已按 (溢出, CP) 降序排好）
    {
        const St& g = R.front[0];
        if (EXPECTED) printf("FINAL %.2f %.2f %.2f %d %d\n", g.prog, g.qual, g.qualRaw, g.dur, g.cp);
        else printf("FINAL %d %d %d %d %d\n", (int)llround(g.prog), (int)llround(g.qual), (int)llround(g.qualRaw), g.dur, g.cp);
        string out;
        for (size_t i = 0; i < R.bestSeq.size(); i++) { if (i) out += ","; out += AKEY[R.bestSeq[i]]; }
        printf("SEQ %s\n", out.c_str());

        reportHQ(R.bestSeq); // 解完即算 HQ 效益（以品质条溢出为根据）
    }
    return 0;
}

