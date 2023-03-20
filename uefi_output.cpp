#include "output.h"

#include "logger.h"

#include <cassert>
#include <cstdlib>

#include <string>
#include <vector>

std::vector<std::string> get_path() {
    const char* v = getenv("PATH");
    assert(v);

    std::vector<std::string> paths;

    std::string path;
    while(*v) {
        if(*v == ':') {
            paths.push_back(std::move(path));
            path.clear();
        } else {
            path += *v;
        }

        v++;
    }

    return paths;
}

std::string locate_executable(const std::vector<std::string>& paths, const std::string& name) {
    std::string path;
    for(const auto& p : paths) {
        if(p.ends_with('/')) {
            path = p + name;
        } else {
            path = p + '/' + name;
        }

        if(access(path.c_str(), X_OK) == 0) {
            return path;
        }
    }

    return "";
}

UEFIOutput::UEFIOutput(int width, int height)
    : COutput(width, height) {
    auto paths = get_path();

    m_compiler = locate_executable(paths, "clang");
    m_linker = locate_executable(paths, "lld");

    Logger::Debug("Using Clang: {}", m_compiler);
    Logger::Debug("Using LLD: {}", m_linker);
}

void UEFIOutput::set_output_file(const char* path) {
    m_outputPath = path;
}

void UEFIOutput::finish() {
    COutput::finish();
}
