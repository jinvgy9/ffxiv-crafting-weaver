# ffxiv-crafting-weaver

这是一个**Final Fantasy XIV 配方求解器** —— 给定配方信息与生产三维属性，得出「尽可能最少步数把品质拉满并完成制作」的工序，并给出该步数层的容错前沿（品质溢出/CP余量）与HQ效益评估

> ⚠️ **纯玩家作品**：与 Square Enix 无关联、未获其背书。仓库内**不含任何游戏素材**（图像/音乐/模型/logo），也**不含任何与游戏交互的代码**，这是一个离线计算器
> 
> 数据来源、非商业声明与移除承诺见 [`NOTICE.md`](NOTICE.md)

> ⚠️ 部分内容由**Deepseek v4.1 Flash**协助制作（感谢蓝色大肥鱼x）

## 它解决什么问题

FF14 的生产制作本质即有资源约束的最短路径问题：耐久有限、CP 有限，而进度推满是硬约束（某种程度上，品质拉满也是）。玩家借助生产模拟器手工排工序会花费不少时间，且很难摸清两点：

1. **最少要几步？**（还有没有更短的）
2. **工序有多稳？**（被黑球/延迟爆破后还有没有容错，能不能救）

本项目用 **BFS + 4维Pareto支配剪枝** 回答第1点（并给出同一步数层内所有不被支配的容错取舍），再借**HQ概率表**把"品质条溢出"换算成较为直观的抗干扰收益来回答第2点

> ⚠️ **注意：** 不给探索可能性就探索不出可能性，没带上足够探索量级的"最少步数"结论是不全面的，探索量级设定过大会消耗更多性能、影响效率，可自行权衡调整

## 如何构建

*Windows（MinGW + 自带工具链的一键脚本）*

```
powershell -ExecutionPolicy Bypass -File build.ps1 -Clean -Test
```

`build.ps1` 只把工具链目录加进本次进程的PATH，**不改系统环境**

*通用（任何平台）*

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

产物：CMake构建在 `build/craftweave`（Windows为`craftweave.exe`）；`build.ps1`默认的g++直编则直接放在项目根目录

> **Windows 上注意**：`build.ps1` 默认走 **g++ 直编**（不需要 CMake）；加 `-CMake` 才走 CMake+Ninja。
> 另外，一般**默认禁止运行 `.ps1`**，所以要带 `-ExecutionPolicy Bypass`（仅单次生效，不改全局）

## 测试

#### ① 引擎与搜索断言（66 项：机制 + 数据层 + 搜索）

```
powershell -ExecutionPolicy Bypass -File build.ps1 -Test
```

#### ② 搜索回归（cases.csv 里的已知题，逐题比对 STEPS）

```
powershell -ExecutionPolicy Bypass -File tests\run-cases.ps1
powershell -ExecutionPolicy Bypass -File tests\run-cases.ps1 -Only main-rlv770    # 只跑一题
```

走 CMake 构建时，`ctest --test-dir build` 同①

## 使用方法

**并没有图形界面**（暂时...私密马赛x），请在终端里跑（CMD / PowerShell / Windows Terminal 都行）。

记得先`cd`一下到放这个项目的位置：

```
cd <项目目录>
```

**例（参数表见后文）**：

```
craftweave.exe --rlv=770 --cm=5878 --ctrl=5623 --cp=749 --q0=50 --depth=20 --perlayer=2000000
```

→**想把输出留档**（结构化结果给 stdout、进度条与 HQ 效益给 stderr）：

```
craftweave.exe --rlv=770 --cm=5878 --ctrl=5623 --cp=749 --q0=50 > out.txt 2>&1
```

→**`--help`查询**：

```
craftweave.exe --help
```

## 参数表

`--rlv` 是核心参数：难度、品质、耐久上限与修正系数、压制系数等都由它查`data/`自动带出

