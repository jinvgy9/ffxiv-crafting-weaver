// 引擎自测：覆盖"机制要点 + 数据加载"，不引第三方框架，失败即非零退出（ctest 认这个）。
//   输出**刻意用英文**：PowerShell 5.1 读子进程的中文输出会乱码，测试日志要能直接看。
//   数据目录：环境变量 WEAVER_DATA（CMake 里设好），或用 argv[1]。
//   注：搜索（BFS）目前仍在 main.cpp 内，故这里只测引擎与数据层；搜索回归见 tests/cases.csv。
#include "craft/csv.hpp"
#include "craft/engine.hpp"
#include "craft/hqreport.hpp"
#include "craft/model.hpp"
#include "craft/solver.hpp"
#include "craft/tables.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace std;

static int gFail = 0, gTotal = 0;
static void okd(const char* label, double got, double want) {
    gTotal++;
    const bool pass = fabs(got - want) < 1e-6;
    if (!pass) gFail++;
    printf("%s %-52s got=%.4f want=%.4f\n", pass ? "[ ok ]" : "[FAIL]", label, got, want);
}
static void okb(const char* label, bool got) { okd(label, got ? 1 : 0, 1); }

// 按 key 施加一手，返回动作是否可用
static bool step(const char* key) {
    const int k = findActByKey(key);
    if (k < 0) return false;
    St s = newState();
    return true;
}

