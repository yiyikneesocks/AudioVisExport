# AudioVisExport — 项目历史（里程碑 + 变更日志）

> **定位**：已完成内容的详细记录与解析——新协作 AI 读完本文档即可了解"做过什么、为什么、
> 怎么验证的"，作用等同于交接上下文。长期方向见 `docs/ROADMAP.md`，当前迭代见 `docs/PLAN.md`。
>
> **章节引用约定**：本文档历史条目中的 `§N` 指撰写当时的 `ARCHITECTURE.md` 章节号（原文未改写）。
> 2026-09-08 四拆后 ARCHITECTURE.md 重编号：旧 §6/§7/§12 迁出本文档与 ROADMAP；旧 §9.5→§7.5、
> 旧 §10→§8、旧 §11→§9，§1-§5 不变。读历史条目时按此映射换算。
>
> **发版流程（每次小版本都要走完）**：
> 1. 本文档 §2 顶部新增版本条目（变更内容 + 验证记录）
> 2. 撰写**更新公告** → `docs/RELEASE_NOTES.md`（用户视角：新功能 / 修复 / 升级注意事项）
> 3. `git add -A && git commit`（**本地提交，此时严禁 push**）
> 4. 部署测试包 → **停下请用户验证**；用户明确回复「全部通过」前严禁 push/tag/Release
>    （验证闸门规则全文见 `ARCHITECTURE.md` 开头）
> 5. 用户通过后，**主动提醒用户"尚未推送 GitHub"**，等用户明确下推送指令后：
>    `git push origin main` + 打 tag `git tag vX.Y.Z && git push origin vX.Y.Z`
> 6. 在 GitHub 用 `docs/RELEASE_NOTES.md` 内容创建 Release
>
> **（push 环境事实，实测 2026-09-07）**：
> · 系统 git（gnutls 后端）连 GitHub 必 TLS 中断；须用 conda 环境 gitenv 的 openssl 版 git：
>   ```bash
>   PATH=/home/azulores/miniconda3/envs/gitenv/bin:$PATH git -c http.version=HTTP/1.1 push origin main
>   PATH=/home/azulores/miniconda3/envs/gitenv/bin:$PATH git -c http.version=HTTP/1.1 push origin vX.Y.Z
>   ```
> · TLS 偶发抖动（openssl 后端也偶发 "unexpected eof"），失败就重试 1~5 次；
> · 凭据：GitHub PAT（fine-grained，仅本仓库 Contents 读写）存 `~/.git-credentials`
>   （0600，credential.helper=store），remote URL 保持干净形式（不含 token，防泄漏）；
>   **协作 AI 注意：不要要求用户把 token 贴进对话/命令行明文**——用 credentials 文件机制，
>   token 轮换由用户在 GitHub 网页 Regenerate 后自行写入该文件；
> · push 后发版 = GitHub API 创建 Release（正文取 `docs/RELEASE_NOTES.md` 对应版本节）。

---

## 1. 已完成的里程碑

| 步骤 | 内容 | 状态 |
|------|------|------|
| Step 1 | 骨架可编译（删 projectM、CMake、空骨架、CLI、VisPipeline） | ✅ |
| Step 2 | SpectrumCore 算法实现（双路 FFT + band mapping + 平滑 + 峰值） | ✅ |
| Step 3 | Y2KLineStyle 完整实现（Catmull-Rom 平滑曲线 + 填充 + 描边 + 峰值） | ✅ |
| Step 4 | 透明背景验证（ARGB PNG alpha=0，PR 可叠加） | ✅ |
| Step 5 | 真实音频端到端测试（PUPA 30s 完整导出 + WebM alpha） | ✅ |
| Step 6 | 参数对比验证（6 组 10s 不同参数 MP4） | ✅ |
| Step 7 | BarStyle 完整实现（柱状图 + 渐变填充 + 峰值帽） | ✅ |
| Step 8 | PolylineStyle 完整实现（折线 + 填充 + 描边 + 峰值） | ✅ |
| Step 9 | CrystalStyle 完整实现（3-pass 水晶效果：辉光 + 玻璃体 + 高光） | ✅ |
| Step 10 | AudioVisGUI 实时预览界面（拖入音频 + 播放同步 + 参数面板 + 一键导出） | ✅ |

---

## 2. 变更日志（每发版必更，最新在上）

> 发版流程见本文档页首；push 环境事实（gitenv + HTTP/1.1 + PAT）也已固化在页首，此处不再重复。
> **章节编号对照**：2026-09-08 文档四拆后 `ARCHITECTURE.md` 重编号——旧 §6→`HISTORY.md` §1、
> 旧 §7→`docs/ROADMAP.md`、旧 §9.5→§7.5、旧 §10→§8、旧 §11→§9（§1-§5 编号不变）。

### v0.5.4 — 2026-09-10（频谱样式专项 + 蒙版图片系统 + 基线轴 + 崩溃报告 + 多项UI增强）

> 说明：v0.5.4 于 2026-09-09~11 开发，主体已推送到 GitHub main（commit `5d4b42e..14b5104`），
> 其后按用户 INBOX 反馈迭代：`f45eaa5`（五连 #1/#2/#7/#8/#9）→ `c755f73`（#10 y2k 两臂颜色）→
> `5877024`（#1b outline 必崩真根因），**当前 HEAD=`5877024` 已推送**。
> **尚未发版**（用户实测进行中，含多轮 INBOX 反馈迭代）。以下正文为截至 14b5104 的变更，
> 之后的反馈迭代记在本节末尾（「v0.5.4 收尾补丁」及其后小节）。

**变更（频谱样式增强 + 蒙版图片 + 基线轴 + crash reporter + bar布局重做）**：

- **A1 ColorMap 落地（`source/core/ColorMap.{h,cpp}` 新增）**：
  - `gradient`：按 `normalized` 强度上色，primary→secondary→白渐变；
  - `rainbow`：按频带相位铺彩虹色相环；
  - `solid`：单色（原有行为，向后兼容）；
  - 公共 helper `colourForBand()` / `horizontalGradient()` 供 bar/bar-line（逐柱）、
    polyline/y2k-line/crystal（沿频率横向渐变）共用；
  - `--set visual.colorMap=gradient|rainbow|solid` CLI 贯通。

- **A2 镜像柱 `bar-mirror`（`source/styles/BarMirrorStyle.{h,cpp}` 新增）**：
  - 柱绕画布水平中线上下镜像（上半柱+下半柱），峰值帽同步镜像；
  - 工厂注册 + CMake + GUI 下拉第 6 项 + CLI help；
  - ColorMap 逐柱上色已接入。

- **A3 CrystalStyle v2 真 bloom**：
  - pass0 的 4 层假描边 → JUCE 真高斯 `applyGaussianBlurEffect(radius)`（透明背景上验证 alpha 不糊脏）；
  - ColorMap 横向渐变已接入。

- **A4 频谱蒙版图片（`source/core/SpectrumMask.{h,cpp}` 新增）**：
  - 图片只在"频谱轮廓"内可见（频谱=窗口/蒙版）；
  - bar 逐柱（gap 处不铺图）、line 整块（下边界到基线）；
  - 描边沿轮廓内侧勾边，默认色=**图片平均色**（可关/可手动覆盖）；
  - 图片与频谱**绑定整体拖动**，「编辑图片位置」按钮开启后可在轮廓内单独平移/缩放/旋转、点轮廓外自动退出编辑；
  - `compose()` 为 VisPipeline（导出）与 SpectrumCanvas（GUI预览）共用的合成函数；
  - `adjustedImageCached()` 使用 bounded 8-entry map + CriticalSection double-checked lock（GUI paint线程与导出线程安全）；
  - BUG1 修复：图片随电平漂移 → 几何改独立 `VisTransform`（与电平无关，实测 0 漂移）；
  - BUG2 修复：图片不可独立拉伸 → 「Edit image position」给独立手柄（角缩放/边拉伸/平移/旋转）+ 吸附频谱画框边/中线；
  - 新增常驻回归 `scripts/vis_mask_test.cpp`（contain 居中/不漂移/手柄=渲染 三断言）。

- **Tabbed UI（#6，`source/gui/ParamPanel.{h,cpp}` 重做）**：
  - 4 个选项卡：Spectrum / Image / Mask / Export；
  - 画布选中元素自动切换到对应 tab（频谱→Spectrum / 图片→Image / 蒙版图片→Mask）；
  - Image tab：被选图片的 Brightness/Contrast/Saturation 三滑块；
  - Mask tab：Mask 颜色按钮（Primary/Secondary/Background）+ 两个新按钮（Use average / Use average colour）；
  - 新增 `ParamPanel::refreshStyleDependentControls()`：CapPull / Line-only / Peak caps 开关在不适用的样式下灰掉。

- **基线轴（#8，`baselineY` 参数，0=底，1=顶）**：
  - 新参 `SpectrumParams::baselineY`（JSON `visual.baselineY`，CLI `--set visual.baselineY`）；
  - 画布叠加层：水平虚线 + 可拖动方块手柄；双击/拖动吸附到 [0,¼,⅓,½,⅔,¾,1]，½ 时显示"mirror"提示；
  - bar 样式（bar/bar-line/bar-mirror）：柱体对称生长——top = a+(1−a)n，bottom = a(1−n)；
  - line 样式（y2k/polyline）：a>0 时生成双曲线（上臂=baselineTop，下臂=baselineBottom），双填充+双描边；
  - CrystalStyle：a>0 时双曲线 + 下臂镜像填充；
  - `VisTransform.h` 新增 `baselineTop(n,a)` / `baselineBottom(n,a)` 静态 inline helper；
  - `makeContainTransform` pos 公式修正：旧 `out/2−s·c` → 正确 `out/2−c`（修复所有图片图层 s≠1 时的定位偏差）。

