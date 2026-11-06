#include "ltc/Checkpoint.h"
#include "ltc/LTC.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace ltc {

namespace {

// ---- Portable little-endian I/O helpers ----

static void writeU8(std::ofstream& f, uint8_t v)   { f.write(reinterpret_cast<const char*>(&v), 1); }
static void writeU32(std::ofstream& f, uint32_t v)
{
    uint8_t b[4] = { uint8_t(v), uint8_t(v>>8), uint8_t(v>>16), uint8_t(v>>24) };
    f.write(reinterpret_cast<const char*>(b), 4);
}
static void writeU64(std::ofstream& f, uint64_t v)
{
    uint8_t b[8];
    for (int i = 0; i < 8; ++i) b[i] = uint8_t(v >> (8*i));
    f.write(reinterpret_cast<const char*>(b), 8);
}
static void writeF64(std::ofstream& f, double v)
{
    uint64_t bits;
    std::memcpy(&bits, &v, 8);
    writeU64(f, bits);
}
static void writePad(std::ofstream& f, size_t n)
{
    static const uint8_t zero[8] = {};
    while (n >= 8) { f.write(reinterpret_cast<const char*>(zero), 8); n -= 8; }
    if (n) f.write(reinterpret_cast<const char*>(zero), n);
}

static uint8_t  readU8(std::ifstream& f) { uint8_t v; f.read(reinterpret_cast<char*>(&v), 1); return v; }
static uint32_t readU32(std::ifstream& f)
{
    uint8_t b[4]; f.read(reinterpret_cast<char*>(b), 4);
    return uint32_t(b[0]) | (uint32_t(b[1])<<8) | (uint32_t(b[2])<<16) | (uint32_t(b[3])<<24);
}
static uint64_t readU64(std::ifstream& f)
{
    uint8_t b[8]; f.read(reinterpret_cast<char*>(b), 8);
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= (uint64_t(b[i]) << (8*i));
    return v;
}
static double readF64(std::ifstream& f) { uint64_t bits = readU64(f); double v; std::memcpy(&v, &bits, 8); return v; }
static void   skipBytes(std::ifstream& f, size_t n) { f.seekg(static_cast<std::streamoff>(n), std::ios::cur); }

// Sum all bytes of a stream from pos 0 to current pos (exclusive end).
static uint64_t computeChecksum(const std::string& path, uint64_t byteCount)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return 0;
    uint64_t sum = 0;
    uint64_t remaining = byteCount;
    char buf[4096];
    while (remaining > 0) {
        size_t chunk = remaining < sizeof(buf) ? static_cast<size_t>(remaining) : sizeof(buf);
        f.read(buf, static_cast<std::streamsize>(chunk));
        size_t got = static_cast<size_t>(f.gcount());
        for (size_t i = 0; i < got; ++i) sum += static_cast<uint8_t>(buf[i]);
        remaining -= got;
        if (got < chunk) break;
    }
    return sum;
}

static void writeCellRecord(std::ofstream& f, const LUTCell& cell)
{
    writeU8 (f, cell.completed ? 1 : 0);
    writeU8 (f, cell.hasFresnelCoefficient ? 1 : 0);
    writePad(f, 2);                              // pad [2..3]
    writeU32(f, static_cast<uint32_t>(cell.status));
    writeF64(f, cell.parameters.m11);
    writeF64(f, cell.parameters.m22);
    writeF64(f, cell.parameters.m13);
    // Basis: 9 doubles in column-major order
    for (int col = 0; col < 3; ++col)
        for (int row = 0; row < 3; ++row)
            writeF64(f, cell.parameters.basis[col][row]);
    writeF64(f, cell.magnitude);
    writeF64(f, cell.fresnelCoefficient);
    writeF64(f, cell.initialError);
    writeF64(f, cell.finalError);
    writeU32(f, cell.iterations);
    writePad(f, 4);                              // pad [140..143]
    writeU64(f, cell.evaluations);
    // Total: 152 bytes per cell record.
}