int main(int argc, char** argv) {
    string dataDir = (argc > 1) ? argv[1] : "";
    if (dataDir.empty()) { const char* e = getenv("WEAVER_DATA"); if (e) dataDir = e; }
    if (dataDir.empty()) { printf("[FAIL] data dir not given (set WEAVER_DATA or pass argv[1])\n"); return 2; }

    // ---------------- 数据加载 ----------------
    okb("loadActions(data/actions.csv)", loadActions(dataDir + "/actions.csv"));
    okb("loadTables(data/)", loadTables(dataDir));
    okd("action count", (double)A.size(), 29);

    // 关键技能属性（这些错了会静默算错，所以逐条钉住）
    {
        const int k = findActByKey("basicTouch");
        okd("basicTouch eff", A[k].eff, 100);
        okd("basicTouch cp", A[k].cp, 18);
        okd("basicTouch type(0=quality)", A[k].type, 0);
    }
    {
        const int k = findActByKey("delicateSynthesis");
        okd("delicateSynthesis type(2=both)", A[k].type, 2);
        okd("delicateSynthesis progEff", A[k].progEff, 150);
        okd("delicateSynthesis qualEff", A[k].qualEff, 100);
    }
    {
        const int k = findActByKey("byregot");
        okd("byregot iqPer", A[k].iqPer, 20);
        okd("byregot effCap", A[k].effCap, 300);
        okd("byregot needIQ", A[k].needIQ, 1);
        okd("byregot clearIQ", A[k].clearIQ, 1);
    }
    // buff_id 必须是 0-based（这一列曾整列偏移 +1，只在多手序列才暴露）
    okd("innovation buffId", A[findActByKey("innovation")].buffId, 0);
    okd("veneration buffId", A[findActByKey("veneration")].buffId, 1);
    okd("greatStrides buffId", A[findActByKey("greatStrides")].buffId, 2);
    okd("wasteNotII buffId", A[findActByKey("wasteNotII")].buffId, 3);
    okd("manipulation buffId", A[findActByKey("manipulation")].buffId, 4);
    okd("finalAppraisal buffId", A[findActByKey("finalAppraisal")].buffId, 5);
    okd("manipulation steps", A[findActByKey("manipulation")].steps, 8);
    okd("groundwork halfIfShort", A[findActByKey("groundwork")].halfIfShort, 1);
    okd("trainedPerfection noDurNext", A[findActByKey("trainedPerfection")].noDurNext, 1);
    okd("rapidSynthesis success", A[findActByKey("rapidSynthesis")].success, 0.5);
    okd("standardTouch fromMask", (double)A[findActByKey("standardTouch")].fromMask,
        (double)(1u << findActByKey("basicTouch")));

    // rlv 表 + 等级压制（取 rlv 770：PD170 QD150 / 难度10040 品质21200 耐久70 掩码15）
    {
        const RlvRow& r = RTAB[770];
        okd("rlv770 pd", r.pd, 170);
        okd("rlv770 qd", r.qd, 150);
        okd("rlv770 pm%", r.pm, 90);
        okd("rlv770 qm%", r.qm, 75);
        okd("rlv770 difficulty", r.diff, 10040);
        okd("rlv770 quality", r.qual, 21200);
        okd("rlv770 durability", r.dur, 70);
        okd("rlv770 mask", r.mask, 15);
        okd("equiv level 100", EQUIV[100], 690);
        okd("equiv level 1", EQUIV[1], 1);
        okd("equiv level 71", EQUIV[71], 381);
    }
    // HQ 表
    okd("hqProb(99)", hqProb(99), 98);
    okd("hqProb(0)", hqProb(0), 1);
    okd("hqProb(100)", hqProb(100), 100);

    // ---------------- 机制 ----------------
    // 装上 rlv770 的配方参数（主配方：作业 5878 / 加工 5623），基准值应为 进展 312 / 品质 307
    const RlvRow& r770 = RTAB[770];
    C.level = 100;
    C.progMax = r770.diff;
    C.qualMax = r770.qual;
    C.durMax = r770.dur;
    C.cpMax = 749;
    C.startQual = 10600;   // 品质条初始 50%
    const bool suppressed = (EQUIV[100] <= 770);
    C.baseProg = (int)floor(((5878.0 * 10) / r770.pd + 2) * (suppressed ? r770.pm / 100.0 : 1.0));
    C.baseQual = (int)floor(((5623.0 * 10) / r770.qd + 35) * (suppressed ? r770.qm / 100.0 : 1.0));
    okd("baseProg @rlv770", C.baseProg, 312);
    okd("baseQual @rlv770", C.baseQual, 307);

    // 闲静：品质 307×3.0 = 921，内静结算后 +2
    {
        St s = newState();
        St t; double gp, gq; bool dn = false, fl = false;
        okb("reflect usable on first step", canUse(s, findActByKey("reflect"), false, false));
        applyAct(s, findActByKey("reflect"), t, gp, gq, dn, fl);
        okd("reflect gainQual", gq, 921);
        okd("reflect -> iq", t.iq, 2);
        okd("first flag cleared", fget(t, FB_FIRST) ? 1 : 0, 0);
    }
    // 坚信是首手专用：第二手不可用
    {
        St s = newState();
        St t; double gp, gq; bool dn = false, fl = false;
        applyAct(s, findActByKey("basicSynthesis"), t, gp, gq, dn, fl);
        okb("muscleMemory rejected after first step", !canUse(t, findActByKey("muscleMemory"), false, false));
    }
    // 连击链：加工 → 中级加工 是 18 CP；坯料加工 → 中级加工 不是（中级加工自己的 CP 是 32）
    {
        St s = newState();
        St t; double gp, gq; bool dn = false, fl = false;
        applyAct(s, findActByKey("basicTouch"), t, gp, gq, dn, fl);
        okd("basicTouch->standardTouch cp", cpCostOf(t, findActByKey("standardTouch")), 18);
    }
    {
        St s = newState();
        St t; double gp, gq; bool dn = false, fl = false;
        applyAct(s, findActByKey("preparatoryTouch"), t, gp, gq, dn, fl);
        okd("preparatoryTouch->standardTouch cp", cpCostOf(t, findActByKey("standardTouch")), 32);
    }
    // 俭约加工在俭约状态下不可用
    {
        St s = newState();
        St t; double gp, gq; bool dn = false, fl = false;
        applyAct(s, findActByKey("wasteNot"), t, gp, gq, dn, fl);
        okb("prudentTouch rejected under wasteNot", !canUse(t, findActByKey("prudentTouch"), false, false));
    }
    // 改革：上 buff 那手不占计数（窗口 4 手，第 5 手归零）
    {
        St s = newState();
        St t; double gp, gq; bool dn = false, fl = false;
        applyAct(s, findActByKey("innovation"), t, gp, gq, dn, fl);
        okd("innovation counter after cast", bget(t, BI), 4);
        int seq2 = 0;
        for (int i = 0; i < 4; i++) { applyAct(t, findActByKey("basicTouch"), t, gp, gq, dn, fl); seq2 = bget(t, BI); }
        okd("innovation counter after 4 steps", seq2, 0);
    }
    // 完成判定：完工优先于耐久耗尽（最后一手把进度推满，即使耐久为负也算成功）
    //   ★ 注意不能写 applyAct(t, k, t, ...)：s0 与 s 同一对象会自赋值后又被 fset 改到，逻辑被污染
    {
        // progMax 取 900：三手累计 374 + 0 + 561 = 935（坯料制作在耐久 10 时触发减半，561 而非 1123），
        // 刚好越过 900 完工，而前两手都还没完工 —— 参数必须算准，否则测的是别的东西
        C.progMax = 900; C.qualMax = 0; C.durMax = 30; C.startQual = 0; C.startQualPct = 0;
        St s = newState(), t1, t2, t3;
        double gp, gq; bool dn = false, fl = false;
        applyAct(s, findActByKey("basicSynthesis"), t1, gp, gq, dn, fl); // 耐久 30→20
        applyAct(t1, findActByKey("basicTouch"), t2, gp, gq, dn, fl);    // 20→10
        applyAct(t2, findActByKey("groundwork"), t3, gp, gq, dn, fl);    // 10−20 = −10，但本手推满进度
        okd("finish wins over durability: done", dn ? 1 : 0, 1);
        okd("finish wins over durability: dur", t3.dur, -10);
        // 复原主配方参数
        C.progMax = r770.diff; C.qualMax = r770.qual; C.durMax = r770.dur; C.startQual = 10600; C.startQualPct = 0;
    }
    // 工匠的绝技：无步数倒计时（跨 buff 仍挂着）
    {
        St s = newState(), t1, t2, t3, t4;
        double gp, gq; bool dn = false, fl = false;
        applyAct(s, findActByKey("trainedPerfection"), t1, gp, gq, dn, fl);
        applyAct(t1, findActByKey("innovation"), t2, gp, gq, dn, fl);
        applyAct(t2, findActByKey("greatStrides"), t3, gp, gq, dn, fl);
        okd("trainedPerfection survives buffs", fget(t3, FB_NO_DUR) ? 1 : 0, 1);
        const int durBefore = t3.dur;
        applyAct(t3, findActByKey("basicTouch"), t4, gp, gq, dn, fl);
        okd("no durability spent on next consuming action", t4.dur, durBefore);
        okd("and it is consumed then", fget(t4, FB_NO_DUR) ? 1 : 0, 0);
    }
    // 掌握：0 耐久消耗的 buff 手也回耐（★ 要先扣掉耐久，否则"封顶"会把效果盖住）
    {
        St s = newState(), t1, t2, t3;
        double gp, gq; bool dn = false, fl = false;
        applyAct(s, findActByKey("basicTouch"), t1, gp, gq, dn, fl);      // 先花 10 耐久
        applyAct(t1, findActByKey("manipulation"), t2, gp, gq, dn, fl);   // 上掌握（使用那一手本身不回）
        const int d0 = t2.dur;
        applyAct(t2, findActByKey("innovation"), t3, gp, gq, dn, fl);     // 0 耐久消耗的手
        okd("manipulation restores on 0-durability step", t3.dur, d0 + 5);
    }

    // ---------------- 搜索（拆出 solver.cpp 之后才可能测到这里）----------------
    {
        // 先保存会被改动的配置
        const int sProgMax = C.progMax, sQualMax = C.qualMax, sDurMax = C.durMax, sCpMax = C.cpMax;
        const double sStartQual = C.startQual, sStartQualPct = C.startQualPct;
        const int sBaseProg = C.baseProg, sBaseQual = C.baseQual;
        const int sMaxDepth = C.maxDepth, sPerLayer = C.perLayer;
        const bool sQuiet = C.quiet;

        C.quiet = true;            // 测试里不要进度条
        C.perLayer = 250000;
        C.startQual = 0; C.startQualPct = 0;
        C.baseProg = 100; C.baseQual = 50;   // 让数值可手算：制作(120%) = 120 进展

        // 受控动作集：只放这三个，避免"坯料制作 360% / 高速制作 500%"这类大招把步数一口吃满
        //   制作 = 120% 进展 ｜ 加工 = 100% 品质 ｜ 改革 = 品质 +50%（不推进度）
        vector<int> few;
        few.push_back(findActByKey("basicSynthesis"));
        few.push_back(findActByKey("basicTouch"));
        few.push_back(findActByKey("innovation"));

        // 用例 1：进度上限 50 → 一手「制作」(100×1.2=120) 就打满，品质上限 0 自动达标 ⇒ 必然 1 步
        C.progMax = 50; C.qualMax = 0; C.durMax = 40; C.cpMax = 100; C.maxDepth = 4;
        SolveResult r1 = solve(few);
        okb("solve: found", r1.found);
        okd("solve: steps == 1 (one-shot recipe)", r1.steps, 1);
        okd("solve: bestSeq length", (double)r1.bestSeq.size(), 1);
        okd("solve: bestSeq[0] is basicSynthesis", (double)r1.bestSeq[0], (double)findActByKey("basicSynthesis"));
        okd("solve: front not empty", (double)(r1.front.size() > 0 ? 1 : 0), 1);

        // 用例 2：进度上限 150 → 一手 120 不够，必须两手 ⇒ 2 步
        C.progMax = 150;
        SolveResult r2 = solve(few);
        okb("solve: 2-step found", r2.found);
        okd("solve: steps == 2", r2.steps, 2);

        // 用例 3：进度上限 50 但深度只给…… 不设限时仍应 1 步；改为把 maxDepth 压到 0 之外的边界不易构造，
        //         这里改测"品质上限也给一点"⇒ 需要品质动作参与
        C.progMax = 50; C.qualMax = 100; C.durMax = 40; C.cpMax = 100; C.maxDepth = 6;
        SolveResult r3 = solve(few);
        okb("solve: prog+qual found", r3.found);
        okd("solve: prog+qual quality met", (r3.found && r3.front[0].qual >= C.qualMax) ? 1 : 0, 1);
        okd("solve: prog+qual progress met", (r3.found && r3.front[0].prog >= C.progMax) ? 1 : 0, 1);

        // 复原
        C.progMax = sProgMax; C.qualMax = sQualMax; C.durMax = sDurMax; C.cpMax = sCpMax;
        C.startQual = sStartQual; C.startQualPct = sStartQualPct;
        C.baseProg = sBaseProg; C.baseQual = sBaseQual;
        C.maxDepth = sMaxDepth; C.perLayer = sPerLayer; C.quiet = sQuiet;
    }

    printf("\n%s  total=%d  failed=%d\n", gFail == 0 ? "ALL PASS" : "FAILURES", gTotal, gFail);
    return gFail == 0 ? 0 : 1;
}
