# AudioVisExport 更新公告（Release Notes）

> 本文件按 `ARCHITECTURE.md` 开头「发布规则」随每次小版本更新：**每次小版本发版必须新增一节
> 用户视角的公告（新功能 / 修复 / 升级注意事项），并作为 GitHub Release 正文随 tag 一起推送。**
> 开发视角的完整变更记录按版归档在 `docs/history/<系列>/<版本>.md`（索引见 `docs/HISTORY.md` §2）。本文件自 v0.4.2 起启用。

---

## v0.5.4 — 2026-09-11（频谱样式专项 + 蒙版图片 + 基线轴 + 峰帽双侧 + 图层列表 + 崩溃修复）

> 正式版。主体＝频谱样式专项（ColorMap / bar-mirror→基线轴 / crystal 真 bloom / 蒙版图片）+ 选项卡 UI
> + 基线轴 + 峰帽双侧 + 图层栈 + 多轮实测反馈修复（含两次崩溃根因）。
> 开发视角完整记录见 `docs/history/v0.5/v0.5.4.md`。

### 新功能
- **ColorMap 颜色映射（三种模式）**：
  - `gradient`：按频谱强度渐变（Primary→Secondary→白色），最常用；
  - `rainbow`：按频带相位铺彩虹色相环，视觉冲击强；
  - `solid`：单色（原有行为，向后兼容）。
  - 面板 Appearance 区新增 "Color map" 下拉；CLI `--set visual.colorMap=gradient|rainbow|solid`。
- **镜像柱改由"基线轴"实现**：原先计划的独立 `bar-mirror` 样式已取消（与 bar + 基线轴 50% 完全重复）；
  想做 DJ 式上下镜像：选 `bar` 或 `bar-line`，把 **Baseline 拖到 50%**，峰帽会自动出现在上下两侧。
- **CrystalStyle v2 真高斯辉光**：替换旧 4 层假描边，效果更自然（透明背景 alpha 处理安全）。
- **峰帽 / 峰线全面跟随基线轴，且双侧对称**：`bar` 的峰值帽以前不随轴移动、只在上侧；
  现在上帽 = 轴上生长位、下帽 = 轴下镜像位（轴在中间时上下都有），柱体下臂外缘也补了一条清晰的缘线。
- **"Peak caps" 开关对折线类样式生效**：以前 y2k-line / polyline / crystal 的该开关是灰的、峰线关不掉；
  现在统一由它控制（关 = 峰值虚线完全不画）。
- **拖动基线轴后的折线类下侧修复（三处）**：y2k-line 下臂此前**没有描边**（颜色被上一段填充覆盖，几乎不可见）→ 现在与上臂同样清晰；
  三种折线样式此前**只有上侧有峰线** → 现在下侧也有一条镜像峰线；
  crystal 此前把轴拖高后**整个下半屏被填充糊住**（下臂数值反推出错）→ 现在下臂只按镜像高度生长。
- **可见图层列表（图层栈）**：Layers 页顶部新增一个列表，按"从上到下"的叠放顺序列出
  Spectrum / 各 Image / 蒙版图片，每行直接标出：文件名、**文件是否还在 / 能不能解码**、当前缩放倍数，
  并高亮当前选中的那一层。点一行 = 在画布上选中那个元素（点蒙版行 = 进出"编辑图片位置"）。
- **填充渐变改为"从基线轴出发向上下淡出"**：以前柱体填充的最浓色固定在柱子底缘（整根"下浓上淡"），
  把轴拖到中间时看着仍像从画布底往上打。现在 `bar` / `bar-line` / `crystal` 三者的渐变都以**轴**为起点，
  向上下各自淡化（轴在底部时与以前完全一致）。
- **图层页改名 Layers**：原 "Image" 标签改为 "Layers"（频谱/图片/蒙版统一在此选择，顶部图层栈列表顶→底排列）；
  按钮 `Move up/Move down` → `Up/Down`，透明度滑块改名 `Layer opacity`（只作用于选中层）。
- **GUI 中文不再乱码**：以老式拖放协议进入程序的中文文件名以前会显示成乱码（现在已修复）。
- **频谱蒙版图片**：图片只在"频谱轮廓"内可见（bar 逐柱 gap 处不铺图、line 整块）；
  描边沿轮廓内侧勾边，默认色=图片平均色（可关/可手动覆盖）；
  图片绑定频谱整体拖动，「编辑图片位置」模式内可独立平移/缩放/旋转。
