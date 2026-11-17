# 兼容性说明：FL Studio 与固定大小缓冲区

- 适用版本：Iridescent Flower 1.1.0（Windows x64 / VST3）
- 更新日期：2026-11-17
- 状态：已知宿主兼容性问题；FL Studio 用户请按本文的推荐设置使用

## 现象

在部分 FL Studio 环境中，插件加载后即使机架为空，只要任意模块开始处理（`MIX` 大于 0），就可能出现持续的高频噪声或爆音。典型表现：

- Bypass 插件后正常
- 清空机架后正常
- Filter、EQ、Delay、Cloud、Grain 等模块越启用，噪声越明显
- Filter/EQ 提升或 Cloud/Delay 的 feedback、decay 增加时更明显
- 在 Ableton Live 等其他宿主中没有同样问题

这不是参数错误，也不是音频文件损坏。插件的干声路径本身是干净的；问题出现在宿主以可变大小 block 调用插件时，模块 wet 路径对块边界和缓冲区状态的敏感性。

## 原因

VST3 允许宿主用可变大小的 block 调用 `processBlock`。FL Studio 默认会以可变 block 向插件提交音频；其他宿主（例如 Ableton Live）通常使用固定 block。

Iridescent Flower 包含多条并行 Bus、32 个独立槽位、Morph 交叉以及多个带反馈的模块。在这套结构下，宿主 buffer 的边界、未使用通道和调用方式会进入 wet 路径，并被 Filter/EQ 的提升及 Delay/Cloud 的反馈放大。

在 FL Studio 中启用 **Use fixed size buffers** 后问题消失，说明触发点是 FL 的 wrapper/buffer 行为，而不是某一个滤波器的系数错误。

插件内部曾尝试固定块适配、输入清理和调用串行化，但这些只能改变 DSP 看到的块大小，无法完全复制 FL wrapper 在 fixed-buffer 模式下的缓冲区与线程行为。VST3 也没有标准接口可以让插件强制宿主启用固定大小 buffer。

## 推荐设置

在 FL Studio 中，对 `Iridescent Flower` 执行一次以下设置：

1. 打开插件窗口。
2. 点击左上角的齿轮/扳手图标（插件设置，不是 FL 主设置）。
3. 打开 **Troubleshooting**（部分版本为 **Processing**）标签页。
4. 勾选 **Use fixed size buffers**。
5. 出现警告时点击 **Yes**。
6. 关闭并重新打开插件，或重新开始播放。

如果仍有问题，可在同一页点击 **More**，尝试：

- **Process maximum size buffers**
- **Use maximum buffer size from host**

这两个选项可能增加延迟和 CPU 占用；通常先只开启 `Use fixed size buffers` 即可。

## 影响

- 这是 FL Studio 的插件级 wrapper 设置，不会影响其他插件。
- 可能略微增加延迟和 CPU 占用；FL 的延迟补偿（PDC）会自动处理。
- 设置只需对当前插件执行一次。
- 插件仍可在 Ableton Live、Reaper 等其他 VST3 宿主中使用，不需要此设置。

## 其他插件也有类似要求

这不是 Iridescent Flower 独有的问题。以下插件/厂商在 FL Studio 中也被官方或社区建议启用固定大小 buffer：

- [Output Arcade：FL Studio 中建议 Use fixed size buffers](https://support.output.com/en/articles/10297614-loading-arcade-in-fl-studio)
- [Acustica Audio：FL Studio buffering issues](https://acusticaudio.freshdesk.com/support/solutions/articles/35000321927-how-to-fix-buffering-issues-in-fl-studio)
- [FL Studio wiki：部分插件需要 Use fixed buffer size](https://w.atwiki.jp/flstudio2/?cmd=backup&action=show&pageid=25&id=1070#2)
- [Vital 论坛：FL 中 Fixed Size Buffers 解决爆音](https://forum.vital.audio/t/clicks-and-pops/7251/13)

因此，该设置是 FL Studio 的通用兼容性选项，不代表插件本身损坏或工程数据有问题。

## 报告问题时请提供

如果按上述设置后仍然异常，请提供：

- FL Studio 版本
- Windows 版本
- 工程采样率与 buffer size
- 插件版本
- 出问题的模块组合
- 是否只在 FL Studio 出现，以及是否在 Bypass/空机架时仍然存在

这些信息有助于判断是宿主 wrapper 行为、特定模块组合，还是其他环境问题。
