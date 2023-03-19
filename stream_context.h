// Reused some of my code from LemonOS :3

#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>

class StreamContext {
    friend void PlayAudio(StreamContext*);

public:
    struct SampleBuffer {
        uint8_t* data;
        int samples;

        // Timestamp in seconds of last frame in buffer
        float timestamp;
    };

    StreamContext();
    ~StreamContext();

    void set_output_format(int outputWidth, int outputHeight);

    inline bool is_playing() const { return m_isDecoderRunning; }

    // Gets progress into song in seconds
    float playback_progress() const;
    void playback_start();
    void playback_pause();
    // Stop playing audio and unload the file
    void playback_stop();
    // Seek to a new position in the song
    void playback_seek(float timestamp);

    // Play the track given in info
    // returns 0 on success
    int play_track(std::string file);

    // Decoder waits for a buffer to be processed
    std::condition_variable decoderWaitCondition;
    // Decoder thread will block whilst it is not decoding an audio file
    std::condition_variable decoderShouldRunCondition;
    int numValidBuffers;

    // Lock when using/changing m_surface
    std::mutex surfaceLock;

    struct Frame*(*acquire_buffer)() = nullptr;
    void(*push_buffer)(struct Frame*) = nullptr;

private:
    // Decoder Loop
    void decode();
    void decode_video(struct AVPacket* packet);
    // Decodes a frame of audio and fills the next available buffer
    void decoder_decode_frame(struct AVFrame* frame);
    // Perform the requested seek to m_seekTimestamp
    void decoder_do_seek();

    // A packet is considered invalid if we need to seek
    // or the decoder is not marked as running
    inline bool is_decoder_packet_invalid() {
        return m_requestSeek || !m_isDecoderRunning;
    }

    void initialize_rescaler();

    // File descriptor for pcm output
    int m_pcmOut;
    int m_pcmSampleRate;
    int m_pcmChannels;
    int m_pcmBitDepth;

    // Decoder runs parallel to playback and GUI threads
    std::thread m_decoderThread;
    // Held by the decoderThread whilst it is running
    std::mutex m_decoderLock;
    // Lock for m_isDecoderRunning
    std::mutex m_decoderStatusLock;

    bool m_shouldThreadsDie = false;
    bool m_isDecoderRunning = false;
    bool m_endOfFile = false;

    int m_outputWidth;
    int m_outputHeight;
    
    bool m_requestSeek = false;
    // Timestamp in seconds of where to seek to
    float m_seekTimestamp;
    // Last timestamp wd by the playback thread
    float m_lastTimestamp;

    struct AVFormatContext* m_avfmt = nullptr;
    struct AVCodecContext* m_vcodec = nullptr;
    struct SwsContext* m_rescaler = nullptr;

    struct AVStream* m_videoStream = nullptr;
    int m_videoStreamIndex = 0;
};