- **Bar 布局 pitch 模型重做（#25 + #1' + #3''）**：
  - 不变量：`gap = pitch − width`；`barPitchRatio` 驱动 slot 宽度；
  - `bandCount = floor(100 / pitch%)`，与 Band count 滑块联动互锁；
  - 所有 bar 渲染器（bar/bar-line/bar-mirror）改用 `slotW = pitchRatio × innerWidth`（而非旧 `innerWidth/N`）；
  - 字段默认值修正：`barPitchRatio` 从 1.0 改为 1/90（修复新建时单细条 bug）。

- **Bar-line 峰帽 v3（#9，连贯分段式）**：
  - `capN_[]` 每带状态：首原则 cap ≥ bar top；fall rate 含高度因子；
  - cap 宽度 = bar 宽度（段跨柱宽），bar 之间有间隙，邻峰拉平用 2-pass Laplacian（`capPull` 参数控制强度，0=legacy 长斜线）；
  - baselineY > 0 时底部也生成镜像 cap；
  - last-bar edge lift（末柱边缘不衰减）。

- **Line-only 切换（#6/lineOnly）**：
  - 新参 `SpectrumParams::lineOnly`（JSON `visual.lineOnly`，CLI `--set visual.lineOnly`）；
  - y2k-line / polyline / crystal：lineOnly=on 时跳过填充（只画线+峰值）；
  - bar / bar-line / bar-mirror 不受此开关影响。

- **Scale snapping（#3，`applyScaleSnap()`）**：
  - 角/边拖拽时与目标画布/元素中心/边自动对齐（复用移动吸附同一套候选集）。

- **崩溃报告（#8，`source/core/CrashReporter.{h,cpp}` 新增，Windows-only）**：
  - `SetUnhandledExceptionFilter` → MiniDump + 文本报告写入 `exe/crash/`；
  - GUI (`Main.cpp`) 与 CLI (`CliMain.cpp`) 均已接入；
  - `EXCEPTION_HEAP_CORRUPTION` 使用常量 `0xC0000374`（excpt.h 宏在交叉环境不可用）；
  - `CMakeLists.txt` 两个 Windows target 链接 `dbghelp`。

- **mp3/flac 支持（部分）**：
  - `PcmSource::sharedFormatManager` 改调 `registerBasicFormats()`（WAV/AIFF/FLAC/OGG；MP3/WMA 仅 Windows 下有 WindowsMediaAudioFormat）；
  - GUI 文件格式过滤器 + 原生拖放路由更新为 `*.wav;*.aif;*.aiff;*.mp3;*.flac;*.ogg;*.wma`；
  - **已知问题**：用户实测 mp3/flac 仍无法解码——可能 `JUCE_USE_FLAC`/`JUCE_USE_OGGVORBIS` 编译标志未启用，待排查。

- **Timestamped deploy + 自动清理**：
  - 部署文件名改为 `AudioVisGUI_MMDDHHmm.exe`（避免版本重名覆盖）；
  - 部署前自动删除旧版本（跳过被 Windows 锁定的文件）；
  - `Run_AudioVisGUI.bat` 自动拉起最新时间戳版本。

- **文档新增**：
  - `docs/STYLE_PROPOSALS.md`：A 组（Y2Kmeter 参考：spectrogram/3D/scope/VU/Milkdrop）+ B 组（原创：radial/LED/waveform/particles/text/terrain），含预览链接；
  - `docs/Y2KMETER_AUDIT.md`：7 项借鉴项 + 12 项候选评估（top 推荐：Milkdrop 层 via libprojectM 4 + WasapiLoopbackCapture）。

- **收件箱三文件协作协议（本地专用，已 gitignore）**：
  - `docs/inbox/INBOX.md`：用户输入箱，首行 `状态 0/1` 闸门（1=编辑中→AI 只读）；
  - `docs/inbox/INBOX_WORKLOG.md`：AI 台账（备份/指纹去重/归档）；
  - `docs/inbox/INBOX_REPLY.md`：AI 给用户的信息（三固定子节结构）；
  - 协议规则写入 `docs/PLAN.md`（#0/#0+/#10/#11 轮次节奏、REPLY 结构等）。

**修复**：
- `VisTransform::makeContainTransform` pos 公式：`out/2−s·c` → `out/2−c`（修复 s≠1 时图片不居中）。
- SpectrumMask averageColour：JPEG 图片为非 ARGB 格式 → 读像素前先 `convertedToFormat(ARGB)`（修复越界崩溃）。
- compose() scratch buffer：`std::vector<int> tmp, er` 改为 `static thread_local`（per-frame 70MB 分配 → 复用），加 null guard（**但用户实测仍崩溃，见下方已知问题**）。
- BarStyle / BarLineStyle：capPull / slotW 公式从 `innerWidth/N` 改为 `pitchRatio × innerWidth`。
- ParamPanel：hidden-row labels 重新隐藏（防止 Tab 文字重叠）。
- SpectrumCanvas：scaleSnap 拖拽角/边时正确应用 `applyScaleSnap()`。
- makeContainTransform pos 公式修正影响所有图片图层的默认定位（含蒙版图片）。

**用户实测进度（滚动更新，截至 2026-09-11 18:0x，明细见 `docs/inbox/INBOX_REPLY.md`）**：
- ✅ **compose() 平均色描边崩溃 → 已根除**（渲染不再逐帧算均色；均色只在加载图片/Use average 按钮时算一次写入描边色，见下方收尾补丁）。
- ✅ **mp3 解码失败 → 已修复**（开启 `JUCE_USE_MP3AUDIOFORMAT` 软件解码 + PcmSource 注册 `MP3AudioFormat`）。
- ✅ **#2 基线轴与频谱底部/左缘重合 → 用户确认通过**（"4.基线轴与频谱底部/左缘完全重合已通过"）。
- ✅ **#7 音频格式（wav/aiff/flac/ogg/mp3）→ 用户确认全数通过**。
- ✅ **#8 pitch↔bandCount 新模型 + 默认 90 柱全宽 → 用户确认通过**。
- ✅ **#10 line 样式拖轴两侧颜色一致 → 用户确认通过**（"10.和3中情况一样，颜色已经通过"）。
- ✅ **拖图到 Mask 页设蒙版 + 蒙版图片几何 → 用户确认"蒙版图没问题"**。
- ❌ **勾 Outline 必崩 → 用户重报，根因并非均色**：真根因＝compose 描边越界读（见下方 #1b 小节，已修，待用户复测）。
- ✅ **新 #3 peak-caps 四件套 + #5 派生项 → 已修**（3.1 删除 bar-mirror 样式（别名向后兼容）/ 3.2 bar 峰帽跟随基线轴且双侧 / 3.3 "Peak caps" 对 line 系取消置灰并真正门控峰线 / 3.4 y2k 下臂描边着色 + 三样式下臂峰线 + crystal 下臂填充不再溢出半屏；新增 `vis_peaks_test` 16 断言 + 负对照，见下方 #3 小节）→ 待用户复测。
- ✅ **新 #6 蒙版图片本体 → 用户确认"没问题"，已按用户要求从 INBOX 删除**；双按钮（Border colour 手选 / Use average 一次算色）已随 #1/#1b 落地可用。
- ✅ **用户 09-11 实测确认通过**：勾 Outline 不再崩溃（A）/ bar-mirror 确已删除（B）/ bar 双侧帽跟随轴（C）/ line 系 Peak caps 开关可用（D）。
- ✅ **新 #E crystal 两侧亮度不一致 → 已修**（下臂此前只吃到辉光+填充，实体描边与高光都只画上臂），待复测。
- ✅ **新 #G 切页残留 → 结构性消除**（一行多控件 + 未登记即隐藏的安全网），待复测。
- ✅ **新 #H 拖放两现象 → 三处根因已修**（图片被当音频加载 / 状态先写后验证 / 0 尺寸 contain 炸框）+ 采纳图层栈建议，待复测。
- ✅ **#F 频带高度三问** → 报告已写入 INBOX_REPLY「关键汇报」首节（用户点名位置）。

**v0.5.4 收尾补丁（INBOX #1/#2/#7/#8/#9，commit `f45eaa5`，HEAD `22a966f` 后）**：
- **#1 平均色描边崩溃根除**：SpectrumCanvas 渲染路径删除逐帧 `maskAverageColourCached`，渲染统一读 `strokeColor`；均色只在 `applyMaskImageFile`（加载图）/ `onUseMaskAvgColour`（Use average 按钮）时算一次写入；VisPipeline 导出保留首帧缓存（CLI 参数固定，实际只算一次）。
- **#2 基线轴与频谱底部/左缘重合**：全部 12 处样式 `canvas.reduced(2)` → `canvas`；`paddingLeft 32→0`、`paddingBottom 16→0`（SpectrumStyle.h 默认值 + MainComponent 固定值同步）；柱底/左缘贴画框 = baselineY=0 轴线。
- **#7 mp3 全平台支持**：CMake 全局 `JUCE_USE_MP3AUDIOFORMAT=1`（FetchContent 前声明传入 JUCE 子目录）；PcmSource 注册 `MP3AudioFormat`；CLI/GUI 报错消息更新为 5 格式列表。Linux 实测 mp3 probe + 152 帧导出 OK。
- **#8 pitch 默认值**：`barPitchRatio` 1.0 → `1/90`（SpectrumParams.h），与 bandCount=90 一致；probe-spectrum 回归 bandCount=90 / ok=true。
- **#9 末柱斜面报告**：详见 `docs/inbox/INBOX_REPLY.md`（左端高 edge[N-1]=(n[N-2]+n[N-1])/2；右端高 edge[N]=max(n[N-1], n[N-2]/2)）。

**v0.5.4 追加补丁（commit `c755f73`，用户测试后回报的 #10）**：
- **#10 y2k-line 拖动基线轴时上下两侧"颜色不一致"**：根因＝`Y2KLineStyle::render` 下臂填充（`fillDn`）
  沿用了上臂**描边**留下的 1.0f 不透明渐变/颜色状态，下臂因此比上臂"实"很多；修复＝填充下臂前重置为
  与上臂填充同参数（`cm.horizontalGradient(..., 0.25f)` / `rp.secondary.withAlpha(0.25f)`）。
