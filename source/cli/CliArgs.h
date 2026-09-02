// =============================================================================
// CliArgs.h — CLI 参数解析（--config / --set / 糖 flag → SpectrumParams）
//
// 解析顺序（applyToParams）：
//   1) --config <path>    —— 从 JSON 加载（最先应用）
//   2) --set key=value    —— 通用点路径覆盖（可多次）
//   3) 糖 flag（--width / --fps / --style / ...）—— 按出现顺序覆盖
// 后者覆盖前者。
//
// 另外解析非参数类 flag：
//   --preview-frame <out.png>   —— 单帧预览模式（dump 第 N 帧为 PNG）
//   --probe-spectrum <audio> <frameIdx>  —— 数值调试模式
//   --frame-index N             —— 配合 --preview-frame 选择第 N 帧
//   --gen-tone / --probe-pcm    —— 保留（与 v1 一致）
// =============================================================================
#pragma once

#include <juce_core/juce_core.h>
#include "../core/SpectrumParams.h"

struct CliArgs
{
    // 模式（互斥）
    enum class Mode {
        Help,            // 无参数 / --help
        GenTone,         // --gen-tone <out.wav>
        ProbePcm,        // --probe-pcm <in.wav>
        ProbeSpectrum,   // --probe-spectrum <audio> <frameIdx>
        PreviewFrame,    // --preview-frame <out.png>
        Export,          // --export <audio> <out_dir|out_video>
    };

    Mode mode = Mode::Help;

    // 模式参数
    juce::String genToneOut;
    juce::String probePcmIn;
    juce::String probeSpectrumAudio;
    int probeSpectrumFrameIdx = 0;
    juce::String previewFrameOut;
    int frameIndex = 0;

    // 参数系统
    SpectrumParams params;
    juce::String configPath;          // --config
    juce::StringArray setOverrides;   // --set key=value（按出现顺序）
    juce::StringArray sugarOverrides; // 糖 flag 转成的 key=value（按出现顺序）

    // 解析错误信息（applyToParams 后填充）
    juce::String errorMessage;

    // 解析 argv。失败返回 false 并填 errorMessage。
    bool parse (int argc, char** argv);

    // 把 --config + --set + 糖 应用到 params（按计划顺序）。
    // 失败返回 false 并填 errorMessage。
    bool applyToParams();

    // 打印帮助
    static juce::String helpText();
};
