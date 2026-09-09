// =============================================================================
// SpectrumParams.h — 参数结构 + JSON 序列化 + CLI 覆盖
//
// 设计意图：
//   · 一份结构承载所有可调参数（FFT / 频率映射 / 时间 / 动态强度 / 视觉 / 输出）。
//   · JSON 文件作为"预设模板"，CLI --set 与糖 flag 可任意覆盖。
//   · 应用顺序：默认值 → --config JSON → --set / 糖（按出现顺序）。
//
// 后续大工程衔接：剪辑软件可在 UI 里把 SpectrumParams 暴露成滑块组，
// 满意后导出 preset.json 给 CLI 用，或直接传给 SpectrumCore / SpectrumStyle。
// =============================================================================
#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>  // Colour
#include <vector>
#include "VisTransform.h"                 // 频谱元素自由变换

// 图片图层（v0.4.2）：一张静态图片作为画布元素，与频谱共用同一套变换/交互
struct ImageLayer
{
    juce::String path;             // 图片文件绝对路径
    VisTransform transform;        // 与频谱同一套变换（输出分辨率坐标系）
    float opacity = 1.0f;          // 0..1
    bool  aboveSpectrum = false;   // false = 频谱下方（背景），true = 频谱上方（前景）
    bool  visible = true;
};

// 频谱蒙版图片（v0.5.4）：图片只在"频谱轮廓"覆盖到的区域可见——
//   频谱填充区变成一扇"窗口"，图片从窗口里透出来；频谱自身退为可选描边。
//   与频谱元素绑定（跟随 p.transform 一起拖动/缩放/旋转）；offset/scale 是
//   图片在轮廓 bbox 内的相对微调（GUI"编辑图片"模式下拖动改这两个）。
struct MaskImageLayer
{
    bool  enabled = false;
    juce::String path;
    float offsetX = 0.0f;          // 相对频谱 bbox 中心额外平移（输出像素）
    float offsetY = 0.0f;
    float scale   = 1.0f;          // 相对"铺满 bbox(cover)"的额外缩放
    // 描边（沿轮廓内侧勾边）
    bool  strokeEnabled   = false;
    float strokeWidth     = 2.0f;
    bool  strokeAutoColor = true;  // true = 用图片平均色；false = 用 strokeColor
    juce::Colour strokeColor { 0xffffffff };
};

struct SpectrumParams
{
    // ---- FFT ----
    int  fftOrder          = 11;        // fftSize = 1<<11 = 2048
    int  fftOrderLo        = 13;        // fftSizeLo = 8192
    bool enableLowFreqPath = true;
    float crossoverHz      = 500.0f;
    int  hopSize           = 0;         // 0 = no overlap (Y2K 主路默认)
    int  hopSizeLo         = 0;         // 0 = fftSizeLo/4 (75% overlap)
    enum WindowFunc { Hann, Hamming, Blackman, BlackmanHarris, Rectangular } windowFunc = Hann;

    // ---- 频率映射 ----
    float minHz   = 20.0f;
    float maxHz   = 20000.0f;
    enum FreqScale { Log, Linear, Mel, Bark } freqScale = Log;
    int bandCount = 160;

    // ---- 时间 ----
    double fps                 = 30.0;
    float  attackMs            = 30.0f;    // 由 Y2K alpha=0.55 @30fps 反推 ≈ 30ms
    float  releaseMs          = 200.0f;   // Y2K alpha=0.12 @30fps 反推 ≈ 250ms，取 200
    float  peakHoldMs         = 3500.0f;  // Y2K 原值
    float  peakDecayDbPerSec  = 12.0f;    // Y2K 原值
    float  peakDecayAccelDbPerSec2 = 0.0f;  // v0.5.3: 峰值帽下落加速度 dB/s²（0=匀速=旧行为）
    float  temporalSmoothing  = 0.5f;     // 0..1 列间模糊强度（0=关闭）

    // ---- 动态强度（y 轴非线性）----
    enum DynCurve { LinearDyn, Sqrt, LogLog, Perceptual } dynCurve = LinearDyn;
    float dynGain  = 1.0f;        // 乘到 (db - minDb) 上：>1 = 跳得更猛
    float dynGamma = 1.0f;        // pow(normalized, gamma)：gamma<1 = 压低端=跳，gamma>1 = 抑制
    bool  slopeEnabled    = true;
    float slopeDbPerOct   = 4.5f;
    float minDb           = -80.0f;
    float maxDb           = 0.0f;

