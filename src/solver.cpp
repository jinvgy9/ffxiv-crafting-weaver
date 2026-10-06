#include "craft/solver.hpp"
#include "craft/engine.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>
using namespace std;

// 搜索：BFS 逐层扩展 ＋ 每层按 4 维 Pareto 支配剪枝（品质↑ 进度↑ 耐久↑ CP↑ 全不劣才算被支配）
// 支配剪枝有理论保证；每层的 C.perLayer 裁剪没有
SolveResult solve(const vector<int>& allowed) {
    SolveResult R;
    const vector<int>& ALL = allowed;
    St init = newState();
    vector<St> layer; layer.push_back(init);
    // 父指针：每层一个 par 数组（指向上一层下标），act 数组同理
    vector<vector<int>> parHist, actHist;
    bool found = false; int foundDepth = 0;
    struct Goal { St st; int par; int act; };
    vector<Goal> bestGoals;
    auto t0 = chrono::steady_clock::now();
    auto layerStart = t0;
    vector<double> layerSecs;   // 每层耗时，用于估算剩余时间

    for (int d = 1; d <= C.maxDepth && !found; d++) {
        unordered_map<uint64_t, vector<int>> buckets; // key -> next 层下标
        vector<St> next; vector<int> par, act;
        vector<Goal> goals;
        for (size_t si = 0; si < layer.size(); si++) {
            const St& s = layer[si];
            bool qFull = (s.qual >= C.qualMax);
            for (int k : ALL) {
                if (qFull && A[k].type == 0) continue; // 品质封顶后纯品质动作零收益
                if (!canUse(s, k, false, false)) continue;
                St t; double gp, gq; bool dn, fl;
                applyAct(s, k, t, gp, gq, dn, fl);
                if (fl) continue;
                if (dn) { goals.push_back({ t, (int)si, k }); continue; } // 收集本层全部解（不立即停）
                uint64_t key = keyOf(t);
                auto it = buckets.find(key);
                if (it == buckets.end()) { buckets.emplace(key, vector<int>{ (int)next.size() }); }
                else {
                    auto& lst = it->second;
                    bool dom = false;
                    for (int idx : lst) {
                        const St& o = next[idx];
                        if (o.qual >= t.qual && o.prog >= t.prog && o.dur >= t.dur && o.cp >= t.cp) { dom = true; break; }
                    }
                    if (dom) continue;
                    for (size_t z = lst.size(); z-- > 0;) {
                        const St& o = next[lst[z]];
                        if (t.qual >= o.qual && t.prog >= o.prog && t.dur >= o.dur && t.cp >= o.cp) lst.erase(lst.begin() + z);
                    }
                    lst.push_back((int)next.size());
                }
                next.push_back(t); par.push_back((int)si); act.push_back(k);
            }
        }
        if (!goals.empty()) {
            // 本层即最少步数层：按 (溢出, CP) 去重，再求 Pareto 前沿（不加权）
            sort(goals.begin(), goals.end(), [](const Goal& x, const Goal& y) {
                int ox = x.st.qualRaw - C.qualMax, oy = y.st.qualRaw - C.qualMax;
                if (ox != oy) return ox > oy;
                return x.st.cp > y.st.cp;
            });
            vector<Goal> uniq;
            {   // 去重：同 (溢出,CP) 只留耐久最好的一个
                vector<char> dead(goals.size(), 0);
                for (size_t i = 0; i < goals.size(); i++) {
                    if (dead[i]) continue;
                    for (size_t j = i + 1; j < goals.size(); j++) {
                        if (dead[j]) continue;
                        if (goals[i].st.qualRaw == goals[j].st.qualRaw && goals[i].st.cp == goals[j].st.cp) {
                            if (goals[j].st.dur > goals[i].st.dur) goals[i] = goals[j];
                            dead[j] = 1;
                        }
                    }
                    uniq.push_back(goals[i]);
                }
            }
            vector<Goal> front;
            for (size_t i = 0; i < uniq.size(); i++) {
                bool dom = false;
                for (size_t j = 0; j < uniq.size(); j++) {
                    if (i == j) continue;
                    if (uniq[j].st.qualRaw >= uniq[i].st.qualRaw && uniq[j].st.cp >= uniq[i].st.cp &&
                        (uniq[j].st.qualRaw > uniq[i].st.qualRaw || uniq[j].st.cp > uniq[i].st.cp)) { dom = true; break; }
                }
                if (!dom) front.push_back(uniq[i]);
            }
            sort(front.begin(), front.end(), [](const Goal& x, const Goal& y) {
                if (x.st.qualRaw != y.st.qualRaw) return x.st.qualRaw > y.st.qualRaw;
                return x.st.cp > y.st.cp;
            });
            found = true; foundDepth = d; bestGoals = front;

            fprintf(stderr, "本层共找到 %zu 个 %d 步可行解（去重后 %zu 种容错组合）；Pareto 前沿 %zu 个：\n",
                    goals.size(), d, uniq.size(), front.size());
            for (size_t i = 0; i < front.size() && i < 8; i++) {
                if (EXPECTED) fprintf(stderr, "  · 溢出 %8.2f ｜ CP 余 %3d ｜ 耐久余 %3d\n", front[i].st.qualRaw - C.qualMax, front[i].st.cp, front[i].st.dur);
                else fprintf(stderr, "  · 溢出 %4d ｜ CP 余 %3d ｜ 耐久余 %3d\n", (int)llround(front[i].st.qualRaw - C.qualMax), front[i].st.cp, front[i].st.dur);
            }
            break;
        }
        layer.swap(next); parHist.push_back(move(par)); actHist.push_back(move(act));
        if ((int)layer.size() > C.perLayer) {
            // 兜底层上限：按完成度（品质完成度 + 进度完成度）排序保留
            vector<int> ord(layer.size()); for (size_t i = 0; i < ord.size(); i++) ord[i] = (int)i;
            auto sc = [&](int i) { const St& s = layer[i]; return min(1.0, (double)s.qual / C.qualMax) + min(1.0, (double)s.prog / C.progMax); };
            sort(ord.begin(), ord.end(), [&](int x, int y) { return sc(x) > sc(y); });
            vector<St> nl; vector<int> np, na;
            for (int i = 0; i < C.perLayer; i++) { nl.push_back(layer[ord[i]]); np.push_back(parHist.back()[ord[i]]); na.push_back(actHist.back()[ord[i]]); }
            layer.swap(nl); parHist.back() = move(np); actHist.back() = move(na);
        }
        // ---- 进度 + 剩余时间粗估 ----
        {
            const auto now = chrono::steady_clock::now();
            const double layerT = chrono::duration<double>(now - layerStart).count();
            const double totalT = chrono::duration<double>(now - t0).count();
            layerSecs.push_back(layerT);
            layerStart = now;
            if (!C.quiet) {
                // 剩余估计：
                //   · 只拿"耗时 >= 0.05s"的层当基线（前几层的 0.0s 会把增长比算爆）
                //   · 层大小一旦触顶（== perlayer），后续层代价≈常数 → 线性外推
                //   · 未触顶时按涨幅外推，但封顶 1.4/层（实测涨幅远小于此）
                double eta = -1.0;
                {
                    vector<double> sig;
                    for (size_t i = 0; i < layerSecs.size(); i++)
                        if (layerSecs[i] >= 0.05) sig.push_back(layerSecs[i]);
                    if (!sig.empty()) {
                        const size_t m = sig.size();
                        double base = sig[m - 1];
                        if (m >= 3) base = (sig[m - 1] + sig[m - 2] + sig[m - 3]) / 3.0;
                        double ratio = 1.0;
                        const bool capped = (layer.size() >= (size_t)C.perLayer);
                        if (!capped && m >= 4) {
                            const double prev = (sig[m - 3] + sig[m - 4]) / 2.0;
                            if (prev > 1e-6) ratio = base / prev;
                            if (ratio < 1.0) ratio = 1.0;
                            if (ratio > 1.4) ratio = 1.4;
                        }
                        double sum = 0.0, term = base;
                        for (int i = 0; i < C.maxDepth - d; i++) { term *= ratio; sum += term; if (sum > 1e9) break; }
                        eta = sum;
                    }
                }
                int bars = (int)(20.0 * d / C.maxDepth + 0.5);
                if (bars > 20) bars = 20; if (bars < 0) bars = 0;
                char bar[24];
                for (int i = 0; i < 20; i++) bar[i] = (i < bars) ? '#' : '-';
                bar[20] = 0;
                char etaBuf[48];
                if (eta < 0) snprintf(etaBuf, sizeof(etaBuf), "粗估中…");
                else if (eta < 60) snprintf(etaBuf, sizeof(etaBuf), "~%.0f 秒", eta);
                else if (eta < 3600) snprintf(etaBuf, sizeof(etaBuf), "~%.1f 分", eta / 60.0);
                else if (eta < 21600) snprintf(etaBuf, sizeof(etaBuf), "~%.1f 小时", eta / 3600.0);
                else snprintf(etaBuf, sizeof(etaBuf), "很久（>6 小时）");
                fprintf(stderr, "[%s] %2d/%2d 层 ｜ 状态 %zu%s ｜ 本层 %.1fs ｜ 累计 %.1fs ｜ 剩余 %s\n",
                        bar, d, C.maxDepth, layer.size(),
                        layer.size() >= (size_t)C.perLayer ? "（触顶）" : "",
                        layerT, totalT, etaBuf);
            }
        }
        if (layer.empty()) break;
    }
    R.secs = chrono::duration<double>(chrono::steady_clock::now() - t0).count();
    fprintf(stderr, "\n耗时 %.1fs\n", R.secs);
    if (!found) return R;                 // found=false：main 负责打印 NO_SOLUTION
    R.found = true;
    R.steps = foundDepth;
    for (size_t i = 0; i < bestGoals.size(); i++) R.front.push_back(bestGoals[i].st);

    // 回溯"溢出最大"那条解（front[0]，已按 (溢出, CP) 降序）得到动作序列
    {
        const Goal& g = bestGoals[0];
        vector<int> seq;
        seq.push_back(g.act);
        int cur = g.par;
        for (int lv = foundDepth - 1; lv >= 1; lv--) {
            seq.push_back(actHist[lv - 1][cur]);
            cur = parHist[lv - 1][cur];
        }
        reverse(seq.begin(), seq.end());
        R.bestSeq = seq;
    }
    return R;
}
