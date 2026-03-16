#include "oz/core/file.h"
#include <fstream>
#include <stdexcept>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <limits.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace oz::file {

std::vector<char> readFile(const std::string &filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file: " + filename);
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

std::string getExecutablePath() {
#if defined(_WIN32)
    char result[MAX_PATH];
    GetModuleFileName(NULL, result, MAX_PATH);
    return std::string(result);
#elif defined(__linux__)
    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    return std::string(result, (count > 0) ? count : 0);
#elif defined(__APPLE__)
    char result[PATH_MAX];
    uint32_t size = sizeof(result);
    if (_NSGetExecutablePath(result, &size) == 0)
        return std::string(result);
    else
        return std::string();
#else
    return std::string();
#endif
}

std::string getBuildPath() { return BUILD_DIR; }

std::string getSourcePath() { return SOURCE_DIR; }

} // namespace oz::file
