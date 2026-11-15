# Iridescent Flower — 功能概览视频文案

> 定位：功能介绍，不是教程，不做营销。
> 建议时长：4:30–5:10。
> 语气：技术、直接、克制；只讲插件实际能做什么。

## 标题备选

1. 开源 VST3 多总线 FX 机架：Iridescent Flower 功能概览
2. Iridescent Flower：机架、粒子、Morph 与自定义模块
3. 不聊营销，只看功能：Iridescent Flower

## 视频简介

Iridescent Flower 是一个开源的 Windows x64 VST3 多总线效果器，采用 AGPL-3.0 许可。

这期视频按功能演示它的五个重点：

- 三条并行 Bus + Master 的 FX 机架
- Grain 粒子处理
- Morph 交叉与 LFO
- 自定义 DLL 模块
- 开源项目与构建方式

视频只做功能概览，不评价插件适合谁，也不做购买引导。参数和声音仅用于演示，实际使用以当前版本为准。

## 口播文案

### 00:00–00:30 开场

这期视频介绍 Iridescent Flower，一个开源的 Windows VST3 效果器。

它不是单一效果，而是一个多总线 FX 机架：三条并行 Bus、一条 Master 链，再加一个固定的 Morph 输出级。

这期只做功能概览。我会按机架、粒子、Morph、自定义模块和开源这几个部分来讲，不堆卖点，也不做购买引导。

源码、构建说明和安装包都在 GitHub，许可证是 AGPL-3.0。

**画面**

- 封面标题
- 主界面全景
- 快速切过 BUS 1 / BUS 2 / BUS 3 / MASTER / MORPH
- 最后停在 GitHub 仓库页

### 00:30–01:35 FX 机架

先看机架结构。

三条并行 Bus，每条有 8 个效果槽；后面还有一条 Master 链，同样是 8 个槽。加起来是 32 个可编辑槽位。

每个槽可以独立选择模块，参数也独立保存。同一个模块在同一个 Bus 里放两次，参数不会联动；Bus 1 的 Compressor 和 Master 的 Compressor 也是两套独立状态。

槽位可以拖动换序。删除一个模块后，后面的模块会立即左移，空位留在链尾。

新建插件实例默认加载工厂预设 `PAD_EasyStrings`。顶部 `PRESET` 菜单里还有另外四个工厂预设，也可以保存和读取自己的 `.iflower` 文件。

界面只保留 EQ 和 Filter 的频谱图；其他模块显示头像面板，头像可以替换。

**画面**

- 在 Bus 1 装载 Compressor、EQ、Filter
- 在 Bus 2 装载 Grain、Echo
- 在 Master 装载 Cloud
- 拖动两个模块换序
- 删除一个模块，展示左移
- 打开 `PRESET → FACTORY PRESETS`
- 快速切过 EQ / Filter 的频谱和头像面板

### 01:35–02:35 粒子：Grain

接下来是 Grain 粒子模块。

`SIZE` 控制粒子长度，`DENSITY` 控制基础触发密度，`RATE` 控制粒子触发速率。`PITCH` 是独立的移调参数，改变 Rate 不会改变音高。

`SPRAY` 打散粒子的读取位置，`FEEDBACK` 加入回授，`REVERSE` 控制反向播放，`WIDTH` 调整立体声宽度，`MIX` 控制干湿比例。

打开 `SYNC` 后，`DENSITY` 会切换成节奏分割，跟随宿主 BPM。`SNAP` 可以把读取位置对齐到网格。

演示用两段素材：一段鼓循环，一段持续 Pad。鼓循环上主要听 Spray 和 Rate 的变化；Pad 上主要听 Size、Pitch 和 Feedback。

**画面**

- 打开 Grain 模块
- 依次调整 Size、Density、Rate、Pitch、Spray、Feedback
- 切换 Sync，展示 Division
- 鼓循环：干声 → 粒子化 → 加 Feedback
- Pad：长粒子 + Pitch + Reverse
- 频谱/波形不需要额外动画，保留实际声音

### 02:35–03:35 Morph

Morph 是固定输出级，不属于可替换槽。

它把 Bus 1 和 Bus 2 交叉，也可以把 B 侧换成 Sidechain 或内部信号。`MORPH` 控制 A/B 比例，`B TRIM` 调整 B 侧电平，`CURVE` 调整交叉曲线，`B WIDTH` 调整立体声宽度。

`B SOURCE` 有三个选项：`SIDECHAIN`、`BUS 2`、`INTERNAL`。

LFO 只调制 Morph 量，不单独占一个效果槽。`LFO RATE`、`LFO DEPTH`、`LFO PHASE`、`LFO SHAPE` 都可以调，Depth 默认是 0。

这里要注意：`BUS 2 OUT` 是 Bus 2 的直接并行发送，和 Morph 的 B 输入是两条路径。Bus 2 直接输出为 0% 时，Morph 仍然可以取到 Bus 2 链的信号。

演示用一个 Sidechain Pad：Bus 1 放干净的 Pad，Bus 2 放粒子化的 Pad，用 Sidechain 信号推动 Morph，再用很慢的 LFO 加一点持续运动。

**画面**