- 用户已确认通过（"10.和3中情况一样，颜色已经通过"）。
- ⚠️ 同批用户重报"勾选 outline 必然崩溃"→ 说明 #1 未真正根除，见下一小节 #1b。

**INBOX #1b（2026-09-11 01:1x，commit `5877024`）·勾 Outline 必崩真根因 = compose 描边越界读**：
- 取证：用户 `crash_20260911_001832/002113.dmp`（minidump 解析 ACCESS_VIOLATION@0x7ff6e51ab355，模块基址 0x7ff6e5190000
  → RVA 0x1b355）+ sha256 对齐部署 exe + `build_win/AudioVisGUI.map` 符号化 + objdump 反汇编 `sub esi,[r13+rax]`。
- 根因：`rim = m[x] − e[idx]`，`e` 已是行指针、`idx=y*W+x` → 实读 `tmp[2·y·W+x]`，y≥H/2 越界读堆（Windows 必崩），
  且前半行用错位腐蚀数据（下边缘描边一直是错的）。上一轮"根除"的均色路径是**另一条**真路径，本次与均色无关。
- 修复：`e[idx]` → `e[x]`。回归：`vis_mask_test` 用例 6（1200×800 stroke：四边环对称 top/bottom=2505、left/right=1505，
  深内部/轮廓外零污染）；负对照还原旧码 → 3 FAIL。此前 stroke 路径零测试覆盖 = 崩溃漏网原因。

**文档协作协议强化（2026-09-11，`docs/PLAN.md` §0 新增「写入安全」）**：
- 本轮重写 `docs/inbox/INBOX_REPLY.md` 待办速览时，用「读全文→切片重组→整写」的方式截断了其按轮归档尾部（约 110 行，09-09/09-08 轮 + 归档节）；
  事实记录未受损（`INBOX_WORKLOG.md` 逐轮台账 + 本文件均完整），已按台账 + `git log` 核对 commit 号后重建为摘要，并在 REPLY 内如实记录该事故。
- 由此固化规则：改三件套前先整体备份、归档尾部只准顶部插入不得重写、用户原话必须逐字备份、改后比对行数确认未缩水。

**INBOX #3（2026-09-11 02:2x，commit 见下）· peak-caps 四件套（3.1~3.4）+ 派生 #5**：
- **3.1 删除 bar-mirror 样式**：用户指出"bar 能拖轴以后和 bar-mirror 重复"→ 删 `source/styles/BarMirrorStyle.{h,cpp}` +
  CMake + GUI 下拉 + CLI `--style` 列表；`SpectrumStyle::create` 保留 `"bar-mirror"|"barmirror"|"mirror"` → **BarStyle** 别名，
  旧 JSON 预设不失效（实测 `--style bar-mirror` 与 `bar` 出图一致）。替代用法：bar/bar-line + Baseline=50%。
- **3.2 bar 峰帽不随基线轴动**：根因＝`BarStyle` 帽用 `normalizedToY_(pn)` 原始值，未过 baseline 映射（bar-line 已过，故表现正常）。
  修复＝上帽 `baselineTop(pn,a)`、`a>0` 时下帽 `baselineBottom(pn,a)`（**双侧帽**）；同时补下臂外缘 1px 缘线（顶替被删样式的清晰轮廓）。
- **3.3 line 系无法勾选 Peak caps**：根因＝`refreshStyleDependentControls` 把开关 `setEnabled(barFam)`，line 系永远置灰。
  修复＝开关全样式可用，并在 y2k-line / polyline / crystal 内以 `rp.barParticles` 门控峰值虚线（关＝一条不画）；
  参数注释语义同步为"bar 系=峰帽横线 / line 系=峰值保持虚线"。
- **3.4 line 系拖轴后下侧异常（三个独立 bug）**：
  - **y2k-line 下臂无描边**：`curveDn` 描边前**漏 `setColour`** → 继承上方 fill 的 `secondary 0.25f`，几乎不可见。修复＝显式设 primary/1.0f 渐变（与上臂同式）。
  - **三样式下臂无峰线**：峰线只算 `baselineTop`。修复＝`a>0.001` 时追加 `baselineBottom` 镜像峰线（y2k 手构折线绕开贴底剪枝；polyline/crystal 同法）。
  - **crystal 下臂"特别大、几乎填满"**：`buildMirrorPoints_` 由上臂 y **反推 n 时漏减 a**（`baselineTop` 的逆应为 `((bottom−y)/H − a)/(1−a)`），
    反推出 `n+a/(1−a)` 被抬高并 clamp 到 1 → 下臂点全部落到画布底 → 整个下半屏被填充。修复＝补 `- a`。
    另 crystal pass2 峰线原先完全不过轴（直接 `dbToY_`），一并改为 baseline 映射 + 双侧。
- **测试补强**：新增常驻回归 `scripts/vis_peaks_test.cpp`（CMake target `vis_peaks_test`）16 断言——
  bar 帽 a=0 零回归 / a=0.5 上下帽精确落在 0.6·H、0.4·H 且无残留旧行；三样式 Peak caps 开关的 on/off 像素差 > 500 且轴上下各 > 100；
  y2k 下臂 primary 描边存在且高度正确；crystal 下半屏填充不超 25% 面积。**负对照**（逐个还原 3 个 bug）→ 精准 5 条 FAIL，证明用例有效。
- **验证**：Linux 全量 0 error；`vis_peaks_test` / `vis_mask_test` / `vis_anchor_test` 三套 ALL PASS；
  6 样式 `--preview-frame`（baselineY=0.5）出图全 OK；Windows 交叉构建 34/34 → 部署 `AudioVisGUI_09110217.exe`。

**INBOX #E/#F/#G/#H（2026-09-11 17:5x）· crystal 两侧亮度 + 频带三问报告 + 选项卡结构保证 + 拖放三根因**：
- **#E crystal 下臂比上臂暗**：`CrystalStyle::renderPass` 只有 pass0 辉光覆盖了下臂曲线，pass1 的双层实体描边
  （0.40f/0.90f）与 pass2 的 2.5× 高光都只作用于上臂 `pts` → 下臂亮度必然低。修复＝两处都补
  `buildMirrorPoints_` 下臂路径，颜色参数与上臂**逐字相同**（不是"调亮一点"）。
- **#F 三问答复（核实过的代码事实）**：带内取 **max**（`peakInRange`，`SpectrumCore.cpp:206-228`），不是平均；
  91 个 mappedEdges **无缝首尾相接**铺满 [minHz,maxHz]（`FreqMap.cpp:96-101`）→ 末带上缘恰为 20000，不存在"缺下一根"；
  唯一的加权平均是**跨带** `[1,2,1]/4`（`SpectrumCore.cpp:371-383`，掺入比＝`temporalSmoothing`，**首末带显式豁免**）；
  斜面端点共享 `edge[k]=(n[k-1]+n[k])/2`，末点 `edge[N]=max(n[N-1], n[N-2]/2)`（`BarLineStyle.cpp:50-57`）。
  默认（log、90 带、20–20000）实测数字：每带比上一带宽 7.98%（比例 1.07978）、含 1000Hz 的带＝[928,1002]、末带＝[18522,20000]。
- **#G 选项卡残留（用户点名"以后着重注意这种不消失问题"）**：根因不是漏了一个按钮，而是
  `Row{label, editor, h, tab}` 结构上**一行只能挂一个 editor**；Colors(4 按钮)/描边色(2)/W×H(2) 都是并排多控件，
  `showTab`+`resized` 只认 `r.editor` → `maskAvgBtn`/`secondaryBtn`/`peakBtn`/`bgBtn`/`heightEditor` 五个控件
  **从来没有被隐藏过**。修法（结构性）＝`Row::editors` 改向量 + `addRowGroup(label,{…})` 整行统一驱动布局与显隐 +
  `keepVisibleOnAllTabs()` 登记常驻件（页签行/底部导出条）+ `resized()` 末尾**安全网**：
  不属于任何行且非常驻的直接子组件一律 `setVisible(false)`（判据 `findUnownedChildren()` 公开化，
  供安全网与测试共用一份实现）。**失败方向由此翻转**：漏登记的表现从「静默残留在别的页」变成「控件不出现」。
- **#H 拖放三处根因**（"有时候"＝只对某类文件/某条投递链发作，故此前难复现）：
  1. **兜底拖放目标把图片当音频加载**：`MainComponent::filesDropped` 的 `isAudio` 判定里混进了 png/jpg/… 扩展名，
     命中后一律 `loadFile()` → 拖到面板（非 Mask 页）的图片永远建不出图层。
  2. **先写状态后验证（蒙版）**：`applyMaskImageFile` 无条件 `maskImage.path=…; enabled=true`，之后才
     `if (im.isValid())` 算均色 → 解不开的图留下"已设置但画面毫无变化"的死状态，
     下次拖入只会弹 "Mask image already set"（用户原话"没反应，但提示里已经加载过图片"）。
  3. **0 尺寸 contain 炸框**：`addImageLayer` 不验证解码结果（且 `ImageCache::getFromFile` 调两次），
     图无效时尺寸 0 → `makeContainTransform(0,0,…)` 内 `jmax(1.0f, elemW)` 把 0 当 1 → **scale 静默 = 720**；
     画布画手柄时元素尺寸又走"图片无效则退化为画布尺寸"的分支 → 1280×720 被放大 720 倍 →
     **框跑到画布上千倍外**（用户原话"提示的边框范围远超画布，原因不明"）。
  修复＝`loadValidatedImage()`（文件不存在/解不开 → 弹具体原因：文件名 + 字节数 + 可读格式清单，且**不改任何状态**）；
  `routeDroppedFile(file, 屏幕落点)` 成为 OLE 兜底链与 WM_DROPFILES 直连链**共用**的唯一路由（两链原先各写一份规则）；
  `makeContainTransform` 非正尺寸退回 1:1 居中（数学上不可能再炸框）；`onUseMaskAvgColour` 原先解不开静默 return → 改为弹提示；
  每次成功拖入在底部状态行写明目的地（`Image layer #N added: 名 (WxH)` / `Mask image set: …`）。
