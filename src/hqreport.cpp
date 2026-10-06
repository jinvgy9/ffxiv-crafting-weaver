#include "craft/hqreport.hpp"
#include "craft/engine.hpp"
#include "craft/tables.hpp"
#include <cstdio>
#include <cmath>
#include <algorithm>
using namespace std;

void reportHQ(const vector<int>& seq) {
    fprintf(stderr, "\n=== HQ 效益（以品质条溢出为根据）===\n");
    if (!HQok) { fprintf(stderr, "（未读到 HQ 概率表；用 --hqtable=<path> 指定）\n"); return; }
    St cur2 = newState();
    vector<pair<int, double> > gains; // (手序, 品质增益)
    int stp = 0;
    for (size_t i = 0; i < seq.size(); i++) {
        St nx; double gp, gq; bool dn, fl;
        applyAct(cur2, seq[i], nx, gp, gq, dn, fl);
        cur2 = nx; stp++;
        if (gq > 1e-9) gains.push_back(make_pair(stp, gq));
        if (dn || fl) break;
    }
    const double qmax = C.qualMax;
    const double of = cur2.qualRaw - qmax;
    fprintf(stderr, "品质上限 %.0f ｜ 未封顶累计 %.2f ｜ **溢出 %.2f** ｜ 品质手 %zu 个\n", qmax, cur2.qualRaw, of, gains.size());
    vector<pair<int, double> > bad;
    fprintf(stderr, "各品质手若吃黑球（×0.5）：\n");
    for (size_t i = 0; i < gains.size(); i++) {
        int st = gains[i].first; double gq = gains[i].second;
        double loss = gq - floor(gq * 0.5);
        double bar = min(cur2.qualRaw - loss, qmax);
        double barNo = min(qmax - loss, qmax);
        int p1 = hqProb(bar / qmax * 100.0), p2 = hqProb(barNo / qmax * 100.0);
        bool notFull = (bar < qmax - 1e-9);
        if (notFull) bad.push_back(make_pair(st, loss));
        fprintf(stderr, "  第 %2d 手 %-12s 品质 %8.2f → 损失 %8.2f ｜ 品质条 %6.2f%% → HQ %3d%%（无溢出时 %3d%%）%s\n",
                st, A[seq[st - 1]].cn, gq, loss, bar / qmax * 100.0, p1, p2, notFull ? "" : "  [仍满品质]");
    }
    if (bad.empty()) {
        fprintf(stderr, "=> 溢出 %.2f 足以吸收任意单手的黑球损失：**没有任何一手吃黑球会掉出满品质（HQ 恒 100%%）**\n", of);
        return;
    }
    pair<int, double> mn = bad[0], mx = bad[0];
    for (size_t i = 1; i < bad.size(); i++) {
        if (bad[i].second < mn.second) mn = bad[i];
        if (bad[i].second > mx.second) mx = bad[i];
    }
    int m1 = mn.first, m2 = mx.first; double l1 = mn.second, l2 = mx.second;
    double b1 = min(cur2.qualRaw - l1, qmax), n1 = min(qmax - l1, qmax);
    double b2 = min(cur2.qualRaw - l2, qmax), n2 = min(qmax - l2, qmax);
    fprintf(stderr, "【损失最小且品质不满】第 %d 手「%s」损失 %.2f => 有溢出 品质条 %.2f%% → **HQ %d%%**；无溢出 品质条 %.2f%% → HQ %d%%（**品质条贡献 %+.2f 点 ｜ 溢出贡献 %+.2f 点**）\n",
            m1, A[seq[m1 - 1]].cn, l1, b1 / qmax * 100.0, hqProb(b1 / qmax * 100.0),
            n1 / qmax * 100.0, hqProb(n1 / qmax * 100.0),
            (b1 - n1) / qmax * 100.0, (double)(hqProb(b1 / qmax * 100.0) - hqProb(n1 / qmax * 100.0)));
    fprintf(stderr, "【损失最大　　　　　】第 %d 手「%s」损失 %.2f => 有溢出 品质条 %.2f%% → **HQ %d%%**；无溢出 品质条 %.2f%% → HQ %d%%（**品质条贡献 %+.2f 点 ｜ 溢出贡献 %+.2f 点**）\n",
            m2, A[seq[m2 - 1]].cn, l2, b2 / qmax * 100.0, hqProb(b2 / qmax * 100.0),
            n2 / qmax * 100.0, hqProb(n2 / qmax * 100.0),
            (b2 - n2) / qmax * 100.0, (double)(hqProb(b2 / qmax * 100.0) - hqProb(n2 / qmax * 100.0)));
}

