#pragma once

#include <string>
#include <vector>

namespace oz::file {

std::vector<char> readFile(const std::string &filename);

std::string getExecutablePath();
std::string getBuildPath();
std::string getSourcePath();

} // namespace oz::file
