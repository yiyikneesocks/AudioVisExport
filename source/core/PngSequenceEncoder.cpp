// =============================================================================
// PngSequenceEncoder.cpp — 帧序列 → PNG 文件 + 最后 ffmpeg 合成视频
//
// 数据流：
//   startSession → 每帧 writeFrame(juce::Image) → finalizeAndMux
//
// PNG 写入：
//   JUCE PNGImageFormat::writeImageToStream(FileOutputStream)
//
// ffmpeg 调用：
//   juce::ChildProcess 调 ffmpeg.exe (hinted / env FFMPEG_PATH / PATH)
//   · MOV:  qtrle + rgba  → 无损 alpha（实测 pix_fmt=argb）
//   · WebM: libvpx-vp9    → 无 alpha 预览片（libvpx 未开 VP9-alpha 的构建实测丢 alpha）
//   · PNG-seq: 不调 ffmpeg
// =============================================================================
#include "PngSequenceEncoder.h"

PngSequenceEncoder::~PngSequenceEncoder() = default;

bool PngSequenceEncoder::startSession (const Config& cfg)
{
    if (cfg.width <= 0 || cfg.height <= 0 || cfg.fps <= 0 || cfg.digits < 4)
        return false;
    if (cfg.outputDir.isEmpty())
        return false;

    juce::File dir (cfg.outputDir);
    auto ok = dir.createDirectory();
    if (! ok.wasOk())
        return false;

    // 清空已存在的同名 PNG 文件，避免新旧序列混合。
    // 如果 dir 本身是一个文件（不是目录），createDirectory 上面已经失败，不会到这。
    juce::Array<juce::File> matches;
    dir.findChildFiles (matches, juce::File::findFiles, false,
                        cfg.baseName + "*" + ".png");
    for (auto& f : matches)
        f.deleteFile();

    cfg_ = cfg;
    frameIndex_ = 0;
    return true;
}

void PngSequenceEncoder::writeFrame (const juce::Image& frame)
{
    jassert (frame.isValid());
    jassert (frame.getWidth() == cfg_.width && frame.getHeight() == cfg_.height);

    const auto path = getFrameFilePath (frameIndex_);
    juce::File f (path);
    auto stream = f.createOutputStream (1 << 16);  // 64KB buffer
    if (stream == nullptr)
    {
        ++frameIndex_;  // 保持单调计数，即使写失败
        return;
    }

    juce::PNGImageFormat png;
    png.writeImageToStream (frame, *stream);

    ++frameIndex_;
}

int PngSequenceEncoder::finalizeAndMux (const juce::String& audioPath,
                                         const juce::String& outputVideoPath,
                                         const juce::String& ffmpegPath)
{
    // PNG-seq 模式不需要 mux；空输出路径属于调用方错误
    if (cfg_.encoder == SpectrumParams::PngSeq)
    {
        printf ("[PngSequenceEncoder] finalizeAndMux skipped (PNG-seq mode)\n");
        return 0;
    }
    if (outputVideoPath.isEmpty())
    {
        printf ("[PngSequenceEncoder] FATAL: outputVideoPath is empty\n");
        return -3;
    }

    const auto exe = findFfmpeg_ (ffmpegPath);
    if (exe.isEmpty())
    {
        printf ("[PngSequenceEncoder] FATAL: ffmpeg.exe not found (hint='%s' / env FFMPEG_PATH / PATH)\n",
                ffmpegPath.toRawUTF8());
        return -2;
    }

    // 输出视频可能不存在（正常），也可能上次遗留 → 先删（ffmpeg 自己也会 -y 覆盖，这里保险）
    juce::File out (outputVideoPath);
    if (out.existsAsFile())
        out.deleteFile();
    // 保证父目录存在
    out.getParentDirectory().createDirectory();

    const auto args = buildFfmpegArgs_ (audioPath, outputVideoPath);
    if (args.isEmpty())
    {
        printf ("[PngSequenceEncoder] FATAL: empty ffmpeg args (encoder=%s)\n",
                SpectrumParams::encoderName (cfg_.encoder).toRawUTF8());
        return -3;
    }

    // JUCE ChildProcess::start(const StringArray&): arguments[0] = 可执行文件路径，
    // arguments[1..N] = 命令行参数。
    juce::StringArray allArgs;
    allArgs.add (exe);
    allArgs.addArray (args);

    printf ("[PngSequenceEncoder] running ffmpeg: %s\n",
            exe.toRawUTF8());
    printf ("[PngSequenceEncoder] args: %s\n",
            args.joinIntoString (" ").toRawUTF8());

    juce::ChildProcess proc;
    const bool started = proc.start (allArgs);
    if (! started)
    {
        printf ("[PngSequenceEncoder] FATAL: ChildProcess::start failed for ffmpeg\n");
        return -3;
    }
    printf ("[PngSequenceEncoder] child launched, waiting for exit...\n");
    fflush (stdout);

    // 增量 drain 输出，避免 stderr 管道满导致子进程阻塞（经典 deadlock）。
    // 注意：JUCE ChildProcess 在 Windows 下合并 stdout/stderr 走同一管道，
    //       readAllProcessOutput 非阻塞读取当前 buffer 中就绪内容即可。
    juce::String combinedOutput;
    bool finished = false;
    int elapsedMs = 0;
    constexpr int kPollMs = 200;
    constexpr int kTimeoutMs = 60 * 60 * 1000;  // 1 小时上限
    while (! finished && elapsedMs < kTimeoutMs)
    {
        finished = proc.waitForProcessToFinish (kPollMs);
        juce::String chunk = proc.readAllProcessOutput();
        if (chunk.isNotEmpty())
            combinedOutput += chunk;
        if (! finished)
            elapsedMs += kPollMs;
    }
    // 最后再 drain 一次（进程退出后尾部数据）
    {
        juce::String chunk = proc.readAllProcessOutput();
        if (chunk.isNotEmpty())
            combinedOutput += chunk;
    }

    if (combinedOutput.isNotEmpty())
    {
        printf ("----- ffmpeg output begin -----\n");
        printf ("%s", combinedOutput.toRawUTF8());
        printf ("----- ffmpeg output end -----\n");
        fflush (stdout);
    }

    if (! finished)
    {
        printf ("[PngSequenceEncoder] FATAL: ffmpeg timed out (>1h), killing...\n");
        proc.kill();
        return -4;
    }

    const int exitCode = proc.getExitCode();
    if (exitCode == 0)
    {
        printf ("[PngSequenceEncoder] OK: video written -> %s (%s)\n",
                outputVideoPath.toRawUTF8(),
                out.existsAsFile() ? juce::String::formatted ("size=%lld bytes",
                                                              (long long)out.getSize()).toRawUTF8()
                                   : "FILE NOT FOUND!");
    }
    else
    {
        printf ("[PngSequenceEncoder] FAIL: ffmpeg exitCode=%d\n", exitCode);
    }
    return exitCode;
}

