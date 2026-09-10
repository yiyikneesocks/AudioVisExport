# AudioVisExport — 下一步计划（滚动文件）

> **生命周期**：开版时写计划（经用户确认）→ 执行 → 发版后把执行结果摘要并入
> `docs/HISTORY.md` §2 变更日志，然后**清空本文件**写下一版计划。
> 历史计划快照：`docs/PLAN_v0.5.0.md`（Windows 交叉编译专项）。长期方向见 `docs/ROADMAP.md`。

---

## 工作流模板（含验证闸门，强制）

1. 写计划（本文件）→ 用户确认
2. **GitHub 参考检索（务必优先，非阻断）**：本步动手前，只要环境能联网，就先搜同类实现 / 范式
   （REST API 搜索 → `webfetch` 读 README/源码 → 必要时 `git clone --depth 1` 到 `/tmp/opencode`），
   在「当前状态」记可借鉴点 / 坑 + 来源 `owner/repo`；**无网才跳过并注明**。详见 `ARCHITECTURE.md`「参考优先原则」
3. 执行：代码 + 双端构建 + 部署测试包 + 文档（`HISTORY.md` / `RELEASE_NOTES.md` / 版本号）
4. `git commit`（本地；**此时严禁 push / tag / Release**）
5. **【验证闸门】** 停下请用户验证 GUI/CLI；用户明确回复「全部通过」之前不得推送
6. 用户通过后，**主动提醒用户「尚未推送 GitHub」**，等待用户明确下达推送指令
7. 收到指令后：push + tag + GitHub Release 一并完成（push 命令速查见 `ARCHITECTURE.md` §7.5）

---

## 文档更新触发点（checklist，防遗漏）

| 时刻 | 必须更新的文档 |
|---|---|
| 开版：计划获批 | `PLAN.md` 写入任务拆分与验证方案 |
| 每完成一步 / 发现偏差 | `PLAN.md`「当前状态」+ 最后更新时间戳 |
| 闸门前（本地 commit 时） | `HISTORY.md` §2 版本条目 + `RELEASE_NOTES.md` + `ARCHITECTURE.md` 版本映射表 + 代码版本号（CMake/CLI） |
| 发版推送后（用户指令） | `PLAN.md` 摘要并入 `HISTORY.md` 后清空重写 |

> **⚠️ 纪律红线（2026-09-10，v0.5.4 教训）**：
> - **每完成一个 INBOX 任务** → 必须同步更新 `PLAN.md`「当前状态」（含时间戳），不可积压。
> - **每次 `git commit`（功能变更）前** → 检查 `PLAN.md` 是否反映最新状态。
> - **每次部署前** → 确认 `PLAN.md` 已同步。
> - **绝对禁止** 出现「代码已推 20 个 commit 但文档停留在上个版本」的情况。v0.5.4 期间连续 20 个 commit 推到 main 但文档停留在 v0.5.3，直到用户提醒才补——这就是反面案例。
> - **违反此纪律 = 流程事故**，必须立即暂停编码、补完文档后才能继续。


> **📥 收件箱三文件协议（防读写抢占，务必遵守；`docs/inbox/` 已 gitignore，缺失时按 WORKLOG 头部模板重建）**：
> `docs/inbox/INBOX.md`=**文档1 用户输入箱**（用户写；AI 操作前先读首行「状态」：**1=编辑中→完全只读**；**0=空闲→仅可精确 `edit` 整行删除「已宣布完成且已备份」的编号行，绝不 `write` 重写全文**）；
> `docs/inbox/INBOX_WORKLOG.md`=**文档2 AI 台账**（接手前先复制条目做备份、记「已处理指纹」去重、做完归档+commit）；
> `docs/inbox/INBOX_REPLY.md`=**文档3 AI 给用户的信息**（AI 写、用户只读）。
> **每完成一条任务立刻重读文档1**（防漏 `!!` 加急）。
> **收尾强制规则（用户 #11，2026-09-10，最高优先）**：本轮所有任务做完后**必须**重读 `docs/inbox/INBOX.md`：
> 有新任务 → 删掉已处理条目后**不要停顿，直接开新一轮继续做**；无新任务 → 删除已处理条目后暂停收工。
> 唯一合法停顿点：下一步确实依赖用户的测试结果/决策而无法推进。
> **轮次节奏（用户 #0，2026-09-10）**：一轮内**连续做完所有能做的任务，中途不停**、不逐条清文档；
> 只有「没用户信息推不动」或「必须等用户测试」才停。做完全部 → 更新 REPLY → 结束本轮。
> 下一轮开始 = 读文档1 新任务 + 顺手清理（状态 0 时）两件事一起做，然后继续推进。
> **REPLY 结构（用户 #0+）**：开头 = **待办速览**（待办 / 待用户实测项 / 关键汇报——仅用户明确期待回复或 AI 认为必须知悉的）；
> 普通工作汇报放后面。用户测完的条目由 AI 删掉并更新。三个 inbox 文档 AI 都可按逻辑清理。
> **任务排序规则（用户 #10）**：加急(`!!`) > 无争议/实现简单/不需要用户手动验证就能自证的 > 需用户实测确认的 > 需拍板的设计项（列计划）。被"缺用户测试结果"卡住的跳过不阻塞。

