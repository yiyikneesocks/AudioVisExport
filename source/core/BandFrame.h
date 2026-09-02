// =============================================================================
// BandFrame.h — 频谱单帧数据（header-only）
//
// 由 SpectrumCore::getBandFrame 输出，作为 SpectrumStyle::render 的输入。
// 每个带含三组数据：
//   · db         —— 平滑 + slope 后的 dB（clamped [minDb, maxDb]）
//   · peakDb     —— 峰值保持 dB
//   · normalized —— 动态曲线后 [0,1]，直接喂给 style 画
// =============================================================================
#pragma once

#include <vector>

struct BandFrame
{
    int bandCount = 0;
    std::vector<float> db;          // 平滑 + slope 后的 dB
    std::vector<float> peakDb;      // 峰值保持
    std::vector<float> normalized;  // 动态曲线后 [0,1]

    void resize (int n)
    {
        bandCount = n;
        db.assign (n, 0.0f);
        peakDb.assign (n, 0.0f);
        normalized.assign (n, 0.0f);
    }
};
