# 新样式候选计划（v0.5.4 #7 · 供预览拍板）

> 用法：每条含**效果描述 + 预览方式**（维基/官方仓库可点开；本地参照可直接跑隔壁 Y2Kmeter 看实物）。
> 你圈中意的编号回 INBOX，我按 #10 排序规则实施。成本 = S(小)/M(中)/L(大)。

## A. 参照 Y2Kmeter 现成实现（成本低，可直接抄思路/参数）

| # | 样式 | 效果 | 预览 | 成本 |
|---|---|---|---|---|
| A1 | **频谱瀑布图 Spectrogram** | 时间从右往左滚动的像素方格热力图（复古像素风），频率纵轴、亮度=能量 | 维基：https://en.wikipedia.org/wiki/Spectrogram ；本地实物：跑 Y2Kmeter → Spectrogram 模块（`~/CodingProgram/AudioVisualizer/Y2Kmeter/source/ui/modules/SpectrogramModule.cpp`） | M |
| A2 | **3D 瀑布** | 瀑布图的伪 3D 透视版（Y2Kmeter 有 Spectrogram3D） | Y2Kmeter Spectrogram3D 模块 | L |
| A3 | **示波器波形 / X-Y 李萨如** | 实时波形线；或左右声道合成的李萨如图形 | 维基：https://en.wikipedia.org/wiki/Lissajous_curve ；Y2Kmeter OscilloscopeModule | M |
| A4 | **VU 指针表** | 复古模拟指针电平表（Y2Kmeter VuMeterModule），可作装饰图层 | Y2Kmeter 实物 | M |
| A5 | **Milkdrop 全屏图层** | libprojectM 音频反应式迷幻视觉（我们已预留 dll 目录） | https://github.com/projectM-visualizer/projectm （仓库首页有动图）；Y2Kmeter Milkdrop 模块实物 | L |

## B. 自创/经典移植（无现成参照，从零画）

| # | 样式 | 效果 | 成本 |
|---|---|---|---|
| B1 | **径向频谱 Ring** | 频谱柱绕圆心放射排列（正/内反向双层），可随低频脉动缩放——主流可视化器最抓眼样式 | M |
| B2 | **LED 点阵条** | 复古 LED 电平条（分段点阵 + 峰值保持），配 Pixel 字体更像 | S |
| B3 | **波形进度条** | 整首歌的静态波形 + 播放进度覆盖（可导出全曲时间轴动画） | M |
| B4 | **粒子星域** | 音频能量驱动的粒子喷泉/星域（速度/颜色映射频段） | M |
| B5 | **文字驱动** | 用户文字按频谱形变（轮廓随 band 起伏） | L |
| B6 | **地形剖面 Terrace** | 多层历史曲线堆叠成"山丘地形"（每层为过去 N 帧的曲线，向下透明渐隐） | M |

## C. 建议实施顺序（拍板后）
1. **B1 径向频谱**（最出效果、独立性强，与基线轴/蒙版体系兼容）
2. **A1 瀑布图**（Y2Kmeter 参照在手）
3. **A3 示波器**（装饰图层价值高）
4. 其余按你圈选。

> 兼容性注记：新样式默认接齐 ColorMap / 基线轴 / 蒙版 / 变换四件套（bar 系现成机制平移）。
