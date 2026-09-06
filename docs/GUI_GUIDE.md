# AudioVisGUI 使用说明

> 本文档针对 **AudioVisGUI**（实时预览 + 一键导出）编写。
> 命令行工具 AudioVisExport CLI 的使用方式见 `ARCHITECTURE.md` §5 / `--help`。

---

## 1. 构建与启动

### 1.1 前置依赖（WSL / Linux，2026-09-05 起）
- **工具链与 JUCE 依赖**：`cmake ninja-build pkg-config` + JUCE Linux 库
  （libasound2-dev、libx11-dev、libxext-dev、libxrandr-dev、libxinerama-dev、
  libxcursor-dev、libxcomposite-dev、libxrender-dev、libfreetype6-dev、
  libfontconfig1-dev、libgl1-mesa-dev），一键安装命令见 `ARCHITECTURE.md` §5。
- **ffmpeg**：视频导出（WebM / MOV）需要调用系统 ffmpeg（WSL 下 `sudo apt-get install ffmpeg`）。
  - 可用 `which ffmpeg` 验证；找不到时导出 WebM/MOV 会失败并提示（也可设 `FFMPEG_PATH` 指向可执行文件）。
- **显示 / 音频**：WSLg 自动提供（GUI 直接弹窗、音频走 PulseAudio），无需额外配置。
- **JUCE**：`third_party/JUCE` 符号链接复用 Y2Kmeter 的本地 8.0.12 checkout，构建时加 `-DAVX_USE_LOCAL_JUCE=ON`。
- （历史）Windows x64 + Visual Studio 时期的 `build/` 已删除；Windows 构建流程见 git 历史版本本文档。

### 1.2 构建（Release）
```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DAVX_USE_LOCAL_JUCE=ON
cmake --build build -j --target AudioVisGUI
```

产物位置：
```
build/AudioVisGUI_artefacts/Release/AudioVisGUI
```
终端运行 `./build/AudioVisGUI_artefacts/Release/AudioVisGUI`（WSLg 下直接弹出原生窗口）。

---

## 2. 界面布局

```
┌─────────────────────────────────────────────────────┐
│  SpectrumCanvas（频谱预览画布，透明区域显示棋盘格）      │
│     · 可自由拖动 / 缩放 / 拉伸 / 旋转频谱元素           │
│     · 支持拖放文件进窗口（音频加载 / 其他类型弹提示）    │
│     · 空白处点击 → 文件选择器                         │
├──────────────┬──────────────────────────────────────┤
│ 参数面板      │  传输条： [Play] [========seek====] 时间 │
│ (右侧滚动)    └──────────────────────────────────────┘
│  Style / 时间 / 动态 / 外观 / 导出 共 5 组             │
└─────────────────────────────────────────────────────┘
```

右侧参数面板自顶向下 6 组：

| 分组 | 控件 |
|---|---|
| **Style** | Render style（y2k-line / bar / **bar-line** / polyline / crystal）、Band count、Bar gap %、Bar width %（bar / bar-line 生效）、**Reset element transform**、Freq scale、Min/Max Hz |
| **Layers** | **[Add image]**、[Up] / [Down]（图层顺序）、[Remove]、Opacity（选中图层） |
| **Time** | FPS、Attack ms、Release ms、Peak hold ms、Peak decay dB/s、Temporal smooth |
| **Dynamics** | Y-axis curve、Gain、Gamma、Slope comp、Slope dB/oct |
| **Appearance** | Line width、Opacity、Draw grid、Axis labels、Checkerboard BG（仅预览）、**Peak caps**（柱样式峰值帽开关）、**三色 Primary/Secondary/Peak（⚠️ 在本组，面板较长需要滚动才能看到）** |
| **Export** | W × H、Encoder 下拉、`[Browse]`、`[Export]`、**`[Export Video]`**、输出目录文本、进度文本 |

面板底部 Export 区实际样子：

