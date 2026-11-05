#pragma once
#include <cstdint>
#include <filesystem>
#include <string>

namespace ltc {

enum class ValidationProfile { Reference, Unreal, Quality };

struct ValidationThresholds {
    double coefficientRmse = 0.01;
    double distributionL1 = 0.05;
    double lightingRelative = 0.10;
    double lightingAbsolute = 0.001;
    uint32_t maxInvalidCells = 0;
};

struct ValidationConfig {
    ValidationProfile profile = ValidationProfile::Quality;
    uint32_t distributionSamples = 65536;
    uint32_t lightingSamples = 65536;
    ValidationThresholds thresholds;
    std::filesystem::path scenariosDirectory;  // optional
    bool skipMissingReference = true;
};

} // namespace ltc
