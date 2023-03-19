#include "output.h"

#include "frame.h"
#include "logger.h"
#include "image.h"
#include "time.h"

#include <cassert>
#include <cstdio>
#include <cstring>

#include <chrono>
#include <vector>

#define BLOCK_CHARACTER {0xE2,0x96,0x88}
#define HALFBLOCK_TOP_CHARACTER {0xE2, 0x96, 0x80}
#define HALFBLOCK_BOTTOM_CHARACTER {0xE2, 0x96, 0x84}

void frame_data_to_string(uint8_t* top, uint8_t* bottom, unsigned width, std::vector<char>& outString) {
    const uint8_t* end = top + width;
    while(top < end) {
        // Cut out any alpha
        // Get rid of dark greys
        if(GRAY_TO_MONOCHROME(*top++)) {
            if(GRAY_TO_MONOCHROME(*bottom++)) {
                const unsigned char block[3] = BLOCK_CHARACTER;
                outString.resize(outString.size() + 3);

                memcpy(outString.data() + outString.size() - 3, block, 3);
            } else {
                const unsigned char block[3] = HALFBLOCK_TOP_CHARACTER;
                outString.resize(outString.size() + 3);

                memcpy(outString.data() + outString.size() - 3, block, 3);
            }
        } else if(GRAY_TO_MONOCHROME(*bottom++)) {
            const unsigned char block[3] = HALFBLOCK_BOTTOM_CHARACTER;
            outString.resize(outString.size() + 3);

            memcpy(outString.data() + outString.size() - 3, block, 3);
        } else {
            outString.push_back(' ');
        }
    }

    outString.push_back(0);
}

void print_frame_data(std::vector<std::vector<char>>& rows) {
    puts("\033c");
    for(std::vector<char> r : rows) {
        puts(r.data());
    }
}

TTYOutput::TTYOutput(int width, int height)
    : Output(width, height) {
    m_out = stdout;
}

TTYOutput::~TTYOutput() {
    if(m_currentFrame)
        free_frame(m_currentFrame);
    if(m_lastFrame)
        free_frame(m_lastFrame);
    if(m_nextFrame)
        free_frame(m_nextFrame);
}

void TTYOutput::run() {
    std::unique_lock lock{m_frameLock};
    // TODO: Should probably figure out a clean way to use condition_variable
    // so this function doesn't hog the lock.
    // Shouldn't be an issue tho since the kernel should use FIFO to ensure
    // each thread gets time on the lock
    if(!m_nextFrame) {
        return;
    }

    m_currentFrame = m_nextFrame;
    m_nextFrame = nullptr;

    lock.unlock();
    m_frameCondition.notify_all();

    std::vector<std::vector<char>> strings;
    for(unsigned i = 0; i < m_height; i += 2) {
        std::vector<char> string = {};
        frame_data_to_string(m_currentFrame->data + i * m_width, m_currentFrame->data + (i + 1) * m_width, m_width, string);
        assert(string.back() == 0);

        strings.push_back(std::move(string));
    }

    lock.lock();

    // TODO: In theory it is possible for the decoder to not ask for a new
    // frame in time and hence m_lastFrame will still be valid.
    // For now just crash in this case.
    assert(!m_lastFrame);

    int currentTs = m_currentFrame->usTimestamp;
    m_lastFrame = m_currentFrame;
    m_currentFrame = nullptr;

    // We are done with the frame data
    lock.unlock();
    m_frameCondition.notify_all();

    // Sleep until it is time to draw the next frame
    if(m_lastFrameTimestamp >= 0) {
        long sleepTime = (currentTs - m_lastFrameTimestamp)
             - std::chrono::duration_cast<std::chrono::microseconds>(m_lastFrameDrawn - std::chrono::steady_clock::now()).count();

        // Don't sleep for any more than a second
        if(sleepTime > 0 && sleepTime < 1000000) {
            usleep(sleepTime);
        }
    }

    print_frame_data(strings);

    m_lastFrameTimestamp = currentTs;
    m_lastFrameDrawn = std::chrono::steady_clock::now();

    Logger::Debug("ts: {}", currentTs);
}
