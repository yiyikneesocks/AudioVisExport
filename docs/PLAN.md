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
> `docs/inbox/INBOX.md`=**文档1 用户输入箱**（用户写；**AI 读/删只能经唯一入口 `tools/inbox.py`**（新4：`read` 先读全文+状态；`prune --expect "编号=读到的原文片段"` = 重读盘→状态1拒删→逐条核对原文匹配才删、用户改过的自动保留→删前自动 `.bak` 备份→回显删了哪几号+删后全文）。把"读+删"绑成**不可分割的一步**，杜绝先删后读/并行改误删；**禁止** `write` 整写或手 `edit` 删编号行）；
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
> **写入安全（2026-09-11 事故后新增，强制）**：因 `docs/inbox/` 不入 git、**没有任何历史副本**，改三件套前必须
> ① 先整体备份（`cp docs/inbox/*.md /tmp/<tag>/`）；② **禁止用「读全文 → 切片重组 → 整写」的方式重写 REPLY/WORKLOG 的归档尾部**——
> 新轮次一律**只在其顶部插入**、旧内容原样保留；③ 用户原话必须**逐字**备份（不得摘要，摘要是丢信息的主因）；
> ④ 改完立即用 `grep -c ''` 对比行数，确认未意外缩水，并在汇报中如实说明任何截断/丢内容事故。
> **任务排序规则（用户 #10）**：加急(`!!`) > 无争议/实现简单/不需要用户手动验证就能自证的 > 需用户实测确认的 > 需拍板的设计项（列计划）。被"缺用户测试结果"卡住的跳过不阻塞。

---

## 当前状态（最后更新：2026-09-12 03:1x）

- **⏸ 用户 2026-09-12 口述：三条记账、暂不动工**（计划见 `docs/PLAN_v0.5.6.md`，原话留档见 WORKLOG"已记录·暂不动工"）：
  ① 暂时**禁用 Ctrl+Z**（bug 多；顺带发现 `addSpectrumLayer` 双快照 bug）；
  ② **大幅优化实时渲染**（性能 + Mask 页控件"乱"）；
  ③ **多选系统**（Ctrl+A 全选 / Ctrl+点击多选 / 批量删除 / 一起移动，**禁用一起旋转拉伸**）。
  → 本轮**未改任何源码**，仅出计划。

