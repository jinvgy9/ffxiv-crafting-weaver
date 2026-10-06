#pragma once
// 极简 CSV 读取
// 支持带引号字段、字段内逗号、"" 转义；自动跳过表头行
#include <filesystem>
#include <string>
#include <vector>

bool readCsv(const std::filesystem::path& p, std::vector<std::vector<std::string> >& rows);
int icol(const std::vector<std::string>& c, size_t i);
double dcol(const std::vector<std::string>& c, size_t i);
std::string scol(const std::vector<std::string>& c, size_t i);
