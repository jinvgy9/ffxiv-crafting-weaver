#pragma once
// 机制引擎：技能表构建、每手结算（公式 / 内静 / buff 时序 / 完成判定）、状态键
#include "craft/model.hpp"
#include <filesystem>
#include <string>
#include <vector>

extern Cfg C;                              // 全局配方配置（main 填好后供各模块读）
extern std::vector<Act> A;                 // 技能表
extern std::vector<std::string> AKEY;      // 技能表对应的 key（与 CSV/CLI 一致）
extern bool EXPECTED;                      // false=固定模式；true=概率&期望模式
extern int ACT_OBSERVE, ACT_STANDARD;      // 常用动作索引（连击判定用）

int findActByKey(const char* key);                        // 按 key 找（推荐）
bool loadActions(const std::filesystem::path& csvPath);   // 从 data/actions.csv 建技能表
St newState();
int cpCostOf(const St& s, int key);
bool canUse(const St& s, int key, bool done, bool failed);
bool applyAct(const St& s0, int key, St& s, double& gainProg, double& gainQual, bool& done, bool& failed);
uint64_t keyOf(const St& s);
