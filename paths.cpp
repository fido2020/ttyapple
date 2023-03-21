#include "paths.h"

#include "logger.h"

#include <string>
#include <vector>

#include <cassert>

#include <limits.h>
#include <unistd.h>

std::vector<std::string> get_path_var() {
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

static char* runpath = nullptr;
void find_run_path(char* argv0) {
    char buf[PATH_MAX + 1];
    runpath = realpath(argv0, nullptr);
    if(!runpath) {
        Logger::Error("Failed to find runpath of executable ({}), readlink: {}!", argv0, strerror(errno));
        std::terminate();
    }

    Logger::Debug("runpath: {}", runpath);
}

const char* get_run_path() {
    assert(runpath);
    return runpath;
}
