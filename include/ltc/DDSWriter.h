#pragma once
#include <filesystem>
#include "TextureData.h"

namespace ltc {

class DDSWriter {
public:
    static void write(const std::filesystem::path& path, const TextureData& texture);
};

} // namespace ltc
