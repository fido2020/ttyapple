#include "output.h"

#include "frame.h"
#include "image.h"
#include "logger.h"

#include <cassert>

#include <string>
#include <vector>

// Max characters per line
#define GEN_C_LINE_MAX 80

std::string generate_c_array(const std::string& type, const std::string& name,
                             const std::vector<std::string>& values) {
    std::string result = fmt::format("{} {}[{}] = {{\n    ", type, name, values.size());
    int length = 4;
    for(const auto& v : values) {
        // Wrap to the next line if necessary
        // add 1 to the length to account for the comma
        length += v.length();
        if(length + 1 > GEN_C_LINE_MAX) {
            length = 4 + v.length();
            result += "\n    ";
        }

        result += v + ",";
    }

    result += "};\n";

    return result;
}

std::string generate_c_array_u8(const std::string& name, uint8_t* values, unsigned count) {
    std::string result = fmt::format("uint8_t {}[{}] = {{\n", name, count);
    while(count > 16) {
        // Since we are dealing with 8-bit ints,
        // it will save a little more room to use decimal instead of hex
        result += fmt::format("    {},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},\n",
            values[0], values[1], values[2], values[3],
            values[4], values[5], values[6], values[7],
            values[8], values[9], values[10], values[11],
            values[12], values[13], values[14], values[15]);

        values += 16;
        count -= 16;
    }
    while(count--) {
        result += fmt::format("{},", *(values++));
    }

    result += "};\n";

    return result;
}

COutput::COutput(int width, int height)
    : Output(width, height) {

    // Create a buffer to place the packed monochrome pixels of each frame in,
    // rather than reallocating one every frame.
    // Pad each row to be a 8-pixel multiple
    m_packedPixelBuffer = new uint8_t[((width + 7) / 8) * height];
}

COutput::~COutput() {
    close_file();
}

int COutput::open_file(const char* path) {
    m_out = fopen(path, "wb");
    if(!m_out) {
        Logger::Error("Failed to open '{}' for writing!", path);
        return 1;
    }

    fprintf(m_out, "#include <stdint.h>\n");

    return 0;
}

void COutput::close_file() {
    if(m_out) {
        fclose(m_out);
        m_out = nullptr;
    }
}

void COutput::run() {
    assert(m_out);

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

    int stride = (m_width + 7) / 8;

    std::string array;
    if(m_interlaced) {
        for(int i = (m_frameIndex % 2) * 2; i < m_height / 2; i += 2) {
            // Each row is padded to 8-bits,
            // so pack the pixels one row at a time
            pack_monochrome_pxls(m_packedPixelBuffer + i * stride,
                                m_currentFrame->data + (i * 2) * m_width, m_width);
            pack_monochrome_pxls(m_packedPixelBuffer + (i + 1) * stride,
                                m_currentFrame->data + (i * 2 + 1) * m_width, m_width);
        }

        array = generate_c_array_u8(fmt::format("frame{}", m_frameIndex),
                                    m_packedPixelBuffer,
                                    stride * m_height / 2);
    } else {
        for(int i = 0; i < m_height; i++) {
            // Each row is padded to 8-bits,
            // so pack the pixels one row at a time
            pack_monochrome_pxls(m_packedPixelBuffer + i * stride,
                                m_currentFrame->data + i * m_width, m_width);
        }
        array = generate_c_array_u8(fmt::format("frame{}", m_frameIndex),
                                    m_packedPixelBuffer,
                                    stride * m_height);
    }


    size_t written = fwrite(array.c_str(), 1, array.length(), m_out);
    if(written == 0 && ferror(m_out)) {
        Logger::Error("Error writing C file: {}", strerror(errno));
        std::terminate();
    }

    m_frameIndex++;

    lock.lock();

    // TODO: In theory it is possible for the decoder to not ask for a new
    // frame in time and hence m_lastFrame will still be valid.
    // For now just crash in this case.
    assert(!m_lastFrame);

    m_lastFrame = m_currentFrame;
    m_currentFrame = nullptr;

    // We are done with the frame data
    lock.unlock();
    m_frameCondition.notify_all();
}

void COutput::finish() {
    assert(m_out);

    std::vector<std::string> frameNames;
    for(int i = 0; i < m_frameIndex; i++) {
        frameNames.push_back(fmt::format("frame{}", i));
    }

    std::string text =
        fmt::format("#define FRAME_COUNT ({})\n#define FRAME_WIDTH ({})\n\
        #define FRAME_HEIGHT ({})\n#define FRAME_INTERVAL ({})\n",
            m_frameIndex, m_width, m_height, 1000000 / 24);

    if(m_interlaced) {
        text += "#define USE_INTERLACING\n";
    }

    text += generate_c_array("uint8_t*", "frames", frameNames);
    fwrite(text.c_str(), 1, text.length(), m_out);
    fflush(m_out);
}
