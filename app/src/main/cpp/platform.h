#pragma once
#include <vector>
#include <string>
#include <cstdint>

bool plat_read_asset(const char* path, std::vector<uint8_t>& out);
void plat_log(const char* fmt, ...);
std::string plat_data_dir();
double plat_time();
void plat_quit();
