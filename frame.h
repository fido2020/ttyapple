#pragma once

#include <cstdint>

struct Frame {
    // Actual pixel data of the frame
    uint8_t* data;
    // Timestamp of the frame in microseconds
    long usTimestamp;
};

template<typename Pixel>
inline Pixel* allocate_frame_buffer(int width, int height) {
    Pixel* buffer = new Pixel[width*height];

    return buffer;
}

static inline void free_frame(Frame* frame) {
    if(frame->data)
        delete frame->data;
    delete frame;
}