- **#H 用户建议采纳 → 可见图层栈**：Image 页顶部 `ListBox`（ParamPanel 兼任 `ListBoxModel`），
  按**顶→底**列出上方图片 / Spectrum（带样式名或 `[DELETED]`）/ 下方图片 / Mask image；
  每行附 `[ok | FILE MISSING | NO DECODER]`（后者用 `ImageFileFormat::findImageFormatForFileExtension` **不解码**探测）
  + `scale x…`（异常倍数一眼看穿）；点击行 = 选中该元素，蒙版行 = 进出"编辑图片位置"；内容哈希去抖。
- **测试**：`vis_tabs_test`（新）+ `vis_anchor_test` 情形 11 + `vis_peaks_test` #E 断言；三处**负对照**均精准 FAIL
  （tabs 点名 `Use average` ×4 页；anchor 4 条；peaks 1 条），证明用例真的能抓到这类 bug。
  四套回归 ALL PASS；Linux 全量 0 error；GUI 8s 冒烟存活无断言；Windows 交叉 34/34 → `AudioVisGUI_09111811.exe`。
- ⚠️ **自查两笔**：(a) 上一轮我改 `docs/PLAN.md` 时有一条 `t.replace()` **静默没命中**（脚本无断言仍打印 OK），
  本轮 grep 逐项核验才发现并补上——纪律条款里"改完必查"要包含**按关键词反查落地**，不能只看 diff stat；
  (b) #G 首版用了 `std::erase_if`（C++20）与 `ListBox::setScrollbarAutoHide/updateViewport/listItemClicked` 等
  **不存在的 API**，均由编译期暴露后改正（记入 §7.8）。

**主机侧辅助工具隔离（2026-09-11 18:4x）· 用户要求"别装进 base"**：
- 核查结论：**工程本身零 python 依赖**（`scripts/*.sh` + `CMakeLists.txt` + `cmake/` 无任何 python 调用；
  仓库内无 `.py`；`find -L` 的 5 个都在符号链接指向的 `third_party/JUCE` 内，属上游 CI 脚本）。
- 原先落在 **conda base** 的两个包（按 dist-info mtime 追溯到本项目的两次使用）已卸出：
  `minidump 0.0.24`（09-10 04:46，#1b 崩溃符号化）、`pillow 12.3.0`（09-06 19:21，生成 exe 图标）。
  两者 `Requires:` 均为空 → 零传递依赖，卸载无连带。`~/.local/lib/python3.10/` 本就干净。
- 改由 **venv**（非 conda env，理由：conda 会复制整套解释器+库，几百 MB；venv 只是薄壳。
  需要非 python 二进制才该用 conda —— 参见 `gitenv` 那个 openssl 版 git）承载，
  位置在**仓库外** `~/CodingProgram/AudioVisualizer/tools-venv`（放仓库内会用上万个第三方 `.py`
  污染协作 AI 的 find/grep）。实测 venv 的 `sys.path` 不含 base 的 site-packages（隔离有效）。
- 落库脚本：`tools/setup_env.sh`（幂等；Ubuntu 系统 python 缺 `ensurepip` 时自动回退用 conda python 建）、
  `tools/requirements.txt`、`tools/dmp_report.py`、`tools/gen_icon.py`。
- `dmp_report.py` 固化了此前手工做过两次的符号化链路：解析 dmp（`logging.disable` 抑制库对 PEB 的
  traceback 噪音）→ 异常码/地址 → 模块基址 → RVA → 二分查 `/MAP` → 函数名 + 偏移 + obj；
  并**用链接时间戳硬比对** map 与出事 exe 是否同一次构建（不同就醒目警告，避免采信错位符号）。
  实测四个历史 dump 全部落在 `SpectrumMask::compose`，`ExceptionInformation[0] = 0` = **READ** 违例、
  目标地址 `0x1ee58b89000` 一类野生值（**非 NULL**）→ 从硬证据否掉"Windows 分配失败返回 NULL"的早期猜测，
  与 #1b 的 `tmp[2·y·W+x]` 越界读结论一致。
- `gen_icon.py` 补上"图标只有成品没有源"的缺口（原 PIL 脚本从未入库）。**设计复刻而非逐位复原**：
  均值 新 (70,47,72,244) vs 现存 (76,62,90,229)，差异（右半蓝色更多、边缘更柔）已如实写进脚本头；
  **默认不覆盖** `assets/icon.png`，要换设计才 `--write`。
- 文档：`ARCHITECTURE.md` 新增 §5.9（工具/环境/两个坑/证据补充）+ 速查表加一行 + §7.8 第 1 条补
  「脚本改文件必须二进制/`newline=''`，改完查 `git diff --numstat`」；`.gitignore` 挡住误建在仓库内的 venv。
- ⚠️ 本轮自查又踩一次行尾坑：用 python 文本模式改 `VisTransform.h`/`MainComponent.h`/`SpectrumCanvas.h`
  把 CRLF 转成了 LF（`numstat` 显示整文件重写）→ 按 HEAD 风格还原后 diff 回到 16/3、7/0、0/0。
- **`cmake 4.4.3` 复核后决定保留**，同时纠正我先前"构建完全没用它"的半句错话：
  `~/.bashrc` 有 conda 初始化 + `auto_activate_base: True` → **用户交互式终端里 base 是激活的**，
  `which cmake` = `~/miniconda3/bin/cmake`（4.4.3）；而 AI 工具 shell 非交互、不读 .bashrc，
  `which cmake` = `/usr/bin/cmake`（apt 3.22.1）。也就是说它恰恰是**用户手动构建时真正生效的那个**。
  实测用 4.4.3 配置 + `ninja AudioVisExport` 编译全部通过（36/36、exit 0），且本工程与 JUCE 均声明
  `cmake_minimum_required(VERSION 3.22)` → 不触发 CMake 4 移除 `<3.5` 兼容的坑。故保留不动，
  并把该 PATH 差异写进 `ARCHITECTURE.md` §5.9，提醒后续 AI 别拿自己 shell 的版本下结论。
- `tools/*.py` 增加"缺包自动 execv 到 tools-venv 重跑"（用户终端 `python` = base，已无 minidump/pillow）。
  三个实现坑逐个踩过并修：① `realpath` 比较解释器会**永远判成同一个**（venv 的 `bin/python` 是指向
  seeding 解释器的符号链接）→ 改为比较 `bin` 目录；② `os.execv` 不刷新 stdout → 提示丢失，补 flush；
  ③ 我自己把 import 写成 `from minidump import MinidumpFile`（包顶层不 re-export），
  被 `except ImportError` 吞掉后表现成"venv 里也缺包"的假象，差点误判成卸载失误——
  教训：**宽 except 会把"我用错了 API"伪装成"环境坏了"**，排障时先打印真实异常类型再动手。

**验证记录**：
- Linux + Win 交叉双构建 0 error（`ninja AudioVisGUI AudioVisExport`）。
- `vis_mask_test`：3 断言 ALL PASS（contain 居中 / 漂移=0 / 手柄=渲染）。
- `vis_anchor_test`：28 断言 ALL PASS。
- `vis_peaks_test`（v0.5.4 #3 新增）：16 断言 ALL PASS + 负对照 5 FAIL 有效性验证。
- 18 组合（6 样式×3 colormap）出帧验证；#3 后为 5 样式 + bar-mirror 别名。
- Windows 部署 20 commits 推送到 GitHub main（`5d4b42e..14b5104`），`docs/inbox/` gitignored 不在 remote。

### v0.5.0 — 2026-09-06
**变更（Windows 原生交付 + UIPI 拖放修复 + GUI/样式增强；实施计划见 `docs/PLAN_v0.5.0.md`）**：

- **Windows 原生 exe 交叉编译打通（§5 新章节）**：
  - 工具链：Clang 18（MSVC ABI，`scripts/clang-cl-wrapper.sh` 包 `--target=x86_64-w64-windows-msvc`）
    + lld-link-18 + xwin 拉取的 Windows SDK 10.0.22621 / MSVC CRT 14.44（`~/.xwin-sysroot/`）
  - 新增 `cmake/toolchains/clang-cl-msvc.cmake`（旧 MinGW 版失效原因：缺 d2d1_2/3.h，JUCE 8 Direct2D 无法编译）
  - 新增 `scripts/build_win_cross.sh`（环境检查 / configure / build / 验证 / `--deploy` 到 Windows 测试目录 / `--clean`）
  - CMakeLists.txt：`avx_configure_target` 兼容 Clang-MSVC 前端；CRT 静态链接免 VC_redist（依赖校验仅系统 DLL）
  - **修复 JUCE 源残留**：恢复 `juce_graphics.cpp` 的 `d2d1_3.h` 等 6 处 MinGW 时期补丁（共享 checkout 回到原版 8.0.12）
  - 排错记录：STL1000 版本检查（`_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH`）、大小写符号链接（Dbghelp.h/D2d1.lib 等）、
    SSE intrinsics 不内联（clang resource dir `-isystem` 首位）——三个坑的完整分析在 §5 交叉编译章节
- **UIPI 拖放彻底修复（`source/gui/WinDragCompat.{h,cpp}` 新增）**：
  - 根因链：JUCE 无条件 `RegisterDragDrop` → Explorer 只走 OLE → 提权进程被 UIPI 拦截 → 禁止符不回退
  - 修复：提权进程 `RevokeDragDrop` + `DragAcceptFiles` 切换 WM_DROPFILES 老协议（JUCE 已 `ChangeWindowMessageFilterEx`
    放行该消息，官方允许跨 UIPI）；`SetWindowSubclass` 拦截 WM_DROPFILES → 转 `ComponentPeer::DragInfo`
    → 复用 JUCE `handleDragDrop` 分发链（画布/兜底/图层逻辑零改动）
  - 普通权限下 OLE 全功能保留（含拖放悬停 HUD）；画布红色横幅改为提示"legacy drag-drop active"
