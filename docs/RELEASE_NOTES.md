# AudioVisExport 更新公告（Release Notes）

> 本文件按 `ARCHITECTURE.md` 开头「发布规则」随每次小版本更新：**每次小版本发版必须新增一节
> 用户视角的公告（新功能 / 修复 / 升级注意事项），并作为 GitHub Release 正文随 tag 一起推送。**
> 开发视角的完整变更记录见 `docs/HISTORY.md` §2。本文件自 v0.4.2 起启用。

---

## v0.5.2 — 2026-09-07（频谱 = 真图层 + 对边锚定缩放 + 吸附）

### 新功能
- **频谱彻底变成一个"真正的图层"**（与图片完全同待遇）：
  - 统一 z 序：图片可精确压在频谱下/上，**频谱也可用 [Up]/[Down] 在图片之间自由移动**；
  - **频谱可删除**：选中频谱后按 [Remove]（或键盘 Delete/Backspace）即删除，**音乐不受影响**；
    面板 [Add spectrum] 按钮一键加回（默认铺满画布）；[Select spectrum] 按钮可在频谱被
    图片完全盖住时选中它；
  - 修复"加图片后频谱无法拖放"——点击判定改为严格 z 序（上层图片盖住频谱时点击图片，
    频谱未被盖住的区域点击频谱）。
- **对边锚定缩放（更符合直觉的拉伸）**：拖动某个角/边时，**对角/对边固定不动**——
  拖左下角 = 右上角钉死；拖下边 = 上边钉死；图片旋转后同样成立。
- **吸附系统（面板 Layers 区 "Snapping" 开关，默认开）**：
  - 旋转接近 90°×n（±3°）自动校正；
  - 移动时元素边/中线接近其他元素边/中线或画布中心/边缘（≤8 屏幕像素）自动对齐。

### 升级注意事项
- JSON 新增 `spectrumPresent` / `spectrumIndex` / `snapEnabled` 三个字段；
  **旧预设完全兼容**（频谱默认在场，层级从原 `aboveSpectrum` 标志自动换算）。
- CLI 新增 `--set spectrum.present=false`（删除频谱）、`--set spectrum.index=N`、
  `--set spectrum.snapEnabled=on/off`。

---

## v0.5.1 — 2026-09-07（图片图层系统重构）

### 新功能
- **图片按自身宽高比显示**：新图片等比 contain 居中，不再强制拉伸铺满画布变形；
  双击图片 = 复位到等比居中。
- **手柄框贴合图片实际边缘**：拖拽手柄/命中判定全部改用图片实际尺寸
  （此前固定用画布尺寸，导致选中图片时拉伸难以操作）。
- **"Above spec" 图层上下开关**（Layers 区）：选中图片即可切换频谱下方/上方；
  默认在频谱下方；GUI 预览与导出分层一致（此前 GUI 无视分组、图片永远盖住频谱）。
- JSON `images[]` 与顶层 `transform` 新增 `posX`/`posY`；CLI `--set transform.posX/posY`。

---

## v0.5.0 — 2026-09-06（Windows 原生 exe + 拖放彻底修复 + GUI/样式增强）

### 里程碑：Windows 原生 .exe 交叉编译打通
- **WSL 内直接编译免安装 Windows exe**（Clang 18 MSVC ABI + lld-link + 官方 Windows SDK 10.0.22621，
  经 [xwin] 拉取）：静态 CRT，任何 Win10/11 双击即用，无需 VC_redist。
- 一键脚本 `scripts/build_win_cross.sh`（`--deploy` 自动部署到测试目录 / `--clean` 全量重建）。
- 旧 MinGW 方案废弃（缺 Direct2D 1.2/1.3 头文件，JUCE 8 无法编译）；完整工具链说明与
  三个已知坑的排错记录见 `ARCHITECTURE.md` §5「Windows 交叉编译」。

### 新功能
- **拖放彻底修复（重点）**：管理员 / 普通权限下拖放文件均可用。
  - 提权进程自动降权重启（经 explorer.exe 代理，窗口自动重启一次属正常）；
  - WM_DROPFILES 老协议兜底 + 直连交付（实测 OLE 分发链跨协议 relay 不可靠）；
  - 画布右下角常显诊断行 `[drag] elevated= oleRevoked= fallback= wmDropFiles=N`，
    面板底部同步显示每步状态——拖放异常一眼定位。
- **音频文件管理**：传输条新增 `[Load...]`（随时换曲）/ `[Eject]`（移除当前音频）；
  加载后左下角显示 `♪ 文件名`（悬停看完整路径）——加载状态一目了然，不再靠猜。
- **Colors 4 按钮（新增 BG）**：颜色选择从 Appearance 区末尾（需滚动）上移到区首免滚动；
  补齐 `bgColor` 背景色 GUI 控件（此前仅 JSON/CLI 可调）。
- **峰值帽参数显性化**："Peak hold ms"（下落间隔）/ "Peak decay dB/s"（下落速度）
  从 Time 区上移到 Style 区 Peak caps 开关旁，三件套聚合 + tooltip 说明。