- **v0.5.3 已完整发版**（2026-09-09，tag `v0.5.3` + GitHub Release 已发）。
- **v0.5.4 ✅ 已发版（2026-09-12，tag `v0.5.4` + GitHub Release）**：
  主体 `5d4b42e..14b5104`（20 commits）+ INBOX 反馈迭代
  `f45eaa5`（五连 #1/#2/#7/#8/#9）→ `c755f73`（#10 y2k 两臂颜色）→ `5877024`（#1b outline 崩溃真根因）
  → `a7ac78b`/`22a966f`/`856f228`（文档同步 + 纪律强化）→ `ae6d73c`（#3 peak-caps 四件套）
  → `6e3a235`（#E/#G/#H + #F 报告）→ `1913afa`/`a811c99`/`242de7e`（Python 环境隔离 + §5.9/§5.10）
  → `9c869a9`（INBOX 1~3：渐变跟随轴 / 图层列表黑盒 / 中文乱码）
  → `392a18d`/`da31217`（#6 Layers 改名 + #7 报告 + #5 下一轮设计）。
  公告正文＝`docs/RELEASE_NOTES.md` v0.5.4 节；版本号四处同步
  （CMake project+target ×2 / CLI --help / GUI `getApplicationVersion`——后者此前长期停在 0.3.1，一并修正）。
  **v0.5.5 首项 #5 已实现**（描边四边独立 + 实时平均色四模式 + 预览降频插值；见下方专项节；
  部署 `AudioVisGUI_09120209.exe` 待用户实测；唯一未做＝真·未来帧预渲染，默认关）。
  - 🔧 **v0.5.5 增量（发版后，待用户实测；部署 `AudioVisGUI_09120241.exe`）**：
    #5 描边实时平均色四件套 + 新1 图层列表三点（Mask 作 Spectrum 子行 / 列表内 Delete 可删 / 取消选频谱自动跳页）
    + 新3 快捷键（←→ seek 长按加速 / Ctrl+Z 快照撤回 / Ctrl+A 选频谱，**Ctrl+A 语义待确认**）；
    配套新增 `ParamPanel::syncAllFromParams()`（undo 后统一回填面板）。
  - ✅ 已完成：A1 ColorMap / A2 bar-mirror（**后于 #3.1 删除**，改为 bar + 基线轴 50%）/ A3 CrystalStyle v2 bloom / A4 频谱蒙版图片
    （BUG1 漂移修复 + BUG2 独立拉伸）/ Tabbed UI（4 tab）/ Baseline axis（baselineY）
    / Bar 布局 pitch 模型重做（#25 + #1' + #3''）/ Bar-line 峰帽 v3（连贯分段）
    / Line-only 切换 / Scale snapping / Crash reporter / mp3/flac registerBasicFormats
    / Timestamped deploy /收件箱三文件协议 / style proposals doc / Y2Kmeter audit。 / INBOX 五连修复（崩溃根除+轴重合+mp3+pitch 1/90+末柱报告，23:2x） / INBOX #10 y2k 两臂颜色一致（`c755f73`） / INBOX #1b outline 必崩真根因=compose 描边越界读（`5877024`）
  - 🔧 已知问题：
    - ✅ **compose() 平均色描边崩溃 → 已根除**（渲染不再逐帧算均色；均色只在加载图/Use average 按钮时算一次）。
    - ✅ **#1b 勾 Outline 必崩·真根因（09-11）**：用户重报后读 `crash/*.dmp` 符号化（minidump + `build_win/AudioVisGUI.map` + objdump）
      → `SpectrumMask::compose +0x8f5`：描边环 `e[idx]` 行指针二次偏移（= `tmp[2·y·W+x]`）越界读堆。改 `e[x]` 修复；
      `vis_mask_test` 补 stroke 用例 6（负对照 3 FAIL 证明有效）。部署 `AudioVisGUI_09110110.exe`。
    - ✅ **mp3 解码 → 已支持**（开启 JUCE_USE_MP3AUDIOFORMAT 软件解码 + PcmSource 注册 MP3AudioFormat）。
    - ✅ **baseline axis 与频谱底部/左缘重合** → 已取消全样式 reduced(2) 缩进 + 左/下内边距归 0。
    - ✅ **pitch 默认 1/90**（与 bandCount=90 联动，默认 90 柱全宽）。
    - ✅ **#3 peak-caps 四件套（02:2x）+ 派生 #5**：3.1 删 bar-mirror（工厂保留别名 → bar，旧预设不失效）/
      3.2 bar 帽过 baseline 映射 + a>0 双侧帽 + 下臂缘线 / 3.3 `refreshStyleDependentControls` 取消 Peak caps 置灰
      且三 line 样式内部用 `rp.barParticles` 门控峰线 / 3.4 三个独立 bug（y2k 下臂描边漏 setColour、
      三样式缺下臂镜像峰线、`CrystalStyle::buildMirrorPoints_` 反推漏减 a → 下半屏被填满）。
      新增常驻 `scripts/vis_peaks_test.cpp`（16 断言 + 负对照精准 5 FAIL）。
    - ✅ **#F 末柱斜面 / 频带高度三问报告** → 已写入 `INBOX_REPLY.md`「📣 关键汇报」首节（用户点名位置）。
    - ⏳ 待用户实测：#3 全量（bar 双侧帽 / line 开关 / crystal 下侧）+ #9 汇报是否已看到。
  - 🔧 **本轮新增（用户实测反馈 INBOX 1~4；5~8 见下方专项）**：
    - ✅ **#1 填充渐变改为"从轴出发向上下淡出"**：三处有竖向渐变的样式（bar / bar-line / crystal）
      原先都锚在柱体**外缘**（底浓顶淡），轴拖到中间时最浓处落在下缘而非轴上。
      bar = 拆上下臂各一条渐变；bar-line = 梯形按 axisY 一刀切两块（a=0 时下块零面积 → 与原图完全一致）；
      crystal = 上臂本就以轴为起点（正确），但**下臂直接复用同一条渐变** → 轴以下落在渐变线之外被
      JUCE 钳到端点色 → 整片均匀 0.50 实色（与上臂淡出不称）→ 给它单独一条"轴→画布底"的渐变。
      `vis_peaks_test` 新增断言"最浓行必须在轴附近"（bar/bar-line 各 2 条；实测轴处 alpha=216=0.85×255、
      柱底缘 109），**负对照**（BarStyle 退回单条旧渐变）→ 精准 2 FAIL（161 vs 212，浓在柱底缘）。
      注：y2k-line / polyline 的填充是**平涂 tint**（无竖向渐变），本次无物可改；已在 REPLY 问是否也要加同款淡出。
    - ✅ **#2 图层列表"一直是黑的"= 我上轮引入的回归，已修**：根因＝`ListBox` 把行数缓存在 `totalItems`，
      **只有 `updateContent()` 会重新问 `model->getNumRows()`**，而 JUCE 头文件对 `repaint()` 明示
      "does not invoke updateContent()"。我首次 `updateContent` 发生在 `layerRows` 还为空时 → 缓存 0 行 →
      之后永远不画行，只剩我设的深色底。修复＝`refreshLayerList` 里补 `layerList.updateContent()`；
      顺带把 `ListBox layerList{"LayerStack", this}`（成员初始化器里外泄 this、且早于 layerRows 构造）
      改成构造体内 `setModel (this)`。
      **新增无头渲染回归**（`vis_tabs_test` 用例 4）：把 ListBox 用 `paintEntireComponent` 画进 Image，
      数"文字亮度像素"——修复后 463 px 有字；**负对照**（删掉 updateContent）→ **0 px 纯黑盒 + FAIL**，
      正是用户所见。另加用例 5 断言行颜色串解析为不透明（防"颜色写错→透明字"这一类）。
    - ✅ **#3 GUI 中文乱码：根因是解码不是字体**（用户说"尽可能修复"，我按证据定性）：
      `WinDragCompat::relayDrop` 用 `DragQueryFile`（TCHAR 宏）+ `std::vector<TCHAR>`，而**本工程 CMake 不定义
      UNICODE**（JUCE 不注入；`build.ninja` 实测无 `-DUNICODE`）→ `TCHAR=char` → 走 **A 版返回系统 ANSI
      （中文 Windows=GBK）字节** → 再交 `juce::String(const char*)`，而它按 **`CharPointer_ASCII`** 解析
      （JUCE 源码对 >127 字节自带断言 "can NOT be correctly converted to unicode"）→ 中文路径乱码。
      修复＝**强制 `DragQueryFileW` + `std::vector<wchar_t>` + `String(const wchar_t*)`**（UTF-16 无损）。
      渲染侧确认**无需改**：`FontOptions::fallbackEnabled` 默认 true，JUCE 文本布局缺字形时自动走
      `findSuitableFontForText` → DirectWrite 系统回退（命中微软雅黑）。我一度加全局
      `Font::setPreferredFallbackFamilies(...)`，**Linux 编译即报错——它是 Font 成员函数不是静态**，已撤回并
      在 Main.cpp 留注释说明为何不需逐字体处理。OLE 正常路径由 JUCE 自己用宽字符 API，本来就没这问题
      ——所以症状是"有时候"（仅 WM_DROPFILES 兜底链发作）。
    - ⏳ **#4**：用户要求 `🅿 频带三问` 报告**暂留 REPLY**，并要我做完回头检查 INBOX 是否更新（本轮已执行）。
    - ⏳ 用户挂起项：某个文件"拖不进来提示文件不存在"（已明确报错，暂搁置待其多轮测试）。
  - 🔧 **本轮 17:5x 新增（用户实测反馈 A~H）**：
    - ✅ 用户确认通过：A（outline 不再崩溃）/ B（bar-mirror 已删）/ C（bar 帽跟轴双侧）/ D（line 系开关）。
    - ✅ **#E crystal 两侧亮度不一致**：pass1 实体描边与 pass2 高光原先**只画上臂**，下臂只剩辉光+填充
      → 两处都补 `buildMirrorPoints_` 下臂路径，参数与上臂逐字相同；`vis_peaks_test` 新断言（强 primary 像素
      两侧比）+ 负对照 1 FAIL。
    - ✅ **#G 切页残留 → 结构性消除**：根因＝`Row{label, editor, h, tab}` 一行只能挂一个 editor，
      而 Colors(4)/描边色(2)/W×H(2) 都是并排多控件 → `maskAvgBtn`/`secondaryBtn`/`peakBtn`/`bgBtn`/`heightEditor`
      **从未被隐藏过**（用户只撞见最显眼的 Use average）。修法＝`Row::editors` 向量化 + `addRowGroup(label,{…})`
      整行统一驱动布局与显隐；`keepVisibleOnAllTabs()` 登记常驻件；`resized()` 末尾**安全网**把
      "没登记又不属任何行"的直接子件一律隐藏（失败方向从「静默残留」翻转为「控件不出现」）。
      新增 `vis_tabs_test`：四页切换后 `findUnownedChildren()` 必须为空 + **行为证明**（塞入未登记按钮必须被抓出并隐藏）。
    - ✅ **#H 拖放三处根因**：① 兜底目标 `filesDropped` 的 `isAudio` 里混进了图片扩展名 → 拖到面板（非 Mask 页）的
      图片被当音频 `loadFile`，永不成图层；② `applyMaskImageFile` 先无条件写 path/enabled 再验证 →
      解不开时留下"已设置但看不见"的死状态，下次只弹 "already set"；③ `addImageLayer` 不验证 +
      `makeContainTransform(0,0,…)` 因 `jmax(1.0f, elemW)` 把 0 当 1 → **scale 静默变 720**，画布侧又用
      "无效图退化成画布尺寸"画手柄 → **框炸到画布上千倍外**（= 用户原话"提示的边框范围远超画布"）。
      修法＝`loadValidatedImage()`（失败弹具体原因：文件名+字节数+可读格式清单，且**不改任何状态**）、
      `routeDroppedFile(file, 屏幕落点)` 让 OLE 链与 WM_DROPFILES 链共用同一套规则、
      `makeContainTransform` 非正尺寸退回 1:1（`vis_anchor_test` 情形 11 锁死 + 负对照 4 FAIL）、
      `onUseMaskAvgColour` 的静默 return 也改为弹提示、成功拖入写状态行显示目的地。
    - ✅ **#H 采纳用户建议**：Image 页顶部新增可见**图层栈**（`ListBox`：顶→底排序 +
      `[ok | FILE MISSING | NO DECODER]` + `scale x…`，点击行 = 选中该元素，蒙版行进出"编辑图片位置"；
      内容哈希去抖，每 tick 调用也便宜）。
    - ⏳ 用户新提"具体样式我还想进一步优化" → 等其指名样式与期望（已列待办，不阻塞）。
  - 🧰 **主机侧辅助工具已隔离（2026-09-11 18:4x，用户要求"别装 base"）**：
    核查确认**工程零 python 依赖**（构建只用 CMake/Ninja/clang-cl/xwin；仓库内无 `.py`，
    `find -L` 查到的 5 个都在符号链接指向的 JUCE 里）。原先装进 conda base 的
    `minidump`（崩溃符号化）与 `pillow`（画图标）已**卸出 base**，改由
    `~/CodingProgram/AudioVisualizer/tools-venv`（venv 而非 conda env：conda 会复制整套解释器，
    只有需要非 python 二进制时才值，比如 §7.5 的 gitenv）承载，并落成仓库内脚本：
    `tools/setup_env.sh`（幂等一键重建）+ `tools/requirements.txt` +
    `tools/dmp_report.py`（dmp+/MAP → 函数与偏移；四个历史 dump 实测全部落在
    `SpectrumMask::compose`，`ExceptionInformation[0]=0` 即 **READ**、目标地址是野生值而非 NULL，
    从硬证据上否掉了"分配失败返回 NULL"的旧猜测）+ `tools/gen_icon.py`
    （`assets/icon.png` 原先**只有成品没有源**，现补上设计复刻脚本；默认不覆盖成品，均值差已如实记录）。
    文档：`ARCHITECTURE.md` 新增 §5.9；`.gitignore` 挡住误建在仓库内的 venv。
    ✅ **用户随后自己优化了环境**（权威说明落 `~/CodingProgram/PYTHON_ENVIRONMENT.md`）：
    pip cmake 已被用户亲手移出 base → 终端与 AI shell 的 `cmake` 统一为 apt 3.22.1（上轮"保留"结论作废，
    版本不对称消失）；系统 `python3.10-venv` 已装 → 本项目 tools-venv 已按文档"方式 A"**改由系统
    python 3.10.12 重建**（`pyvenv.cfg home=/usr/bin`，与 conda 彻底解耦），`setup_env.sh` 优先级改为
    系统 python3 → common311 → conda base。两工具 + 自动跳转实测全通过。
    ⚠️ 文档决策树把"一次性临时脚本"指向 `uv`，但机器上**尚未安装 uv**——要装说一声。
  - 📋 协作机制：INBOX 三件套 + status gate + #0/#0+/#10/#11 轮次节奏协议，
    REPLY 三固定子节结构。全部写入 PLAN.md「文档更新触发点」。
  - 🛠 **崩溃报告器已两次立功**（#1b 与本轮排查）：用户只需照常闪退，`exe/crash/*.dmp`
    自动落盘 → 我方读 dump + `build_win/AudioVisGUI.map` 符号化即可定位到函数/指令级，无需用户手工发文件。