```
┌─────────────────────────────┐
│ [Browse]        [Export]     │
│ [ Export Video          ]    │  ← 玫红色醒目按钮
│ <输出目录路径>              │
│ Done: video -> xxx.webm    │  ← 进度 / 结果
└─────────────────────────────┘
```

---

## 3. 快速上手（6 步）

1. **加载音频**：把 `WAV / AIFF` 文件拖进**窗口任意位置**（画布 / 参数面板 / 按钮上方均可），
   或点击画布空白处选择。
   > 拖入非音频文件（图片 / MP3 等）会弹提示"Only WAV / AIFF audio can be used"，不会崩溃。
   > 多声道会自动转成立体声；暂不支持 MP3/FLAC/OGG。

2. **播放检查**：点 `[Play]` 播放，频谱跟随音乐实时跳动；拖动下方进度条可随意 seek。

3. **调参**：右面板任意滑块 / 下拉 / 开关 / 颜色都在 **1/30 秒内**作用于预览画面——所见即所得。
   - 想看清透明区域，打开 `Checkerboard BG`（只在预览显示，不影响导出）。
   - 分辨率在 `W × H` 输入框改（≥64）。

4. **摆放频谱元素（自由变换）**：频谱是一个"图层元素"，可在预览画布内任意
   拖动 / 缩放 / 拉伸 / 旋转。导出视频/PNG 与预览**完全一致**（所见即所得）。

   | 操作 | 效果 |
   |---|---|
   | 拖动频谱主体 | 移动到任意位置 |
   | 拖四角白色小手柄 | 等比缩放 |
   | 拖四边手柄 | 单轴拉伸（压扁 / 拉长比例） |
   | 拖顶边上方青色圆柄 | 绕元素中心自由旋转 |
   | 双击频谱元素 | 复位变换（铺满画布） |

   > 变换坐标是**输出分辨率像素**；改变 `W × H` 后元素保持原绝对位置，双击复位即可回正。
   > 手柄只在加载音频后显示；操作提示在画布底部。

5. **选导出方式**（二选一）：
   - **一键视频**：直接点 `[Export Video]` → 选择输出目录 → 后台自动导出
     **带 alpha 透明通道的 MOV（QTRLE）**，文件名自动生成为 `<音频名>_vis.mov`。
     MOV/QTRLE 是 Premiere / Vegas / DaVinci Resolve 最通用的透明视频格式。
   - **手动控制**：先在 `Encoder` 下拉里选 `png-seq` / `mov-qtrle (alpha)` / `webm-vp9 (no alpha)`，
     再点 `[Export]`。

6. **等进度**：导出在**后台线程**运行，进度显示在 Export 区底部（`Exporting N% ...`）。
   完成后给出结果路径：
   - `Done: video -> <dir>/xxx_vis.mov`
   - 或 `Done: N frames -> <dir>/out_frames`（PNG 序列）

> 导出期间两个导出按钮会被禁用；可以继续拖动参数看预览（不影响后台导出，导出用的是按下按钮那一刻的参数快照）。

---

## 3.5 图层：添加图片 / 调整顺序

频谱和图片都是画布上的"图层元素"，从上到下的叠放次序可以在 Layers 区调整：

1. **添加图片**：直接把 PNG/JPG 拖进画布（自动创建图层、默认铺满输出画布），
   或点 Layers 区的 `[Add image]` 选择文件。
2. **摆放**：点击选中（频谱 / 图片），然后用与频谱相同的手势——拖动移动、
   四角柄等比缩放、四边柄拉伸、顶部圆柄旋转、双击复位。
3. **排序**：`[Up]` / `[Down]` 上移/下移图层（决定谁盖住谁）。
4. **不透明度**：Opacity 滑块；`[Remove]` 删除选中图层。
5. 图片可以是频谱的背景（在频谱下方）或前景贴片（在频谱上方），导出与预览所见即所得。