| 参数 | 含义 |
|---|---|
| `--rlv` | 配方品级，**并不完全等同于物品品级**，可在wiki、Garland等处获取 |
| `--cm` | 作业精度 |
| `--ctrl` |加工精度 |
| `--cp` |制作力 |
| `--lv` | 职业等级（默认100） |
| `--prog` | 配方难度 |
| `--qual` | 配方上限品质 |
| `--dur` | 配方耐久 |
| `--qabs` | 品质条初始值（绝对值） |
| `--q0` | 品质条初始值（百分比，默认0%） |
| `--mode` | `fixed`=固定模式（默认）：不考虑概率技能；`expected`=概率&期望模式：按成功率折算 |
| `--depth` | 最大探索步数（默认22步），**给不够会直接报无解** |
| `--perlayer` | **每层探索量级**（默认150000） |
| `--ban` | 禁用某技能，多参数以逗号分隔（例 `--ban=rapidSynthesis,hastyTouch,daringTouch`） |
| `--seq` | 跳过搜索，直接评估一条序列（支持 `key*N`），并输出它的 HQ 效益 |
| `--datadir` | 数据目录（默认：可执行文件同级的 `data/`） |
| `--quiet` | 不打印进度行（对拍用），**进度只走 stderr**，不影响 stdout 的结果 |
| `--strict` | 强制要求未知参数时直接报错退出，默认只会警告并继续执行任务（便于发现误拼） |

技能参数见`data/actions.csv`第一列（`basicTouch` 加工、`preparatoryTouch` 坯料加工、`groundwork` 坯料制作、`trainedPerfection` 工匠的绝技…）。

## 实用例

#### 求最少步数

配方品级770，作业/加工/制作=5878/5623/749，初始品质50%
规定最大探索步数20步，每层探索量级为2000000

```
craftweave.exe --rlv=770 --cm=5878 --ctrl=5623 --cp=749 --q0=50 --depth=20 --perlayer=2000000 ^
--ban=rapidSynthesis,hastyTouch,daringTouch
```

#### 验证一条工序 + 看 HQ 效益

配方品级770，作业/加工/制作=5878/5623/749，初始品质10600，验证该工序：
坚信→坯料加工→精修→长期俭约→崇敬→坯料制作×4→精密制作→坯料加工×2→巧夺天工→改革→坯料加工×2→阔步→比尔格的祝福→坯料制作

```
craftweave.exe --seq=muscleMemory,preparatoryTouch,masterMend,wasteNotII,veneration,groundwork*4,
delicateSynthesis,preparatoryTouch*2,immaculateMend,innovation,preparatoryTouch*2,greatStrides,
byregot,groundwork --rlv=770 --cm=5878 --ctrl=5623 --cp=749 --qabs=10600
```

## 输出怎么读

**stdout格式（结构化，便于对拍）**

```
STEPS <最少手数>
FRONT <前沿点数（同一步数层内不被支配的解有几个）>
F <溢出> <CP余> <耐久余> <进度> <未封顶品质累计>     ← 每个前沿附一行
FINAL <进度值> <品质值> <未封顶累计> <耐久余> <CP余>
SEQ <动作序列>
```

**stderr（可读性强）**：进度条、逐层状态数、Pareto 前沿、以及 **HQ 效益评估**（炸黑球的损失，损失最小/最大两情形，品质条贡献与溢出贡献）

**进度条**（每层一条进度）：

```
[########------------]  8/20 层 ｜ 状态 200000（触顶） ｜ 本层 1.1s ｜ 累计 3.6s ｜ 剩余 ~12 秒
```

- 剩余时间是预估值，前几层耗时太短，会直接显示「粗估中…」
- 作用是「让你决定要不要继续等」，也可以用于参考探索量级的设置（时间太长了就调低点）

## 注意事项

1. `--perlayer` 探索量级是启发式裁剪，不是有保证的剪枝，**调小会丢解**
2. 无解时可以调大探索量级看看有没有新解
3. 本程序输出 UTF-8，程序侧已规避乱码问题，但若仍有出现，可试输入`chcp 65001`解决

## 数据来源

1. 制造采集相关机制数据分享/探讨：ngabbs.com/read.php?tid=31075581&rand=170
2. FF14灰机wiki：https://ff14.huijiwiki.com/wiki/
3. Garland数据库：www.garlandtools.org/db/#
4. FF14 API站：xivapi-v2.xivcdn.com

感谢上述来源的信息及信息提供者对本项目的理论技术指导和数据支持！

## 版本信息

当前版本：ffxiv-crafting-weaver v0.1 - 20261006202849

对应游戏版本：**FFXIV Patch 7.56**