    // ---- 视觉 ----
    juce::String style         = "y2k-line";
    juce::String colorMap      = "solid";
    juce::Colour primaryColor  { 0xffec4899 };
    juce::Colour secondaryColor{ 0xfff9a8d4 };
    juce::Colour peakColor     { 0xffbe185d };
    juce::Colour bgColor       { 0x00000000 };   // 默认全透明
    float lineWidth = 1.4f;
    float opacity   = 1.0f;
    float barGapRatio   = 0.28f;   // bar 样式：柱间空隙占每带 slot 宽度的比例（0 = 无缝）
    float barWidthRatio = 1.0f;    // bar 样式：柱宽占 (slot - gap) 的比例（>1 时相邻柱可重叠）
    bool  barParticles  = true;    // bar / bar-line 样式：峰值帽（下落小横线）开关，false = 只留柱体
    bool  drawGrid        = false;     // 可视化视频默认不画坐标轴（需要时 CLI 开 --draw-grid on）
    bool  drawAxisLabels  = false;     // 同上（--draw-axis-labels on）

    // ---- 频谱元素变换（GUI 画布自由拖动/缩放/旋转；导出所见即所得）----
    VisTransform transform;            // set=false = 填满画布（旧行为）

    // ---- 图片图层（统一 z 序模型；频谱插在 images[spectrumIndex-1] 与 images[spectrumIndex] 之间）----
    std::vector<ImageLayer> images;
    bool spectrumPresent = true;       // false = 频谱被删除（可一键恢复）
    int  spectrumIndex   = 0;          // 统一 z 序中"频谱之下"的图片数量（上方 = N - spectrumIndex）
    bool snapEnabled     = true;       // 吸附开关（旋转 + 移动）

    // ---- 频谱蒙版图片（v0.5.4）----
    MaskImageLayer maskImage;

    // ---- 输出 ----
    int width  = 1280;
    int height = 720;
    enum Encoder { PngSeq, WebmVp9, MovQtrle } encoder = PngSeq;
    int  digits  = 6;
    juce::String baseName  = "frame_";
    juce::String outputDir = "out_frames";
    juce::String outputVideoPath;          // WebM/MOV 用；PNG-seq 模式下空
    juce::String ffmpegPath;               // 可空 = 自动搜索
    bool bgCheckerboardPreview = false;     // --preview-frame 时叠加棋盘格

    // ---- 音频 ----
    juce::String audioPath;

    // ---- 便捷 ----
    int fftSize()   const noexcept { return 1 << fftOrder; }
    int fftSizeLo() const noexcept { return 1 << fftOrderLo; }

    // ---- JSON I/O ----
    // 解析 JSON 文本填充本结构（缺失字段保留默认值）。
    // 解析失败返回 false 并把错误填入 errorMessage（不抛异常）。
    static SpectrumParams fromJson (const juce::String& jsonText,
                                    juce::String& errorMessage);
    // 序列化为完整 JSON（含所有字段，便于作为 preset 模板）。
    juce::String toJson() const;

    // ---- CLI 覆盖（点路径 key=val）----
    // 例: "fft.fftOrder=12", "visual.style=bar", "visual.primaryColor=#ff00ff"
    // 支持的 value 类型: int / float / bool / string / #rrggbb / #aarrggbb
    // 应用成功返回 true；未知 key 或非法 value 返回 false 并填 errorMessage。
    bool applyOverride (const juce::String& dottedKey,
                        const juce::String& value,
                        juce::String& errorMessage);

    // 工具：枚举字符串解析
    static WindowFunc  parseWindowFunc (const juce::String& s, bool* ok = nullptr);
    static FreqScale   parseFreqScale  (const juce::String& s, bool* ok = nullptr);
    static DynCurve    parseDynCurve   (const juce::String& s, bool* ok = nullptr);
    static Encoder     parseEncoder    (const juce::String& s, bool* ok = nullptr);
    static juce::String windowFuncName (WindowFunc v);
    static juce::String freqScaleName  (FreqScale v);
    static juce::String dynCurveName   (DynCurve v);
    static juce::String encoderName    (Encoder v);
};