- 打开 Morph 页面
- 调整 Morph、B Trim、Curve、B Width
- 切换 B Source：Sidechain / Bus 2 / Internal
- 展示 LFO Rate / Depth / Phase / Shape
- 慢速 LFO 演示
- Bus 2 OUT 归零，Morph 仍然工作

### 03:35–04:35 自定义模块

最后一个重点：自定义模块。

把 Windows x64 DLL 放到：

```text
Documents\Iridescent Flower\Custom Modules
```

插件启动时扫描这个目录，最多加载 8 个 DLL。右键空槽选择 `LOAD FX`，就可以把 DLL 当作模块装载；已经有模块的槽位显示 `REPLACE FX`。

每个自定义模块提供 8 个参数，参数值以 0 到 1 的归一化值传入 DLL。

ABI 只有四个导出函数：

```cpp
if_get_module_info()
if_create(sampleRate, maxBlockSize)
if_destroy(instance)
if_process(instance, left, right, numSamples, params[8], 8, bpm)
```

接口定义在 `Source/CustomModuleApi.h`。仓库里有 `CustomModules/Bbd/` 示例，CMake 构建目标是 `IridescentBbdCustom`，产物就是 `BbdDelay.dll`。

自定义模块不需要重新编译主插件。

**画面**

- 打开 `Documents\Iridescent Flower\Custom Modules`
- 展示 `BbdDelay.dll`
- 在空槽选择 `LOAD FX → BBD DELAY`
- 调整 8 个参数
- 打开 `Source/CustomModuleApi.h`
- 打开 `CustomModules/Bbd/` 和 CMake 目标
- 用鼓或合成器演示 BBD

### 04:35–05:10 开源与结尾

Iridescent Flower 以 AGPL-3.0 发布，使用 JUCE 8 和 VST3，当前提供 Windows x64 版本。

插件包含 147 个参数、32 个槽位、五个工厂预设，以及刚才演示的 Grain、Morph 和自定义模块。

源码、安装包、构建说明和自定义模块文档都在 GitHub。视频里不展开安装步骤，README 里有完整的构建和安装说明。

这期没有营销结论。它就是一个开源的多总线 FX 机架，功能范围如上；需要看实现细节可以直接读源码。

**画面**

- GitHub 仓库首页
- README 的功能列表
- LICENSE
- Release 页面
- 定格封面

## 分镜与录制清单

| 时间 | 内容 | 建议画面 |
| --- | --- | --- |
| 00:00 | 开场 | 封面、主界面、五个页面快切 |
| 00:30 | 机架 | Bus 1/2/Master 装载模块，拖动、删除 |
| 01:35 | Grain | 鼓循环和 Pad 两段 A/B |
| 02:35 | Morph | Sidechain Pad，Morph + 慢 LFO |
| 03:35 | 自定义模块 | 文件夹、LOAD FX、ABI、BBD |
| 04:35 | 开源 | GitHub、README、LICENSE、Release |

录制建议：

- 1920×1080，60 fps；插件界面按整数倍放大，避免模糊。
- 音频单独录音，不要用屏幕录制自带的麦克风。
- 每次参数变化前保留 1–2 秒干声，方便后期做 A/B。
- 字幕只标参数名和当前值，不要持续堆大字。
- 不使用版权音乐；演示素材用自己录的鼓、合成器或 Pad。

## 60 秒短版

### 0–5 秒

画面：封面 + 主界面

口播：

> 这是一个开源的 Windows VST3 多总线 FX 机架，Iridescent Flower。

### 5–18 秒

画面：Bus 1/2/3 + Master，装载、拖动、删除

口播：

> 三条并行 Bus，每条 8 个槽，再加 8 个 Master 槽。每个槽的模块和参数都是独立的。

### 18–32 秒

画面：Grain

口播：

> Grain 粒子模块：Size、Density、Rate、Pitch、Spray、Feedback。Rate 和 Pitch 分开，Sync 跟随宿主 BPM。

### 32–45 秒

画面：Morph

口播：

> Morph 把 Bus 1、Bus 2 或 Sidechain 交叉。Curve、B Trim、B Width 可调，LFO 只调制 Morph。

### 45–55 秒

画面：自定义模块 + BBD

口播：

> 自定义 DLL 模块放进 Documents 文件夹，最多 8 个；每个模块 8 个参数，不需要重新编译主插件。

### 55–60 秒

画面：GitHub / LICENSE

口播：

> AGPL-3.0 开源，源码和构建说明都在 GitHub。

## 上传用简介模板

```text
Iridescent Flower 是一个开源的 Windows x64 VST3 多总线效果器，AGPL-3.0。

这期是功能概览，不是教程，也不做购买引导。

00:00 介绍
00:30 FX 机架：3 × 8 Bus + 8 Master
01:35 Grain 粒子模块
02:35 Morph 与 LFO
03:35 自定义 DLL 模块
04:35 开源、构建与结尾

项目与源码：
https://github.com/iwillenterausername/iridescent-flower

Release：
https://github.com/iwillenterausername/iridescent-flower/releases/tag/v1.1.0

版本：1.1.0
平台：Windows x64 / VST3
参数：147
工厂预设：PAD_EasyStrings、PLUCK_EasyGrains、BASS_ALittleBitWide、KEY_SpaceBits、DRUM_Stereo

License: AGPL-3.0
```
