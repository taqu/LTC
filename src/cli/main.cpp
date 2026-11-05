#include "ltc/DDSReader.h"
#include "ltc/Version.h"
#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>
#include <array>
#include <cmath>
#include <iostream>
#include <string>

// Helper: encode a double for JSON (NaN/Inf → null or string)
static nlohmann::json doubleToJson(double v) {
    if (std::isnan(v))  return nullptr;
    if (std::isinf(v))  return (v > 0.0) ? nlohmann::json("+Inf") : nlohmann::json("-Inf");
    return nlohmann::json(v);
}

// Helper: parse "x,y" pixel coordinate string
static bool parsePixelCoord(const std::string& s, uint32_t& x, uint32_t& y) {
    auto comma = s.find(',');
    if (comma == std::string::npos) return false;
    try {
        size_t idx1 = 0, idx2 = 0;
        long lx = std::stol(s.substr(0, comma), &idx1);
        long ly = std::stol(s.substr(comma + 1), &idx2);
        if (lx < 0 || ly < 0) return false;
        x = static_cast<uint32_t>(lx);
        y = static_cast<uint32_t>(ly);
        return true;
    } catch (...) {
        return false;
    }
}

int main(int argc, char** argv) {
    CLI::App app{"LTC LUT Generator", "ltcgen"};
    app.set_version_flag("--version", std::string(ltc::Version::string()));

    // inspect subcommand
    auto* inspect = app.add_subcommand("inspect", "Inspect a DDS file");
    std::string filepath;
    inspect->add_option("file", filepath, "DDS file to inspect")->required();
    bool jsonOutput = false;
    inspect->add_flag("--json", jsonOutput, "Output as JSON");
    std::string pixelStr;
    inspect->add_option("--pixel", pixelStr, "Inspect pixel at x,y (e.g. 12,24)");
    bool stats = false;
    inspect->add_flag("--stats", stats, "Show per-channel statistics");

    CLI11_PARSE(app, argc, argv);

    if (inspect->parsed()) {
        try {
            ltc::DDSMetadata meta = ltc::DDSReader::readMetadata(filepath);

            if (jsonOutput) {
                nlohmann::json j;
                j["file"]           = filepath;
                j["width"]          = meta.width;
                j["height"]         = meta.height;
                j["depth"]          = meta.depth;
                j["mipLevels"]      = meta.mipLevels;
                j["arraySize"]      = meta.arraySize;
                j["format"]         = ltc::formatToString(meta.format);
                j["channelCount"]   = meta.channelCount;
                j["bytesPerPixel"]  = meta.bytesPerPixel;
                j["dataOffset"]     = meta.dataOffset;
                j["dataSize"]       = meta.dataSize;

                // Optional pixel
                if (!pixelStr.empty()) {
                    uint32_t px = 0, py = 0;
                    if (!parsePixelCoord(pixelStr, px, py)) {
                        std::cerr << "error: invalid pixel coordinate '" << pixelStr
                                  << "' (expected x,y)\n";
                        return 1;
                    }
                    ltc::Pixel pix = ltc::DDSReader::readPixel(filepath, px, py);
                    nlohmann::json jp;
                    jp["x"] = px;
                    jp["y"] = py;
                    nlohmann::json jch = nlohmann::json::array();
                    for (uint32_t c = 0; c < pix.channelCount; ++c) {
                        jch.push_back(static_cast<double>(pix.channels[c]));
                    }
                    jp["channels"] = jch;
                    j["pixel"] = jp;
                }

                // Optional stats
                if (stats) {
                    ltc::ImageStats ist = ltc::DDSReader::computeStats(filepath);
                    nlohmann::json jst = nlohmann::json::array();
                    for (uint32_t c = 0; c < ist.channelCount; ++c) {
                        const ltc::ChannelStats& cs = ist.channels[c];
                        nlohmann::json jc;
                        jc["channel"]     = c;
                        jc["nanCount"]    = cs.nanCount;
                        jc["posInfCount"] = cs.posInfCount;
                        jc["negInfCount"] = cs.negInfCount;
                        if (cs.hasFinite) {
                            jc["min"]  = doubleToJson(cs.min);
                            jc["max"]  = doubleToJson(cs.max);
                            jc["mean"] = doubleToJson(cs.mean);
                        } else {
                            jc["min"]  = nullptr;
                            jc["max"]  = nullptr;
                            jc["mean"] = nullptr;
                        }
                        jst.push_back(jc);
                    }
                    j["stats"] = jst;
                }

                std::cout << j.dump(2) << "\n";

            } else {
                // Human-readable output
                std::cout << "File:          " << filepath << "\n";
                std::cout << "Dimensions:    " << meta.width << "x" << meta.height;
                if (meta.depth > 1u) std::cout << "x" << meta.depth;
                std::cout << "\n";
                std::cout << "Mip levels:    " << meta.mipLevels << "\n";
                std::cout << "Array size:    " << meta.arraySize << "\n";
                std::cout << "Format:        " << ltc::formatToString(meta.format) << "\n";
                std::cout << "Channels:      " << meta.channelCount << "\n";
                std::cout << "Bytes/pixel:   " << meta.bytesPerPixel << "\n";
                std::cout << "Data offset:   " << meta.dataOffset << "\n";
                std::cout << "Data size:     " << meta.dataSize << "\n";

                if (!pixelStr.empty()) {
                    uint32_t px = 0, py = 0;
                    if (!parsePixelCoord(pixelStr, px, py)) {
                        std::cerr << "error: invalid pixel coordinate '" << pixelStr
                                  << "' (expected x,y)\n";
                        return 1;
                    }
                    ltc::Pixel pix = ltc::DDSReader::readPixel(filepath, px, py);
                    std::cout << "Pixel (" << px << "," << py << "):";
                    for (uint32_t c = 0; c < pix.channelCount; ++c) {
                        std::cout << "  ch" << c << "=" << pix.channels[c];
                    }
                    std::cout << "\n";
                }

                if (stats) {
                    ltc::ImageStats ist = ltc::DDSReader::computeStats(filepath);
                    std::cout << "Statistics:\n";
                    for (uint32_t c = 0; c < ist.channelCount; ++c) {
                        const ltc::ChannelStats& cs = ist.channels[c];
                        std::cout << "  Channel " << c << ":\n";
                        if (cs.hasFinite) {
                            std::cout << "    min:  " << cs.min << "\n";
                            std::cout << "    max:  " << cs.max << "\n";
                            std::cout << "    mean: " << cs.mean << "\n";
                        } else {
                            std::cout << "    min:  (unavailable)\n";
                            std::cout << "    max:  (unavailable)\n";
                            std::cout << "    mean: (unavailable)\n";
                        }
                        if (cs.nanCount    > 0u) std::cout << "    NaN:    " << cs.nanCount    << "\n";
                        if (cs.posInfCount > 0u) std::cout << "    +Inf:   " << cs.posInfCount << "\n";
                        if (cs.negInfCount > 0u) std::cout << "    -Inf:   " << cs.negInfCount << "\n";
                    }
                }
            }

        } catch (const ltc::DDSError& e) {
            std::cerr << "error: " << e.what() << "\n";
            return 1;
        } catch (const std::exception& e) {
            std::cerr << "error: " << e.what() << "\n";
            return 1;
        }
    }

    return 0;
}
