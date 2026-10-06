#pragma once
// 数据表：rlv 表 / 等级压制表 / HQ 概率表 —— 读取与查询
#include <filesystem>
#include <unordered_map>

// rlv 表一行（正表的系数 + 上限节的难度/品质/耐久 合并后的样子）
struct RlvRow {
    int cjl = 0, diff = 0, qual = 0, dur = 0, mask = 0, stars = 0;
    double pd = 0, qd = 0, pm = 0, qm = 0;
};

extern std::unordered_map<int, RlvRow> RTAB;   // rlv → 行
extern std::unordered_map<int, int> EQUIV;     // 工匠等级 → 等效品级（等级压制用）
extern bool RTABok;
extern int HQ[100];                            // 品质% → HQ%
extern bool HQok;

// 品质百分比 → HQ 概率（0 → 1%，≥100 → 100%）
int hqProb(double pct);

// 正式接口：从项目自带数据目录读全部表
// （recipe_level_table / recipe_caps / level_suppression / hq_table）
// 缺任何一个文件都返回 false 并打印缺失的文件名
bool loadTables(const std::filesystem::path& dataDir);
