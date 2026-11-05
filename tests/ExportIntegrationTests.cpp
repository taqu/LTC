#include "ltc/LUTExporter.h"
#include "ltc/LUTPacking.h"
#include "ltc/DDSReader.h"
#include "ltc/Float16.h"
#include "ltc/LUT.h"
#include "ltc/Math.h"
#include "ltc/ParameterGrid.h"
#include <gtest/gtest.h>
#include <cmath>
#include <filesystem>
#include <string>

namespace {

// ---- Helper: build a small synthetic LUTData ----
//
// Creates a width x height LUT where every cell is "completed"
// with identity-like LTC parameters (m11=1, m22=1, m13=0, identity basis).
// Matrix = identity, inverseMatrix = identity, magnitude = 1.0, fresnel = 0.5

static ltc::LUTData makeIdentityLUT(uint32_t w, uint32_t h,
                                     double magnitude = 1.0,
                                     double fresnel   = 0.5)
{
    ltc::LUTMetadata meta;
    meta.brdfName = "TestBRDF";
    meta.width    = w;
    meta.height   = h;

    ltc::LUTData lut(w, h, meta);

    for (uint32_t vi = 0; vi < h; ++vi) {
        for (uint32_t ri = 0; ri < w; ++ri) {
            ltc::LUTCell& cell = lut.at(ri, vi);
            cell.completed             = true;
            cell.status                = ltc::FitStatus::Converged;
            cell.matrix                = ltc::Mat3(1.0); // identity
            cell.inverseMatrix         = ltc::Mat3(1.0); // identity
            cell.magnitude             = magnitude;
            cell.fresnelCoefficient    = fresnel;
            cell.hasFresnelCoefficient = true;

            // Fill LTCParameters for consistency
            cell.parameters.m11   = 1.0;
            cell.parameters.m22   = 1.0;
            cell.parameters.m13   = 0.0;
            cell.parameters.basis = ltc::Mat3(1.0);
        }
    }

    return lut;
}

static std::filesystem::path tempDir(const std::string& name) {
    return std::filesystem::temp_directory_path() / ("ltc_export_" + name);
}

// ---- Test fixture ----

class ExportIntegrationTest : public ::testing::Test {
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

// ---- Unreal profile export ----

TEST_F(ExportIntegrationTest, UnrealExport_FilesExist) {
    auto outDir = createTempDir("unreal_basic");
    auto lut    = makeIdentityLUT(4, 4);

    ltc::ExportConfig cfg;
    cfg.profile         = ltc::ExportProfile::Unreal;
    cfg.matrixFormat    = ltc::TextureFormat::RGBA16F;
    cfg.amplitudeFormat = ltc::TextureFormat::RG16F;
    cfg.outputDirectory = outDir;
    cfg.brdfName        = "GGX";

    ltc::LUTExporter exporter;
    ltc::ExportResult result = exporter.exportLUT(lut, cfg);

    EXPECT_TRUE(std::filesystem::exists(result.matrixFile))
        << "Matrix file should exist: " << result.matrixFile;
    EXPECT_TRUE(std::filesystem::exists(result.amplitudeFile))
        << "Amplitude file should exist: " << result.amplitudeFile;

    // Check filenames
    EXPECT_EQ(result.matrixFile.filename().string(),   "GGX_LTCMat.dds");
    EXPECT_EQ(result.amplitudeFile.filename().string(), "GGX_LTCAmp.dds");

    // Verify counts
    EXPECT_EQ(result.cellCount,  16u);
    EXPECT_EQ(result.validCells, 16u);
}

TEST_F(ExportIntegrationTest, UnrealExport_MatrixMetadata) {
    auto outDir = createTempDir("unreal_mat_meta");
    auto lut    = makeIdentityLUT(4, 4);

    ltc::ExportConfig cfg;
    cfg.profile         = ltc::ExportProfile::Unreal;
    cfg.matrixFormat    = ltc::TextureFormat::RGBA16F;
    cfg.amplitudeFormat = ltc::TextureFormat::RG16F;
    cfg.outputDirectory = outDir;
    cfg.brdfName        = "GGX";

    ltc::LUTExporter exporter;
    ltc::ExportResult result = exporter.exportLUT(lut, cfg);

    ltc::DDSMetadata matMeta = ltc::DDSReader::readMetadata(result.matrixFile);
    EXPECT_EQ(matMeta.width,         4u);
    EXPECT_EQ(matMeta.height,        4u);
    EXPECT_EQ(matMeta.format,        ltc::DDSFormat::RGBA16F);
    EXPECT_EQ(matMeta.channelCount,  4u);
    EXPECT_EQ(matMeta.bytesPerPixel, 8u);
}

TEST_F(ExportIntegrationTest, UnrealExport_AmplitudeMetadata) {
    auto outDir = createTempDir("unreal_amp_meta");
    auto lut    = makeIdentityLUT(4, 4);

    ltc::ExportConfig cfg;
    cfg.profile         = ltc::ExportProfile::Unreal;
    cfg.matrixFormat    = ltc::TextureFormat::RGBA16F;
    cfg.amplitudeFormat = ltc::TextureFormat::RG16F;
    cfg.outputDirectory = outDir;
    cfg.brdfName        = "GGX";

    ltc::LUTExporter exporter;
    ltc::ExportResult result = exporter.exportLUT(lut, cfg);

    ltc::DDSMetadata ampMeta = ltc::DDSReader::readMetadata(result.amplitudeFile);
    EXPECT_EQ(ampMeta.width,         4u);
    EXPECT_EQ(ampMeta.height,        4u);
    EXPECT_EQ(ampMeta.format,        ltc::DDSFormat::RG16F);
    EXPECT_EQ(ampMeta.channelCount,  2u);
    EXPECT_EQ(ampMeta.bytesPerPixel, 4u);
}

TEST_F(ExportIntegrationTest, UnrealExport_IdentityMatrix_PixelValues) {
    // Identity matrix: invM = identity, center diag = 1
    // packed: r=1, g=0, b=0, a=1
    auto outDir = createTempDir("unreal_identity_pix");
    auto lut    = makeIdentityLUT(2, 2);

    ltc::ExportConfig cfg;
    cfg.profile         = ltc::ExportProfile::Unreal;
    cfg.matrixFormat    = ltc::TextureFormat::RGBA16F;
    cfg.amplitudeFormat = ltc::TextureFormat::RG16F;
    cfg.outputDirectory = outDir;
    cfg.brdfName        = "GGX";

    ltc::LUTExporter exporter;
    ltc::ExportResult result = exporter.exportLUT(lut, cfg);

    // Read back pixel (0,0) from matrix file
    ltc::Pixel pix = ltc::DDSReader::readPixel(result.matrixFile, 0, 0);
    EXPECT_EQ(pix.channelCount, 4u);
    // r=1, g=0, b=0, a=1 (from identity matrix packing)
    EXPECT_NEAR(pix.channels[0], 1.0f, 0.01f) << "r should be ~1";
    EXPECT_NEAR(pix.channels[1], 0.0f, 0.01f) << "g should be ~0";
    EXPECT_NEAR(pix.channels[2], 0.0f, 0.01f) << "b should be ~0";
    EXPECT_NEAR(pix.channels[3], 1.0f, 0.01f) << "a should be ~1";
}

TEST_F(ExportIntegrationTest, UnrealExport_AmplitudePixelValues) {
    auto outDir = createTempDir("unreal_amp_pix");
    auto lut    = makeIdentityLUT(2, 2, 0.75, 0.25);

    ltc::ExportConfig cfg;
    cfg.profile         = ltc::ExportProfile::Unreal;
    cfg.matrixFormat    = ltc::TextureFormat::RGBA16F;
    cfg.amplitudeFormat = ltc::TextureFormat::RG16F;
    cfg.outputDirectory = outDir;
    cfg.brdfName        = "GGX";

    ltc::LUTExporter exporter;
    ltc::ExportResult result = exporter.exportLUT(lut, cfg);

    // Read pixel (0,0) from amplitude file
    ltc::Pixel pix = ltc::DDSReader::readPixel(result.amplitudeFile, 0, 0);
    EXPECT_EQ(pix.channelCount, 2u);
    // R = magnitude = 0.75, G = fresnelCoefficient = 0.25
    EXPECT_NEAR(pix.channels[0], 0.75f, 0.01f) << "R should be magnitude ~0.75";
    EXPECT_NEAR(pix.channels[1], 0.25f, 0.01f) << "G should be fresnel ~0.25";
}

// ---- Reference profile export ----

TEST_F(ExportIntegrationTest, ReferenceExport_FilesExist) {
    auto outDir = createTempDir("ref_basic");
    auto lut    = makeIdentityLUT(4, 4);

    ltc::ExportConfig cfg;
    cfg.profile         = ltc::ExportProfile::Reference;
    cfg.matrixFormat    = ltc::TextureFormat::RGBA16F;
    cfg.amplitudeFormat = ltc::TextureFormat::RGBA16F;
    cfg.outputDirectory = outDir;
    cfg.brdfName        = "GGX";

    ltc::LUTExporter exporter;
    ltc::ExportResult result = exporter.exportLUT(lut, cfg);

    EXPECT_TRUE(std::filesystem::exists(result.matrixFile));
    EXPECT_TRUE(std::filesystem::exists(result.amplitudeFile));

    EXPECT_EQ(result.matrixFile.filename().string(),   "GGX_LTCMat_ref.dds");
    EXPECT_EQ(result.amplitudeFile.filename().string(), "GGX_LTCAmp_ref.dds");
}

TEST_F(ExportIntegrationTest, ReferenceExport_AmplitudeIsRGBA16F) {
    auto outDir = createTempDir("ref_amp_format");
    auto lut    = makeIdentityLUT(4, 4);

    ltc::ExportConfig cfg;
    cfg.profile         = ltc::ExportProfile::Reference;
    cfg.matrixFormat    = ltc::TextureFormat::RGBA16F;
    cfg.amplitudeFormat = ltc::TextureFormat::RG16F; // should be overridden to RGBA16F
    cfg.outputDirectory = outDir;
    cfg.brdfName        = "GGX";

    ltc::LUTExporter exporter;
    ltc::ExportResult result = exporter.exportLUT(lut, cfg);

    ltc::DDSMetadata ampMeta = ltc::DDSReader::readMetadata(result.amplitudeFile);
    // Reference profile forces RGBA16F for amplitude
    EXPECT_EQ(ampMeta.format,       ltc::DDSFormat::RGBA16F);
    EXPECT_EQ(ampMeta.channelCount, 4u);
}

TEST_F(ExportIntegrationTest, ReferenceExport_AmplitudeSphericalCap) {
    // Check that channel A contains a spherical cap coefficient
    auto outDir = createTempDir("ref_amp_cap");
    auto lut    = makeIdentityLUT(4, 4);

    ltc::ExportConfig cfg;
    cfg.profile         = ltc::ExportProfile::Reference;
    cfg.matrixFormat    = ltc::TextureFormat::RGBA16F;
    cfg.amplitudeFormat = ltc::TextureFormat::RGBA16F;
    cfg.outputDirectory = outDir;
    cfg.brdfName        = "GGX";

    ltc::LUTExporter exporter;
    ltc::ExportResult result = exporter.exportLUT(lut, cfg);

    // Pixel (0,0): ri=0, vi=0
    // ParameterGrid with 4x4: vi=0 → x=0/(4-1)=0 → cosTheta=1-0=1 → ndotv=1
    // sphericalCap = 2*pi*(1-1) = 0
    ltc::Pixel pix00 = ltc::DDSReader::readPixel(result.amplitudeFile, 0, 0);
    EXPECT_EQ(pix00.channelCount, 4u);
    EXPECT_NEAR(pix00.channels[2], 0.0f, 0.01f) << "B channel should be 0";
    // A channel: spherical cap at vi=0 ≈ 0
    EXPECT_NEAR(pix00.channels[3], 0.0f, 0.1f) << "A at vi=0 should be ~0";

    // Pixel (0, h-1): ri=0, vi=3
    // vi=3: x=3/3=1 → cosTheta=1-1=0 → ndotv=0
    // sphericalCap = 2*pi*(1-0) = 2*pi ≈ 6.283
    ltc::Pixel pix03 = ltc::DDSReader::readPixel(result.amplitudeFile, 0, 3);
    EXPECT_EQ(pix03.channelCount, 4u);
    EXPECT_NEAR(pix03.channels[3], static_cast<float>(2.0 * ltc::kPi), 0.1f)
        << "A at vi=h-1 should be ~2*pi";
}

// ---- Rejection of incomplete LUT ----

TEST_F(ExportIntegrationTest, IncompleteLUT_Throws) {
    auto outDir = createTempDir("incomplete");

    ltc::LUTMetadata meta;
    meta.brdfName = "TestBRDF";
    meta.width    = 2;
    meta.height   = 2;

    ltc::LUTData lut(2, 2, meta);
    // Leave cells at default (completed = false)

    ltc::ExportConfig cfg;
    cfg.profile         = ltc::ExportProfile::Unreal;
    cfg.matrixFormat    = ltc::TextureFormat::RGBA16F;
    cfg.amplitudeFormat = ltc::TextureFormat::RG16F;
    cfg.outputDirectory = outDir;
    cfg.brdfName        = "GGX";

    ltc::LUTExporter exporter;
    EXPECT_THROW(exporter.exportLUT(lut, cfg), std::runtime_error);
}

TEST_F(ExportIntegrationTest, EmptyBrdfName_Throws) {
    auto outDir = createTempDir("empty_brdf");
    auto lut    = makeIdentityLUT(2, 2);

    ltc::ExportConfig cfg;
    cfg.profile         = ltc::ExportProfile::Unreal;
    cfg.matrixFormat    = ltc::TextureFormat::RGBA16F;
    cfg.amplitudeFormat = ltc::TextureFormat::RG16F;
    cfg.outputDirectory = outDir;
    cfg.brdfName        = ""; // empty!

    ltc::LUTExporter exporter;
    EXPECT_THROW(exporter.exportLUT(lut, cfg), std::runtime_error);
}

TEST_F(ExportIntegrationTest, FP32Export_Works) {
    auto outDir = createTempDir("fp32");
    auto lut    = makeIdentityLUT(2, 2);

    ltc::ExportConfig cfg;
    cfg.profile         = ltc::ExportProfile::Unreal;
    cfg.matrixFormat    = ltc::TextureFormat::RGBA32F;
    cfg.amplitudeFormat = ltc::TextureFormat::RG32F;
    cfg.outputDirectory = outDir;
    cfg.brdfName        = "GGX";

    ltc::LUTExporter exporter;
    ltc::ExportResult result = exporter.exportLUT(lut, cfg);

    EXPECT_TRUE(std::filesystem::exists(result.matrixFile));

    ltc::DDSMetadata meta = ltc::DDSReader::readMetadata(result.matrixFile);
    EXPECT_EQ(meta.format, ltc::DDSFormat::RGBA32F);

    // FP32 should have zero quantization error
    EXPECT_NEAR(result.matMaxAbsError, 0.0, 1e-6);
}

TEST_F(ExportIntegrationTest, QuantizationStats_AreReturned) {
    auto outDir = createTempDir("quant_stats");
    auto lut    = makeIdentityLUT(4, 4);

    ltc::ExportConfig cfg;
    cfg.profile         = ltc::ExportProfile::Unreal;
    cfg.matrixFormat    = ltc::TextureFormat::RGBA16F;
    cfg.amplitudeFormat = ltc::TextureFormat::RG16F;
    cfg.outputDirectory = outDir;
    cfg.brdfName        = "GGX";

    ltc::LUTExporter exporter;
    ltc::ExportResult result = exporter.exportLUT(lut, cfg);

    // FP16 encoding of 1.0 is exact, 0.0 is exact
    // So for identity matrix: r=1,g=0,b=0,a=1 → no quantization error
    EXPECT_NEAR(result.matMaxAbsError, 0.0, 1e-4);
    EXPECT_NEAR(result.matMeanAbsError, 0.0, 1e-4);
}

} // namespace