- **4 选项卡参数面板**：Spectrum / **Layers** / Mask / Export 分 tab，画布选中元素自动切换；
  Layers 页 = 图层栈列表 + 添加/排序/删除 + 选中层透明度与色彩调整；Mask 页有独立颜色按钮 + 平均色按钮。
- **基线轴（baselineY）**：拖动水平轴线控制频谱对称生长中心（0=底部，1=顶部）；
  bar 样式对称生长，line 样式 a>0 时生成双曲线+双填充；双击/拖动吸附到 0,¼,⅓,½,⅔,¾,1。
- **Line-only 切换**：y2k-line / polyline / crystal 可只画线不填充（面板 Appearance 区）。
- **Scale snapping**：角/边拖拽时自动对齐画布/元素边缘（复用移动吸附同一套候选）。
- **Windows 崩溃报告**：崩溃时自动写 MiniDump + 文本报告到 `exe/crash/` 目录，
  GUI 与 CLI 均已接入。
- **mp3 全平台解码（v0.5.4 补丁）**：开启 JUCE 软件 MP3 解码宏 `JUCE_USE_MP3AUDIOFORMAT` + PcmSource 显式注册 `MP3AudioFormat`；wav/aiff/flac/ogg/mp3 五种格式均可加载导出。

### 修复
- **拖放图片"没反应 / 提示框跑到画布外面"（重要，三处根因一起修）**：
  ① 拖到参数面板（当前不在 Mask 页）的图片以前会被当成**音频**去加载，于是永远建不出图层；
  ② 蒙版图片以前是"先记下、再验证"，遇到解不开的图就留下"已经设置了但画面上什么都没有"的状态，
     下次再拖只会弹"已有蒙版图"——这正是"拖到 mask 没反应，可提示里说已经加载过图片"；
  ③ 解不开的图尺寸为 0，等比 contain 会把它当成 1 像素 → 缩放倍数变成画布高度（例如 720 倍），
     选中框因此被放大上千倍、远远超出画布。
  现在：图片一律**先解码成功、再改变任何状态**，失败会弹出具体的文件名 / 字节数 / 支持的格式清单；
  两条拖放通道（资源管理器 OLE 与老协议兜底）共用同一套路由规则；0 尺寸在数学上已不可能炸框；
  每次成功拖入都会在面板底部写明它变成了什么（`Image layer #N added: …` / `Mask image set: …`）。
- **切换选项卡后按钮残留（结构性修复）**：同一行并排的多个控件（Colors 的 Secondary/Peak/BG、
  Mask 页的 Use average、导出区的宽度/高度输入框）以前只登记了第一个，其余**从不随切页隐藏**。
  现在一行可挂任意多个控件并整行统一显隐；并加了安全网——任何"没归到任何一行、也不是常驻部件"的控件
  会被直接隐藏（宁可看不见，也不留在别的页上）。
- **图层列表打开是一片黑、看不到任何行**：控件把行数做了缓存，只重绘不通知它重新查询模型 →
  第一次查询时列表还是空的，于是"0 行"被永久缓存，只剩背景色。已补上正确的刷新调用并加了
  无头渲染回归（把控件画成位图数字文字像素，删掉修复会立刻变成 0 像素）。
- **crystal 上下两侧亮度不一致**：轴拖高后下半部分只有柔光与填充、**没有实体描边和高光线**，
  所以明显比上半暗。现在两侧使用完全相同的一套描边 / 高光参数。
- `makeContainTransform` pos 公式：`out/2−s·c` → `out/2−c`（修复所有图片图层 s≠1 时的定位偏差）。
- `averageColour` JPEG 越界：像素访问前先归一化为 ARGB 格式（修复 JPEG 图片平均色崩溃）。
- Bar 布局 slotW 公式：`innerWidth/N` → `pitchRatio × innerWidth`（修复 bar / bar-line 的柱宽计算）。
- Tab 文字重叠：hidden-row labels 重新隐藏。
- Scale snap 拖拽：角/边拖拽时正确应用 `applyScaleSnap()`。
- **基线轴与频谱底部/左缘重合（INBOX #2）**：取消全部样式 `reduced(2)` 缩进 + 左(32)/下(16) 内边距归 0——柱底/左缘贴画框，与 baselineY=0 轴重合。
- **平均色描边崩溃根除（INBOX #1）**：渲染不再逐帧算平均色（删除逐帧缓存），均色只在加载图片/Use average 按钮时算一次写入描边色。
- **pitch 默认值 1/90（INBOX #8）**：`barPitchRatio` 默认从 1.0 改为 1/90，与 bandCount=90 联动，默认 90 柱全宽。
- **y2k-line 基线轴两侧颜色一致（INBOX #10）**：下臂填充此前误用了上臂**描边**留下的 1.0f 不透明渐变，
  导致下臂明显比上臂"实"；现填充前重置为与上臂填充同参数（0.25f 透明度 / 同色映射渐变）。