---

## 当前状态（最后更新：2026-09-11 01:1x）

- **v0.5.3 已完整发版**（2026-09-09，tag `v0.5.3` + GitHub Release 已发）。
- **v0.5.4 编码中（重点＝频谱样式 + 蒙版图片 + 基线轴 + 崩溃报告）**：
  已推送到 GitHub main（`5d4b42e..14b5104`，20 commits；当前 HEAD=22a966f 本地 + 五连修复未提交），**尚未发版**（用户 Windows 实测进行中）。
  - ✅ 已完成：A1 ColorMap / A2 bar-mirror / A3 CrystalStyle v2 bloom / A4 频谱蒙版图片
    （BUG1 漂移修复 + BUG2 独立拉伸）/ Tabbed UI（4 tab）/ Baseline axis（baselineY）
    / Bar 布局 pitch 模型重做（#25 + #1' + #3''）/ Bar-line 峰帽 v3（连贯分段）
    / Line-only 切换 / Scale snapping / Crash reporter / mp3/flac registerBasicFormats
    / Timestamped deploy /收件箱三文件协议 / style proposals doc / Y2Kmeter audit。 / INBOX 五连修复（崩溃根除+轴重合+mp3+pitch 1/90+末柱报告，23:2x）
  - 🔧 已知问题：
    - ✅ **compose() 平均色描边崩溃 → 已根除**（渲染不再逐帧算均色；均色只在加载图/Use average 按钮时算一次）。
    - ✅ **#1b 勾 Outline 必崩·真根因（09-11）**：用户重报后读 `crash/*.dmp` 符号化（minidump + `build_win/AudioVisGUI.map` + objdump）
      → `SpectrumMask::compose +0x8f5`：描边环 `e[idx]` 行指针二次偏移（= `tmp[2·y·W+x]`）越界读堆。改 `e[x]` 修复；
      `vis_mask_test` 补 stroke 用例 6（负对照 3 FAIL 证明有效）。部署 `AudioVisGUI_09110110.exe`。
    - ✅ **mp3 解码 → 已支持**（开启 JUCE_USE_MP3AUDIOFORMAT 软件解码 + PcmSource 注册 MP3AudioFormat）。
    - ✅ **baseline axis 与频谱底部/左缘重合** → 已取消全样式 reduced(2) 缩进 + 左/下内边距归 0。
    - ✅ **pitch 默认 1/90**（与 bandCount=90 联动，默认 90 柱全宽）。
    - ⏳ #3（grey-out controls）和 #4（bar restored original cap）尚未由用户 Windows 实测。
    - ⏳ **下一轮**：INBOX 新 #3 peak-caps 四件套（3.1 bar-mirror 删除 / 3.2 bar 双侧帽跟随轴 / 3.3 line 系峰帽开关 / 3.4 下侧异常）+ 新 #6 蒙版双按钮（outline 已解锁）。
    - ⏳ 末柱斜面决定因素报告 → 已写入 INBOX_REPLY（等用户知悉）。
  - 📋 协作机制：INBOX 三件套 + status gate + #0/#0+/#10/#11 轮次节奏协议，
    REPLY 三固定子节结构。全部写入 PLAN.md「文档更新触发点」。
- **发版后遗留**：
  - ⏳ 导出侧范围内裁剪回归待用户复测（v0.5.3 遗留）。
  - P-verify 缩放/旋转时的吸附辅助线（v0.5.3 可选项）。

## 候选下一版（v0.5.4 → 重点：频谱样式，草案待用户拍板）

