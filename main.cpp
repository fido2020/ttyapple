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
#include "output.h"
#include "frame.h"
#include "image.h"
#include "stream_context.h"

void load_image_data(const char* str, std::vector<uint8_t>& data, int sWidth, int sHeight) {
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

void print_usage() {
    printf("Usage: ttyapple <video|frames> <file>");
}

Output* output;

void video_decoder_push_frame(Frame* frame) {
    output->send_frame(frame);
}

Frame* video_decoder_acquire_frame() {
    return output->acquire_frame();
}

enum class OutputFormat {
    Invalid = 0,
    Terminal,
    PortableC,
    UEFI,
};

OutputFormat get_format_for_string(const char* s) {
    if(!strcmp(s, "tty")) {
        return OutputFormat::Terminal;
    } else if(!strcmp(s, "c")) {
        return OutputFormat::PortableC;
    } else if(!strcmp(s, "uefi")) {
        return OutputFormat::UEFI;
    }

    return OutputFormat::Invalid;
}

Output* make_output(OutputFormat fmt, int width, int height) {
    switch(fmt) {
    case OutputFormat::Terminal:
        return new TTYOutput(width, height);
    case OutputFormat::PortableC:
        return new COutput(width, height);
    case OutputFormat::UEFI:
        return new UEFIOutput(width, height);
    default:
        Logger::Error("Invalid output format {}!", (int)fmt);
        return nullptr;
    };
}

int main(int argc, char** argv) {
    static const struct option opts[] = {
        {"width", required_argument, nullptr, 'w'},
        {"height", required_argument, nullptr, 'h'}, 
        {"output", required_argument, nullptr, 'o'},
        {nullptr, 0, nullptr, 0}
    };
    
    int width = 96;
    int height = 72;

    OutputFormat outputFormat = OutputFormat::Terminal;

    char opt;
    while((opt = getopt_long(argc, argv, "w:h:", opts, nullptr)) != -1) {
        if(opt == 'w') {
            width = std::stoi(optarg);
        } else if(opt == 'h') {
            height = std::stoi(optarg);
        } else if(opt == 'o') {
            outputFormat = get_format_for_string(optarg);
            if(outputFormat == OutputFormat::Invalid) {
                printf("Invalid output '%s'! Valid options are: tty, c, uefi", optarg);
                return 1;
            }
        }
    }

    if(argc - optind < 2) {
        print_usage();
        return 1;
    }

    assert(width > 0 && height > 0);

    const char* source = argv[optind];
    const char* sourceFile = argv[optind + 1];

    output = make_output(outputFormat, width, height);
    assert(output);

    if(outputFormat == OutputFormat::PortableC) {
        if(((COutput*)output)->open_file("output.c")) {
            return 2;
        }
    }

    if(!strcmp(source, "frames")) {
        for(unsigned i = 1; i <= 7777; i++) {
            char filepath[PATH_MAX];
            snprintf(filepath, PATH_MAX, "%s/frame%03d.png", sourceFile, i);

            std::vector<uint8_t> frameData;
            load_image_data(filepath, frameData, width, height);

            Frame* frame = output->acquire_frame();
            frame->usTimestamp = 1000000 / 24 * i;
            frame->data = frameData.data();

            output->send_frame(frame);
            output->run();
        }
    } else if(!strcmp(source, "video")) {
        StreamContext decoder;
        decoder.acquire_buffer = video_decoder_acquire_frame;
        decoder.push_buffer = video_decoder_push_frame;

        decoder.set_output_format(width,height);
        decoder.play_track(sourceFile);

        while(decoder.is_playing()) {
            output->run();
        }

        output->finish();
    } else {
        print_usage();
        return 1;
    }

    return 0;
}
