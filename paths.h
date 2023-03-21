#pragma once

#include <string>
#include <vector>

std::vector<std::string> get_path_var();
std::string locate_executable(const std::vector<std::string>& paths,
                              const std::string& name);

void find_run_path(char* argv0);
// Returns the run path of this executable
const char* get_run_path();
