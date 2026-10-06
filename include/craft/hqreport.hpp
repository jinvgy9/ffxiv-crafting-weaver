#pragma once
// 品质条溢出 → HQ 效益评估
#include <vector>

// 对一条完整序列做评估：逐手炸黑球损失、只统计"未满"的步骤、取损失最小/最大两情形
// 输出走 stderr（易读），不污染 stdout 的结构化结果
void reportHQ(const std::vector<int>& seq);
