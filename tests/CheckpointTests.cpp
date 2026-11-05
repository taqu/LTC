#include <gtest/gtest.h>
#include "ltc/Checkpoint.h"
#include "ltc/LUTGenerator.h"
#include "ltc/GGX.h"
#include <filesystem>
#include <fstream>
#include <string>

using namespace ltc;

namespace {

static GenerationConfig checkpointConfig()
{
    GenerationConfig c;
    c.width   = 4;
    c.height  = 4;
    c.threads = 1;
    c.fitting.sampleResolution = 8;
    c.fitting.maxIterations    = 20;
    return c;
}

// Generate a small LUT and return it plus a temp path.
static std::string tempPath(const std::string& suffix)
{
    return (std::filesystem::temp_directory_path() / ("ltc_test_" + suffix + ".checkpoint")).string();
}

} // namespace

// ---- Save + Load round-trip ----

TEST(Checkpoint, SaveAndLoadPreservesAllCells)
{
    GGX brdf;
    LUTGenerator gen;
    auto original = gen.generate(brdf, checkpointConfig());

    std::string path = tempPath("roundtrip");
    Checkpoint::save(original, path);

    auto loaded = Checkpoint::load(path);
    std::filesystem::remove(path);

    ASSERT_EQ(loaded.width(),  original.width());
    ASSERT_EQ(loaded.height(), original.height());

    for (uint32_t t = 0; t < original.height(); ++t) {
        for (uint32_t a = 0; a < original.width(); ++a) {
            SCOPED_TRACE("cell (" + std::to_string(a) + "," + std::to_string(t) + ")");
            const auto& co = original.at(a, t);
            const auto& cl = loaded.at(a, t);
            EXPECT_EQ(cl.completed, co.completed);
            EXPECT_EQ(cl.status,    co.status);
            EXPECT_DOUBLE_EQ(cl.parameters.m11,  co.parameters.m11);
            EXPECT_DOUBLE_EQ(cl.parameters.m22,  co.parameters.m22);
            EXPECT_DOUBLE_EQ(cl.parameters.m13,  co.parameters.m13);
            EXPECT_DOUBLE_EQ(cl.magnitude,        co.magnitude);
            EXPECT_DOUBLE_EQ(cl.fresnelCoefficient, co.fresnelCoefficient);
            EXPECT_DOUBLE_EQ(cl.initialError,     co.initialError);
            EXPECT_DOUBLE_EQ(cl.finalError,       co.finalError);
            EXPECT_EQ(cl.iterations,              co.iterations);
            EXPECT_EQ(cl.evaluations,             co.evaluations);
        }
    }
}

// ---- Verify ----

TEST(Checkpoint, VerifyPassesOnValidFile)
{
    GGX brdf;
    LUTGenerator gen;
    auto lut = gen.generate(brdf, checkpointConfig());

    std::string path = tempPath("verify");
    Checkpoint::save(lut, path);
    EXPECT_TRUE(Checkpoint::verify(path));
    std::filesystem::remove(path);
}

// ---- Corrupted checkpoint ----

TEST(Checkpoint, CorruptedCheckpointThrows)
{
    GGX brdf;
    LUTGenerator gen;
    auto lut = gen.generate(brdf, checkpointConfig());

    std::string path = tempPath("corrupt");
    Checkpoint::save(lut, path);

    // Corrupt one byte in the middle of the cell data section.
    {
        std::fstream f(path, std::ios::binary | std::ios::in | std::ios::out);
        f.seekp(Checkpoint::kHeaderSize + 20);
        char bad = static_cast<char>(0xFF);
        f.write(&bad, 1);
    }

    EXPECT_FALSE(Checkpoint::verify(path));
    EXPECT_THROW(Checkpoint::load(path), CheckpointError);
    std::filesystem::remove(path);
}

// ---- Truncated checkpoint ----