> **新增候选（用户 #10，2026-09-10）**：**音频驱动震动（shake）**——低频能量驱动整体/分层抖动位移，音画打击感；与峰帽手感同批调优。

> **本节为 v0.5.4「样式专项」规划草案。** 已按 `ARCHITECTURE.md`「参考优先原则」先做了
> GitHub 检索（结论与出处见下）。**尚未动任何代码 / 未构建 / 未改版本号**——等用户亲自看工程后拍板，
> 再据此展开"分步实施计划书"。用户当前指示：只写文档，其余不做。

### 0. 现状盘点（已读代码核实，非推测）

| 项 | 现状 | 位置 |
|---|---|---|
| 样式接口 | `SpectrumStyle::create(name)` 工厂 + `render(g,canvas,frame,rp)` | `source/core/SpectrumStyle.h/.cpp` |
| 已有样式 | `y2k-line` / `bar` / `bar-line` / `polyline` / `crystal` | `source/styles/*.cpp` |
| 数据源 | `BandFrame{ db[], peakDb[], normalized[] }` —— **只有频域带数据，无时域波形** | `source/core/BandFrame.h` |
| colorMap | `RenderParams.colorMap` 字段存在，但**仅 `"solid"` 生效**，`gradient`/`rainbow` 是空钩子 | `SpectrumStyle.h` `RenderParams` |
| 多 pass | `getNumPasses()/renderPass()` 接口在，但 **VisPipeline 未真正分图 + blend 合成**；CrystalStyle 是"同一画布顺序叠 3 遍" | `SpectrumStyle.h` / `VisPipeline.cpp` |
| 真高斯模糊 | **JUCE 自带** `Image::PixelData::applyGaussianBlurEffect(radius)`（`GlowEffect` 内部即用）→ 真 bloom 无需手搓 | `juce_graphics/effects/juce_GlowEffect.cpp` |
| 复用工具 | `freqToX_`（对数频率→x）/ `dbToY_`（dB→y）helper 已在 Y2KLineStyle | `source/styles/Y2KLineStyle.cpp:25/37` |
| 硬约束 | 输出是**透明背景 ARGB**（给剪辑软件叠加）→ 一切效果须在 alpha 通道下成立 | 全程 |

**关键结论**：
- 只靠现有 `BandFrame` 就能画的新样式 = 径向谱 / 镜像柱 / 真辉光线柱 / colormap。
- 波形·示波器·瀑布·Joy Division 堆叠线 = 需要**先给 SpectrumCore 暴露时域样本 / 帧历史缓冲**（改引擎，成本最高）。
- "真 bloom / 样式叠加 / 滤镜链" 的天花板取决于**先把多 pass 真合成架构（C2）落地**。

### 1. 外部参考（检索出处，GPL 合规 + 避坑用）

| 参考 | 取什么 | 出处 |
|---|---|---|
| 权威索引清单 | 全局门类 / 素材 / 教程导航 | `willianjusten/awesome-audio-visualization`（⭐5070） |
| 2D 出片分类基准 | bars / **mirrored bars** / line 三型是最通用范式 | Nutilz Audio Waveform Generator（清单 Experiments 内） |
| 终端频谱柱 | 经典柱/对数刻度/衰减手感 | `karlstav/cava` |
| 音乐刻度频谱 | 频率→音高映射的可视化巧思 | `mfcc64/youtube-musical-spectrum`、`mfcc64/html5-showcqtbar` |
| 径向/环形波 | 圆形谱/环形柱范式（本表 B1 参考） | `kelvinau/circular-audio-wave` |
| 波形绘制成熟实现 | 若将来做时域波形/示波器（C1）参考其降采样+包络 | `katspaugh/wavesurfer.js`（⭐10404） |
| JUCE 同类插件 | 频谱/波形/vectorscope 三视图工程化组织 | `blubass/FunkyMooseViz`、`H1R0Vocaloid/SpectraView` |
| 审美参考（不借实现） | Milkdrop/projectM 预设的色彩/运动美感 | `projectM-visualizer/projectm`（⭐4431）、`milkdrop2077/MilkDrop3` |

> 说明：projectM/Milkdrop 是实时 GL 着色器体系，与本 2D 离线 ARGB 管线不同构，**仅吸收审美取向**，不移植代码。

### 2. 拟定方向（分档，供勾选）