juce::String PngSequenceEncoder::getFrameFilePath (int index) const
{
    auto padded = juce::String (index).paddedLeft ('0', cfg_.digits);
    return cfg_.outputDir
         + juce::File::getSeparatorString()
         + cfg_.baseName
         + padded
         + ".png";
}

juce::String PngSequenceEncoder::findFfmpeg_ (const juce::String& hint) const
{
    // 1) hint 直接用（可能是绝对路径或相对路径）
    if (hint.isNotEmpty())
    {
        juce::File f (hint);
        if (f.existsAsFile())
            return f.getFullPathName();
#if JUCE_WINDOWS
        // hint 可能没带 .exe 后缀；Windows 需要完整文件名才能 ChildProcess::start
        if (! hint.endsWithIgnoreCase (".exe"))
        {
            juce::File f2 (hint + ".exe");
            if (f2.existsAsFile())
                return f2.getFullPathName();
        }
#endif
    }

    // 2) 环境变量 FFMPEG_PATH
    auto env = juce::SystemStats::getEnvironmentVariable ("FFMPEG_PATH", {});
    if (env.isNotEmpty())
    {
        juce::File f (env);
        if (f.existsAsFile())
            return f.getFullPathName();
#if JUCE_WINDOWS
        if (! env.endsWithIgnoreCase (".exe"))
        {
            juce::File f2 (env + ".exe");
            if (f2.existsAsFile())
                return f2.getFullPathName();
        }
        // 也可能 FFMPEG_PATH 是目录，里面有 ffmpeg.exe
        if (f.isDirectory())
        {
            juce::File fexe = f.getChildFile ("ffmpeg.exe");
            if (fexe.existsAsFile())
                return fexe.getFullPathName();
        }
#endif
    }

    // 3) 遍历 PATH 找 ffmpeg(.exe)
    auto pathEnv = juce::SystemStats::getEnvironmentVariable ("PATH", {});
    juce::StringArray dirs;
#if JUCE_WINDOWS
    dirs.addTokens (pathEnv, ";", {});
    const juce::String exeName = "ffmpeg.exe";
#else
    dirs.addTokens (pathEnv, ":", {});
    const juce::String exeName = "ffmpeg";
#endif
    int dirIndex = 0;
    for (const auto& dirRaw : dirs)
    {
        // 清理：去掉前后空白和可能的引号（系统 PATH 偶尔有 "D:\foo" 带引号的项）
        auto dir = dirRaw.trim();
        if (dir.isEmpty()) { ++dirIndex; continue; }
        while (dir.isNotEmpty() && (dir.startsWithChar ('"') || dir.startsWithChar ('\'') ||
                                    dir.endsWithChar   ('"') || dir.endsWithChar   ('\'')))
        {
            if (dir.startsWithChar ('"') || dir.startsWithChar ('\'')) dir = dir.substring (1).trimEnd();
            if (dir.endsWithChar   ('"') || dir.endsWithChar   ('\'')) dir = dir.dropLastCharacters (1).trimStart();
        }
        // 基本合法性过滤：绝对路径必须非空且长度 <= MAX_PATH；必须含路径分隔符
        if (dir.isEmpty() || dir.length() > 512) { ++dirIndex; continue; }
        if (! dir.containsChar (juce::File::getSeparatorChar())) { ++dirIndex; continue; }

        // Windows：跳过潜在的离线路径（以 \\ 开头的 UNC、未映射的网络盘）以避免 exists*
        // 调用卡几十秒。先做 cheap 测试：如果是 UNC \\server\share 或 drive letter
        // 不在常用列表里，先尝试快速 PathFileExists (JUCE 内部会做类似判断，但我们
        // 打印每个 dir 便于诊断 hang)。
        printf ("[findFfmpeg_] dir[%d] = %s\n", dirIndex, dir.toRawUTF8());
        fflush (stdout);
        ++dirIndex;

        juce::File d (dir);
        if (! d.isDirectory()) continue;
        juce::File f = d.getChildFile (exeName);
        if (f.existsAsFile())
        {
            printf ("[findFfmpeg_]  FOUND -> %s\n", f.getFullPathName().toRawUTF8());
            fflush (stdout);
            return f.getFullPathName();
        }
    }

    printf ("[findFfmpeg_] not found anywhere (hint/env/PATH scan complete)\n");
    fflush (stdout);
    return {};
}