---

## 3.6 拖文件显示禁止符？（管理员权限 / UIPI，仅 Windows）

> **本节仅适用于 Windows 时期**；WSL / Linux 下没有 UIPI 机制，正常拖放即可。

**如果你以管理员身份运行了 AudioVisGUI**（或从管理员终端启动它），Windows 的 UIPI 安全机制
会**静默拦截**普通权限资源管理器向提权进程的拖放——表现就是全程禁止符，这是系统行为，
不是程序 bug。两个自查方法：

- 看画布顶部是否出现**红色横幅**"Running as Administrator ..."（出现即中招）；
- 任务管理器 → 详细信息 → 右键列标题 → 选择列 → 勾选 **Elevated** → 看 AudioVisGUI.exe。

**解决**：关掉程序，用普通方式双击 exe 重新打开（不要右键"以管理员身份运行"）。
程序内拖放正常时，拖文件进窗口会显示"Drag detected: <文件名>"提示。

---

## 4. 三种输出格式怎么选

| 选项 | 实际产物 | 透明通道 | 用途 |
|---|---|---|---|
| `png-seq` | `frame_000001.png` 序列 | ✅ 100% ARGB 保真 | 后期合成/抠像最优，但费磁盘 |
| `mov-qtrle (alpha)` | 单个 `.mov` 视频（QTRLE + rgba）| ✅ **无损保留 alpha**（实测 `pix_fmt=argb`）| **最推荐**；Premiere / Vegas / DaVinci Resolve 全支持 |
| `webm-vp9 (no alpha)` | 单个 `.webm` 视频（VP9）| ❌ 无 alpha | 仅供体积小的网页/快速预览 |

> ⚠️ **重要实测结论**：`libvpx-vp9` 虽在参数列表里声称支持 `yuva420p`，但绝大多数发行版 ffmpeg
> **并未真正编码 alpha**，实测导出后 alpha 被静默丢弃（变成 `yuv420p`）。
> 因此本工具**不把 WebM 作为透明导出选项**；要透明视频请用 **MOV QTRLE** 或 **PNG 序列**。
>
> ⚠️ **不要在 NLE 里把透明素材转导出成 MP4/h264**——h264 没有 alpha 通道，透明区会变黑。

---

## 5. 常见问题

**Q1: 点 Export Video 后提示找不到 ffmpeg？**
A: 把 ffmpeg 所在目录加入 PATH 后重启程序；或设置环境变量 `FFMPEG_PATH` 指向 ffmpeg 可执行文件
   （Windows 为 `ffmpeg.exe`，Linux 为 `ffmpeg`；程序会按平台自动搜索）。

**Q2: 导出的视频在剪辑软件里透明区是黑的？**
A: 确认你导出的是 `mov-qtrle (alpha)` 或 `png-seq` —— 只有这两种保留 alpha。
   `webm-vp9` 实测在本工具下**不含 alpha**（libvpx 未开 VP9-alpha，参数声明的 yuva420p 实际被丢）。
   另外个别播放器（PotPlayer 老版本）不显示 alpha，属播放器问题，导入 NLE 即可验证。

**Q3: 为什么 MOV 导出文件这么大？**
A: QTRLE 是无损编码，体积大是正常的（透明区 + 每帧全存）。要小体积就用 WebM（无 alpha）或
   PNG 序列自行压缩。

**Q4: 导出中关掉程序？**
A: 析构会 `join()` 后台线程等它完成再退出，保证不丢半成品。

**Q5: 想恢复默认参数？**
A: 关闭重开 App 即回默认；预设 JSON 管理走 CLI（`--config assets/spectrum/default.json`）。

**Q6: 频谱元素拖出画布 / 想整体复位？**
A: 变换没有强制限制，可拖到任意位置（越界部分导出时同样被裁掉）。
   想整体回正：双击频谱元素，或点右侧 **Reset element transform** 按钮。

---