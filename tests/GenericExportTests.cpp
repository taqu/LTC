#include <gtest/gtest.h>
#include "ltc/Beckmann.h"
#include "ltc/DisneyDiffuse.h"
#include "ltc/GGX.h"
#include "ltc/LUTExporter.h"
#include "ltc/LUTPacking.h"
#include "ltc/LUT.h"
#include "ltc/Math.h"
#include <filesystem>
#include <string>

using namespace ltc;

namespace {

static LUTData makeIdentityLUT(uint32_t w, uint32_t h,
                                const std::string& brdf = "TestBRDF",
                                double magnitude = 1.0,
                                double fresnel   = 0.5)
{
    LUTMetadata meta;
    meta.brdfName = brdf;
    meta.width    = w;
    meta.height   = h;

    LUTData lut(w, h, meta);

    for (uint32_t vi = 0; vi < h; ++vi) {
        for (uint32_t ri = 0; ri < w; ++ri) {
            LUTCell& cell = lut.at(ri, vi);
            cell.completed             = true;
            cell.status                = FitStatus::Converged;
            cell.matrix                = Mat3(1.0);
            cell.inverseMatrix         = Mat3(1.0);
            cell.magnitude             = magnitude;
            cell.fresnelCoefficient    = fresnel;
            cell.hasFresnelCoefficient = true;
            cell.parameters.m11        = 1.0;
            cell.parameters.m22        = 1.0;
            cell.parameters.m13        = 0.0;
            cell.parameters.basis      = Mat3(1.0);
        }
    }

    return lut;
}

static std::filesystem::path tempDir(const std::string& name) {
    return std::filesystem::temp_directory_path() / ("ltc_generic_" + name);
}

class GenericExportTest : public ::testing::Test {
protected:
    void TearDown() override {
        for (const auto& d : createdDirs) {
            std::error_code ec;
            std::filesystem::remove_all(d, ec);
        }
    }

    std::filesystem::path createTempDir(const std::string& name) {
        auto d = tempDir(name);
        std::filesystem::create_directories(d);
        createdDirs.push_back(d);
        return d;
    }

    std::vector<std::filesystem::path> createdDirs;
};

} // anonymous namespace

// ---- Generic profile export ----

TEST_F(GenericExportTest, GenericExport_FilesExist)
{
    auto outDir = createTempDir("generic_basic");
    auto lut    = makeIdentityLUT(2, 2, "Beckmann");

    ExportConfig cfg;
    cfg.profile         = ExportProfile::Generic;
    cfg.matrixFormat    = TextureFormat::RGBA16F;
    cfg.amplitudeFormat = TextureFormat::RG16F;
    cfg.outputDirectory = outDir;
    cfg.brdfName        = "Beckmann";

    LUTExporter exporter;
    ExportResult result = exporter.exportLUT(lut, cfg);

    EXPECT_TRUE(std::filesystem::exists(result.matrixFile))
        << "Matrix file should exist: " << result.matrixFile;
    EXPECT_TRUE(std::filesystem::exists(result.amplitudeFile))
        << "Amplitude file should exist: " << result.amplitudeFile;
}

TEST_F(GenericExportTest, GenericExport_FilenamesMatchBrdfName)
{
    auto outDir = createTempDir("generic_filenames");
    auto lut    = makeIdentityLUT(2, 2, "Beckmann");

    ExportConfig cfg;
    cfg.profile         = ExportProfile::Generic;
    cfg.matrixFormat    = TextureFormat::RGBA16F;
    cfg.amplitudeFormat = TextureFormat::RG16F;
    cfg.outputDirectory = outDir;
    cfg.brdfName        = "Beckmann";

    LUTExporter exporter;
    ExportResult result = exporter.exportLUT(lut, cfg);

    EXPECT_EQ(result.matrixFile.filename().string(),    "Beckmann_LTCMat.dds");
    EXPECT_EQ(result.amplitudeFile.filename().string(), "Beckmann_LTCAmp.dds");
}

TEST_F(GenericExportTest, GenericExport_CellCounts)
{
    auto outDir = createTempDir("generic_cells");
    auto lut    = makeIdentityLUT(2, 2, "Beckmann");

    ExportConfig cfg;
    cfg.profile         = ExportProfile::Generic;
    cfg.matrixFormat    = TextureFormat::RGBA16F;
    cfg.amplitudeFormat = TextureFormat::RG16F;
    cfg.outputDirectory = outDir;
    cfg.brdfName        = "Beckmann";

    LUTExporter exporter;
    ExportResult result = exporter.exportLUT(lut, cfg);

    EXPECT_EQ(result.cellCount,  4u);
    EXPECT_EQ(result.validCells, 4u);
}

// ---- Unreal profile regression for GGX ----

TEST_F(GenericExportTest, UnrealExport_GGXRegression)
{
    auto outDir = createTempDir("unreal_ggx");
    auto lut    = makeIdentityLUT(2, 2, "GGX");

    ExportConfig cfg;
    cfg.profile         = ExportProfile::Unreal;
    cfg.matrixFormat    = TextureFormat::RGBA16F;
    cfg.amplitudeFormat = TextureFormat::RG16F;
    cfg.outputDirectory = outDir;
    cfg.brdfName        = "GGX";

    LUTExporter exporter;
    ExportResult result = exporter.exportLUT(lut, cfg);

    EXPECT_TRUE(std::filesystem::exists(result.matrixFile));
    EXPECT_TRUE(std::filesystem::exists(result.amplitudeFile));
    EXPECT_EQ(result.matrixFile.filename().string(),    "GGX_LTCMat.dds");
    EXPECT_EQ(result.amplitudeFile.filename().string(), "GGX_LTCAmp.dds");
}

// ---- contextAlpha consistency ----

TEST(GenericExportContextAlpha, GGXContextAlphaSquaresRoughness)
{
    GGX brdf;
    EXPECT_NEAR(brdf.contextAlpha(0.5),  0.25,  1e-15);
    EXPECT_NEAR(brdf.contextAlpha(0.25), 0.0625, 1e-15);
    EXPECT_NEAR(brdf.contextAlpha(0.0),  1e-5,  1e-20); // clamped
}

TEST(GenericExportContextAlpha, DisneyDiffuseContextAlphaIdentity)
{
    DisneyDiffuse brdf;
    EXPECT_DOUBLE_EQ(brdf.contextAlpha(0.5),  0.5);
    EXPECT_DOUBLE_EQ(brdf.contextAlpha(0.25), 0.25);
    EXPECT_DOUBLE_EQ(brdf.contextAlpha(1.0),  1.0);
}

TEST(GenericExportContextAlpha, BeckmannContextAlphaSquaresRoughness)
{
    Beckmann brdf;
    EXPECT_NEAR(brdf.contextAlpha(0.5),  0.25,  1e-15);
    EXPECT_NEAR(brdf.contextAlpha(0.0),  1e-5,  1e-20); // clamped
}