- **GUI 参数面板（ParamPanel）**：
  - Colors 行（Primary/Secondary/Peak/**新增 BG**）从 Appearance 末尾上移到区首（原位置 y≈852px 超首屏需滚动）
  - BG 按钮接入 `params.bgColor`（JSON/CLI 原已支持，补 GUI 控件）；透明底显示为半透明灰以免不可见
  - "Peak hold ms" / "Peak decay dB/s" 两滑块从 Time 区上移到 Style 区 Peak caps 开关之后（峰值帽三件套聚合），
    加 tooltip 说明"下落间隔/下落速度"语义；面板新增 TooltipWindow
- **bar-line 样式重构（斜面柱顶，`BarLineStyle.{h,cpp}` 重写）**：
  - 柱体从矩形改为五边形梯形：顶边斜线（柱左缘高 = 相邻带归一化中点插值，右缘同）；同高带 → 水平顶
  - 首柱左缘/末柱右缘用自身值；空带柱不画但边缘插值仍参与（无悬空跳变）；gap 保留（间隙断开，用户确认）
  - 删除旧"柱顶中点连线"与水平描边；峰值帽逻辑不变
  - 像素级验证：扫频音频下 bar-line 顶缘平台占比 26% vs bar 84%（斜面连续 vs 全平顶），柱宽/空隙序列与 bar 逐位一致
- **验证记录**：Linux 双 target 编译零错误 + GUI 冒烟通过；交叉编译全量通过并已部署
  `C:\Users\yiyikneesocks\Desktop\AudioVisExport_test\`（COMCTL32 导入确认，无 CRT DLL 依赖）；
  Windows 侧 UIPI 拖放 / 颜色 / bar-line / 峰值帽实测待用户回归（S6）

**追加（v0.5.0 同日，实测反馈修复）**：
- **拖放实测结论 + 修复升级**：用户实测提权下禁止符消失但 WM_DROPFILES drop 无反应
  → 方案升级为两层：①**首选 = 自动降权重启**（`relaunchDeElevated()`，经 explorer.exe 代理
  重启为 Medium 完整性级别，OLE 拖放完整可用含 HUD；`initialise` 提权时自动触发，旧实例 `quit()`）；
  ②兜底 = WM_DROPFILES relay 保留（降权失败时生效），新增 `onStatus` 诊断回调 → 面板进度文本
  实时显示"WM_DROPFILES arrived / relaying / delivered"每一步（远程排障可见）
- **峰值帽斜面化（BarLineStyle v0.5.1 增强）**：每带形状捕获状态机——峰值未在下落时每帧
  重置帽形状 = 当时柱顶斜率（与柱顶贴合）；下落开始冻结形状、整条斜线随 peakDb 刚性下落。
  像素验证：保持期/下落期 peak 线全部呈斜线段（0 平段）
- **暂停峰值抖动修复（SpectrumCore::setPeaksFrozen）**：根因 = getBandFrame 峰值段是有状态的
  （GUI 暂停时 timer 仍 30Hz 调用 → hold 倒计时/衰减随 wall-clock dt 持续推进 → 峰值帽继续下落）。
  修复 = 冻结标志跳过衰减（保留新峰跟随，seek 快进峰值建立不受影响）；MainComponent 每 tick
  按 `transport.isPlaying()` 设置；离线导出默认不冻结零影响
- **第二轮实测反馈修复（同日）**：
  - **拖放方案再升级**：`installDragCompat` 改为**无条件 DragAcceptFiles**（假设 B：OLE 注册失败时
    Explorer 自动回退 WM_DROPFILES，此前仅提权分支启用是漏洞）；relaunchDeElevated 加
    `--avx-deelevated` 防死循环标志（UAC 最低档环境下降权必然失败，防无限重启）；画布右下角
    常显诊断行 `[drag] elevated= oleRevoked= fallback=on wmDropFiles=N`（不靠滚动找）
  - **bar-line 峰值帽状态机 v2**（修复用户实测"看不到峰值帽"）：v1 两缺陷——帽端点画在柱宽内
    与柱顶描边重叠视觉淹没；保持期每帧重捕获导致下落前冻结的形状≈水平。v2：仅峰值刷新
    （peakDb 上升）时捕获形状；帽 x 伸出 gap 两侧 + 斜率线性外推。像素验证：下落期帽悬停
    柱顶上方 9-11px、斜线保持
  - **音频文件管理（用户需求）**：传输条新增 `[Load...]`（随时换曲）/ `[Eject]`（移除当前音频：
    停止播放 + transport/readerSource/pcm 全清 + 画布回空拖放提示态）；加载后显示
    `♪ 文件名`（左下角 nowPlayingLabel，悬停 helpText 显示完整路径）
  - **拖放 relay 链最终修复（T3/T4 实测驱动）**：T3 证实 explorer.exe 代理**无法传命令行参数**
    （args 里的 --avx-deelevated 被 explorer 当作导航位置解析，打开 Documents 而非程序）→
    改用**环境变量 AVX_DEELEVATED** 传标志（ShellExecuteW 子进程继承环境块）；T4 证实
    WM_DROPFILES 到达子类（wmDropFiles+1）但 JUCE `handleDragMove/findDragAndDropTarget`
    分发链跨协议 relay 不可靠（静默吞 drop）→ relayDrop 改为**直连
    onNativeFilesDropped 回调**（同消息线程确定性交付：图片→图层 / 音频→loadFile / 其他→提示），
    JUCE 链降为后备。T2 对照同时证实：非管理员禁止符已消失（无条件 DragAcceptFiles 生效，
    Explorer 全程走 WM_DROPFILES —— 即 OLE RegisterDragDrop 在该环境静默失败，与杀软无关）
- **T2/T3/T4 最终闭环（2026-09-07）**：T3 降权重启修复验证通过（Documents 误开根因 =
  explorer.exe 代理无法传命令行参数，改环境变量 AVX_DEELEVATED）；T4 wmDropFiles+1 定位
  JUCE 分发链断点 → 直连交付修复，拖放全场景可用。杀软对照结论：开关无差异，与杀软无关。
  git push 链路（gitenv openssl git + HTTP/1.1 + PAT）已固化，见本文档页首「发版流程」与 `ARCHITECTURE.md` §7.5 速查。

**追加（v0.5.1 — 2026-09-07，图片图层系统重构）**：
- **图片元素 = 自身宽高比，不再强制拉伸铺满**（用户反馈：拉伸框跟画布不跟图片、拉伸困难）：
  - 图片元素基础矩形 = 图片自然尺寸 (imW, imH)；导出 `drawImageLayer` 与 GUI `paintImageLayer`
    完全同源（原生尺寸绘制；v0.5.0 之前导出拉伸/GUI 原始尺寸互相矛盾）
  - `VisTransform` 新增**附加平移分量 posX/posY**（默认 0，频谱与旧 JSON 行为完全兼容）；
    `buildVisAffine` 末尾平移叠加 pos
  - 新增 `makeContainTransform()`：新图片 / 图片首次交互 / 双击复位 = 等比 contain 居中
    （scale=min(outW/imW, outH/imH)，枢轴=图片中心，pos=画布中心−s·图片中心）
  - 手柄框 / 命中测试 / 拖拽中心 / 单轴拉伸 half 全部改用**元素尺寸**（频谱=画布，图片=imW,imH）：
    框永远贴图片实际边缘；paintOverlay 中心改四角平均（修复选中图片时旋转柄方向错误）
  - Move 拖拽统一走 posX/posY（频谱初始 0 行为等价）；JSON（顶层 transform + images[]）
    与 `--set transform.posX/posY` 三处贯通
- **图层上下彻底可控**（用户反馈：图片永远盖住频谱）：
  - 根因：GUI 预览无视 aboveSpectrum 全画在频谱上层（导出管线分组正确但 GUI 不同步）
  - GUI `paintImages` 拆分组渲染：下方组(aboveSpectrum=false) → 频谱 → 上方组(true)，与导出一致
  - ParamPanel Layers 区新增 **"Above spec"** 开关（onReadLayerAbove/onWriteLayerAbove 回调，
    写 `aboveSpectrum`）；`refreshLayerControls` 在画布选中元素变化时同步开关与透明度
  - `aboveSpectrum` 默认 false = 默认在频谱下方；图片间 z 序仍用 [Up]/[Down]
  - **像素级验证**：400×600 竖图 contain 到 960×540 → bbox (300,0) 359×539 宽高比 0.666 等比精确；
    below 模式图片区频谱透出 574px / above 模式 0px，分组正确

### v0.4.2 — 2026-09-05
**变更（图片图层系统 + bar-line 样式 + 拖放诊断 + 峰值帽开关）**：

- **外部拖放问题定性（UIPI 提权隔离，非程序 bug）**：
  - 深入排查结论：代码侧 OLE 注册（`RegisterDragDrop` 无条件调用）与目标命中逻辑均正常；
    禁止符的真因是 **Windows UIPI**——以管理员身份运行的进程（GUI 主进程完整性级别 High）
    **收不到普通权限 Explorer（Medium）发起的 OLE 拖放**，整条拖放消息链被内核静默拦截，
    表现恰好是全程禁止符（`ChangeWindowMessageFilterEx` 只放行指定消息，OLE 拖放链不在放行之列）。
  - **程序内新增两个诊断装置**（`SpectrumCanvas`）：
    - 启动时检测提权状态（`CheckTokenMembership`/`IsUserAnAdmin` 路径）：若进程为 Elevated，
      画布顶部显示**红色警告横幅**"Running as Administrator — file drag-drop from Explorer is blocked by Windows (UIPI)"
    - 拖放悬停 HUD：拖文件进入窗口时画布显示"Drag detected: <文件名>"——
      若能看到 HUD 说明消息链已通（此时禁止符不存在）；看不到 HUD + 禁止符 = UIPI 拦截
  - **用户自查步骤**：任务管理器 → 详细信息 → 添加"Elevated"列 → 看 AudioVisGUI.exe 是否 Elevated；
    或直接看画布是否出现红色横幅。**解决：不要以管理员身份运行 GUI**（也不要从提权终端 start 它）
- **图片图层系统（第一阶段：静态图片）**：
  - `SpectrumParams::ImageLayer`（namespace 级结构，`images` 数组成员）：path / centerX / centerY /
    scaleX / scaleY / rotationDeg / opacity / aboveSpectrum / visible，复用 `VisTransform`
  - JSON 持久化（`images: [ {...} ]`，像素坐标空间与频谱变换一致）；`--set` 暂不支持数组元素
  - **VisPipeline 合成**：belowSpectrum 组 → 频谱 → aboveSpectrum 组，图片默认铺满输出画布再套
    `VisTransform`（与频谱同基准），导出与预览共用同一合成函数（所见即所得）
  - **画布交互**：拖图片文件进画布即创建图层（默认铺满）；点击切换选中（频谱/图片）；
    选中图片后与频谱共用同一套 8 角柄/旋转柄/移动/双击复位交互；按图层 z 序做命中测试
  - **面板新增 Layers 区**：[Add image]（文件选择）/ [Up] / [Down] / [Remove] / Opacity 滑块
  - CLI `--preview-frame` 与导出全链路验证：img_on/img_off 对照字节不同 ✅
- **新增样式 `bar-line`**（柱体 + 柱顶直线连接）：
  - `styles/BarLineStyle.{h,cpp}`：逐柱画柱体（沿用 bar 的 gap/width 布局参数），
    相邻柱顶点用直线段连接（可选择是否带填充），峰值帽同样受 barParticles 开关控制
  - 工厂注册 + CMake + 面板下拉第 5 项；CLI 渲染验证 ✅
- **`barParticles` 参数（柱样式"粒子"开关，默认开）**：
  - 用户所称"粒子"实为**峰值帽**（peak caps，随峰值缓慢下落的小横线），命名沿用引擎术语
  - `visual.barParticles`：JSON 写/读/CLI `--set` 三处贯通；GUI Appearance 区新增
    **"Peak caps"** 开关；BarStyle 与 BarLineStyle 双双接入；开/关对照渲染字节不同 ✅
- **颜色选择器确认未丢失**：Primary/Secondary/Peak 三色按钮 + 取色器一直在 Appearance 区
  （面板变长后需要**滚动**才能看到）；在 GUI 指南中注明位置
- **修复**：`SpectrumCanvas::fileDragExit` 签名对齐 JUCE 基类（`const StringArray&`）；
  `SpectrumParams::ImageLayer` 限定名统一；`isProcessElevated` 补 `<windows.h>`；
  ParamPanel 重复的 `addHeader("Export")` 去重；MainComponent `buildRp` 补接 `rp.barParticles`
  （否则 GUI 预览峰值帽永远关闭、与导出不一致）
- **CLI UTF-8 修复 + 版本号同步**：
  - 根因：`juce::String(const char*)` 按 ASCII 处理 UTF-8 中文，多字节被逐字节重编码成双重编码乱码
    （Release 下 jassert 被禁用无提示）；正确做法是用 `CharPointer_UTF8` 包装
  - `source/cli/CliArgs.cpp` helpText() 改用 `juce::String(juce::CharPointer_UTF8(...))`；
    版本号从 `v0.2.0` → `v0.4.2`（与文档一致）
  - `CMakeLists.txt`：`project(VERSION)` 与两个 target 的 `VERSION` 全部从 `0.2.0`/`0.3.0` 同步到 `0.4.2`
  - **已重新交付** Windows 测试包（`C:\Users\yiyikneesocks\Desktop\AudioVisExport_test\` 内二进制更新时间戳）
 - **验证记录**：双 target 构建零错误；CLI 四组对照（bar-line / 图层 on-off / 峰值帽 on-off）
   全部通过；GUI 启动冒烟通过；CLI `--help` 中文版本号显示正确（UTF-8 修复验证）

**追加（v0.5.2 — 2026-09-07，统一图层模型 + 对边锚定缩放 + 吸附）**：

- **P1 统一图层模型——频谱成为真正的层**（用户反馈：加图片后频谱无法拖放/无法删除）：
  - "无法拖放"根因：`mouseDown` 图片命中优先于频谱判定，任何覆盖点击点的图片抢走选中。
    修复 = **统一 z 序拾取**：上方图片组（顶→底）→ 频谱 → 下方图片组，首个命中者胜出
  - 新参数：`spectrumPresent`（false = 频谱真删除，渲染/命中/导出全部跳过，音乐不受影响）、
    `spectrumIndex`（统一 z 序中频谱之下的图片数；images[0..k) 在频谱下，images[k..N) 在上）、
    `snapEnabled`（吸附开关，默认开）
  - `aboveSpectrum` 变为**派生值**（i >= spectrumIndex）；旧 JSON 读取时从标志推导 index，
    新 JSON 两者都写（向后兼容）；per-image flag 仍在结构体中仅作缓存
  - `moveSelectedLayer` 重写为统一 z 序：概念位置 = 图片 i<k?i:i+1 / 频谱 k；Up/Down 对
    频谱与图片一视同仁（跨边界移动自动调整 spectrumIndex）；Above spec 开关等效跨边界移动
  - 新图片默认插在频谱下方（insert at spectrumIndex 然后 ++index）
  - **删除/恢复**：[Remove] 对选中元素通吃（图片=erase；频谱=置 spectrumPresent=false）；
    键盘 **Delete/Backspace** = 等效点 [Remove]（画布 mouseDown grabKeyboardFocus +
    keyPressed → onDeleteRequested 回调）；面板新增 [Add spectrum]（频谱在场时禁用，
    恢复默认铺满变换）与 [Select spectrum]（频谱被盖住时选中它）按钮
  - ParamPanel `refreshLayerControls` 扩展：Add/Select spectrum 按钮可用性随 spectrumPresent
    同步；Remove 按钮在频谱在场时也可用
- **P2 对边锚定缩放**（用户反馈：拖角中心不动反直觉，应"拖左下角=右上角不动"）：
  - `VisTransform.h` 新增纯函数 `applyAnchorScaled(startT, anchorElem, draggedElem, outPoint, axis)`
    + `enum VisScaleAxis { Both, OnlyX, OnlyY }`：角柄=对角锚定等比；边柄=对边中点锚定单轴
  - 数学：新 scale = 锚距比 f；锚定补偿 pos = start.pos + anchorOut − (c + s′·R·(a−c))，
    使锚点输出位置恒定（旋转保持）；枢轴 = startT.centerX/centerY（无需外部传尺寸）
  - `mouseDown` 记录 `dragAnchorElem` / `dragHandleElem`（元素坐标）；mouseDrag 调纯函数
  - 独立断言测试 `scripts/vis_anchor_test.cpp`（CMake target `vis_anchor_test`，EXCLUDE_FROM_ALL）：
    11 项断言全过（contain 基线 / 锚点固定 / 距离比 / 被拖点跟手 / 旋转 30° 锚定 / 单轴仅 Y 变）
- **P3 吸附系统**（`snapEnabled` 默认开 + 面板 "Snapping" 开关 + JSON + `--set spectrum.snapEnabled`）：
  - 旋转吸附：结果角接近 90°×n（±3°）自动校正
  - 移动吸附：被拖元素 AABB 的中心/四边 接近 其他元素 AABB 中心/边、画布中心/边缘
    （≤8 屏幕像素）→ 自动校正；X/Y 独立取最近候选
  - `--set spectrum.present / spectrum.index / spectrum.snapEnabled` CLI 覆盖三连
- **验证记录**：Linux + Win 交叉双构建零错误；CLI 像素级五组对照全过——
  (1) spectrumPresent=false：全画布无频谱像素（19200 红像素完整保留）
  (2) k=0 图片在上：图片区纯红（频谱被盖）
  (3) k=1 图片在下：图片区频谱透出 121px
  (4) 旧 JSON（仅 aboveSpectrum 标志）与 k=1 渲染逐像素一致（兼容层验证）
  (5) 锚定数学 11 断言 ALL PASS；其余 z 序/键盘/恢复按钮为 GUI 手测项

**追加（v0.5.3 — 2026-09-08，图层护栏清理 + 画布体验修复 + 锚定缩放数学修正 + 峰值帽二阶下落）**：

- **B1 图层移动跨层护栏移除**：moveSelectedLayer 不再限制频谱/图片的跨边界移动路径
  （v0.5.2 统一 z 序后护栏已无必要；z 序仍由 spectrumIndex 派生规则保证）
- **B2 画布键盘焦点 + 每 tick 面板刷新**：
  - SpectrumCanvas `setWantsKeyboardFocus(true)`：画布可持焦，Delete/Backspace 路径稳定
  - MainComponent `keyPressed` 兜底：画布未持焦时 Delete/Backspace 同样触发 removeSelectedLayer
    （画布自处理时返回 true，不双触发；文本输入控件自消费按键，无冲突）
  - 移除 `layerSelCache` 守卫 → `refreshLayerControls` 每 tick 调用：Remove / Add spectrum
    按钮可用性实时跟随"选中图片或频谱在场"（修复"没图时删不掉频谱"）
- **B3 锚定缩放数学修正（旋转 + 单轴组合）**：
  - `applyAnchorScaled` 新增第 5 参 `grabOut`（被拖点**起始**输出位置）；
    锚定补偿由 R·S·a 修正为 **S·(R·a)**——与 buildVisAffine 的 `translation().rotated().scaled()`
    复合序一致（`c + pos + S·R·(p−c)`）；scaleX==scaleY 或旋转 0 时公式等价（旧行为不变）
- **B4 旋转柄命中仅限圆柄圆心**（`kRotHitRadiusPx = 14`）：修复"上边中点被旋转抢走"；
  绘制圆柄直径同步 10px → 14px，视觉与命中区一致
- **B5 吸附作用域限定 `draggingImage`**：频谱拖动不再触发画布/边缘吸附
- **F1 峰值帽二阶下落**：新参 `peakDecayAccelDbPerSec2`（0 = 旧一阶行为；>0 峰值下落随时间加速），
  Params/Core/ParamPanel/JSON 全链路 + `--set time.peakDecayAccelDbPerSec2` CLI 覆盖
- **测试**：`scripts/vis_anchor_test.cpp` 扩至 16 项断言（新增 旋转30°+单轴锚定补偿 3 项、
  grab 基线 2 项）ALL PASS；修正迁移期 4 处以鼠标当前位置冒充 grabOut 的旧断言
  （正确语义 = 被拖点起始输出位置）
- **验证记录**：Linux 全量构建零错误；锚定测试 16/16 ALL PASS；
  B1–B5 / F1 GUI 手测项见交付清单
- **进度状态（2026-09-09，已发版）**：
  - ✅ **v0.5.3 已发版**：tag `v0.5.3`（指向本次发版 commit）；`origin/main` 已推；GitHub Release 正文取
    `docs/RELEASE_NOTES.md` v0.5.3 节。
  - ✅ **提交构成**：`688564a`（B1–B5 + F1 + 真·B3 + B6 平行四边形修复 + Above spectrum UI 移除 +
    版本号 0.5.3）→ `07acd2e`（N1 自吸附 + N2 CAD 辅助线 / 9 特征点对齐 + N3 空格 + 范围内外视觉 +
    空格焦点修复）→ `52c8603`（参考优先原则入档 + PLAN 状态同步）→ 本次发版 docs commit。
  - ✅ **代码全部完成**：B1–B5 + F1 + B3（真因）+ B6 + N1–N4；`vis_anchor_test` 扩至 **28 项断言 ALL PASS**
    （含旋转+非等比正交性、pos≠0 锚点、θ=0 R·S≡S·R 等价）。Linux 全量构建零错误。
  - ✅ **Windows 交叉编译部署完成**：产物部署 `C:\Users\yiyikneesocks\Desktop\AudioVisExport_test\`
    （AudioVisGUI.exe / AudioVisExport.exe），CLI 版本串 "AudioVisExport v0.5.3"。
  - ✅ **用户 GUI 手测通过**：锚定缩放（对角钉死 / 旋转后方正）、吸附辅助线 / 边边对齐、范围内外视觉、
    空格播放/暂停（焦点修复后）、频谱拖动无自我吸附、删除按钮兜底等主功能均已确认。
  - ⏳ **发版后遗留**：导出侧范围内裁剪回归（用户暂未复测，导出仅未改 `VisPipeline`，理论不受影响）；
    缩放态吸附辅助线（P2，未做）。

**追加（v0.5.3 续 — 2026-09-09，B3 真因复核 + B6 旋转拉伸平行四边形 + Above spectrum UI 移除）**：

> 说明：v0.5.3 自始未 commit / 未 tag（最后发版仍是 v0.5.2 `0a92842`）。下列改动与
> 上面 v0.5.3 主体块一并纳入同一未发版工作区，本条为续。

- **B3 真因复核（订正上文 B3 描述）**：上条把 B3 记为"补偿由 R·S·a 改为 S·(R·a)"——
  经复核该改动对 `pos≠0` 漂移**无效**（v0.5.2 起本就是 S·R·a，属空操作）。真正根因是
  `applyAnchorScaled` 把 `startT.pos` 多算一次：`pos = startT.pos + (anchorOut − anchorNew)`
  → 最终锚点 = `anchorOut + startT.pos`，故 pos≠0 时"钉死的对角"瞬移 |pos|（被 pos=0 测试用例掩盖）。
  - **修复**：`r.posX = anchorOut.getX() - anchorNewX`（去掉 `startT.pos +`）。
  - 测试新增**情形 7/8**（pos=40,10 锚点固定 + f=1 时 pos 不变），旧代码漂移 41px FAIL，修复后 PASS。
- **B6 旋转后拉伸变平行四边形**：`buildVisAffine` 原合成序 **S·R**（先旋转后缩放）→ 缩放作用在
  **画布轴**；当旋转 θ≠0 且 scaleX≠scaleY 时，正方形四角不再垂直（相邻边点积 =(Sy²−Sx²)·sinθ·cosθ≠0），
  渲染成平行四边形。
  - **修复**：`buildVisAffine` 改合成序为 **R·S**（先沿元素本地轴缩放，再旋转）→ 恒保直角；
    `applyAnchorScaled` 锚点补偿公式同步改 `c + R·S₁·(a−c)`；边缘中点拖拽的缩放系数 f 由"画布距离比"
    改为"位移在本地轴方向上的**投影比**"（角拖 Both 的距离比在任意旋转下已正确，未改）。
  - 测试新增**情形 9**（旋转37°+非等比，拖角/拖边后**四角正交性断言** + 锚点不动 + 被拖点跟鼠标）、
    **情形 10**（θ=0 时 R·S 与旧 S·R 逐点等价 → 频谱/导出零回归）。共 28/28 ALL PASS。
- **Above spectrum UI 按钮移除**：删 `ParamPanel` 的 "Above spectrum" ToggleButton +
  `onReadLayerAbove`/`onWriteLayerAbove` 回调 + `MainComponent` 对应接线（4 处）；
  `refreshLayerControls` 签名去掉 `above` 形参。**底层 `aboveSpectrum` 标志、JSON 兼容、
  跨层 Up/Down 逻辑（B1）全部保留**——层级仍由数组顺序 + spectrumIndex 派生。
- **验证记录**：Linux 主程序 + GUI 全量构建零错误；`vis_anchor_test` 28/28 ALL PASS；
  Windows 交叉编译 32/32 部署至 `AudioVisExport_test\`（CLI 版本串 v0.5.3）。
- **发现待办（未修，见 PLAN.md）**：仅频谱在场拖动频谱时"手感卡顿"——
  `mouseDrag` 的 Move 吸附块把**被拖频谱自身的实时 corners** 当作吸附候选
  （`SpectrumCanvas.cpp:556` 条件 `selectedImage < 0` 恰为"自身"），导致自我吸附抖动。
  计划连同"吸附辅助线/对齐点提示（CAD 风格）"一起在下一版处理。


**追加（v0.4.2 同日）— Windows → WSL Ubuntu 22.04 迁移（仅文档/环境，无功能代码变更）**：
- **工程迁移**：`d:\Study 'n' Work\Program\AudioVisExport` → `~/CodingProgram/AudioVisualizer/AudioVisExport`；
  Y2Kmeter 现位于同级 `~/CodingProgram/AudioVisualizer/Y2Kmeter`
- **文档路径修正**：
  - `ARCHITECTURE.md`：§2.1 Y2Kmeter 参考路径、§3 产物路径（Linux 无 `.exe`）、
    §4.5.7 启动命令、§5 构建章节整体重写为 bash + Ninja（含 WSL 依赖一键安装命令 + 本地 JUCE 说明）、
    §6 手动 ffmpeg 命令代码块、示例输出路径 `D:\...` → `<dir>`
  - `docs/GUI_GUIDE.md`：§1 构建与启动重写为 WSL/Linux（apt 依赖、Ninja 构建、WSLg 说明）、
    §3.6 UIPI 节标注"仅 Windows"、FAQ Q1 的 FFMPEG_PATH 说明按平台区分
- **third_party/JUCE 符号链接重建** → `../../Y2Kmeter/third_party/JUCE`
  （复用 Y2Kmeter 的 JUCE 8.0.12 本地 checkout；构建需 `-DAVX_USE_LOCAL_JUCE=ON`，见 §5/FAQ）
- **Windows MSVC 旧 `build/` 已删除**（`.sln/.vcxproj` 与指向 `d:/` 的 CMakeCache 对 Linux 无效），Linux 下重新 configure
- **代码跨平台核查结论（无需改动）**：
  - `SpectrumCanvas::isProcessElevated` 与提权横幅已有 `#if JUCE_WINDOWS` 守卫——非 Windows 恒 false，横幅不显示
  - `PngSequenceEncoder::findFfmpeg_` 的 PATH 分隔符（`;` vs `:`）、可执行名（`ffmpeg.exe` vs `ffmpeg`）、
    `.exe` 后缀补全逻辑全部已分平台处理，Linux 下开箱即用
- **环境补齐（WSL 侧，一次性）**：apt 安装 cmake / ninja-build / ffmpeg / pkg-config +
  JUCE Linux 依赖（libasound2-dev、libx11-dev、libxext-dev、libxrandr-dev、libxinerama-dev、
  libxcursor-dev、libxcomposite-dev、libxrender-dev、libfreetype6-dev、libfontconfig1-dev、
  libgl1-mesa-dev），完整命令见 §5
- **运行时形态**：GUI 显示走 WSLg（`DISPLAY=:0` / `WAYLAND_DISPLAY=wayland-0`），
  音频走 WSLg PulseAudio；ffmpeg 用 apt 版（Windows 期 gyan.dev 下载包的说法仅适用于旧环境）
- **验证记录（2026-09-05 WSL 构建，Ninja + 本地 JUCE 8.0.12）**：
  - 双 target 编译链接零错误（⚠️ 实测 12 核 / 7.7GB 内存下 `-j$(nproc)` 会在 JUCE 大编译单元上
    OOM（`cc1plus Killed`），须 `-j2` 续编；已写入 §5 构建命令备查）
  - CLI 冒烟全过：`--gen-tone`（44.1k 立体声 5s 1kHz -6dBFS）→ `--probe-pcm` →
    `--probe-spectrum` → `--preview-frame`（crystal / bar-line 两样式出图）→
    `--export` png-seq（480x270@15，75 帧 / 0.9s）
  - `--export --encoder mov-qtrle` 全链路：Linux PATH 自动找到 apt ffmpeg 4.4.2，
    产物 ffprobe 实测 `codec=qtrle / pix_fmt=argb`（alpha 无损保留，与 Windows 期结论一致）
  - GUI 冒烟：WSLg 下启动运行正常（timeout 8s 退出码 124 = 全程存活；
    `ALSA seq` 提示为 WSL 无 MIDI 设备，无害）
  - **已交付 Windows 测试包**：`C:\Users\yiyikneesocks\Desktop\AudioVisExport_test\`
    （AudioVisExport / AudioVisGUI 二进制 + test_tone.wav + sample_mov/test_tone_vis.mov
    + Run_AudioVisGUI.bat + README_测试说明.txt；bat 经 `wsl -e` 拉起，WSLg 显示窗口）

### v0.4.1 — 2026-09-05
**变更（频谱元素自由变换 + 输出分辨率所见即所得）**：
- **新增核心变换类型 `VisTransform`（`source/core/VisTransform.h`）**：
  - 坐标空间 = 输出分辨率像素；`set/centerX/centerY/scaleX/scaleY/rotationDeg`
  - `buildVisAffine()`：绕中心（平移→旋转→缩放→平移回中心），GUI 与导出共用同一仿射；
    `visCorners() / visContains() / visDistanceToSegment()` 供画布命中测试与手柄绘制，
    后续图片/视频图层可直接复用
- **`SpectrumParams` 新增 `transform` 成员**：JSON 序列化/解析（`transform.*` 键）+
  CLI `--set transform.centerX=640` 等覆盖
- **`VisPipeline::renderFrame` 改为两段式合成**：
  1) 基础层：频谱按输出分辨率渲染到透明 ARGB（不带变换）
  2) 合成层：棋盘/半透明 bg + `buildVisAffine(transform)` 叠加 → **导出应用与预览相同的变换**，所见即所得
- **`SpectrumCanvas` 全重写为"元素式"交互画布**：
  - 基础层固定渲染到 `params.width×params.height`，画布显示区按 letterbox 居中适配
  - 鼠标交互：拖动主体 = 移动；四角手柄 = 等比缩放；四边手柄 = 单轴拉伸（非等比）；
    顶部青色圆柄 = 绕中心自由旋转；双击元素 = 复位铺满画布
  - 画布坐标 ↔ 输出坐标经 `displayAffine()`（逆变换）互换，全部命中测试在输出坐标系完成
  - 悬停光标区分：移动/角缩放/边拉伸/旋转/默认
- **GUI 新增 "Reset element transform" 按钮**（Style 组，全宽按钮行 `addButton` 布局支持）
- **文档同步**：§4.5.6 渲染管线（两段式）、§10 参数表（transform.* 6 项）、
  §11 控件映射（Reset 按钮）、`docs/GUI_GUIDE.md`（第 4 步"摆放频谱元素自由变换"）
- **已知限制**：变换是"整元素仿射"，网格与坐标轴文字随元素一起旋转/拉伸（符合所见即所得）；
  改变 W×H 后元素保持绝对像素位置，双击复位即可回正

### v0.4.0 — 2026-09-03
**变更（GUI 视频导出 + 编码器实测校准）**：
- **GUI 新增 "Export Video" 一键按钮**（ParamPanel Export 区，玫红色）：
  - 默认把 `params.encoder` 切到 `MovQtrle`（用户已在 Encoder 下拉选了则尊重）
  - 自动生成输出文件名 `<导出目录>/<音频名>_vis.mov`（无需手填 `outputVideoPath`）
  - 目录未设时先弹目录选择，选完回调继续导出（`exportKindPending` 标志）
  - 导出期间与普通 Export 按钮一起禁用；完成消息显示 `Done: video -> <路径>`
- **PngSequenceEncoder 编码器真正实现**（修复原 buildFfmpegArgs_ 写死 libx264→mp4 的遗留问题）：
  - `Config` 新增 `encoder` 字段；`buildFfmpegArgs_` 按 encoder 分支：
    - MOV：`qtrle + rgba`（**实测输出 pix_fmt=argb，无损 alpha**），音轨 `pcm_s16le`
    - WebM：`libvpx-vp9 + crf 32 + b:v 0 + row-mt 1`（**实测不保留 alpha**，定位为小体积预览片），音轨 `libopus`
    - PngSeq：跳过 ffmpeg（finalizeAndMux 提前返回）
  - `finalizeAndMux` 增加空 `outputVideoPath` / 空 args 的防御性校验
- **重要实测结论（写入本变更日志备查）**：`libvpx-vp9` 官方像素格式表声明支持 `yuva420p`，
  但 gyan.dev 2026-01 ffmpeg 实测编码后 ffprobe 显示 `yuv420p`，alpha 被静默丢弃
  （加 `-auto-alt-ref 0` 亦无效）。因此透明视频导出以 **MOV QTRLE** 为准。
- **VisPipeline::run**：视频模式自动补全 `outputVideoPath`（`<outputDir>/<音频名>_vis.<mov|webm>`），结果键 `mp4_path/mp4_status` → `video_path/video_status`
- **GUI Encoder 下拉**：调整为 `png-seq` / `mov-qtrle (alpha)` / `webm-vp9 (no alpha)`，避免误导
- **新增 `docs/GUI_GUIDE.md`**：中文 GUI 使用说明（构建 / 布局 / 5 步上手 / 格式选择 / FAQ）
- **待办勾销**：§7.2 编码器条目按实测结果校准（MOV alpha 完成；WebM alpha 记录为不可行）
- **注意**：需要系统 `ffmpeg.exe`（hint / `FFMPEG_PATH` / PATH 任一命中）；推荐 gyan.dev 构建

**追加（v0.4.0 同日）— 外部拖放修复 + Bar 样式参数**：
- **外部文件拖放修复**：`MainComponent` 原先未实现 `FileDragAndDropTarget`，窗口内唯一的拖放目标
  是 `SpectrumCanvas` 且 `isInterestedInFileDrag` 只对 `.wav/.aif/.aiff` 返回 true——
  拖非音频文件时 `ComponentPeer::handleDragMove` 沿父链找不到任何目标，
  全程 `DROPEFFECT_NONE`（禁止符），表现为"外部文件拖不进来"。修复：
  - `SpectrumCanvas::isInterestedInFileDrag` 改为接受任意文件；非音频走新回调
    `onNonAudioDropped` → `MainComponent` 弹警告框
  - `MainComponent` 补实现 `FileDragAndDropTarget` 兜底（画布外的面板/按钮上方也能接住拖放）
- **Bar 样式新增两参数**（全管线贯通：SpectrumParams → RenderParams →
  VisPipeline::buildRenderParams / GUI buildRp → BarStyle 布局）：
  - `barGapRatio`（0..1，默认 0.28）：柱间空隙占每带 slot 宽度的比例
  - `barWidthRatio`（0.05..2，默认 1.0）：柱宽占 (slot - gap) 的比例，>1 时相邻柱重叠
  - 柱改为在 slot 内**居中**；默认值组合与旧版（bar=0.72*slot）视觉完全一致
  - JSON 键 `visual.barGapRatio` / `visual.barWidthRatio`；CLI `--set visual.barGapRatio=0.5`
  - GUI 新滑块 **"Bar gap %"**（0..100）与 **"Bar width %"**（5..200），位于 Band count 下方

### v0.3.2 — 2026-09-02
**变更（文档重写，纯 .md 变更，无代码修改）**：
- 完全重写 **§4.5 GUI 章节**（v0.3 新增的 GUI skeleton 章节过于粗），拆成 8 个子节：
  - 4.5.1 源文件 ↔ 类 ↔ 关键成员 映射表（5 个源文件，精确字段名）
  - 4.5.2 MainComponent 成员字段速查表（6 大类 × 字段/类型/行为，全部源自 .h/.cpp）
  - 4.5.3 `timerCallback @30Hz` 主循环伪代码 + `currentTargetFrame` / `rebuildCoreLight` / `advanceCoreTo` 子函数行为
  - 4.5.4 ParamPanel ↔ MainComponent 5 条回调通道 + 2 种特殊行布局约定
  - 4.5.5 后台导出线程（`exporting/exportPct/exportDone` 原子变量 + `exportMsgMtx` + UI tick 轮询 join）
  - 4.5.6 SpectrumCanvas 渲染管线逐行（ARGB Image + checkerboard only-in-GUI）+ 拖放扩展名过滤
  - 4.5.7 基于当前状态的 GUI 快速上手指南（5 步 + 窗口分区说明）
  - 4.5.8 当前实现 10 条已知限制（L1-L10），附扩展建议，避免后续 AI 误报为 bug
- §5 "GUI 使用" 章节替换为英文步骤 + Checkerboard toggle 行为说明
- 顶部版本映射表新增 v0.3.2（本次）；末尾协作声明保留。
- **协作注意**：本版本是纯文档修订，未涉及 `source/`、`CMakeLists.txt` 等代码文件，因此不触发 GUI target 重新编译。

### v0.3.1 — 2026-09-02
**变更**：
- GUI 文案全部改为英文（避免非 Unicode locale 下的乱码），全局 LookAndFeel 固定字体为 "Segoe UI"
- 新增 §1-已实现功能 / §10-完整可调参数表 / §11-GUI 控件映射 / §12-变更日志 四节文档
- CMake: `juce_add_gui_app()` target 正确链接 `juce_gui_extra`（ColourSelector）
- 所有 `source/gui/` 成员按钮、标签文字与实际 addToggle/addCombo 参数一致

### v0.3.0 — 2026-09-02（首次提交，c495faf → 9d619f5）
**变更**：
- Engine: SpectrumCore (双路 FFT)、SpectrumStyle (y2k-line/bar/polyline/crystal)、SpectrumParams JSON+Override、VisPipeline、PcmSource、PngSequenceEncoder
- CLI: AudioVisExport (`--export` / `--preview-frame` / `--probe-spectrum` / etc.)
- GUI skeleton: AudioVisGUI (drag-drop, transport, ParamPanel, export thread)
- 6 组参数对比视频（10s）+ 4 种 style 10s 对比视频 生成于 `build/compare_10s/`（不入 git）

---

*维护者：AudioVisExport 项目（GPL-3.0） · 本文档随发版更新（流程见页首）*