TEST(Checkpoint, TruncatedCheckpointThrows)
{
    GGX brdf;
    LUTGenerator gen;
    auto lut = gen.generate(brdf, checkpointConfig());

    std::string path = tempPath("truncated");
    Checkpoint::save(lut, path);

    // Truncate to half the expected size.
    auto fullSize = std::filesystem::file_size(path);
    std::filesystem::resize_file(path, fullSize / 2);

    EXPECT_FALSE(Checkpoint::verify(path));
    EXPECT_THROW(Checkpoint::load(path), CheckpointError);
    std::filesystem::remove(path);
}

// ---- Invalid magic ----

TEST(Checkpoint, WrongMagicThrows)
{
    std::string path = tempPath("badmagic");
    {
        std::ofstream f(path, std::ios::binary | std::ios::trunc);
        f.write("BADHDR!!", 8);
        // Write minimal junk
        char buf[200] = {};
        f.write(buf, sizeof(buf));
    }
    EXPECT_THROW(Checkpoint::load(path), CheckpointError);
    std::filesystem::remove(path);
}

// ---- Incompatible config detection ----

TEST(Checkpoint, IncompatibleConfigReturnsFalse)
{
    GGX brdf;
    LUTGenerator gen;
    auto lut = gen.generate(brdf, checkpointConfig());

    std::string path = tempPath("incompat");
    Checkpoint::save(lut, path);

    // Different width → incompatible
    GenerationConfig otherCfg = checkpointConfig();
    otherCfg.width = 8;
    EXPECT_FALSE(Checkpoint::isCompatible(path, otherCfg, "ggx"));

    // Same config → compatible
    EXPECT_TRUE(Checkpoint::isCompatible(path, checkpointConfig(), "ggx"));

    // Different BRDF name → incompatible
    EXPECT_FALSE(Checkpoint::isCompatible(path, checkpointConfig(), "lambert"));

    std::filesystem::remove(path);
}

// ---- Resume: partial generation + continue ----

TEST(Checkpoint, ResumeMatchesUninterruptedGeneration)
{
    GGX brdf;
    LUTGenerator gen;

    // Generate fully without checkpoint.
    auto full = gen.generate(brdf, checkpointConfig());

    // Simulate a checkpoint by saving a fully completed LUT,
    // then loading and running generate() with resume enabled
    // (should skip all cells and return same result).
    std::string path = tempPath("resume");
    Checkpoint::save(full, path);

    GenerationConfig resumeCfg = checkpointConfig();
    resumeCfg.enableCheckpoint      = false; // don't overwrite
    resumeCfg.resumeFromCheckpoint  = true;
    resumeCfg.checkpointPath        = path;

    auto resumed = gen.generate(brdf, resumeCfg);
    std::filesystem::remove(path);

    ASSERT_EQ(resumed.width(),  full.width());
    ASSERT_EQ(resumed.height(), full.height());
    for (uint32_t t = 0; t < full.height(); ++t) {
        for (uint32_t a = 0; a < full.width(); ++a) {
            SCOPED_TRACE("cell (" + std::to_string(a) + "," + std::to_string(t) + ")");
            const auto& cf = full.at(a, t);
            const auto& cr = resumed.at(a, t);
            EXPECT_DOUBLE_EQ(cr.parameters.m11, cf.parameters.m11);
            EXPECT_DOUBLE_EQ(cr.parameters.m22, cf.parameters.m22);
            EXPECT_DOUBLE_EQ(cr.finalError,     cf.finalError);
        }
    }
}

// ---- Thread count does not affect compatibility ----

TEST(Checkpoint, DifferentThreadCountIsCompatible)
{
    GGX brdf;
    LUTGenerator gen;
    auto lut = gen.generate(brdf, checkpointConfig());

    std::string path = tempPath("threads");
    Checkpoint::save(lut, path);

    // threads is not stored in checkpoint — always compatible
    GenerationConfig cfg2 = checkpointConfig();
    cfg2.threads = 4;
    EXPECT_TRUE(Checkpoint::isCompatible(path, cfg2, "ggx"));
    std::filesystem::remove(path);
}