static LUTCell readCellRecord(std::ifstream& f)
{
    LUTCell cell;
    cell.completed              = readU8(f) != 0;
    cell.hasFresnelCoefficient  = readU8(f) != 0;
    skipBytes(f, 2);                             // pad
    cell.status                 = static_cast<FitStatus>(readU32(f));
    cell.parameters.m11         = readF64(f);
    cell.parameters.m22         = readF64(f);
    cell.parameters.m13         = readF64(f);
    for (int col = 0; col < 3; ++col)
        for (int row = 0; row < 3; ++row)
            cell.parameters.basis[col][row] = static_cast<Scalar>(readF64(f));
    cell.magnitude              = readF64(f);
    cell.fresnelCoefficient     = readF64(f);
    cell.initialError           = readF64(f);
    cell.finalError             = readF64(f);
    cell.iterations             = readU32(f);
    skipBytes(f, 4);                             // pad
    cell.evaluations            = readU64(f);

    // Reconstruct full matrices from parameters.
    if (cell.completed) {
        LTC ltc;
        const auto& p = cell.parameters;
        Vec3 bX(p.basis[0]), bY(p.basis[1]), bZ(p.basis[2]);
        if (ltc.setBasis(bX, bY, bZ) && ltc.setParameters(p.m11, p.m22, p.m13)) {
            cell.matrix        = ltc.matrix();
            cell.inverseMatrix = ltc.inverseMatrix();
        }
    }
    return cell;
}

} // anonymous namespace

// ---- Public API ----

void Checkpoint::save(const LUTData& lut, const std::string& path)
{
    const std::string tmp = path + ".tmp";

    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) throw CheckpointError("Checkpoint: cannot open '" + tmp + "' for writing");

        const auto& meta = lut.metadata();

        // Header (128 bytes)
        f.write("LTCBIN01", 8);                              // magic
        writeU32(f, kVersion);                               // [8]
        writeU32(f, lut.width());                            // [12]
        writeU32(f, lut.height());                           // [16]
        writeU32(f, static_cast<uint32_t>(meta.fittingProfile)); // [20]
        writeU32(f, meta.sampleResolution);                  // [24]
        writeU32(f, meta.maxIterations);                     // [28]
        writeF64(f, 0.05);  // simplexStep (default; not stored in LUTMetadata)
        writeF64(f, 1e-5);  // tolerance
        writeF64(f, 1e-7);  // minScale
        // BRDF name: 64 bytes, null-padded  [56..119]
        {
            char name[64] = {};
            const std::string& bn = meta.brdfName;
            size_t len = bn.size() < 63 ? bn.size() : 63;
            std::memcpy(name, bn.c_str(), len);
            f.write(name, 64);
        }
        // Flags [120..127]
        uint64_t flags = 0;
        // Check if all cells are complete
        bool allDone = true;
        for (const auto& cell : lut.cells()) if (!cell.completed) { allDone = false; break; }
        if (allDone) flags |= 1;
        writeU64(f, flags);

        // Cell records
        for (uint32_t t = 0; t < lut.height(); ++t)
            for (uint32_t a = 0; a < lut.width(); ++a)
                writeCellRecord(f, lut.at(a, t));

        f.flush();
        if (!f) throw CheckpointError("Checkpoint: write error on '" + tmp + "'");
    } // close tmp file

    // Compute checksum over header + cells
    uint64_t dataSize = kHeaderSize + static_cast<uint64_t>(lut.width()) * lut.height() * kCellSize;
    uint64_t checksum = computeChecksum(tmp, dataSize);

    {
        std::ofstream f(tmp, std::ios::binary | std::ios::app);
        writeU64(f, checksum);
        f.flush();
    }

    // Atomically replace previous checkpoint (best-effort on Windows).
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        // Fall back to copy+delete on cross-device rename failure.
        std::filesystem::copy_file(tmp, path, std::filesystem::copy_options::overwrite_existing, ec);
        if (!ec) std::filesystem::remove(tmp, ec);
        if (ec) throw CheckpointError("Checkpoint: cannot replace '" + path + "': " + ec.message());
    }
}

