#pragma once

#include <condition_variable>
#include <mutex>

struct Frame;

class Output {
public:
    Output(int width, int height);

    // Any data accessed by these two functions should be protected by a lock
    // as they may be called from another thread.
    virtual void send_frame(Frame* frame) = 0;
    virtual Frame* acquire_frame() = 0;

    virtual void run() = 0;

protected:
    int m_width;
    int m_height;
};

class TTYOutput : public Output {
public:
    TTYOutput(int width, int height);
    ~TTYOutput();

    void send_frame(Frame* frame) override;
    Frame* acquire_frame() override;

    void run() override;

private:
    FILE* m_out;

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

class COutput : public Output {
public:
    void send_frame(Frame* frame) override;
    Frame* acquire_frame() override;
};

class UEFIOutput : public Output {
public:
    void send_frame(Frame* frame) override;
    Frame* acquire_frame() override;
};
