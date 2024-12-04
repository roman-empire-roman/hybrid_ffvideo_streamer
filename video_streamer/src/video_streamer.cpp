#include "video_streamer.h"

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavdevice/avdevice.h>
    #include <libavfilter/avfilter.h>
    #include <libavfilter/buffersink.h>
    #include <libavfilter/buffersrc.h>
    #include <libavformat/avformat.h>
    #include <libavutil/error.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/opt.h>
}

#define RAPIDJSON_SSE2
#include <rapidjson/document.h>

#include <algorithm>
#include <cstring>
#include <frozen/string.h>
#include <frozen/unordered_map.h>
#include <iostream>
#include <span>

#include "common_functions.h"
#include "ptr_wrapper.h"
#include "signal_number_setter.h"
#include "simple_wrapper.h"

namespace {
    constexpr int g_frameRate = 30;
    constexpr int g_frameWidth = 640;
    constexpr int g_frameHeight = 480;
    constexpr const char* g_outputStreamFormat = "flv";
    constexpr AVCodecID g_encoderId = AVCodecID::AV_CODEC_ID_H264;
    constexpr unsigned int g_watermarkWidth = 45;
    constexpr unsigned int g_watermarkHeight = 45;

    constexpr frozen::unordered_map<frozen::string, int, 9> g_logLevels = {
        { "quiet", AV_LOG_QUIET },
        { "panic", AV_LOG_PANIC },
        { "fatal", AV_LOG_FATAL },
        { "error", AV_LOG_ERROR },
        { "warning", AV_LOG_WARNING },
        { "info", AV_LOG_INFO },
        { "verbose", AV_LOG_VERBOSE },
        { "debug", AV_LOG_DEBUG },
        { "trace", AV_LOG_TRACE } // default log level
    };
}

VideoStreamer::VideoStreamer() {
    avdevice_register_all();
    avformat_network_init();

    m_timeoutChecker = std::make_shared<TimeoutChecker>();
}

VideoStreamer::~VideoStreamer() {
    deallocateResources();
    m_timeoutChecker.reset();
    avformat_network_deinit();
}