- **发版后遗留**：
  - ⏳ 导出侧范围内裁剪回归待用户复测（v0.5.3 遗留）。
  - P-verify 缩放/旋转时的吸附辅助线（v0.5.3 可选项）。

## ✅ 已完成：#5 蒙版描边"按内容实时平均色"四件套（2026-09-12 已实现）

> 用户 INBOX #5（a~f）**已全部实现**（`vis_mask_test` 用例 7~10 锁死；部署 `AudioVisGUI_09120209.exe` 待实测）。
> 默认值（我替用户拍定）：`outlinePreviewFps=8`、`outlineTemporal=true`、`outlineLookaheadFrames=0`。
> ⚠️ 唯一未做＝真·未来帧预渲染（5e 的一种实现，默认关，收益小成本高）；若用户要再加。
> 下面原设计记录保留（实现即按其执行；模式 token 因简化统一存**小写** `perbar`/`perframe`）。

### 数据模型（`SpectrumParams::MaskImageLayer` 追加）
- `outlineColorMode`（String，默认 `perBar`）四选一，覆盖 a/b/d/f：
  - `perBar`（a）：每根柱的描边色＝该柱**可视区**（蒙版命中的图片像素）的平均色；
  - `perFrameRegion`（b）：line 系用，本帧整个可视区一个平均色；
  - `uniformAllBars`（d）：所有柱共用一个色＝全部可视区（并集）的平均色；
  - `imageAvg`（f）：**保留**现状——图片本身平均色（加载/按钮时算一次，不随帧）。
