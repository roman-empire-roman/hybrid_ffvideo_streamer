#ifndef VIDEO_STREAMER_H
#define VIDEO_STREAMER_H

#include <memory>
#include <optional>
#include <pybind11/pybind11.h>

#include "timeout_checker.h"

extern "C" {
    struct AVCodec;
    struct AVCodecContext;
    struct AVFilterContext;
    struct AVFilterGraph;
    struct AVFormatContext;
    struct AVFrame;
    struct AVPacket;
}

extern "C" {
    #include <libavutil/pixfmt.h>
}

class VideoStreamer {
public:
    VideoStreamer();

    VideoStreamer(const VideoStreamer& other) = delete;
    VideoStreamer& operator=(const VideoStreamer& other) = delete;
    ~VideoStreamer();
    VideoStreamer(VideoStreamer&& other) = delete;
    VideoStreamer& operator=(VideoStreamer&& other) = delete;

    bool setup(std::string configFileName);
    bool process();

private:
    bool parseConfig(const std::string& configFileName);
    bool encodeWriteFrame(bool readyToFlush, AVFrame* filteredFrame);
    bool filterEncodeWriteFrame(AVFrame* decoderFrame, AVFrame* filteredFrame);
    bool flushEncoder(AVFrame* filteredFrame);
    void deallocateResources();
    std::optional<const AVPixelFormat> getPixelFormat(const AVCodec* encoder) const;

private:
    AVFormatContext* m_inputContext = nullptr;
    int m_videoStreamIndex = -1;

    AVCodecContext* m_decoderContext = nullptr;

    AVFilterContext* m_bufferSrcContext = nullptr;
    AVFilterGraph* m_filterGraph = nullptr;
    AVFilterContext* m_bufferSinkContext = nullptr;

    AVCodecContext* m_encoderContext = nullptr;
    AVPacket* m_encoderPacket = nullptr;

    AVFormatContext* m_outputContext = nullptr;

    std::shared_ptr<TimeoutChecker> m_timeoutChecker{ nullptr };

    struct ConfigParams {
        std::string inputStreamName;
        std::optional<std::string> watermarkLocation{ std::nullopt };
        std::string rtmpUrl;
        int ffmpegLogLevel = 0;
    };
    ConfigParams m_configParams;
};

PYBIND11_MODULE(video_streamer, streaming_module) {
    pybind11::class_<VideoStreamer>(streaming_module, "VideoStreamer")
        .def(pybind11::init<>())
        .def("setup", &VideoStreamer::setup)
        .def("process", &VideoStreamer::process);
}

#endif /* VIDEO_STREAMER_H */
