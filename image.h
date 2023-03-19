#pragma once

#include <cassert>
#include <vector>

// Downscale an image
// using a really dodgy nearest neighbour algorithm
template<typename Pixel>
std::vector<Pixel> downscale_image(Pixel** rows, int sourceWidth, int sourceHeight, int destWidth, int destHeight) {
    assert(sourceWidth > destWidth);
    assert(sourceHeight > destHeight);
    
    std::vector<Pixel> result;
    result.resize(destWidth * destHeight);
    for(unsigned i = 0; i < destHeight; i++) {
        Pixel* source = rows[sourceWidth * i / destWidth];
        Pixel* row = &result[destWidth * i];

        for(unsigned j = 0; j < destWidth; j++) {
            row[j] = source[sourceHeight * j / destHeight];
        }
    }

    return std::move(result);
}

// For now just mask the highest 3-bits to determine whether to treat a gray
// as white or black
#define GRAY_TO_MONOCHROME(x) (x & 0xc0)

// Packs up to 8 gray pixels into an 8-bit integer
static inline void pack_monochrome_pxls(uint8_t* buffer, uint8_t* grayPixels, unsigned amount) {
    uint8_t packed = 0;
    while(amount >= 8) {
        packed = 0;

        int i = 8;
        while(i--) {
            packed <<= 1;

            uint8_t pixel = *(grayPixels++); 
            if(GRAY_TO_MONOCHROME(pixel)) {
                packed |= 1;
            }
        }

        *(buffer++) = packed;
        amount -= 8;
    }

    packed = 0;
    while(amount--) {
        packed <<= 1;

        uint8_t pixel = *(grayPixels++); 
        if(GRAY_TO_MONOCHROME(pixel)) {
            packed |= 1;
        }
    }
    *buffer = packed;
}