**A 档 — 便宜、复用现有架构（低风险快赢）**
- [x] **A1 ColorMap 落地**：`gradient`（按 `normalized` 强度 primary→secondary→白 上色）+ `rainbow`（按频带相位上色）；
      加公共 `ColorMap::colourForBand / horizontalGradient` helper，供 bar/polyline/y2k/crystal 共用。`visual.colorMap` 已可 `--set`。
- [x] **A2 镜像柱 `bar-mirror`**：柱绕画布水平中线上下对称（BarStyle 变体）。
- [x] **A3 CrystalStyle v2 真 bloom**：`applyGaussianBlurEffect` 替换 4 层假描边（色散列为后续可选）。
- [x] **A4 频谱蒙版图片**（本轮新增）：频谱轮廓=图片窗口；bar 逐柱 gap 不铺、line 整块；描边默认图片平均色（可关/覆盖）；
      图片绑定频谱整体拖动，「编辑图片位置」模式内单独平移、点外退出。共享 `source/core/SpectrumMask.{h,cpp}`。

**B 档 — 中等、新样式（建议挑 1）**
- [ ] **B1 径向谱 `radial`**：bands 绕圆环画（环形柱或闭合曲线），新参数圆心/内外半径/起始角。**带数据足够，无需改引擎**，视频观感强。
- [ ] **B2 霓虹线 `neon`**：曲线/面积 + 真高斯 glow 外发光。

**C 档 — 需先动引擎 / 架构（建议单列或推后）**
- [ ] **C1 时域暴露 → 波形/示波器/瀑布/堆叠线**：给 `SpectrumCore` 增加"当前帧窗口样本"输出 + 样式侧帧历史缓冲。价值高、改动最大。
- [ ] **C2 真·分层合成 + blend + 样式叠加**：把 `getNumPasses` 在 `VisPipeline` 落成"每 pass 独立 ARGB + `setCompositeOperation`（Screen/LinearDodge/additive）合成"，并支持一帧叠多个 style。**A3/B1/B2 的高级形态都依赖它。**

### 3. 非样式项（v0.5.4 是否顺带，待定）

- [ ] 缩放 / 旋转时的吸附辅助线（v0.5.3 只做了移动吸附，列为可选 P2）
- [ ] FLAC / MP3 / OGG 解码支持（JUCE 核心未含第三方解码库，需挂 `registerFormat`）
- [ ] P-verify：v0.5.3 范围内/外裁剪的导出侧回归（用户复测）
- **🅿 蒙版图片后续增强（用户 2026-09-09 明确"先记下、后续问我具体实现"）**：
  - [ ] **对蒙版图片做色彩调整**：RGB 三通道增益 / 饱和度 / 亮度 / 对比度 / 色相旋转（实时、作用于被裁剪后的图片层）。
        待问：作用点=预乘像素级 pass？是否需要曲线/HSL 分离？是否要预设滤镜？
  - [ ] **一键加图片 / 图库**：从预设或最近使用里一键选图套为蒙版。待问：图库来源（内置素材/系统最近文件/收藏目录）？批量？
  - [ ] （可选）蒙版图片独立**淡入淡出/随强度调制透明度**（图亮度随频谱能量起伏）。

### 4. 待用户拍板（看完工程后回这几项，我据此写详细实施计划书）

1. **范围**：只 A 档？还是 A + B（例如把**径向谱**纳入，视觉冲击大）？要不要本版就啃 **C2 真合成架构**？
2. **C1 时域样式**要不要排进 v0.5.4，还是留 v0.5.5？（它是"波形/瀑布"的前提，改引擎）
3. **新样式优先级排序**：A1 colormap、A3 真 bloom、B1 径向——你最想要哪个先出来？
4. **GUI 同步**：新样式的参数（radial 圆心/半径、colormap 选单等）确认要同步进 `ParamPanel` + `SpectrumParams` + JSON/CLI 全链路？
5. **透明约束**再确认：所有效果以"透明背景叠加素材"为硬指标，bloom/glow 的 alpha 处理按此验收。

### 5. 下一步（拍板后我会做的）

- 把选定项展开成**分步实施计划书**（每步：改动文件 + 验证方案 + 双端构建 + 部署），沿用本文件工作流模板（含每步先 GitHub 检索、验证闸门）。
- 涉及新 style 的先加 `vis_anchor_test` 式的最小数学/渲染断言（例如 colormap 颜色可复现、radial 极坐标映射正确）。

---

*维护者：AudioVisExport 项目（GPL-3.0） · 发版后本文件清空重写*

