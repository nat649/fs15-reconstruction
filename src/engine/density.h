#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>
namespace fs15world {
struct DensityGrid {unsigned width=0,height=0,value_bytes=0;std::vector<std::uint16_t> values;};
DensityGrid load_grle(const std::filesystem::path&);
}
