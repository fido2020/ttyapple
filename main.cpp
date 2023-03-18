#include <assert.h>

#include <unistd.h>
#include <png.h>
#include <getopt.h>

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "logger.h"
#include "stream_context.h"

#define BLOCK_CHARACTER {0xE2,0x96,0x88}
#define HALFBLOCK_TOP_CHARACTER {0xE2, 0x96, 0x80}
#define HALFBLOCK_BOTTOM_CHARACTER {0xE2, 0x96, 0x84}

// Downscale an image
// using a really dodgy nearest neighbour algorithm
template<typename Pixel>
std::vector<Pixel*> downscale_image(Pixel** rows, int sourceWidth, int sourceHeight, int destWidth, int destHeight) {
    assert(sourceWidth > destWidth);
    assert(sourceHeight > destHeight);
    
    std::vector<Pixel*> result;
    for(unsigned i = 0; i < destHeight; i++) {
        Pixel* row = new Pixel[destWidth];

        Pixel* source = rows[sourceWidth * i / destWidth];
        for(unsigned j = 0; j < destWidth; j++) {
            row[j] = source[sourceHeight * j / destHeight];
        }

        result.push_back(row);
    }

    return std::move(result);
}

void load_image_data(const char* str, std::vector<uint8_t*>& data, int sWidth, int sHeight) {
    FILE* imageFile = fopen(str, "rb");
    assert(imageFile);

    // assume this is a png bc im lazy

    png_structp png = nullptr;
    png_infop info = nullptr;

    png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    assert(png);

    info = png_create_info_struct(png);
    assert(info);

    int e = setjmp(png_jmpbuf(png));
    if(e) {
        printf("setjmp error %d you dumbfuck\n", e);
        exit(1);
    }

    fseek(imageFile, 8, SEEK_SET);
    png_init_io(png, imageFile);
    png_set_sig_bytes(png, 8);

    png_read_info(png, info);

    png_uint_32 width = png_get_image_width(png, info);
    png_uint_32 height = png_get_image_height(png, info);

    if(png_get_color_type(png, info) == PNG_COLOR_TYPE_GRAY)
        png_set_expand_gray_1_2_4_to_8(png);

    if (png_get_color_type(png, info) == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);

    if (png_get_color_type(png, info) == PNG_COLOR_TYPE_RGB)
        png_set_filler(png, 0xff, PNG_FILLER_AFTER);

    png_set_bgr(png);

    assert(width < INT_MAX);
    assert(height < INT_MAX);

    std::vector<uint8_t*> rows;
    if(png_get_color_type(png, info) == PNG_COLOR_TYPE_GRAY) {
        for (png_uint_32 i = 0; i < height; i++) {
            uint8_t* row = new uint8_t[width];

            png_read_row(png, (uint8_t*)row, NULL);

            rows.push_back(row);
        }
    } else {
        for (png_uint_32 i = 0; i < height; i++) {
            uint8_t* row = new uint8_t[width];

            uint32_t rgbRow[width];
            png_read_row(png, (uint8_t*)rgbRow, NULL);

            for(unsigned i = 0; i < width; i++) {
                uint32_t color = rgbRow[i];
                row[i] = ((color & 0xff) | ((color >> 8) & 0xff) | ((color >> 16) & 0xff));
            }

            rows.push_back(row);
        }
    }

    png_destroy_read_struct(&png, &info, nullptr);

    data = downscale_image<uint8_t>(rows.data(), width, height, sWidth, sHeight);

    for(const auto& r : rows) {
        delete r;
    }

    fclose(imageFile);
}