LUTData Checkpoint::load(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) throw CheckpointError("Checkpoint: cannot open '" + path + "'");

    // Read magic
    char magic[8];
    f.read(magic, 8);
    if (std::strncmp(magic, "LTCBIN01", 8) != 0)
        throw CheckpointError("Checkpoint: invalid magic in '" + path + "'");

    uint32_t version = readU32(f);
    if (version != kVersion)
        throw CheckpointError("Checkpoint: unsupported version " + std::to_string(version));

    uint32_t width   = readU32(f);
    uint32_t height  = readU32(f);
    uint32_t profile = readU32(f);
    uint32_t sampleRes  = readU32(f);
    uint32_t maxIter    = readU32(f);
    skipBytes(f, 24); // simplexStep, tolerance, minScale (8 bytes each)

    char name[64] = {};
    f.read(name, 64);

    skipBytes(f, 8); // flags

    if (width < 2 || height < 2)
        throw CheckpointError("Checkpoint: invalid grid dimensions");

    // Verify checksum before loading cells.
    uint64_t dataSize = kHeaderSize + static_cast<uint64_t>(width) * height * kCellSize;
    uint64_t expected = computeChecksum(path, dataSize);
    f.seekg(static_cast<std::streamoff>(dataSize));
    uint64_t stored = readU64(f);
    if (!f) throw CheckpointError("Checkpoint: truncated file '" + path + "'");
    if (stored != expected)
        throw CheckpointError("Checkpoint: checksum mismatch in '" + path + "'");

    // Now read cells.
    f.seekg(static_cast<std::streamoff>(kHeaderSize));

    LUTMetadata meta;
    meta.brdfName         = std::string(name);
    meta.width            = width;
    meta.height           = height;
    meta.fittingProfile   = static_cast<FittingProfile>(profile);
    meta.sampleResolution = sampleRes;
    meta.maxIterations    = maxIter;

    LUTData lut(width, height, std::move(meta));

    for (uint32_t t = 0; t < height; ++t)
        for (uint32_t a = 0; a < width; ++a)
            lut.at(a, t) = readCellRecord(f);

    return lut;
}

bool Checkpoint::isCompatible(const std::string& path,
                               const GenerationConfig& config,
                               const std::string& brdfName)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    char magic[8];
    f.read(magic, 8);
    if (std::strncmp(magic, "LTCBIN01", 8) != 0) return false;

    uint32_t version = readU32(f);
    if (version != kVersion) return false;

    uint32_t width  = readU32(f);
    uint32_t height = readU32(f);
    if (width != config.width || height != config.height) return false;

    readU32(f); // profile — allow mismatch (user may change profile on resume)
    uint32_t sampleRes = readU32(f);
    uint32_t maxIter   = readU32(f);
    if (sampleRes != config.fitting.sampleResolution) return false;
    if (maxIter   != config.fitting.maxIterations)    return false;

    skipBytes(f, 24); // simplexStep, tolerance, minScale

    char name[64] = {};
    f.read(name, 64);
    if (std::strncmp(name, brdfName.c_str(), 63) != 0) return false;

    return true;
}

bool Checkpoint::verify(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    char magic[8];
    f.read(magic, 8);
    if (std::strncmp(magic, "LTCBIN01", 8) != 0) return false;

    uint32_t version = readU32(f);
    if (version != kVersion) return false;

    uint32_t width  = readU32(f);
    uint32_t height = readU32(f);
    if (width < 2 || height < 2) return false;

    uint64_t dataSize = kHeaderSize + static_cast<uint64_t>(width) * height * kCellSize;

    f.seekg(0, std::ios::end);
    uint64_t fileSize = static_cast<uint64_t>(f.tellg());
    if (fileSize < dataSize + 8) return false;

    uint64_t expected = computeChecksum(path, dataSize);
    f.seekg(static_cast<std::streamoff>(dataSize));
    uint64_t stored = readU64(f);
    return stored == expected;
}

} // namespace ltc
