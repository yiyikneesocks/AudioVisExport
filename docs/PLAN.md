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

---

## 当前状态（最后更新：2026-09-09）

- **v0.5.2 已完整发版**（commit `0a92842`、tag `v0.5.2`、GitHub Release 已发）
- **v0.5.3 已完整发版**（2026-09-09，tag `v0.5.3` + GitHub Release 已发）：
  B1–B5 + F1 + 真·B3 + B6（旋转后拉伸平行四边形，S·R→R·S）+ Above spectrum UI 移除
  + N1 频谱自吸附修复 + N2 CAD 吸附辅助线（9 特征点↔9 特征点，边-边/角-角全对齐）
  + N3 空格播放/暂停（含键盘焦点修复）+ N4 范围内外视觉区分（超范围内容变暗发灰）。
  执行详情见 `docs/HISTORY.md`「v0.5.3」条目与 `docs/RELEASE_NOTES.md` v0.5.3 节。
- **发版后遗留（回归/可选）**：
  - ⏳ 导出侧范围内裁剪回归待用户复测（本轮仅改 GUI `SpectrumCanvas`，`VisPipeline` 未动，理论上输出不变）。
  - P-verify 缩放 / 旋转时的吸附辅助线（v0.5.3 仅做移动吸附辅助线，缩放辅助线列为可选）。
- **v0.5.4 编码中（重点＝频谱样式 + 频谱蒙版图片）**：已动代码，**双端构建 0 error、已交叉编译部署到 Windows**，
  **尚未 commit / 未推 GitHub / 未改版本号**，等用户预览效果满意后一起提交。已完成：
  - ✅ **A1 ColorMap 接入**：`gradient`（按 `normalized` 强度上色）/ `rainbow`（按频带相位铺彩虹）/ `solid`，
    已接进 bar / bar-line（逐柱）、polyline / y2k-line / crystal（沿频率横向渐变）。`--set visual.colorMap=…`。
  - ✅ **A2 镜像柱 `bar-mirror`**：新样式，柱绕水平中线上下镜像 + 峰帽；工厂 / CMake / GUI 下拉 / CLI help 全接。
  - ✅ **A3 CrystalStyle v2 真 bloom**：pass0 的 4 层假描边 → JUCE 真高斯 `applyGaussianBlurEffect`（透明背景上验证 alpha 不糊脏）。
  - ✅ **A4 频谱蒙版图片**（用户临时追加需求）：图片只在"频谱轮廓"内可见（频谱=窗口/蒙版）；
    bar 逐柱（gap 处不铺图）、line 整块（下边界到基线）；描边沿轮廓内侧勾边，默认色=**图片平均色**（可关/可手动覆盖）；
    图片与频谱**绑定整体拖动**，「编辑图片位置」按钮开启后可在轮廓内单独平移图片、点轮廓外自动退出编辑。
    实现＝读 `base`（频谱 ARGB 层的 alpha）做 style-agnostic 像素蒙版（预乘安全），`VisPipeline` 与 `SpectrumCanvas` 共用 `SpectrumMask::compose`（预览即所得）。
  - 验证：18 组合（6 样式×3 colormap）出帧 + 蒙版逐柱/整块像素断言（gap 透明、填充被图片替换、auto 平均色、空路径零回归）+ `vis_anchor_test` 28/28。
  - ⏳ **待用户预览**后一起 commit（含本 v0.5.4 全部改动）。
  - 🔧 **蒙版 Bug 修复轮**（见 `docs/INBOX.md`「✅」）：BUG1 图片随电平漂移 → 几何改独立 `VisTransform`（与电平无关，锚定画框，实测 0 漂移）；
    BUG2 图片不可独立拉伸 → 「Edit image position」给独立手柄（角缩放/边拉伸/平移/旋转）+ 吸附频谱画框边/中线。已双端构建 0 error + 部署。
  - 🆕 **协作机制**：新增 `docs/INBOX.md`「任务收件箱」——用户往里写问题，AI 边做边读、自主推进、做完归档，只在需拍板时回问。

## 候选下一版（v0.5.4 → 重点：频谱样式，草案待用户拍板）

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

