cmake_minimum_required(VERSION 3.20)
project(ffxiv-crafting-weaver LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT MSVC)
  add_compile_options(-O2 -Wall -Wextra)
endif()

# 核心库：可被 CLI 链接，也可被别的程序当库用（这就是保留 include/ 的理由）
add_library(craftcore STATIC
  src/csv.cpp        # 极简 CSV 读取
  src/tables.cpp     # rlv 表 / 等级压制表 / HQ 表（读项目自带 data/）
  src/engine.cpp     # 技能表装载 + 每手结算（公式 / 内静 / buff 时序 / 完成判定）
  src/hqreport.cpp   # 品质条溢出 → HQ 效益评估
  src/solver.cpp     # 搜索：BFS ＋ 4 维 Pareto 支配剪枝
)
target_include_directories(craftcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# CLI（搜索已拆到 solver.cpp，与 main.cpp 分离）
add_executable(craftweave src/main.cpp)
target_link_libraries(craftweave PRIVATE craftcore)

# 测试：不引第三方框架，失败即非零退出
enable_testing()
add_executable(test_core tests/test_main.cpp)
target_link_libraries(test_core PRIVATE craftcore)
add_test(NAME core COMMAND test_core)
set_tests_properties(core PROPERTIES ENVIRONMENT "WEAVER_DATA=${CMAKE_CURRENT_SOURCE_DIR}/data")