- 四边独立（c，bar 与 line 各一套，全局作用非逐柱）：
  `outlineTop/Bottom/Left/Right`（bool，默认 **true/true/true/true**）
  `outlineWTop/Bottom/Left/Right`（float px，默认 **2/2/2/2**；line 默认 **1.5/1.5/1.5/1.5**）。
- 预览性能（e）：
  - `outlinePreviewFps`（float，默认 **8**）：描边平均色每帧不必重算，节流到 ≤8 次/秒（视频仍 30fps）；
  - `outlineTemporalSmooth`（bool，默认 **true**）：每次节流刷新后，用指数插值把当前色**平滑逼近**目标色，
    8Hz 刷新 + 30fps 显示看起来连续（就是用户说的"平滑渐变到下一秒"）；
  - `outlineLookaheadFrames`（int，默认 **0 = 关**）：真正的"预渲染未来几帧"。

### 计算/渲染改动
- `SpectrumMask` 加 `averageColourInRegion(baseAlpha, image, rect|bandSlots, ...)`（预乘安全，复用现有 un/pre-multiply 逻辑）；
  bar 逐带 = 该带柱形覆盖的像素框；line = 整块可视区。
- `compose()` 描边段：按 mode 解析每段/每柱的 resolvedStroke；四边各自开关/厚度（erosion 环拆成 top/bottom/left/right 四条，
  受 `outline*` 控制）；**保留 scratch buffer + null guard + 已修的 `e[x]` 不越界**（绝不回退 #1b）。
