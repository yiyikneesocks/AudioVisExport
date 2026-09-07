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
