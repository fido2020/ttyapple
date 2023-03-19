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
    COutput(const char* file, int width, int height);

    void run() override;
    void finish() override;

protected:
    FILE* m_out;

    uint8_t* m_packedPixelBuffer;
    int m_frameIndex = 0;
};

class UEFIOutput : public Output {
public:
};
