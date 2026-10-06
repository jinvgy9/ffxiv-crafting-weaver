#pragma once
// 搜索：BFS ＋ 4 维 Pareto 支配剪枝，求"最少步数把品质与进度都拉满"的工序
//
//     · 本模块只负责计算，进度与剩余时间估计写到 stderr（C.quiet可关）
//     · stdout 的结构化结果由 main 负责打印，HQ 效益也由 main 调 reportHQ
//
//   每层的C.perLayer裁剪是启发式，不是有保证的剪枝
//   调太小会丢解。报"最少 N 步"时会连同 depth 与 perLayer 一起报（有效反馈）
#include "craft/model.hpp"
#include <vector>

struct SolveResult {
    bool found = false;
    int steps = 0;                  // 找到解时的手数（＝最少步数）
    std::vector<St> front;          // 同一步数层内的非支配解，按 (溢出, CP) 降序
    std::vector<int> bestSeq;       // front[0] 的动作下标序列（可直接喂 AKEY / reportHQ）
    double secs = 0.0;              // 搜索耗时
};

// allowed：允许使用的动作下标（已由等级与 --ban 过滤好）
SolveResult solve(const std::vector<int>& allowed);
