#include "stream_context.h"

#include "frame.h"
#include "logger.h"

#include <assert.h>
#include <errno.h>

#include <unistd.h>
#include <time.h>

// The libav* libraries do not add extern "C" when using C++,
// so specify here that all functions are C functions and do not have mangled names
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

StreamContext::StreamContext() {
    m_decoderThread = std::thread(&StreamContext::decode, this);
}

StreamContext::~StreamContext() {
    m_shouldThreadsDie = true;

    // Wait for playback to stop before exiting
    playback_stop();
}

void StreamContext::set_output_format(int outputWidth, int outputHeight) {
    std::unique_lock lockStatus{m_decoderStatusLock};

    m_outputWidth = outputWidth;
    m_outputHeight = outputHeight;

    if (m_isDecoderRunning) {
        initialize_rescaler();
    }
}

void StreamContext::decode_video(AVPacket* packet) {
    AVFrame* frame = av_frame_alloc();

    // Send the packet to the decoder
    if (int ret = avcodec_send_packet(m_vcodec, packet); ret) {
        printf("Could not send packet for decoding");
        return;
    }

    ssize_t ret = 0;
    while (!is_decoder_packet_invalid() && ret >= 0) {
        // Decodes the audio
        ret = avcodec_receive_frame(m_vcodec, frame);
        if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
            // Get the next packet and retry
            break;
        } else if (ret) {
            Logger::Error("Could not decode frame: {}", ret);
            // Stop decoding audio
            m_isDecoderRunning = false;
            break;
        }

        std::unique_lock lockSurface{surfaceLock};

        int stride = m_outputWidth;
        Frame* buffer = acquire_buffer();

        sws_scale(m_rescaler, frame->data, frame->linesize, 0, m_vcodec->height, &buffer->data, &stride);
        // PTS is in milliseconds
        buffer->usTimestamp = packet->pts * 1000;

        push_buffer(buffer);
        m_lastTimestamp = packet->pts / 1000.0;

        av_frame_unref(frame);
    }

    av_packet_unref(packet);
}

void StreamContext::decode() {
    while (!m_shouldThreadsDie) {
        {
            std::unique_lock lockStatus{m_decoderStatusLock};
            decoderShouldRunCondition.wait(lockStatus, [this]() -> bool { return m_isDecoderRunning; });
        }

        m_decoderLock.lock();

        initialize_rescaler();

        // Reset the sample buffer read and write indexes
        numValidBuffers = 0;

        AVPacket* packet = av_packet_alloc();

        int frameResult = 0;
        while (m_isDecoderRunning && (frameResult = av_read_frame(m_avfmt, packet)) >= 0) {
            if (packet->stream_index == m_videoStreamIndex) {
                decode_video(packet);
            } else {
                av_packet_unref(packet);
            }
        }

        // If we got to the end of file (did not encounter errors)
        // let the main thread know to play the next track
        if (frameResult == AVERROR_EOF) {
            // We finished playing
            m_endOfFile = true;
        }

        // Clean up after ourselves
        // Set the decoder as not running
        std::unique_lock lockStatus{m_decoderStatusLock};
        m_isDecoderRunning = false;
        numValidBuffers = 0;

        // Free the codec and format contexts
        avcodec_free_context(&m_vcodec);

        avformat_free_context(m_avfmt);
        m_avfmt = nullptr;

        // Unlock the decoder lock letting the other threads
        // know this thread is almost done
        m_decoderLock.unlock();
    }
}

void StreamContext::decoder_do_seek() {
    assert(m_requestSeek);

    // TODO

    m_lastTimestamp = m_seekTimestamp;
    // Set m_requestSeek to false indicating that seeking has finished
    m_requestSeek = false;
}

void StreamContext::initialize_rescaler() {
    assert(m_isDecoderRunning && m_vcodec);

    if (m_rescaler) {
        sws_freeContext(m_rescaler);
    }

    m_rescaler = sws_getContext(m_vcodec->width, m_vcodec->height, m_vcodec->pix_fmt, m_outputWidth,
                                m_outputHeight, AV_PIX_FMT_GRAY8, SWS_BILINEAR, NULL, NULL, NULL);
}

float StreamContext::playback_progress() const {
    if (!m_isDecoderRunning) {
        return 0;
    }

    // Return timestamp of last played buffer
    return m_lastTimestamp;
}

void StreamContext::playback_stop() {
    if (m_isDecoderRunning) {
        // Mark the decoder as not running
        std::unique_lock lockDecoderStatus{m_decoderStatusLock};
        m_isDecoderRunning = false;
        // Make the decoder stop waiting for a free sample buffer
        decoderWaitCondition.notify_all();
    }
}

void StreamContext::playback_seek(float timestamp) {
    if (m_isDecoderRunning) {
        m_seekTimestamp = timestamp;
        m_requestSeek = true;

        // Let the decoder thread know that we want to seek
        decoderWaitCondition.notify_all();
        while (m_requestSeek)
            usleep(1000); // Wait for seek to finish, just wait 1ms as not to hog the CPU
    }
}

int StreamContext::play_track(std::string file) {
    // Stop any audio currently playing
    if (m_isDecoderRunning) {
        playback_stop();
    }

    // Block the decoder from running until we are done here
    std::lock_guard lockDecoder(m_decoderLock);

    assert(!m_avfmt);
    m_avfmt = avformat_alloc_context();

    // Opens the audio file
    if (int err = avformat_open_input(&m_avfmt, file.c_str(), NULL, NULL); err) {
        Logger::Error("Failed to open {}", file);
        return err;
    }

    // Gets metadata and information about the audio contained in the file
    if (int err = avformat_find_stream_info(m_avfmt, NULL); err) {
        Logger::Error("Failed to get stream info for {}", file);
        return err;
    }

    int streamIndex = av_find_best_stream(m_avfmt, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (streamIndex < 0) {
        Logger::Error("Failed to get video stream for {}", file);
        return streamIndex;
    }
    m_videoStream = m_avfmt->streams[streamIndex];
    m_videoStreamIndex = streamIndex;

    // Find a decoder for the video data we have been given
    const AVCodec* decoder = avcodec_find_decoder(m_videoStream->codecpar->codec_id);
    if (!decoder) {
        Logger::Error("Failed to find codec for '{}'", file);
        return 1;
    }

    m_vcodec = avcodec_alloc_context3(decoder);
    assert(m_vcodec);

    if (avcodec_parameters_to_context(m_vcodec, m_videoStream->codecpar)) {
        Logger::Error("Failed to initialie codec context.");
        return 1;
    }

    if (avcodec_open2(m_vcodec, decoder, NULL) < 0) {
        Logger::Error("Failed to open codec!");
        return 1;
    }

    m_requestSeek = false;

    // Notify the decoder thread and mark the decoder as running
    std::scoped_lock lockDecoderStatus{m_decoderStatusLock};
    m_isDecoderRunning = true;
    decoderShouldRunCondition.notify_all();
    return 0;
}