bool VideoStreamer::setup(std::string configFileName) {
    using namespace PtrWrapperSpace;

    if (configFileName.empty()) {
        std::cerr << "{VideoStreamer::setup}; configuration file name is empty" << std::endl;
        return false;
    }
    if (m_inputContext) {
        std::cerr << "{VideoStreamer::setup}; input context is already set" << std::endl;
        return false;
    }
    if (-1 != m_videoStreamIndex) {
        std::cerr << "{VideoStreamer::setup}; video stream index is already set" << std::endl;
        return false;
    }
    if (m_decoderContext) {
        std::cerr << "{VideoStreamer::setup}; decoder context is already set" << std::endl;
        return false;
    }
    if (m_outputContext) {
        std::cerr << "{VideoStreamer::setup}; output context is already set" << std::endl;
        return false;
    }
    if (m_filterGraph) {
        std::cerr << "{VideoStreamer::setup}; filter graph is already set" << std::endl;
        return false;
    }
    if (m_encoderPacket) {
        std::cerr << "{VideoStreamer::setup}; encoder packet is already set" << std::endl;
        return false;
    }
    if (m_encoderContext) {
        std::cerr << "{VideoStreamer::setup}; encoder context is already set" << std::endl;
        return false;
    }
    if (m_bufferSrcContext) {
        std::cerr << "{VideoStreamer::setup}; buffer src context is already set" << std::endl;
        return false;
    }
    if (m_bufferSinkContext) {
        std::cerr << "{VideoStreamer::setup}; buffer sink context is already set" << std::endl;
        return false;
    }
    if (nullptr == m_timeoutChecker) {
        std::cerr << "{VideoStreamer::setup}; pointer to timeout checker is NULL" << std::endl;
        return false;
    }

    SignalNumberSetter::getInstance();

    if (!parseConfig(configFileName)) {
        return false;
    }

    if (!m_timeoutChecker->setup()) {
        return false;
    }

    auto logger = [] (
        [[maybe_unused]] void* ptr, [[maybe_unused]] int level,
        const char* format, va_list args
    ) {
        vprintf(format, args);
    };
    av_log_set_level(m_configParams.ffmpegLogLevel);
    auto logLevel = av_log_get_level();
    if (logLevel != m_configParams.ffmpegLogLevel) {
        std::cerr << "{VideoStreamer::setup}; FFmpeg log level was NOT set" << std::endl;
        return false;
    }
    av_log_set_callback(logger);

    auto openResult = avformat_open_input(
        &m_inputContext, m_configParams.inputStreamName.c_str(), nullptr, nullptr
    );
    if (openResult < 0) {
        std::cerr << "{VideoStreamer::setup}; unable to open stream '" << m_configParams.inputStreamName << "'; "
            "open result: '" << openResult << " (" << av_err2str(openResult) << ")'" << std::endl;
        return false;
    }
    if (nullptr == m_inputContext) {
        std::cerr << "{VideoStreamer::setup}; pointer to input context is NULL" << std::endl;
        return false;
    }

    auto readResult = avformat_find_stream_info(m_inputContext, nullptr);
    if (readResult < 0) {
        std::cerr << "{VideoStreamer::setup}; unable to read packets from input context to get stream information; "
            "read result: '" << readResult << " (" << av_err2str(readResult) << ")'" << std::endl;
        return false;
    }

    if (nullptr == m_inputContext->streams) {
        std::cerr << "{VideoStreamer::setup}; pointer to stream list is NULL" << std::endl;
        return false;
    }
    auto nStreams = static_cast<std::size_t>(m_inputContext->nb_streams);
    for (std::size_t i = 0; i < nStreams; ++i) {
        if (nullptr == m_inputContext->streams[i]) {
            continue;
        }
        if (nullptr == m_inputContext->streams[i]->codecpar) {
            continue;
        }
        auto decoderParameters = m_inputContext->streams[i]->codecpar;
        if (AVMediaType::AVMEDIA_TYPE_VIDEO == decoderParameters->codec_type) {
            m_videoStreamIndex = static_cast<int>(i);
            break;
        }
    }
    if (-1 == m_videoStreamIndex) {
        std::cerr << "{VideoStreamer::setup}; video stream index is NOT set" << std::endl;
        return false;
    }
    auto videoStreamIndex = static_cast<std::size_t>(m_videoStreamIndex);

    auto decoderParameters = m_inputContext->streams[videoStreamIndex]->codecpar;
    if (g_frameWidth != decoderParameters->width) {
        std::cerr << "{VideoStreamer::setup}; frame width was NOT set to '" << g_frameWidth << "' using "
            "qv4l2/guvcview application for video4linux devices; "
            "current frame width: '" << decoderParameters->width << "'" << std::endl;
        return false;
    }
    if (g_frameHeight != decoderParameters->height) {
        std::cerr << "{VideoStreamer::setup}; frame height was NOT set to '" << g_frameHeight << "' using "
            "qv4l2/guvcview application for video4linux devices; "
            "current frame height: '" << decoderParameters->height << "'" << std::endl;
        return false;
    }

    const AVCodec* decoder = avcodec_find_decoder(decoderParameters->codec_id);
    if (nullptr == decoder) {
        std::cerr << "{VideoStreamer::setup}; unable to find registered decoder; "
            "decoder id: '" << decoderParameters->codec_id << "'"<< std::endl;
        return false;
    }

    m_decoderContext = avcodec_alloc_context3(decoder);
    if (nullptr == m_decoderContext) {
        std::cerr << "{VideoStreamer::setup}; unable to allocate memory for decoder context" << std::endl;
        return false;
    }

    auto fillResult = avcodec_parameters_to_context(m_decoderContext, decoderParameters);
    if (fillResult < 0) {
        std::cerr << "{VideoStreamer::setup}; unable to fill decoder context"
            "fill result: '" << fillResult << " (" << av_err2str(fillResult) << ")'" << std::endl;
        return false;
    }

    /* Inform the decoder about the timebase for the packet timestamps.
     * This is highly recommended, but not mandatory. */
    m_decoderContext->pkt_timebase = m_inputContext->streams[videoStreamIndex]->time_base;

    auto guessFrameRate = av_guess_frame_rate(
        m_inputContext, m_inputContext->streams[videoStreamIndex], nullptr
    );

    if ((g_frameRate != guessFrameRate.num) || (1 != guessFrameRate.den)) {
        std::cerr << "{VideoStreamer::setup}; frame rate was NOT set to '" << g_frameRate << "/1' using "
            "qv4l2 or guvcview application for video4linux devices; "
            "estimated frame rate: '" << guessFrameRate.num << "/" << guessFrameRate.den << "'" << std::endl;
        return false;
    }
    m_decoderContext->framerate = guessFrameRate;

    /* Open decoder */
    auto decoderInitResult = avcodec_open2(m_decoderContext, decoder, nullptr);
    if (decoderInitResult < 0) {
        std::cerr << "{VideoStreamer::setup}; unable to initialize decoder context to use the given decoder; "
            "initialize result: '" << decoderInitResult << " (" << av_err2str(decoderInitResult) << ")'" << std::endl;
        return false;
    }

    auto allocationResult = avformat_alloc_output_context2(
        &m_outputContext, nullptr, g_outputStreamFormat, m_configParams.rtmpUrl.c_str()
    );
    if (allocationResult < 0) {
        std::cerr << "{VideoStreamer::setup}; unable to allocate output context; "
            "allocation result: '" << allocationResult << " (" << av_err2str(allocationResult) << ")'" << std::endl;
        return false;
    }
    if (nullptr == m_outputContext) {
        std::cerr << "{VideoStreamer::setup}; pointer to output context is NULL" << std::endl;
        return false;
    }

    AVStream* outputStream = avformat_new_stream(m_outputContext, nullptr);
    if (nullptr == outputStream) {
        std::cerr << "{VideoStreamer::setup}; unable to add new stream" << std::endl;
        return false;
    }

    const AVCodec* encoder = avcodec_find_encoder(g_encoderId);
    if (nullptr == encoder) {
        std::cerr << "{VideoStreamer::setup}; unable to find registered encoder; "
            "encoder id: '" <<  static_cast<int>(g_encoderId) << "'"<< std::endl;
        return false;
    }

    m_encoderContext = avcodec_alloc_context3(encoder);
    if (nullptr == m_encoderContext) {
        std::cerr << "{VideoStreamer::setup}; unable to allocate memory for encoder context" << std::endl;
        return false;
    }

    /* In this example, we transcode to same properties (picture size,
     * sample rate etc.). These properties can be changed for output
     * streams easily using filters */
    m_encoderContext->height = m_decoderContext->height;
    m_encoderContext->width = m_decoderContext->width;
    m_encoderContext->sample_aspect_ratio = m_decoderContext->sample_aspect_ratio;
    /* take first format from list of supported formats */
    auto encoderPixelFormat = getPixelFormat(encoder);
    m_encoderContext->pix_fmt =
        encoderPixelFormat.has_value() ?
            encoderPixelFormat.value() :
            m_decoderContext->pix_fmt;

    /* video time_base can be set to whatever is handy and supported by encoder */
    m_encoderContext->time_base = av_inv_q(m_decoderContext->framerate);

    if (nullptr == m_outputContext->oformat) {
        std::cerr << "{VideoStreamer::setup}; pointer to output format of output context is NULL" << std::endl;
        return false;
    }
    if (AVFMT_GLOBALHEADER & m_outputContext->oformat->flags) {
        m_encoderContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    /* Third parameter can be used to pass settings to encoder */
    auto encoderInitResult = avcodec_open2(m_encoderContext, encoder, nullptr);
    if (encoderInitResult < 0) {
        std::cerr << "{VideoStreamer::setup}; unable to initialize encoder context to use the given encoder; "
            "initialize result: '" << encoderInitResult << " (" << av_err2str(encoderInitResult) << ")'" << std::endl;
        return false;
    }
    auto copyResult = avcodec_parameters_from_context(outputStream->codecpar, m_encoderContext);
    if (copyResult < 0) {
        std::cerr << "{VideoStreamer::setup}; unable to fill encoder context; "
            "copy result: '" << copyResult << " (" << av_err2str(copyResult) << ")'" << std::endl;
        return false;
    }
    outputStream->time_base = m_encoderContext->time_base;

    if (!(AVFMT_NOFILE & m_outputContext->oformat->flags)) {
        {
            auto hostName = CommonFunctions::extractHostNameFromRtmpUrl(m_configParams.rtmpUrl);
            if (!hostName.has_value()) {
                return false;
            }
            if (!CommonFunctions::isHostNameValid(hostName.value())) {
                return false;
            }
        }

        int (*timeoutCallback)(void*) = &TimeoutChecker::onProxyReadyToCheckTimeout;
        auto checkerPtr = static_cast<void*>(m_timeoutChecker.get());
        static const AVIOInterruptCB interruptCallback = {
            .callback = timeoutCallback, .opaque = checkerPtr
        };

        AVDictionary* options = nullptr;
        auto setResult = av_dict_set(&options, "protocol_whitelist", "tcp,rtmp", 0);
        if (setResult < 0) {
            std::cerr << "{VideoStreamer::setup}; unable to set key-value pair; "
                "set result: '" << setResult << " (" << av_err2str(setResult) << ")'" << std::endl;
            return false;
        }

        if (m_outputContext->pb) {
            std::cerr << "{VideoStreamer::setup}; pointer to bytestream output context is already set" << std::endl;
            return false;
        }
        auto initResult = avio_open2(
            &m_outputContext->pb, m_configParams.rtmpUrl.c_str(),
            AVIO_FLAG_WRITE, &interruptCallback, &options
        );
        if (initResult < 0) {
            std::cerr << "{VideoStreamer::setup}; unable to initialize output context; "
                "initialize result: '" << initResult << " (" << av_err2str(initResult) << ")'" << std::endl;
            return false;
        }
        if (options) {
            std::cerr << "{VideoStreamer::setup}; pointer to dictionary is NOT NULL" << std::endl;
            return false;
        }
        if (nullptr == m_outputContext->pb) {
            std::cerr << "{VideoStreamer::setup}; pointer to bytestream output context is NULL" << std::endl;
            return false;
        }
    }

    /* init muxer, write output file header */
    auto writeResult = avformat_write_header(m_outputContext, nullptr);
    if (writeResult < 0) {
        std::cerr << "{VideoStreamer::setup}; unable to write header; "
            "initialize result: '" << writeResult << " (" << av_err2str(writeResult) << ")'" << std::endl;
        return false;
    }

    PtrWrapper<AVFilterInOut> outputsWrapper(
        avfilter_inout_alloc, avfilter_inout_free
    );
    if (nullptr == outputsWrapper.get()) {
        std::cerr << "{VideoStreamer::setup}; unable to allocate memory for linked-list element" << std::endl;
        return false;
    }

    PtrWrapper<AVFilterInOut> inputsWrapper(
        avfilter_inout_alloc, avfilter_inout_free
    );
    if (nullptr == inputsWrapper.get()) {
        std::cerr << "{VideoStreamer::setup}; unable to allocate memory for linked-list element" << std::endl;
        return false;
    }

    m_filterGraph = avfilter_graph_alloc();
    if (nullptr == m_filterGraph) {
        std::cerr << "{VideoStreamer::setup}; unable to allocate memory for filter graph" << std::endl;
        return false;
    }

    const AVFilter* bufferSrc = avfilter_get_by_name("buffer");
    if (nullptr == bufferSrc) {
        std::cerr << "{VideoStreamer::setup}; pointer to buffer src filter definition is NULL" << std::endl;
        return false;
    }

    const AVFilter* bufferSink = avfilter_get_by_name("buffersink");
    if (nullptr == bufferSink) {
        std::cerr << "{VideoStreamer::setup}; pointer to buffer sink filter definition is NULL" << std::endl;
        return false;
    }

    char filterArgs[ 512 ] = { 0 };
    auto printResult = snprintf(
        filterArgs, sizeof(filterArgs),
        "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d:frame_rate=%d/%d",
        m_decoderContext->width, m_decoderContext->height,
        m_decoderContext->pix_fmt,
        m_decoderContext->pkt_timebase.num, m_decoderContext->pkt_timebase.den,
        m_decoderContext->sample_aspect_ratio.num, m_decoderContext->sample_aspect_ratio.den,
        m_decoderContext->framerate.num, m_decoderContext->framerate.den
    );
    if (printResult < 0) {
        std::cerr << "{VideoStreamer::setup}; unable to construct filter argument list" << std::endl;
        return false;
    }

    auto createResult = avfilter_graph_create_filter(
        &m_bufferSrcContext, bufferSrc, "in", filterArgs, nullptr, m_filterGraph
    );
    if (createResult < 0) {
        std::cerr << "{VideoStreamer::setup}; unable to create or add input filter instance into existing graph; "
            "create result: '" << createResult << " (" << av_err2str(createResult) << ")'" << std::endl;
        return false;
    }
    if (nullptr == m_bufferSrcContext) {
        std::cerr << "{VideoStreamer::setup}; pointer to buffer src context is NULL" << std::endl;
        return false;
    }

    createResult = avfilter_graph_create_filter(
        &m_bufferSinkContext, bufferSink, "out", nullptr, nullptr, m_filterGraph
    );
    if (createResult < 0) {
        std::cerr << "{VideoStreamer::setup}; unable to create or add output filter instance into existing graph; "
            "create result: '" << createResult << " (" << av_err2str(createResult) << ")'" << std::endl;
        return false;
    }
    if (nullptr == m_bufferSinkContext) {
        std::cerr << "{VideoStreamer::setup}; pointer to buffer sink context is NULL" << std::endl;
        return false;
    }

    auto& pixelFormat = m_encoderContext->pix_fmt;
    auto castedPtrToPixelFormat = reinterpret_cast<uint8_t*>(
        std::addressof(pixelFormat)
    );
    auto setResult = av_opt_set_bin(
        static_cast<void*>(m_bufferSinkContext), "pix_fmts", castedPtrToPixelFormat,
        static_cast<int>(
            sizeof(m_encoderContext->pix_fmt)
        ),
        static_cast<int>(AV_OPT_SEARCH_CHILDREN)
    );
    if (setResult < 0) {
        std::cerr << "{VideoStreamer::setup}; unable to set pixel format; "
            "set result: '" << setResult << " (" << av_err2str(setResult) << ")'" << std::endl;
        return false;
    }

    /* Endpoints for the filter graph. */
    outputsWrapper->name       = av_strdup("in");
    outputsWrapper->filter_ctx = m_bufferSrcContext;
    outputsWrapper->pad_idx    = 0;
    outputsWrapper->next       = nullptr;

    inputsWrapper->name       = av_strdup("out");
    inputsWrapper->filter_ctx = m_bufferSinkContext;
    inputsWrapper->pad_idx    = 0;
    inputsWrapper->next       = nullptr;

    if (nullptr == outputsWrapper->name) {
        std::cerr << "{VideoStreamer::setup}; pointer to outputs name is NULL" << std::endl;
        return false;
    }

    if (nullptr == inputsWrapper->name) {
        std::cerr << "{VideoStreamer::setup}; pointer to inputs name is NULL" << std::endl;
        return false;
    }

    char filterDescription[ 512 ] = { 0 };
    if (m_configParams.watermarkLocation) {
        printResult = snprintf(
            filterDescription, sizeof(filterDescription),
            "movie=%s [wm];[in][wm] overlay=10:main_h-overlay_h-10 [out]",
            m_configParams.watermarkLocation.value().c_str()
        );
    } else {
        printResult = snprintf(
            filterDescription, sizeof(filterDescription), "null"
        );
    }
    if (printResult < 0) {
        std::cerr << "{VideoStreamer::setup}; unable to construct filter description" << std::endl;
        return false;
    }

    auto parseResult = avfilter_graph_parse_ptr(
        m_filterGraph, filterDescription, inputsWrapper.getAddress(), outputsWrapper.getAddress(), nullptr
    );
    if (parseResult < 0) {
        std::cerr << "{VideoStreamer::setup}; unable to parse filter description; "
            "parse result: '" << parseResult << " (" << av_err2str(parseResult) << ")'" << std::endl;
        return false;
    }

    auto checkResult = avfilter_graph_config(m_filterGraph, nullptr);
    if (checkResult < 0) {
        std::cerr << "{VideoStreamer::setup}; filter graph is NOT valid; "
            "check result: '" << checkResult << " (" << av_err2str(checkResult) << ")'" << std::endl;
        return false;
    }

    m_encoderPacket = av_packet_alloc();
    if (nullptr == m_encoderPacket) {
        std::cerr << "{VideoStreamer::setup}; unable to allocate memory for encoder packet" << std::endl;
        return false;
    }
    return true;
}

bool VideoStreamer::process() {
    using namespace SimpleWrapperSpace;

    if (nullptr == m_inputContext) {
        std::cerr << "{VideoStreamer::process}; pointer to input context is NULL" << std::endl;
        return false;
    }
    if (-1 == m_videoStreamIndex) {
        std::cerr << "{VideoStreamer::process}; video stream index is NOT set" << std::endl;
        return false;
    }
    if (nullptr == m_decoderContext) {
        std::cerr << "{VideoStreamer::process}; pointer to decoder context is NULL" << std::endl;
        return false;
    }
    if (nullptr == m_outputContext) {
        std::cerr << "{VideoStreamer::process}; pointer to output context is NULL" << std::endl;
        return false;
    }
    if (nullptr == m_timeoutChecker) {
        std::cerr << "{VideoStreamer::process}; pointer to timeout checker is NULL" << std::endl;
        return false;
    }

    AVFrame* decoderFrame = nullptr;
    AVFrame* filteredFrame = nullptr;
    AVPacket* packet = nullptr;

    auto resourceDeallocator = [
        this, &decoderFrame, &filteredFrame, &packet
    ] () {
        if (packet) {
            av_packet_free(&packet);
            packet = nullptr;
        }
        if (filteredFrame) {
            av_frame_free(&filteredFrame);
            filteredFrame = nullptr;
        }
        if (decoderFrame) {
            av_frame_free(&decoderFrame);
            decoderFrame = nullptr;
        }
        deallocateResources();
    };
    SimpleWrapper simpleWrapper(nullptr, resourceDeallocator);

    decoderFrame = av_frame_alloc();
    if (nullptr == decoderFrame) {
        std::cerr << "{VideoStreamer::process}; unable to allocate memory for decoder frame" << std::endl;
        return false;
    }

    filteredFrame = av_frame_alloc();
    if (nullptr == filteredFrame) {
        std::cerr << "{VideoStreamer::process}; unable to allocate memory for filtered frame" << std::endl;
        return false;
    }

    packet = av_packet_alloc();
    if (nullptr == packet) {
        std::cerr << "{VideoStreamer::process}; unable to allocate memory for packet" << std::endl;
        return false;
    }

    /* read all packets */
    while (true) {
        auto readResult = av_read_frame(m_inputContext, packet);
        if (readResult < 0) {
            std::cerr << "{VideoStreamer::process}; unable to read packet; "
                "read result: '" << readResult << " (" << av_err2str(readResult) << ")'" << std::endl;
            break;
        }
        if (packet->stream_index < 0) {
            std::cerr << "{VideoStreamer::process}; packet stream index is less than zero" << std::endl;
            return false;
        }
        if (packet->stream_index != m_videoStreamIndex) {
            av_packet_unref(packet);
            continue;
        }

        auto sendResult = avcodec_send_packet(m_decoderContext, packet);
        if (sendResult < 0) {
            std::cerr << "{VideoStreamer::process}; unable to send packet to decoder context; "
                "send result: '" << sendResult << " (" << av_err2str(sendResult) << ")'" << std::endl;
            break;
        }

        while (true) {
            auto receiveResult = avcodec_receive_frame(m_decoderContext, decoderFrame);
            if ((AVERROR(EAGAIN) == receiveResult) || (AVERROR_EOF == receiveResult)) {
                break;
            } else if (receiveResult < 0) {
                std::cerr << "{VideoStreamer::process}; unable to receive decoder frame; "
                    "receive result: '" << receiveResult << " (" << av_err2str(receiveResult) << ")'" << std::endl;
                return false;
            }
            decoderFrame->pts = decoderFrame->best_effort_timestamp;

            if (!filterEncodeWriteFrame(decoderFrame, filteredFrame)) {
                return false;
            }
        }

        av_packet_unref(packet);
        if (SignalNumberSetter::getInstance().isSet()) {
            std::cout << "{VideoStreamer::process}; Ctrl+C" << std::endl;
            break;
        }
    }

    /* flush decoder */
    auto sendResult = avcodec_send_packet(m_decoderContext, nullptr);
    if (sendResult < 0) {
        std::cerr << "{VideoStreamer::process}; unable to flush decoder context; "
            "send result: '" << sendResult << " (" << av_err2str(sendResult) << ")'" << std::endl;
        return false;
    }

    while (true) {
        auto receiveResult = avcodec_receive_frame(m_decoderContext, decoderFrame);
        if (AVERROR_EOF == receiveResult) {
            break;
        } else if (receiveResult < 0) {
            std::cerr << "{VideoStreamer::process}; unable to receive decoder frame; "
                "receive result: '" << receiveResult << " (" << av_err2str(receiveResult) << ")'" << std::endl;
            return false;
        }
        decoderFrame->pts = decoderFrame->best_effort_timestamp;

        if (!filterEncodeWriteFrame(decoderFrame, filteredFrame)) {
            return false;
        }
    }

    /* flush filter */
    if (!filterEncodeWriteFrame(nullptr, filteredFrame)) {
        return false;
    }

    /* flush encoder */
    if (!flushEncoder(filteredFrame)) {
        return false;
    }

    auto writeTrailerResult = av_write_trailer(m_outputContext);
    if (writeTrailerResult < 0) {
        std::cerr << "{VideoStreamer::process}; unable to write trailer; "
            "write result: '" << writeTrailerResult << " (" << av_err2str(writeTrailerResult) << ")'" << std::endl;
        return false;
    }
    return true;
}

bool VideoStreamer::parseConfig(const std::string& configFileName) {
    if (configFileName.empty()) {
        std::cerr << "{VideoStreamer::parseConfig}; configuration file name is empty" << std::endl;
        return false;
    }
    if (!m_configParams.inputStreamName.empty() && !m_configParams.rtmpUrl.empty()) {
        std::cout << "{VideoStreamer::parseConfig}; required configuration parameters are already set" << std::endl;
        return true;
    }

    rapidjson::Document settings;
    {
        std::string fileContents;
        if (!CommonFunctions::getFileContents(configFileName, fileContents)) {
            return false;
        }
        settings.Parse(fileContents.c_str());
    }

    if (settings.HasParseError()) {
        std::cerr << "{VideoStreamer::parseConfig}; unable to parse "
            "configuration file '" << configFileName << "'; "
            "parse error: '" << settings.GetParseError() << "'" << std::endl;
        return false;
    }

    if (!settings.HasMember("programSettings")) {
        std::cerr << "{VideoStreamer::parseConfig}; section 'programSettings' was NOT found in "
            "configuration file '" << configFileName << "'" << std::endl;
        return false;
    }
    if (!settings["programSettings"].IsObject()) {
        std::cerr << "{VideoStreamer::parseConfig}; parse error" << std::endl;
        return false;
    }

    if (!settings["programSettings"].HasMember("input")) {
        std::cerr << "{VideoStreamer::parseConfig}; parse error" << std::endl;
        return false;
    }
    if (!settings["programSettings"]["input"].IsString()) {
        std::cerr << "{VideoStreamer::parseConfig}; parse error" << std::endl;
        return false;
    }
    auto streamName = settings["programSettings"]["input"].GetString();
    if (nullptr == streamName) {
        std::cerr << "{VideoStreamer::parseConfig}; pointer to stream name is NULL" << std::endl;
        return false;
    }
    std::string inputStreamName(streamName, std::strlen(streamName));
    if (inputStreamName.empty()) {
        std::cerr << "{VideoStreamer::parseConfig}; input stream name is empty" << std::endl;
        return false;
    }
    if (!CommonFunctions::fileExists(inputStreamName)) {
        return false;
    }
    if (!CommonFunctions::isCharacterFile(inputStreamName)) {
        return false;
    }
    m_configParams.inputStreamName = inputStreamName;
    std::cout << "{VideoStreamer::parseConfig}; input stream name: '" << inputStreamName << "'" << std::endl;

    if (!settings["programSettings"].HasMember("watermark")) {
        std::cerr << "{VideoStreamer::parseConfig}; parse error" << std::endl;
        return false;
    }
    if (!settings["programSettings"]["watermark"].IsObject()) {
        std::cerr << "{VideoStreamer::parseConfig}; parse error" << std::endl;
        return false;
    }
    if (!settings["programSettings"]["watermark"].HasMember("enabled")) {
        std::cerr << "{VideoStreamer::parseConfig}; parse error" << std::endl;
        return false;
    }
    if (!settings["programSettings"]["watermark"]["enabled"].IsBool()) {
        std::cerr << "{VideoStreamer::parseConfig}; parse error" << std::endl;
        return false;
    }
    auto isWatermarkEnabled = settings["programSettings"]["watermark"]["enabled"].GetBool();
    if (isWatermarkEnabled) {
        std::cout << "{VideoStreamer::parseConfig}; watermark is enabled" << std::endl;
        if (!settings["programSettings"]["watermark"].HasMember("fullFileName")) {
            std::cerr << "{VideoStreamer::parseConfig}; parse error" << std::endl;
            return false;
        }
        if (!settings["programSettings"]["watermark"]["fullFileName"].IsString()) {
            std::cerr << "{VideoStreamer::parseConfig}; parse error" << std::endl;
            return false;
        }
        auto location = settings["programSettings"]["watermark"]["fullFileName"].GetString();
        if (nullptr == location) {
            std::cerr << "{VideoStreamer::parseConfig}; pointer to location is NULL" << std::endl;
            return false;
        }
        std::string watermarkLocation(location, std::strlen(location));
        if (watermarkLocation.empty()) {
            std::cerr << "{VideoStreamer::parseConfig}; watermark location is empty" << std::endl;
            return false;
        }
        if (!CommonFunctions::fileExists(watermarkLocation)) {
            return false;
        }
        if (!CommonFunctions::isRegularFile(watermarkLocation)) {
            return false;
        }

        unsigned int watermarkWidth = 0;
        unsigned int watermarkHeight = 0;
        if (!CommonFunctions::getPngSize(watermarkLocation, watermarkWidth, watermarkHeight)) {
            return false;
        }
        if (g_watermarkWidth != watermarkWidth) {
            std::cerr << "{VideoStreamer::parseConfig}; watermark width is NOT equal to '" << g_watermarkWidth << "'; "
                "current watermark width: '" << watermarkWidth << "'; "
                "watermark location: '" << watermarkLocation << "'" << std::endl;
            return false;
        }
        if (g_watermarkHeight != watermarkHeight) {
            std::cerr << "{VideoStreamer::parseConfig}; watermark height is NOT equal to '" << g_watermarkHeight << "'; "
                "current watermark height: '" << watermarkHeight << "'; "
                "watermark location: '" << watermarkLocation << "'" << std::endl;
            return false;
        }

        m_configParams.watermarkLocation = std::make_optional<std::string>(watermarkLocation);
        std::cout << "{VideoStreamer::parseConfig}; watermark location: '" << watermarkLocation << "'" << std::endl;
    } else {
        std::cout << "{VideoStreamer::parseConfig}; watermark is NOT enabled" << std::endl;
    }

    if (!settings["programSettings"].HasMember("output")) {
        std::cerr << "{VideoStreamer::parseConfig}; parse error" << std::endl;
        return false;
    }
    if (!settings["programSettings"]["output"].IsString()) {
        std::cerr << "{VideoStreamer::parseConfig}; parse error" << std::endl;
        return false;
    }
    auto url = settings["programSettings"]["output"].GetString();
    if (nullptr == url) {
        std::cerr << "{VideoStreamer::parseConfig}; pointer to url is NULL" << std::endl;
        return false;
    }
    std::string rtmpUrl(url, std::strlen(url));
    if (rtmpUrl.empty()) {
        std::cerr << "{VideoStreamer::parseConfig}; rtmp url is empty" << std::endl;
        return false;
    }
    m_configParams.rtmpUrl = rtmpUrl;
    std::cout << "{VideoStreamer::parseConfig}; rtmp url: '" << rtmpUrl << "'" << std::endl;

    if (
        settings.HasMember("ffmpegSettings") &&
        !settings["ffmpegSettings"].IsObject()
    ) {
        std::cerr << "{VideoStreamer::parseConfig}; parse error" << std::endl;
        return false;
    }

    if (
        settings.HasMember("ffmpegSettings") &&
        settings["ffmpegSettings"].IsObject() &&
        settings["ffmpegSettings"].HasMember("logLevel")
    ) {
        if (!settings["ffmpegSettings"]["logLevel"].IsString()) {
            std::cerr << "{VideoStreamer::parseConfig}; parse error" << std::endl;
            return false;
        }
        auto level = settings["ffmpegSettings"]["logLevel"].GetString();
        if (nullptr == level) {
            std::cerr << "{VideoStreamer::parseConfig}; pointer to log level is NULL" << std::endl;
            return false;
        }
        std::string logLevel(level, std::strlen(level));
        if (logLevel.empty()) {
            std::cerr << "{VideoStreamer::parseConfig}; log level is empty" << std::endl;
            return false;
        }

        frozen::string frozenLogLevel(level, std::strlen(level));
        auto it = g_logLevels.find(frozenLogLevel);
        if (g_logLevels.cend() == it) {
            std::cerr << "{VideoStreamer::parseConfig}; key '" << logLevel << "' was NOT found in map" << std::endl;
            return false;
        }
        m_configParams.ffmpegLogLevel = it->second;
        std::cout << "{VideoStreamer::parseConfig}; FFmpeg log level: '" << logLevel << "'" << std::endl;
    } else {
        m_configParams.ffmpegLogLevel = AV_LOG_TRACE;
        std::cout << "{VideoStreamer::parseConfig}; default FFmpeg log level: 'trace'" << std::endl;
    }
    return true;
}

bool VideoStreamer::encodeWriteFrame(bool readyToFlush, AVFrame* filteredFrame) {
    if (nullptr == m_encoderPacket) {
        std::cerr << "{VideoStreamer::encodeWriteFrame}; pointer to encoder packet is NULL" << std::endl;
        return false;
    }
    if (nullptr == m_encoderContext) {
        std::cerr << "{VideoStreamer::encodeWriteFrame}; pointer to encoder context is NULL" << std::endl;
        return false;
    }
    if (-1 == m_videoStreamIndex) {
        std::cerr << "{VideoStreamer::encodeWriteFrame}; video stream index is NOT set" << std::endl;
        return false;
    }
    if (nullptr == m_outputContext) {
        std::cerr << "{VideoStreamer::encodeWriteFrame}; pointer to output context is NULL" << std::endl;
        return false;
    }
    if (nullptr == m_timeoutChecker) {
        std::cerr << "{VideoStreamer::encodeWriteFrame}; pointer to timeout checker is NULL" << std::endl;
        return false;
    }

    AVFrame* frame = readyToFlush ? nullptr : filteredFrame;
    av_packet_unref(m_encoderPacket);
    if (frame && (AV_NOPTS_VALUE != frame->pts)) {
        frame->pts = av_rescale_q(
            frame->pts, frame->time_base,
            m_encoderContext->time_base
        );
    }

    /* encode filtered frame */
    auto sendResult = avcodec_send_frame(m_encoderContext, frame);
    if (sendResult < 0) {
        if (frame) {
            std::cerr << "{VideoStreamer::encodeWriteFrame}; unable to send filtered frame to encoder context; "
                "send result: '" << sendResult << " (" << av_err2str(sendResult) << ")'" << std::endl;
        } else {
            std::cerr << "{VideoStreamer::encodeWriteFrame}; unable to flush encoder context; "
                "send result: '" << sendResult << " (" << av_err2str(sendResult) << ")'" << std::endl;
        }
        return false;
    }

    while (true) {
        auto receiveResult = avcodec_receive_packet(m_encoderContext, m_encoderPacket);
        if ((AVERROR(EAGAIN) == receiveResult) || (AVERROR_EOF == receiveResult)) {
            break;
        } else if (receiveResult < 0) {
            std::cerr << "{VideoStreamer::encodeWriteFrame}; unable to receive encoder packet from encoder context; "
                "receive result: '" << receiveResult << " (" << av_err2str(receiveResult) << ")'" << std::endl;
        }

        /* prepare packet for muxing */
        m_encoderPacket->stream_index = m_videoStreamIndex;
        av_packet_rescale_ts(
            m_encoderPacket, m_encoderContext->time_base,
            m_outputContext->streams[
                static_cast<std::size_t>(m_videoStreamIndex)
            ]->time_base
        );

        /* mux encoded frame */
        m_timeoutChecker->setBeginTime();
        auto writeResult = av_interleaved_write_frame(m_outputContext, m_encoderPacket);
        m_timeoutChecker->resetBeginTime();
        if (writeResult < 0) {
            if (AVERROR_EOF == writeResult) {
                std::cout << "{VideoStreamer::encodeWriteFrame}; unable to write encoder packet to output context; "
                    "write result: 'AVERROR_EOF (" << av_err2str(writeResult) << ")'" << std::endl;
            } else {
                if (m_timeoutChecker->isTimeoutReached()) {
                    std::cerr << "{VideoStreamer::encodeWriteFrame}; "
                        "write result: '" << writeResult << " (" << av_err2str(writeResult) << ")'" << std::endl;
                } else {
                    std::cerr << "{VideoStreamer::encodeWriteFrame}; unable to write encoder packet to output context; "
                        "write result: '" << writeResult << " (" << av_err2str(writeResult) << ")'" << std::endl;
                }
            }
            return false;
        }
    }
    return true;
}

bool VideoStreamer::filterEncodeWriteFrame(AVFrame* decoderFrame, AVFrame* filteredFrame) {
    if (nullptr == m_bufferSrcContext) {
        std::cerr << "{VideoStreamer::filterEncodeWriteFrame}; pointer to buffer src context is NULL" << std::endl;
        return false;
    }
    if (nullptr == m_bufferSinkContext) {
        std::cerr << "{VideoStreamer::filterEncodeWriteFrame}; pointer to buffer sink context is NULL" << std::endl;
        return false;
    }

    /* push the decoded frame into the filtergraph */
    auto addResult = av_buffersrc_add_frame_flags(m_bufferSrcContext, decoderFrame, 0);
    if (addResult < 0) {
        std::cerr << "{VideoStreamer::filterEncodeWriteFrame}; unable to add flags; "
            "add result: '" << addResult << " (" << av_err2str(addResult) << ")'" << std::endl;
        return false;
    }

    /* pull filtered frames from the filtergraph */
    bool readyToFlush = false;
    while (true) {
        auto getResult = av_buffersink_get_frame(m_bufferSinkContext, filteredFrame);
        if (getResult < 0) {
            /* if no more frames for output - returns AVERROR(EAGAIN)
             * if flushed and no more frames for output - returns AVERROR_EOF
             * rewrite retcode to 0 to show it as normal procedure completion
             */
            if ((AVERROR(EAGAIN) == getResult) || (AVERROR_EOF == getResult)) {
                break;
            }
            std::cerr << "{VideoStreamer::filterEncodeWriteFrame}; unable to get filtered frame from buffer sink context; "
                "get result: '" << getResult << " (" << av_err2str(getResult) << ")'" << std::endl;
            return false;
        }

        filteredFrame->time_base = av_buffersink_get_time_base(m_bufferSinkContext);
        filteredFrame->pict_type = AVPictureType::AV_PICTURE_TYPE_NONE;
        bool wasWritten = encodeWriteFrame(readyToFlush, filteredFrame);
        av_frame_unref(filteredFrame);
        if (!wasWritten) {
            return false;
        }
    }
    return true;
}

bool VideoStreamer::flushEncoder(AVFrame* filteredFrame) {
    if (nullptr == m_encoderContext) {
        std::cerr << "{VideoStreamer::flushEncoder}; pointer to encoder context is NULL" << std::endl;
        return false;
    }
    if (nullptr == m_encoderContext->codec) {
        std::cerr << "{VideoStreamer::flushEncoder}; pointer to encoder is NULL" << std::endl;
        return false;
    }

    if (!(
        AV_CODEC_CAP_DELAY & m_encoderContext->codec->capabilities
    )) {
        return true;
    }
    bool readyToFlush = true;
    return encodeWriteFrame(readyToFlush, filteredFrame);
}

void VideoStreamer::deallocateResources() {
    if (
        m_outputContext && m_outputContext->pb && m_outputContext->oformat && !(
            AVFMT_NOFILE & m_outputContext->oformat->flags
        ) && m_timeoutChecker
    ) {
        m_timeoutChecker->setBeginTime();
        auto closeResult = avio_closep(&m_outputContext->pb);
        m_timeoutChecker->resetBeginTime();
        if (closeResult < 0) {
            if (AVERROR_EOF == closeResult) {
                std::cout << "{VideoStreamer::deallocateResources}; unable to close output context; "
                    "close result: 'AVERROR_EOF (" << av_err2str(closeResult) << ")'" << std::endl;
            } else {
                if (m_timeoutChecker->isTimeoutReached()) {
                    std::cerr << "{VideoStreamer::deallocateResources}; "
                        "close result: '" << closeResult << " (" << av_err2str(closeResult) << ")'" << std::endl;
                } else {
                    std::cerr << "{VideoStreamer::deallocateResources}; unable to close output context; "
                        "close result: '" << closeResult << " (" << av_err2str(closeResult) << ")'" << std::endl;
                }
            }
        }
    }
    if (m_outputContext) {
        avformat_free_context(m_outputContext);
        m_outputContext = nullptr;
    }

    if (m_encoderPacket) {
        av_packet_free(&m_encoderPacket);
        m_encoderPacket = nullptr;
    }
    if (m_encoderContext) {
        avcodec_free_context(&m_encoderContext);
        m_encoderContext = nullptr;
    }

    if (m_filterGraph) {
        avfilter_graph_free(&m_filterGraph);
        m_filterGraph = nullptr;
    }
    m_bufferSinkContext = nullptr;
    m_bufferSrcContext = nullptr;

    if (m_decoderContext) {
        avcodec_free_context(&m_decoderContext);
        m_decoderContext = nullptr;
    }

    m_videoStreamIndex = -1;
    if (m_inputContext) {
        avformat_close_input(&m_inputContext);
        m_inputContext = nullptr;
    }
}

std::optional<const AVPixelFormat> VideoStreamer::getPixelFormat(const AVCodec* encoder) const {
    if (nullptr == encoder) {
        std::cerr << "{VideoStreamer::getPixelFormat}; pointer to encoder is NULL" << std::endl;
        return std::nullopt;
    }

    const AVPixelFormat* pixelFormatArray = nullptr;
    auto castedAddressOfArray = reinterpret_cast<const void**>(&pixelFormatArray);
    int nPixelFormats = 0;
    auto getResult = avcodec_get_supported_config(
        nullptr, encoder, AV_CODEC_CONFIG_PIX_FORMAT,
        0, castedAddressOfArray, std::addressof(nPixelFormats)
    );
    if (getResult < 0) {
        std::cerr << "{VideoStreamer::getPixelFormat}; unable to get supported pixel formats; "
            "get result: '" << getResult << " (" << av_err2str(getResult) << ")'" << std::endl;
        return std::nullopt;
    }
    if (nullptr == pixelFormatArray) {
        std::cerr << "{VideoStreamer::getPixelFormat}; pointer to pixel format array is NULL" << std::endl;
        return std::nullopt;
    }
    if (nPixelFormats < 0) {
        std::cerr << "{VideoStreamer::getPixelFormat}; number of pixel formats is less than zero" << std::endl;
        return std::nullopt;
    }

    std::span<const AVPixelFormat> pixelFormatSpan(
        pixelFormatArray, static_cast<std::size_t>(nPixelFormats)
    );
    if (pixelFormatSpan.empty()) {
        std::cerr << "{VideoStreamer::getPixelFormat}; number of pixel formats is equal to zero" << std::endl;
        return std::nullopt;
    }
    auto checker = [] (const AVPixelFormat& pixelFormat) {
        return (AV_PIX_FMT_NONE != pixelFormat);
    };
    auto itPixelFormat = std::ranges::find_if(pixelFormatSpan, checker);
    if (pixelFormatSpan.end() == itPixelFormat) {
        std::cerr << "{VideoStreamer::getPixelFormat}; valid pixel format was NOT found in span" << std::endl;
        return std::nullopt;
    }
    const AVPixelFormat pixelFormat = *itPixelFormat;
    std::cout << "{VideoStreamer::getPixelFormat}; "
        "pixel format '" << av_get_pix_fmt_name(pixelFormat) << "'; "
        "encoder name: '" << avcodec_get_name(encoder->id) << "'" << std::endl;
    return std::make_optional<const AVPixelFormat>(pixelFormat);
}
