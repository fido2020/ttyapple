#pragma once

#include <condition_variable>
#include <mutex>

struct Frame;

class Output {
public:
    Output(int width, int height);

    // Any data accessed by these two functions should be protected by a lock
    // as they may be called from another thread.
    virtual void send_frame(Frame* frame);
    virtual Frame* acquire_frame();

    void set_interlacing(bool enabled);

    virtual void run() = 0;
    virtual void finish();

protected:
    int m_width;
    int m_height;

    std::mutex m_frameLock;
    std::condition_variable m_frameCondition;

    // Current frame being processed
    Frame* m_currentFrame;
    // Last frame to be processed
    Frame* m_lastFrame;
    // Frame waiting to be processed
    Frame* m_nextFrame;

    long m_lastFrameTimestamp = -1;

    bool m_interlaced = false;
};

class TTYOutput : public Output {
public:
    TTYOutput(int width, int height);
    ~TTYOutput();

    void run() override;

private:
    FILE* m_out;

    std::chrono::time_point<std::chrono::steady_clock> m_lastFrameDrawn;
};

class COutput : public Output {
public:
    COutput(int width, int height);
    virtual ~COutput();

    int open_file(const char* path);
    void close_file();

    void run() override;
    virtual void finish() override;

protected:
    FILE* m_out = nullptr;

    uint8_t* m_packedPixelBuffer;
    int m_frameIndex = 0;
};

class UEFIOutput : public COutput {
public:
    UEFIOutput(int width, int height);

    void set_output_file(const char* path);

    void finish() override;
private:
    std::string m_outputPath;

    std::string m_compiler;
    std::string m_linker;

    FILE* m_decodeSource;
    FILE* m_uefiMainSource;

    int m_pipe[2];

    pid_t m_compilerPID;
};