- **勾 Outline 必崩·真根因修复（INBOX #1b）**：由崩溃 dump 符号化定位——`SpectrumMask::compose` 描边环行指针二次偏移
  （`tmp[2·y·W+x]` 越界读堆）。修复后描边环四边正确对称（此前下边缘描边实际是错的）。（重要：此前版本"勾选 Outline 必闪退"即此问题）
- **bar 峰帽不随基线轴移动（INBOX #3.2）**：柱顶帽此前用未映射的原始峰值高度定位 → 拖轴后帽"根本不动"。
  现改为与柱顶同构映射，且轴不在端点时**上下两侧各有一条帽**；柱体下臂外缘补 1px 缘线。
- **折线类 Peak caps 开关置灰解绑（INBOX #3.3）**：该开关此前对 y2k-line / polyline / crystal 恒灰、无法关闭峰线；
  现已全样式可用，并在三样式内部真正门控峰值虚线。
- **拖轴后折线类下侧三处异常（INBOX #3.4）**：y2k-line 下臂描边漏设颜色（继承 0.25f 填充 → 看不见）；
  三样式下臂缺镜像峰线；crystal 下臂点反推时漏减基线偏移 → 整个下半屏被填满。均已修复。
- **删除重复样式 bar-mirror（INBOX #3.1）**：与"bar/bar-line + Baseline 50%"完全等价，故从下拉/CLI 列表移除；
  旧 JSON 里的 `bar-mirror` 仍可用（自动按 bar 渲染）。

### 升级注意事项
- JSON 新增 `visual.baselineY`（float 0..1，默认 0=底部）、`visual.lineOnly`（bool，默认 false）、
  `visual.barPitchRatio`（float，默认 1/90）；**旧预设完全兼容**，无迁移步骤。
- 旧 JSON 里的 `visual.style: "bar-mirror"` 仍能打开（自动按 `bar` 渲染），下拉列表不再出现该名称。
- CLI 新增 `--set visual.baselineY=0.5`、`--set visual.lineOnly=on`、`--set visual.barPitchRatio=0.012`
  等（完整键见 `ARCHITECTURE.md` §8 参数表）。
- 折线类样式（y2k-line / polyline / crystal）的 "Peak caps" 开关现在真正生效——
  若你此前依赖"开关是灰的"这个默认状态，升级后请确认它处于勾选状态，否则峰值虚线不显示。
- 若从旧版升级后首次打开工程，频谱会紧贴画框底部（v0.5.4 取消了 2px 内缩与左/下边距）——
  这是刻意修正（柱底与基线轴 0% 重合），不是错位。

---

## 开发中（v0.5.5 起累积，未发版）— 描边实时平均色四件套（INBOX #5）

> v0.5.4 发版后按用户指示直接开工（v0.5.5 累积中，未发版）。实测包 `AudioVisGUI_09120241.exe`。

### 新功能
- **描边"实时按内容平均色"**（`Outline colour` 下拉四模式）：
  - `perbar`：每根柱的描边 = **该柱可视区**的图片平均色（各柱不同）；
  - `uniform`：所有柱共用一个色 = 全部可视区的平均；
  - `perframe`：折线类用，本帧整个可视区平均色；
  - `image`：保留原"整图平均色"（加载/Use average 时算一次，不随帧）。
- **描边四边独立**：上/下/左/右缘各自可开关 + 各自厚度；"Outline width" 主滑块统一预设四边，
  再想单独调某一缘用分缘滑块。
- **预览自动降负载**：描边实时平均色在**预览**里默认按 ~8 次/秒重算并在帧间平滑逼近（颜色连续变化不跳、
  不逐帧硬算防崩）；**导出**始终逐帧精确。`Outline fps (preview)` 与 `Outline smoothing` 可调。