### 样式增强：bar-line 斜面柱顶
- 柱体重构为五边形梯形：**顶边斜线**——相邻柱高度不同时柱顶自然倾斜
  （如左柱 50、右柱 65，界面处从 50 斜升到 65），同高时水平；柱间 gap 保留。
- **斜面峰值帽**：峰值帽不再是水平线——顶到新峰瞬间捕获当时柱顶斜率
  （帽与柱顶贴合），峰值下落时整条斜线刚性下滑（斜率保持），
  保持期帽悬停在柱顶上方清晰可见。
- **暂停冻结峰值**：暂停时峰值帽不再"抖动下落"（此前 getBandFrame 有状态推进
  在暂停期间仍持续衰减）；seek 快进峰值建立不受影响，离线导出零影响。

### 修复
- JUCE 8.0.12 源码 6 处 MinGW 时期补丁还原（d2d1_3.h 等）——共享 checkout 回到原版。
- 管理员降权重启不再误开 Documents 文件夹（explorer.exe 代理无法传命令行参数，
  改用环境变量传标志）。
- `SpectrumCanvas` 拖放横幅文案更新（提权时提示 legacy 协议已启用，不再误导"必须重启"）。
- 版本号三处同步至 v0.5.0（CMake project/target ×3、CLI --help）。

### 升级注意事项
- **构建方式变化**：Windows exe 请用 `bash scripts/build_win_cross.sh`（WSL 内），
  一次性环境安装见 ARCHITECTURE.md §5；WSLg 版（`Run_AudioVisGUI_wslg.bat`）仍可用作后备。
- WSLg 版的拖放仍是全程禁止符（Windows→WSLg 跨界限制，非 bug）；原生 exe 拖放正常。
- 若你的环境 OLE 拖放异常（如被安全软件 hook），程序会自动走 WM_DROPFILES 兜底，
  右下角诊断行可见 `wmDropFiles` 计数增长。

---

---

## v0.4.2 — 2026-09-05（图片图层第一阶段 + bar-line 样式 + WSL 迁移）

### 新功能
- **图片图层系统（第一阶段：静态图片）**：把 PNG/JPG 拖进画布即创建图层（默认铺满输出画布），
  可与频谱一样拖动 / 缩放 / 拉伸 / 旋转 / 双击复位；Layers 区支持 [Add image] / [Up] / [Down] /
  [Remove] / Opacity；图层可放在频谱下方（背景）或上方（前景贴片）；导出与预览所见即所得。
- **新样式 `bar-line`（柱体 + 柱顶连线）**：逐柱画柱体，相邻柱顶用连续折线连接，
  样式下拉第 5 项；柱体布局沿用 bar 的 gap/width 参数。
- **"Peak caps" 峰值帽开关**（Appearance 区）：控制 bar / bar-line 的峰值帽（随峰值缓慢下落的横线）
  显隐，JSON / CLI `--set visual.barParticles` / GUI 三处贯通。
- **拖放诊断（仅 Windows）**：以管理员身份运行时画布顶部显示红色警告横幅；
  拖文件进窗口显示 "DROP -> 文件名" HUD，用于区分 UIPI 拦截与程序故障。

### 平台与构建（本次迁移要点）
- 工程由 Windows/MSVC 迁移到 **WSL2 (Ubuntu 22.04)** 开发构建；**设计目标平台仍为 Windows**。
- 测试产物统一交付到 **`C:\Users\yiyikneesocks\Desktop\AudioVisExport_test\`**，
  附 `Run_AudioVisGUI.bat`（双击经 wsl 拉起，WSLg 在 Windows 桌面直接显示窗口）。
- 构建系统改用 Ninja + 本地 JUCE 8.0.12（`third_party/JUCE` 符号链接复用 Y2Kmeter checkout，
  需 `-DAVX_USE_LOCAL_JUCE=ON`；本机访问 GitHub 不稳定，勿依赖 FetchContent 在线克隆）。

### 修复
- `SpectrumCanvas::fileDragExit` 签名对齐 JUCE 基类；`ImageLayer` 限定名统一；
  ParamPanel 重复的 Export 头去重；GUI 预览峰值帽与导出不一致（`buildRp` 补接 `barParticles`）。
- **CLI UTF-修复 + 版本号同步**：`juce::String(const char*)` 按 ASCII 处理 UTF-8 中文导致双重编码乱码；
  `CliArgs.cpp` 改用 `CharPointer_UTF8` 包装；版本号从 `v0.2.0` 同步到 `v0.4.2`（含 `CMakeLists.txt`
  的 `project(VERSION)` 与两个 target 的 `VERSION`）。

### 升级注意事项
- Windows 时期的 `build/`（.sln/.vcxproj）已删除，两种平台都需重新 configure。
- ffmpeg 搜索逻辑不变（hint / `FFMPEG_PATH` / PATH），Linux 下按 `ffmpeg` 名字搜索；
  透明视频导出仍以 **MOV QTRLE** 为准（WebM VP9 实测不保留 alpha）。

### 历史版本
- v0.4.0 / v0.4.1 的变更见 `docs/HISTORY.md` §2（本公告文件自 v0.4.2 起启用，此前无公告存档）。