juce::StringArray PngSequenceEncoder::buildFfmpegArgs_ (const juce::String& audioPath,
                                                        const juce::String& outputVideoPath) const
{
    // 注意：这里 args 不包含可执行文件路径。调用方会用 findFfmpeg_() 的结果
    // 作为 ChildProcess::start 的第一个参数。
    juce::StringArray args;

    args.add ("-y");  // 覆盖输出

    // 视频输入：PNG 序列
    args.add ("-framerate");
    args.add (juce::String (cfg_.fps));
    const auto pattern = cfg_.outputDir
                       + juce::File::getSeparatorString()
                       + cfg_.baseName
                       + "%0" + juce::String (cfg_.digits) + "d.png";
    args.add ("-i");
    args.add (pattern);

    // 音频输入（可以为空 = 纯视频无音轨）
    const bool hasAudio = audioPath.isNotEmpty();
    if (hasAudio)
    {
        args.add ("-i");
        args.add (audioPath);
    }

    switch (cfg_.encoder)
    {
        case SpectrumParams::WebmVp9:
        {
            // WebM + VP9 —— **注意：此格式不保留 alpha 通道**。
            // 实测（gyan.dev ffmpeg 2026-01）libvpx-vp9 虽声明支持 yuva420p，
            // 但实际编码时自动降级为 yuv420p，alpha 被丢弃。
            // WebM 真正的带 alpha 方案需要 libvpx 编译时开 VP9 alpha / VP8 alpha，
            // 绝大多数发行版 FFmpeg 未启用 → 这里作为"小体积无透明预览片"使用。
            // 带 alpha 的视频导出请用 MOV QTRLE（见下表）。
            args.add ("-c:v");      args.add ("libvpx-vp9");
            args.add ("-crf");      args.add ("32");
            args.add ("-b:v");      args.add ("0");
            args.add ("-deadline"); args.add ("good");
            args.add ("-cpu-used"); args.add ("2");
            args.add ("-row-mt");   args.add ("1");
            if (hasAudio)
            {
                args.add ("-c:a");  args.add ("libopus");
                args.add ("-b:a");  args.add ("160k");
            }
            break;
        }
        case SpectrumParams::MovQtrle:
        {
            // MOV + QTRLE：rgba 无损保留 alpha（实测输出 pix_fmt=argb），
            // Premiere/Vegas/Resolve 均可直接导入。这才是"透明视频"的标准格式。
            // PCM 音轨为 MOV 原生无损格式。文件体积大属于正常。
            args.add ("-c:v");      args.add ("qtrle");
            args.add ("-pix_fmt");  args.add ("rgba");
            if (hasAudio)
            {
                args.add ("-c:a");  args.add ("pcm_s16le");
            }
            break;
        }
        case SpectrumParams::PngSeq:
        default:
            // PNG 序列模式不调 ffmpeg；finalizeAndMux 已提前拦截
            return {};
    }

    // 当音频比视频短时提前结束
    if (hasAudio)
        args.add ("-shortest");

    args.add (outputVideoPath);
    return args;
}