- **图层列表改进**：① Mask image 现在显示为 Spectrum 的**子行**（缩进 └，点它 = 选中频谱，二者当一个图层；
  "编辑图片位置"仍在 Mask 页按钮）；② 在列表里选中图片**直接按 Delete 就能删**（以前没反应，必须在画布上点）；
  ③ 取消"选中频谱自动跳回 Spectrum 选项卡"（不打扰你在当前页操作）。
- **新快捷键**：`←` / `→` 快进快退（单击 ±1 秒；**长按逐级加速** 1→2→5→10 秒）；
  `Ctrl/Cmd + Z` **撤回**（删除图层 / 加图层 / 拖动·缩放·旋转 / 替换蒙版等，最多 30 步）；
  `Ctrl/Cmd + A` 选中频谱。完整快捷键见 `ARCHITECTURE.md` §9。

### 升级注意事项
- 旧预设的 `maskImage` 无 `outlineMode` 字段 → 默认 `image`，描边观感与 v0.5.4 一致（四边默认全开、默认厚 2）。
- CLI 键：`--set mask.outlineMode=perbar|uniform|perframe|image`、`mask.outTop/outBottom/outLeft/outRight=0/1`、
  `mask.outWTop/…`、`mask.outlinePreviewFps`、`mask.outlineTemporal`。
- 模式 token 用小写（`perbar`/`perframe`）。

---

## v0.5.3 — 2026-09-09（锚定缩放修复 + CAD 吸附辅助线 + 范围内外视觉 + 空格播放）

### 新功能
- **吸附辅助线（CAD object-snap 风格）**：拖动元素命中对齐时，实时画出橙色虚线辅助线，
  并在两端标注对齐到的特征点（角点=方框 / 边中点=菱形 / 中心=圆）+ 目标名（`Canvas` / `Spectrum` / `Image N`）；
  松开鼠标即清除。仅 "Snapping" 开启时生效。
- **特征点吸附大升级**：被拖元素与每个目标各按 **4 角 + 4 边中点 + 中心** 共 9 个点对齐比较——
  两张图片之间现在支持**边对边 / 角对角 / 边对中**吸附（此前只有中心线能对齐）。
- **范围内 / 范围外视觉区分**：输出画布（导出会截取的范围）内偏黑（或棋盘），四周范围外偏灰并描一圈边界线；
  图片 / 频谱**超出画布的部分自动变暗发灰**，一眼看出哪些内容不会进入导出。**导出结果不受此显示改动影响**。
- **空格键 播放 / 暂停**：载入音频后无需点击窗口即可直接按空格切换播放/暂停（焦点已自动处理）。
- **峰值帽二阶下落**：新参数 `Decay accel dB/s²`（`peakDecayAccelDbPerSec2`，0 = 旧匀速；>0 峰帽下落随时间加速）。
- **键盘删除兜底 + 面板实时刷新**：无图片选中时也能删除频谱；Remove / Add spectrum 按钮可用性实时跟随选中状态。

### 修复
- **旋转后拉伸变平行四边形**（重要）：此前对旋转过的元素做非等比缩放会被斜切成平行四边形；
  现缩放改沿**元素自身本地轴**，方形图片旋转后任意拉伸仍是方正矩形。
- **锚定缩放对角漂移**：拖动时"钉死的对角/对边"在元素被移动过（偏移≠0）后会瞬移，现已彻底修正——
  任何情况下被拖角/边的对角都纹丝不动。
- **只有频谱时拖动"卡卡的/莫名吸附"**：修复频谱把自身当作吸附目标导致的自我吸附抖动。
- **旋转手柄抢占上边中点**：旋转命中判定收窄到圆柄圆心，上边中点恢复为"纵向拉伸"。
- 移除 **"Above spectrum" 开关按钮**：频谱上/下层级统一由 [Up]/[Down] 与图层顺序控制，逻辑更清晰。

### 升级注意事项
- JSON 新增可选字段 `peakDecayAccelDbPerSec2`（默认 0 = 与旧版一致）；旧预设无需迁移。
- CLI 覆盖：`--set time.peakDecayAccelDbPerSec2=<float>`。
- 代码版本号升至 v0.5.3；**功能与 v0.5.2 图层模型完全兼容**，仅缩放合成顺序内部改为「先缩放后旋转」。

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
- v0.4.0 / v0.4.1 的变更见 `docs/history/v0.4/`（本公告文件自 v0.4.2 起启用，此前无公告存档）。