- GUI `SpectrumCanvas::paint` 与 `VisPipeline::renderFrame` 同一份 `SpectrumMask::compose`，预览即所得不变。

### 参数面板 / 序列化
- Mask 页新增：`Color mode` 下拉 + "Edges" 子区（4 开关 + 4 厚度，用 #G 的 `addRowGroup` 整行登记防残留）。
- JSON `mask.outline.{mode,top,bottom,left,right,wTop,...}`；CLI `--set mask.outline.mode=perBar` 等。
- 旧 JSON 无这些字段 → 全部走上面的默认值（向后兼容）。

### 测试
- `vis_mask_test` 扩：① 只开 top 边 → 上缘有描边、其余三缘 0 描边像素；② `perBar` 两柱取不同底色 → 两柱描边色不同；
  ③ `uniformAllBars` → 所有柱同色；④ 节流+插值：连续帧色单调逼近目标、无跳变；⑤ 负对照：关掉某缘开关该缘消失。

### ⚠️ 需你（用户）确认的一处范围收敛
- #5e 的字面要求是"实时预览提前渲染未来几帧"。我把它拆成**两层**：
  - **本轮先做**：`outlinePreviewFps` 降频 + `outlineTemporalSmooth` 插值（成本可控、直接达到"负担更低 + 观感平滑 + 防崩"）；
  - **`outlineLookaheadFrames` 预渲染未来帧**需要预览端"只跑频谱不合成"地超前解码若干帧，改动大、收益（对描边色而言）有限
    ——建议**默认 0（关）**，若你坚持要真·预渲染我再单独做。请回"降频+插值够了"或"要未来帧预渲染"。

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