void frame_data_to_string(uint8_t* top, uint8_t* bottom, unsigned width, std::vector<char>& outString) {
    const uint8_t* end = top + width;
    while(top < end) {
        // Cut out any alpha
        // Get rid of dark greys
        if((*top++) & 0xc0) {
            if((*bottom++) & 0xc0) {
                const unsigned char block[3] = BLOCK_CHARACTER;
                outString.resize(outString.size() + 3);

                memcpy(outString.data() + outString.size() - 3, block, 3);
            } else {
                const unsigned char block[3] = HALFBLOCK_TOP_CHARACTER;
                outString.resize(outString.size() + 3);

                memcpy(outString.data() + outString.size() - 3, block, 3);
            }
        } else if((*bottom++) & 0xc0) {
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

void print_usage() {
    printf("Usage: terminalapple <video|frames> <file>");
}

struct Frame {
    uint8_t* data;
    long usTimestamp;
};

std::mutex frameDataLock;
Frame nextFrame;
Frame currentFrame;
Frame oldFrame;

template<typename Pixel>
Pixel* allocate_frame_buffer(int width, int height) {
    Pixel* buffer = new Pixel[width*height];

    return buffer;
}

void video_decoder_push_buffer(uint8_t* buffer, int64_t timestamp) {
    std::unique_lock lock{frameDataLock};
    assert(!nextFrame.data);
    nextFrame = {buffer, timestamp * 1000};
}

uint8_t* video_decoder_acquire_buffer() {
    // TODO: ew global variables
    while(1) {
        std::unique_lock lock{frameDataLock};
        if(oldFrame.data) {
            auto buffer = oldFrame.data;

            oldFrame.data = nullptr;
            return buffer;
        }
    }
}

int main(int argc, char** argv) {
    static const struct option opts[] = {
        {"width", required_argument, nullptr, 'w'},
        {"height", required_argument, nullptr, 'h'},
        {nullptr, 0, nullptr, 0}
    };
    
    int width = 96;
    int height = 72;

    char opt;
    while((opt = getopt_long(argc, argv, "w:h:", opts, nullptr)) != -1) {
        Logger::Debug("opt {} {}", opt, optind);

        if(opt == 'w') {
            width = std::stoi(optarg);
        } else if(opt == 'h') {
            height = std::stoi(optarg);
        }
    }

    if(argc - optind < 2) {
        print_usage();
        return 1;
    }

    assert(width > 0 && height > 0);

    const char* source = argv[optind];
    const char* sourceFile = argv[optind + 1];

    if(!strcmp(source, "frames")) {
        for(unsigned i = 1; i <= 7777; i++) {
            char filepath[PATH_MAX];
            snprintf(filepath, PATH_MAX, "%s/frame%03d.png", sourceFile, i);

            std::vector<uint8_t*> frameData;
            load_image_data(filepath, frameData, width, height);

            std::vector<std::vector<char>> strings;

            for(unsigned i = 0; i < frameData.size(); i += 2) {
                std::vector<char> string = {};
                frame_data_to_string(frameData[i], frameData[i + 1], 96, string);
                delete frameData[i];
                delete frameData[i + 1];

                assert(string.back() == 0);

                strings.push_back(std::move(string));
            }
            print_frame_data(strings);
            usleep(1000000 / 24);
        }
    } else if(!strcmp(source, "video")) {
        StreamContext decoder;
        decoder.acquire_buffer = video_decoder_acquire_buffer;
        decoder.push_buffer = video_decoder_push_buffer;

        decoder.set_output_format(width,height);
        decoder.play_track(sourceFile);
        {
            std::unique_lock lock{frameDataLock};
            oldFrame.data = allocate_frame_buffer<uint8_t>(width,height);
            currentFrame.data = allocate_frame_buffer<uint8_t>(width,height);
            nextFrame.data = nullptr;
        }

        while(decoder.is_playing()) {
            while(!currentFrame.data) {
                // TODO: use condition variable 
                usleep(4200);
                std::unique_lock lock{frameDataLock};
                if(nextFrame.data) {
                    currentFrame = nextFrame;
                    nextFrame.data = nullptr;
                }
            }

            std::vector<std::vector<char>> strings;
            for(unsigned i = 0; i < height; i += 2) {
                std::vector<char> string = {};
                frame_data_to_string(currentFrame.data + i * width, currentFrame.data + (i + 1) * width, width, string);
                assert(string.back() == 0);

                strings.push_back(std::move(string));
            }

            long sleepTime = 0;
            {
                std::unique_lock lock{frameDataLock};
                sleepTime = currentFrame.usTimestamp - oldFrame.usTimestamp;

                oldFrame = currentFrame;
                currentFrame.data = nullptr;
            }
	        
            usleep(sleepTime);
            print_frame_data(strings);
        }
    } else {
        print_usage();
        return 1;
    }

    return 0;
}
