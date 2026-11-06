#pragma once
#include <cstdint>
#include <string_view>

namespace ltc {

struct Version {
    static constexpr uint32_t Major = 0;
    static constexpr uint32_t Minor = 1;
    static constexpr uint32_t Patch = 0;

    static std::string_view string() noexcept;
};

} // namespace ltc
