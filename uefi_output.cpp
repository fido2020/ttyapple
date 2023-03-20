#include "output.h"

#include "logger.h"

#include <cassert>
#include <cstdlib>

#include <string>
#include <vector>

#include <fcntl.h>

#include <sys/wait.h>

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
    // lld-link is the Windows linker
    m_linker = locate_executable(paths, "lld-link");

    Logger::Debug("Using Clang: {}", m_compiler);
    Logger::Debug("Using LLD: {}", m_linker);

    if(m_compiler.empty()) {
        Logger::Error("Failed to find Clang binary!");
        std::terminate();
    }

    if(m_linker.empty()) {
        Logger::Error("Failed to find LLD binary!");
        std::terminate();
    }

    m_decodeSource = fopen("data/c_frame_decoder.c", "r");
    if(!m_decodeSource) {
        Logger::Debug("Error opening 'data/c_frame_decoder.c': {}!", strerror(errno));
        std::terminate();
    }

    m_uefiMainSource = fopen("data/uefi_main.c", "r");
    if(!m_uefiMainSource) {
        Logger::Debug("Error opening 'data/uefi_main.c': {}!", strerror(errno));
        std::terminate();
    }

    if(pipe(m_pipe)) {
        Logger::Error("Failed to create pipe: {}!", strerror(errno));
        std::terminate();
    }

    if(fcntl(m_pipe[1], F_SETFD, FD_CLOEXEC)) {
        Logger::Error("Failed to set cloexec on pipe: {}!", strerror(errno));
        std::terminate();
    }

    m_out = fdopen(m_pipe[1], "wb");
    if(!m_out) {
        Logger::Debug("fdopen: {}!", strerror(errno));
        std::terminate();
    }

    std::vector<char*> clangArguments;
    clangArguments.push_back(strdup(m_compiler.c_str()));
    clangArguments.push_back(strdup("-target"));
    clangArguments.push_back(strdup("x86_64-unknown-windows"));
    clangArguments.push_back(strdup("-c"));
    // Read from stdin
    clangArguments.push_back(strdup("-x"));
    clangArguments.push_back(strdup("c"));
    clangArguments.push_back(strdup("-"));
    // Defines
    clangArguments.push_back(strdup("-DMDE_CPU_X64"));
    clangArguments.push_back(strdup("-DUEFI"));
    clangArguments.push_back(strdup("-Iuefi"));

    clangArguments.push_back(strdup("-Wno-microsoft-static-assert"));

    clangArguments.push_back(strdup("-ffreestanding"));
    clangArguments.push_back(strdup("-mno-red-zone"));
    clangArguments.push_back(strdup("-o"));
    clangArguments.push_back(strdup("frames.o"));
    clangArguments.push_back(nullptr);

    m_compilerPID = fork();
    if(!m_compilerPID) {
        if(dup2(m_pipe[0], STDIN_FILENO)) {
            Logger::Debug("dup2: {}!", strerror(errno));
            exit(1);
        }

        if(execv(m_compiler.c_str(), clangArguments.data())) {
            Logger::Debug("Failed to execute {}: {}!", m_compiler.c_str(), strerror(errno));
        }
        exit(1);
    }

    for(char* arg : clangArguments) {
        free(arg);
    }

    if(m_compilerPID < 0) {
        Logger::Error("Failed to create child process: {}!", strerror(errno));
        std::terminate();
    }

    fprintf(m_out, "#include <stdint.h>\n");
}

void UEFIOutput::set_output_file(const char* path) {
    m_outputPath = path;
}

void UEFIOutput::finish() {
    COutput::finish();

    char buf[0x2000];
    size_t result;
    while((result = fread(buf, 1, 0x2000, m_decodeSource)) > 0) {
        if(fwrite(buf, 1, result, m_out) != result) {
            // Likely encountered some error
            break;
        }
    }

    if(ferror(m_decodeSource)) {
        Logger::Error("Failed to read c_decode_frames.c: {}!", strerror(errno));
    }

    if(ferror(m_out)) {
        Logger::Error("Failed to write to pipe: {}!", strerror(errno));
    }

    while((result = fread(buf, 1, 0x2000, m_uefiMainSource)) > 0) {
        fwrite(buf, 1, result, m_out);
    }

    if(ferror(m_uefiMainSource)) {
        Logger::Error("Failed to read uefi_main.c: {}!", strerror(errno));
    }

    if(ferror(m_out)) {
        Logger::Error("Failed to write to pipe: {}!", strerror(errno));
    }

    fclose(m_out);

    m_out = nullptr;

    int status;
    if(waitpid(-1, &status, 0) < 0) {
        Logger::Error("Error waiting for '{}': {}!", m_compiler, strerror(errno));
        std::terminate();
    }

    if(!WIFEXITED(status)) {
        Logger::Error("Process '{}' was terminated with status {}.", m_compiler, status);
        std::terminate();
    }

    if(WEXITSTATUS(status) != 0) {
        Logger::Error("Process '{}' exited with status {}.", m_compiler, WEXITSTATUS(status));
        std::terminate();
    }

    std::vector<char*> lldArguments;
    lldArguments.push_back(strdup("lld-link"));
    lldArguments.push_back(strdup("-subsystem:efi_application"));
    lldArguments.push_back(strdup("-entry:efi_main"));
    lldArguments.push_back(strdup("-out:output.efi"));
    lldArguments.push_back(strdup("frames.o"));
    lldArguments.push_back(nullptr);

    m_compilerPID = fork();
    if(!m_compilerPID) {
        if(execv(m_linker.c_str(), lldArguments.data())) {
            Logger::Debug("Failed to execute {}: {}!", m_linker.c_str(), strerror(errno));
        }
        exit(1);
    }

    for(char* arg : lldArguments) {
        free(arg);
    }

    if(waitpid(-1, &status, 0) < 0) {
        Logger::Error("Error waiting for '{}': {}!", m_linker, strerror(errno));
        std::terminate();
    }

    unlink("frames.o");

    if(!WIFEXITED(status)) {
        Logger::Error("Process '{}' was terminated with status {}.", m_linker, status);
        std::terminate();
    }

    if(WEXITSTATUS(status) != 0) {
        Logger::Error("Process '{}' exited with status {}.", m_linker, WEXITSTATUS(status));
        std::terminate();
    }
}
