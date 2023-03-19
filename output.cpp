#include "output.h"

#include "frame.h"

#include <cassert>

Output::Output(int width, int height)
    : m_width(width), m_height(height) {
    m_currentFrame = new Frame;
    m_lastFrame = new Frame;
    m_nextFrame = nullptr;

    m_currentFrame->data = allocate_frame_buffer<uint8_t>(width, height);
    m_lastFrame->data = allocate_frame_buffer<uint8_t>(width, height);
}

void Output::send_frame(Frame* frame) {
    std::unique_lock lock{m_frameLock};
    m_frameCondition.wait(lock, [this]{return !m_nextFrame;});

    assert(!m_nextFrame);
    m_nextFrame = frame;

    lock.unlock();
    m_frameCondition.notify_all();
}

Frame* Output::acquire_frame() {
    Frame* frame;

    // Reuse the last processed frame

    std::unique_lock lock{m_frameLock};
    m_frameCondition.wait(lock, [this]{return m_lastFrame;});

    assert(m_lastFrame);
    frame = m_lastFrame;
    m_lastFrame = nullptr;

    lock.unlock();
    m_frameCondition.notify_all();

    return frame;
}

void Output::finish() {}
