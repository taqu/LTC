#pragma once
#include "LUT.h"
#include "GenerationConfig.h"
#include <stdexcept>
#include <string>

namespace ltc {

// Thrown when a checkpoint file cannot be read, is corrupted, or is
// incompatible with the requested generation configuration.
class CheckpointError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

// Versioned binary checkpoint I/O.
//
// File layout (little-endian):
//   128-byte header:
//     [0..7]   magic   "LTCBIN01"
//     [8..11]  version uint32  = 1
//     [12..15] width   uint32
//     [16..19] height  uint32
//     [20..23] profile uint32  (0=Reference 1=Quality)
//     [24..27] sampleResolution uint32
//     [28..31] maxIterations    uint32
//     [32..39] simplexStep      double
//     [40..47] tolerance        double
//     [48..55] minScale         double
//     [56..119] brdfName        char[64] null-padded
//     [120..127] flags          uint64  (bit 0 = allComplete)
//
//   width*height cell records (152 bytes each):
//     [0]      completed    uint8
//     [1]      hasFresnel   uint8
//     [2..3]   pad
//     [4..7]   status       uint32
//     [8..31]  m11,m22,m13  double x3
//     [32..103] basis[9]    double x9  (column-major)
//     [104..111] magnitude  double
//     [112..119] fresnel    double
//     [120..127] initialErr double
//     [128..135] finalErr   double
//     [136..139] iterations uint32
//     [140..143] pad
//     [144..151] evaluations uint64
//
//   8-byte checksum (sum of all preceding bytes, stored little-endian)
class Checkpoint
{
public:
    static constexpr uint32_t kVersion    = 1;
    static constexpr size_t   kHeaderSize = 128;
    static constexpr size_t   kCellSize   = 152;

    // Save lut to path (writes to a temporary file, then renames).
    // brdfName must fit in 63 bytes.
    static void save(const LUTData& lut, const std::string& path);

    // Load a checkpoint.  Throws CheckpointError on any problem.
    static LUTData load(const std::string& path);

    // Return true if the checkpoint at path is structurally compatible
    // with config.  Does not verify cell data.
    static bool isCompatible(const std::string& path,
                              const GenerationConfig& config,
                              const std::string& brdfName);

    // Verify checksum and basic structure without fully loading the file.
    static bool verify(const std::string& path);
};

} // namespace ltc
