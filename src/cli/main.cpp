#include "ltc/Benchmark.h"
#include "ltc/BRDFRegistry.h"
#include "ltc/Checkpoint.h"
#include "ltc/DDSReader.h"
#include "ltc/Fitter.h"
#include "ltc/GenerationConfig.h"
#include "ltc/GenerationProgress.h"
#include "ltc/GGX.h"
#include "ltc/LUTExporter.h"
#include "ltc/LUTGenerator.h"
#include "ltc/LUTValidation.h"
#include "ltc/Validation.h"
#include "ltc/Version.h"
#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>
#include <array>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>

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

    // list-brdfs subcommand
    auto* listBrdfs = app.add_subcommand("list-brdfs", "List available BRDF models");
    bool listDetails = false;
    listBrdfs->add_flag("--details", listDetails, "Show BRDF details");

    // fit subcommand
    auto* fit = app.add_subcommand("fit", "Fit an LTC to a BRDF at one (alpha, NdotV) point");
    std::string brdfName = "ggx";
    double fitAlpha = 0.5;
    bool fitAlphaProvided = false;
    double fitRoughness = 0.5;
    bool fitRoughnessProvided = false;
    double fitNdotV = 1.0;
    std::string fitProfile = "reference";
    uint32_t fitSamples = 32;
    uint32_t fitMaxIter = 100;
    bool fitJson = false;
    fit->add_option("--brdf",            brdfName,  "BRDF model (ggx, beckmann, disney-diffuse)")->capture_default_str();
    fit->add_option("--alpha",           fitAlpha,  "Microfacet alpha in (0, 1] (for ggx, beckmann)")->capture_default_str()->each([&](const std::string&){ fitAlphaProvided = true; });
    fit->add_option("--roughness",       fitRoughness, "Perceptual roughness in (0, 1] (for disney-diffuse)")->capture_default_str()->each([&](const std::string&){ fitRoughnessProvided = true; });
    fit->add_option("--ndotv",           fitNdotV,  "cos(theta_view) in (0, 1]")->required();
    fit->add_option("--profile",         fitProfile,"Fitting profile: reference or quality")->capture_default_str();
    fit->add_option("--samples",         fitSamples,"Sample grid resolution N (N×N samples)")->capture_default_str();
    fit->add_option("--max-iterations",  fitMaxIter,"Nelder–Mead maximum iteration count")->capture_default_str();
    fit->add_flag("--json",              fitJson,   "Output as JSON");

    // export subcommand
    auto* exportCmd = app.add_subcommand("export", "Export an LTC LUT to DDS textures");
    std::string exportInput;
    std::string exportProfile = "unreal";
    std::string exportOutput  = ".";
    std::string exportBrdf    = "GGX";
    std::string exportPrecision = "fp16";
    exportCmd->add_option("--input",   exportInput,   "Input .ltcbin checkpoint file")->required();
    exportCmd->add_option("--profile", exportProfile, "Export profile: unreal, reference, or generic")->capture_default_str();
    exportCmd->add_option("--output",  exportOutput,  "Output directory")->capture_default_str();
    exportCmd->add_option("--brdf",    exportBrdf,    "BRDF name used for filenames")->capture_default_str();
    exportCmd->add_option("--precision", exportPrecision, "Precision: fp16 or fp32")->capture_default_str();

    // generate subcommand
    auto* generate = app.add_subcommand("generate", "Generate a full LTC LUT table");
    std::string genBrdf      = "ggx";
    uint32_t    genWidth     = 64;
    uint32_t    genHeight    = 64;
    std::string genProfile   = "reference";
    uint32_t    genSamples   = 32;
    uint32_t    genMaxIter   = 100;
    std::string genThreads   = "1";
    std::string genOutput;
    std::string genCheckpoint;
    uint32_t    genCkptInterval = 64;
    std::string genResume;
    bool        genJson      = false;
    generate->add_option("--brdf",               genBrdf,       "BRDF model (ggx, beckmann, disney-diffuse)")->capture_default_str();
    generate->add_option("--width",              genWidth,      "LUT roughness dimension")->capture_default_str();
    generate->add_option("--height",             genHeight,     "LUT view-angle dimension")->capture_default_str();
    generate->add_option("--profile",            genProfile,    "Fitting profile: reference or quality")->capture_default_str();
    generate->add_option("--samples",            genSamples,    "Sample grid resolution N")->capture_default_str();
    generate->add_option("--max-iterations",     genMaxIter,    "Nelder–Mead max iterations")->capture_default_str();
    generate->add_option("--threads",            genThreads,    "Worker thread count or 'auto'")->capture_default_str();
    generate->add_option("--output",             genOutput,     "Output file (.ltcbin)");
    generate->add_option("--checkpoint",         genCheckpoint, "Checkpoint file path");
    generate->add_option("--checkpoint-interval",genCkptInterval,"Save checkpoint every N cells")->capture_default_str();
    generate->add_option("--resume",             genResume,     "Resume from checkpoint file");
    generate->add_flag("--json",                 genJson,       "Output summary as JSON");

    // validate subcommand
    auto* validate = app.add_subcommand("validate", "Validate an LTC LUT");
    validate->require_subcommand(1);

    // validate reference
    auto* valRef = validate->add_subcommand("reference", "Compare generated DDS with reference DDS");
    std::string valRefInput, valRefReference, valRefReport, valRefProfile = "quality";
    std::string valRefBrdf = "GGX";
    valRef->add_option("--input",     valRefInput,     "Generated DDS directory")->required();
    valRef->add_option("--reference", valRefReference, "Reference DDS directory")->required();
    valRef->add_option("--profile",   valRefProfile,   "Validation profile")->capture_default_str();
    valRef->add_option("--brdf",      valRefBrdf,      "BRDF name")->capture_default_str();
    valRef->add_option("--report",    valRefReport,    "Output JSON report file");

    // validate distribution
    auto* valDist = validate->add_subcommand("distribution", "Validate LTC distribution accuracy");
    std::string valDistInput, valDistReport;
    uint32_t valDistSamples = 65536;
    uint32_t valDistMaxCells = 256;
    valDist->add_option("--input",     valDistInput,    "Input .ltcbin checkpoint file")->required();
    valDist->add_option("--samples",   valDistSamples,  "Sample count")->capture_default_str();
    valDist->add_option("--max-cells", valDistMaxCells, "Max cells to validate")->capture_default_str();
    valDist->add_option("--report",    valDistReport,   "Output JSON report file");

    // validate dds
    auto* valDDS = validate->add_subcommand("dds", "Validate DDS export from checkpoint");
    std::string valDDSInput, valDDSSource, valDDSProfile = "unreal", valDDSReport;
    std::string valDDSBrdf = "GGX";
    valDDS->add_option("--input",   valDDSInput,   "DDS directory")->required();
    valDDS->add_option("--source",  valDDSSource,  "Source .ltcbin file")->required();
    valDDS->add_option("--profile", valDDSProfile, "Validation profile")->capture_default_str();
    valDDS->add_option("--brdf",    valDDSBrdf,    "BRDF name")->capture_default_str();
    valDDS->add_option("--report",  valDDSReport,  "Output JSON report file");

    // validate lighting
    auto* valLighting = validate->add_subcommand("lighting", "Validate lighting accuracy");
    std::string valLightInput, valLightReport, valLightProfile = "quality";
    uint32_t valLightSamples = 65536;
    valLighting->add_option("--input",   valLightInput,   "Input .ltcbin checkpoint file")->required();
    valLighting->add_option("--profile", valLightProfile, "Validation profile")->capture_default_str();
    valLighting->add_option("--samples", valLightSamples, "Sample count")->capture_default_str();
    valLighting->add_option("--report",  valLightReport,  "Output JSON report file");

    // validate all
    auto* valAll = validate->add_subcommand("all", "Run all validation checks");
    std::string valAllInput, valAllSource, valAllProfile = "quality", valAllReportDir = ".";
    std::string valAllBrdf = "GGX";
    std::string valAllReference;
    valAll->add_option("--input",     valAllInput,     "DDS directory")->required();
    valAll->add_option("--source",    valAllSource,    "Source .ltcbin file")->required();
    valAll->add_option("--profile",   valAllProfile,   "Validation profile")->capture_default_str();
    valAll->add_option("--brdf",      valAllBrdf,      "BRDF name")->capture_default_str();
    valAll->add_option("--reference", valAllReference, "Reference DDS directory (optional)");
    valAll->add_option("--report-dir",valAllReportDir, "Output directory for reports")->capture_default_str();

    // benchmark subcommand
    auto* benchmark = app.add_subcommand("benchmark", "Run performance and quality benchmarks");
    benchmark->require_subcommand(1);

    // benchmark fit
    auto* benchFit = benchmark->add_subcommand("fit", "Single-point fitting benchmark");
    std::string benchFitBrdf  = "ggx";
    double      benchFitAlpha = 0.25;
    double      benchFitNdotV = 0.80;
    bool        benchFitCompare = false;
    std::string benchFitOutput;
    benchFit->add_option("--brdf",            benchFitBrdf,    "BRDF model")->capture_default_str();
    benchFit->add_option("--alpha",           benchFitAlpha,   "Microfacet alpha / roughness")->capture_default_str();
    benchFit->add_option("--ndotv",           benchFitNdotV,   "cos(theta_view)")->capture_default_str();
    benchFit->add_flag("--compare-profiles",  benchFitCompare, "Run Reference and Quality profiles");
    benchFit->add_option("--output",          benchFitOutput,  "Output JSON report file");

    // benchmark generate
    auto* benchGen = benchmark->add_subcommand("generate", "LUT generation benchmark");
    std::string benchGenBrdf    = "ggx";
    uint32_t    benchGenWidth   = 64;
    uint32_t    benchGenHeight  = 64;
    uint32_t    benchGenThreads = 1;
    std::string benchGenProfile = "reference";
    std::string benchGenOutput;
    benchGen->add_option("--brdf",    benchGenBrdf,    "BRDF model")->capture_default_str();
    benchGen->add_option("--width",   benchGenWidth,   "LUT roughness dimension")->capture_default_str();
    benchGen->add_option("--height",  benchGenHeight,  "LUT view-angle dimension")->capture_default_str();
    benchGen->add_option("--threads", benchGenThreads, "Worker thread count")->capture_default_str();
    benchGen->add_option("--profile", benchGenProfile, "Fitting profile: reference or quality")->capture_default_str();
    benchGen->add_option("--output",  benchGenOutput,  "Output JSON report file");

    // benchmark suite
    auto* benchSuite = benchmark->add_subcommand("suite", "Multi-BRDF benchmark suite");
    std::string benchSuiteBrdfs  = "ggx,beckmann,disney-diffuse";
    uint32_t    benchSuiteWidth  = 32;
    uint32_t    benchSuiteHeight = 32;
    std::string benchSuiteOutput;
    benchSuite->add_option("--brdfs",  benchSuiteBrdfs,  "Comma-separated BRDF list")->capture_default_str();
    benchSuite->add_option("--width",  benchSuiteWidth,  "LUT roughness dimension")->capture_default_str();
    benchSuite->add_option("--height", benchSuiteHeight, "LUT view-angle dimension")->capture_default_str();
    benchSuite->add_option("--output", benchSuiteOutput, "Output directory for reports");

    // benchmark scaling
    auto* benchScaling = benchmark->add_subcommand("scaling", "Worker scaling benchmark");
    std::string benchScalBrdf    = "ggx";
    std::string benchScalThreads = "1,2,4,8";
    uint32_t    benchScalWidth   = 32;
    uint32_t    benchScalHeight  = 32;
    std::string benchScalOutput;
    benchScaling->add_option("--brdf",    benchScalBrdf,    "BRDF model")->capture_default_str();
    benchScaling->add_option("--threads", benchScalThreads, "Comma-separated thread counts")->capture_default_str();
    benchScaling->add_option("--width",   benchScalWidth,   "LUT roughness dimension")->capture_default_str();
    benchScaling->add_option("--height",  benchScalHeight,  "LUT view-angle dimension")->capture_default_str();
    benchScaling->add_option("--output",  benchScalOutput,  "Output JSON report file");

    CLI11_PARSE(app, argc, argv);

    if (listBrdfs->parsed()) {
        auto registry = ltc::BRDFRegistry::createDefault();
        if (!listDetails) {
            std::cout << "Available BRDFs:\n\n";
            for (const auto& n : registry.names())
                std::cout << n << "\n";
        } else {
            for (const auto& n : registry.names()) {
                auto brdf = registry.create(n);
                auto caps = brdf->capabilities();
                std::cout << n << ":\n";
                std::cout << "  parameter:    " << caps.parameterName << "\n";
                std::cout << "  isotropic:    " << (caps.isotropic ? "yes" : "no") << "\n";
                std::cout << "  amplitude:    " << (caps.amplitudeModel == ltc::AmplitudeModel::SchlickFresnel ? "SchlickFresnel" : "Scalar") << "\n";
                std::cout << "  fresnel:      " << (caps.supportsSchlickFresnel ? "yes" : "no") << "\n";
                std::cout << "  sampling:     " << (caps.supportsImportanceSampling ? "yes" : "no") << "\n";
                std::cout << "\n";
            }
        }
        return 0;
    }

    if (exportCmd->parsed()) {
        try {
            ltc::LUTData lut = ltc::Checkpoint::load(exportInput);

            ltc::ExportConfig cfg;
            cfg.brdfName        = exportBrdf;
            cfg.outputDirectory = exportOutput;

            if (exportProfile == "reference") {
                cfg.profile         = ltc::ExportProfile::Reference;
                cfg.amplitudeFormat = ltc::TextureFormat::RGBA16F;
            } else if (exportProfile == "generic") {
                cfg.profile         = ltc::ExportProfile::Generic;
                cfg.amplitudeFormat = ltc::TextureFormat::RG16F;
            } else {
                cfg.profile         = ltc::ExportProfile::Unreal;
                cfg.amplitudeFormat = ltc::TextureFormat::RG16F;
            }

            if (exportPrecision == "fp32") {
                cfg.matrixFormat    = ltc::TextureFormat::RGBA32F;
                if (exportProfile != "reference") {
                    cfg.amplitudeFormat = ltc::TextureFormat::RG32F;
                }
            } else {
                cfg.matrixFormat    = ltc::TextureFormat::RGBA16F;
            }

            ltc::LUTExporter exporter;
            ltc::ExportResult result = exporter.exportLUT(lut, cfg);

            std::cout << "Matrix:    " << result.matrixFile.string() << "\n";
            std::cout << "Amplitude: " << result.amplitudeFile.string() << "\n";
            std::cout << "Cells:     " << result.validCells << "/" << result.cellCount << "\n";
            std::cout << "MaxIter:   " << result.maxIterCells << "\n";
            std::cout << "Mat error (max/mean):  "
                      << result.matMaxAbsError << " / " << result.matMeanAbsError << "\n";
            std::cout << "Amp error (max/mean):  "
                      << result.ampMaxAbsError << " / " << result.ampMeanAbsError << "\n";
        } catch (const std::exception& e) {
            std::cerr << "error: " << e.what() << "\n";
            return 1;
        }
        return 0;
    }

    if (fit->parsed()) {
        ltc::BRDFRegistry fitRegistry = ltc::BRDFRegistry::createDefault();
        if (!fitRegistry.contains(brdfName)) {
            std::cerr << "error: unknown BRDF '" << brdfName << "' (use list-brdfs to see available)\n";
            return 1;
        }
        auto brdfObj = fitRegistry.create(brdfName);
        auto caps = brdfObj->capabilities();

        if (fitNdotV <= 0.0 || fitNdotV > 1.0) {
            std::cerr << "error: --ndotv must be in (0, 1]\n";
            return 1;
        }

        double fitParam = 0.0;
        if (caps.parameterization == ltc::BRDFParameterization::PerceptualRoughness) {
            if (!fitRoughnessProvided) {
                std::cerr << "error: --roughness required for BRDF '" << brdfName << "'\n";
                return 1;
            }
            if (fitRoughness <= 0.0 || fitRoughness > 1.0) {
                std::cerr << "error: --roughness must be in (0, 1]\n";
                return 1;
            }
            fitParam = fitRoughness;
        } else {
            if (!fitAlphaProvided) {
                std::cerr << "error: --alpha required for BRDF '" << brdfName << "'\n";
                return 1;
            }
            if (fitAlpha <= 0.0 || fitAlpha > 1.0) {
                std::cerr << "error: --alpha must be in (0, 1]\n";
                return 1;
            }
            fitParam = fitAlpha;
        }

        ltc::FittingConfig cfg;
        cfg.sampleResolution = fitSamples;
        cfg.maxIterations    = fitMaxIter;
        if (fitProfile == "quality") {
            cfg.profile = ltc::FittingProfile::Quality;
        } else {
            cfg.profile = ltc::FittingProfile::Reference;
        }

        double sinv = std::sqrt(std::max(0.0, 1.0 - fitNdotV * fitNdotV));
        ltc::Vec3 view = glm::normalize(ltc::Vec3(sinv, 0.0, fitNdotV));
        // For the CLI fit command the user provides the final alpha/roughness
        // value directly (not a perceptual roughness to be squared).
        ltc::BRDFContext ctx{view, ltc::Scalar(fitParam)};

        ltc::LTCFitter fitter;
        ltc::FittingResult r = fitter.fit(*brdfObj, ctx, cfg);

        auto statusStr = [&]() -> std::string {
            switch (r.status) {
                case ltc::FitStatus::Converged:        return "converged";
                case ltc::FitStatus::MaxIterations:    return "max_iterations";
                case ltc::FitStatus::ZeroEnergy:       return "zero_energy";
                case ltc::FitStatus::InvalidInput:     return "invalid_input";
                case ltc::FitStatus::NumericalFailure: return "numerical_failure";
            }
            return "unknown";
        };

        if (fitJson) {
            nlohmann::json j;
            j["status"]     = statusStr();
            j["brdf"]       = brdfName;
            j["param"]      = fitParam;
            j["ndotv"]      = fitNdotV;
            j["magnitude"]  = r.magnitude;
            j["m11"]        = r.parameters.m11;
            j["m22"]        = r.parameters.m22;
            j["m13"]        = r.parameters.m13;
            j["initialError"] = r.initialError;
            j["finalError"]   = r.finalError;
            j["iterations"]   = r.iterations;
            j["evaluations"]  = r.evaluations;
            if (r.hasFresnelCoefficient)
                j["fresnelCoefficient"] = r.fresnelCoefficient;
            std::cout << j.dump(2) << "\n";
        } else {
            std::cout << "BRDF:         " << brdfName << "\n";
            std::cout << "Param:        " << fitParam << "\n";
            std::cout << "NdotV:        " << fitNdotV << "\n";
            std::cout << "Status:       " << statusStr() << "\n";
            std::cout << "Magnitude:    " << r.magnitude << "\n";
            std::cout << "m11:          " << r.parameters.m11 << "\n";
            std::cout << "m22:          " << r.parameters.m22 << "\n";
            std::cout << "m13:          " << r.parameters.m13 << "\n";
            std::cout << "Initial err:  " << r.initialError << "\n";
            std::cout << "Final err:    " << r.finalError << "\n";
            std::cout << "Iterations:   " << r.iterations << "\n";
            std::cout << "Evaluations:  " << r.evaluations << "\n";
            if (r.hasFresnelCoefficient)
                std::cout << "Fresnel coef: " << r.fresnelCoefficient << "\n";
        }
        return 0;
    }

    if (generate->parsed()) {
        ltc::BRDFRegistry genRegistry = ltc::BRDFRegistry::createDefault();
        if (!genRegistry.contains(genBrdf)) {
            std::cerr << "error: unknown BRDF '" << genBrdf << "' (use list-brdfs to see available)\n";
            return 1;
        }

        uint32_t threadCount = 1;
        if (genThreads == "auto") {
            threadCount = std::max(1u, std::thread::hardware_concurrency());
        } else {
            try {
                long v = std::stol(genThreads);
                if (v < 1) {
                    std::cerr << "error: --threads must be >= 1 or 'auto'\n";
                    return 1;
                }
                threadCount = static_cast<uint32_t>(v);
            } catch (...) {
                std::cerr << "error: invalid --threads value '" << genThreads << "'\n";
                return 1;
            }
        }

        ltc::GenerationConfig cfg;
        cfg.width   = genWidth;
        cfg.height  = genHeight;
        cfg.threads = threadCount;
        cfg.fitting.sampleResolution = genSamples;
        cfg.fitting.maxIterations    = genMaxIter;
        cfg.fitting.profile = (genProfile == "quality")
            ? ltc::FittingProfile::Quality
            : ltc::FittingProfile::Reference;

        if (!genCheckpoint.empty()) {
            cfg.enableCheckpoint      = true;
            cfg.checkpointPath        = genCheckpoint;
            cfg.checkpointInterval    = genCkptInterval;
        }
        if (!genResume.empty()) {
            cfg.resumeFromCheckpoint  = true;
            cfg.checkpointPath        = genResume;
        }

        ltc::GenerationProgress progress;
        uint32_t totalCells = genWidth * genHeight;

        auto t0 = std::chrono::steady_clock::now();

        if (!genJson) {
            std::cout << "Generating " << genWidth << "x" << genHeight
                      << " LUT (" << totalCells << " cells)"
                      << "  threads=" << threadCount << "\n";
            std::cout << std::flush;
        }

        progress.onCellComplete = [&]() {
            if (genJson) return;
            uint32_t done = progress.completedCells.load(std::memory_order_relaxed);
            if (done % 64 == 0 || done == totalCells) {
                auto now = std::chrono::steady_clock::now();
                double elapsed = std::chrono::duration<double>(now - t0).count();
                double pct = 100.0 * done / totalCells;
                std::cout << "\r  " << done << "/" << totalCells
                          << "  (" << std::fixed << std::setprecision(1) << pct << "%)"
                          << "  " << std::setprecision(1) << elapsed << "s"
                          << "          " << std::flush;
            }
        };

        auto brdfGen = genRegistry.create(genBrdf);
        ltc::LUTGenerator generator;

        ltc::LUTData lut = generator.generate(*brdfGen, cfg, &progress);

        auto t1 = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(t1 - t0).count();

        if (!genJson) std::cout << "\n";

        if (!genOutput.empty()) {
            try {
                ltc::Checkpoint::save(lut, genOutput);
                if (!genJson)
                    std::cout << "Saved: " << genOutput << "\n";
            } catch (const ltc::CheckpointError& e) {
                std::cerr << "error: failed to save output: " << e.what() << "\n";
                return 1;
            }
        }

        ltc::LUTValidator validator;
        ltc::ValidationReport report = validator.validate(lut);

        if (genJson) {
            nlohmann::json j;
            j["brdf"]             = genBrdf;
            j["width"]            = genWidth;
            j["height"]           = genHeight;
            j["totalCells"]       = report.totalCells;
            j["completedCells"]   = report.completedCells;
            j["failedCells"]      = report.failedCells;
            j["convergedCells"]   = report.convergedCells;
            j["maxIterCells"]     = report.maxIterCells;
            j["isComplete"]       = report.isComplete;
            j["allFinite"]        = report.allFinite;
            j["allPositiveMagnitude"] = report.allPositiveMagnitude;
            j["minFinalError"]    = report.minFinalError;
            j["maxFinalError"]    = report.maxFinalError;
            j["meanFinalError"]   = report.meanFinalError;
            j["rmsFinalError"]    = report.rmsFinalError;
            j["elapsedSeconds"]   = elapsed;
            j["threads"]          = threadCount;
            if (!genOutput.empty()) j["output"] = genOutput;
            std::cout << j.dump(2) << "\n";
        } else {
            std::cout << "Completed:    " << report.completedCells << "/" << report.totalCells << "\n";
            std::cout << "Converged:    " << report.convergedCells << "\n";
            std::cout << "MaxIter:      " << report.maxIterCells << "\n";
            std::cout << "Failed:       " << report.failedCells << "\n";
            std::cout << "All finite:   " << (report.allFinite ? "yes" : "NO") << "\n";
            if (report.finiteErrorCount > 0) {
                std::cout << "Error min:    " << report.minFinalError << "\n";
                std::cout << "Error max:    " << report.maxFinalError << "\n";
                std::cout << "Error mean:   " << report.meanFinalError << "\n";
                std::cout << "Error RMS:    " << report.rmsFinalError << "\n";
            }
            std::cout << "Time:         " << std::fixed << std::setprecision(2) << elapsed << "s\n";
        }
        return 0;
    }

    if (validate->parsed()) {
        auto makeValidationConfig = [](const std::string& profile, uint32_t distSamples, uint32_t lightSamples) {
            ltc::ValidationConfig cfg;
            cfg.distributionSamples = distSamples;
            cfg.lightingSamples = lightSamples;
            if (profile == "reference")  cfg.profile = ltc::ValidationProfile::Reference;
            else if (profile == "unreal") cfg.profile = ltc::ValidationProfile::Unreal;
            else                          cfg.profile = ltc::ValidationProfile::Quality;
            return cfg;
        };

        ltc::ValidationRunner runner;

        if (valRef->parsed()) {
            try {
                ltc::ValidationConfig cfg = makeValidationConfig(valRefProfile, 65536, 65536);
                ltc::TextureComparisonResult result = runner.runReferenceComparison(
                    valRefInput, valRefReference, valRefBrdf);
                ltc::FullValidationReport report;
                report.toolVersion = std::string(ltc::Version::string());
                report.validationProfile = valRefProfile;
                report.referenceAvailable = true;
                report.matrixComparison = result;
                if (!valRefReport.empty()) {
                    ltc::writeJsonReport(report, valRefReport);
                    std::cout << "Report written: " << valRefReport << "\n";
                }
                if (!result.compatible) {
                    std::cerr << "incompatible: " << result.incompatibilityReason << "\n";
                    return 1;
                }
                std::cout << "Reference comparison: compatible\n";
                return 0;
            } catch (const std::exception& e) {
                std::cerr << "error: " << e.what() << "\n";
                return 1;
            }
        }

        if (valDist->parsed()) {
            try {
                ltc::LUTData lut = ltc::Checkpoint::load(valDistInput);
                ltc::ValidationConfig cfg = makeValidationConfig("quality", valDistSamples, 65536);
                auto metrics = runner.runDistributionValidation(lut, cfg, valDistMaxCells);
                std::cout << "Distribution validation: " << metrics.size() << " cells\n";
                if (!valDistReport.empty()) {
                    ltc::FullValidationReport report;
                    report.toolVersion = std::string(ltc::Version::string());
                    report.distributionResults = metrics;
                    ltc::writeJsonReport(report, valDistReport);
                    std::cout << "Report written: " << valDistReport << "\n";
                }
                return 0;
            } catch (const std::exception& e) {
                std::cerr << "error: " << e.what() << "\n";
                return 1;
            }
        }

        if (valDDS->parsed()) {
            try {
                ltc::LUTData lut = ltc::Checkpoint::load(valDDSSource);
                ltc::ValidationConfig cfg = makeValidationConfig(valDDSProfile, 65536, 65536);
                auto metrics = runner.runDistributionValidation(lut, cfg);
                ltc::FullValidationReport report;
                report.toolVersion = std::string(ltc::Version::string());
                report.validationProfile = valDDSProfile;
                report.distributionResults = metrics;
                if (!valDDSReport.empty()) {
                    ltc::writeJsonReport(report, valDDSReport);
                    std::cout << "Report written: " << valDDSReport << "\n";
                }
                std::cout << "DDS validation complete\n";
                return 0;
            } catch (const std::exception& e) {
                std::cerr << "error: " << e.what() << "\n";
                return 1;
            }
        }

        if (valLighting->parsed()) {
            try {
                ltc::LUTData lut = ltc::Checkpoint::load(valLightInput);
                ltc::ValidationConfig cfg = makeValidationConfig(valLightProfile, 65536, valLightSamples);
                auto lightResults = runner.runLightingValidation(lut, cfg);
                std::cout << "Lighting scenarios: " << lightResults.size() << "\n";
                int passed = 0, failed = 0;
                for (const auto& r : lightResults) {
                    bool ok = (r.relError <= cfg.thresholds.lightingRelative ||
                               r.absError <= cfg.thresholds.lightingAbsolute);
                    std::cout << "  " << r.id << ": gt=" << r.groundTruth
                              << " ltc=" << r.ltcResult
                              << " rel=" << r.relError
                              << (ok ? " PASS" : " FAIL") << "\n";
                    if (ok) ++passed; else ++failed;
                }
                if (!valLightReport.empty()) {
                    ltc::FullValidationReport report;
                    report.toolVersion = std::string(ltc::Version::string());
                    report.validationProfile = valLightProfile;
                    report.lightingResults = lightResults;
                    report.summary.passed = passed;
                    report.summary.failed = failed;
                    ltc::writeJsonReport(report, valLightReport);
                    std::cout << "Report written: " << valLightReport << "\n";
                }
                return (failed > 0) ? 1 : 0;
            } catch (const std::exception& e) {
                std::cerr << "error: " << e.what() << "\n";
                return 1;
            }
        }

        if (valAll->parsed()) {
            try {
                ltc::LUTData lut = ltc::Checkpoint::load(valAllSource);
                ltc::ValidationConfig cfg = makeValidationConfig(valAllProfile, 65536, 65536);
                std::optional<std::filesystem::path> refDir;
                if (!valAllReference.empty()) refDir = valAllReference;
                ltc::FullValidationReport report = runner.runAll(
                    lut, valAllInput, cfg, valAllBrdf, refDir);
                std::filesystem::path reportPath = std::filesystem::path(valAllReportDir) / "validation_report.json";
                ltc::writeJsonReport(report, reportPath);
                std::cout << "Report written: " << reportPath.string() << "\n";
                std::cout << "Summary: passed=" << report.summary.passed
                          << " failed=" << report.summary.failed
                          << " skipped=" << report.summary.skipped << "\n";
                return (report.summary.failed > 0) ? 1 : 0;
            } catch (const std::exception& e) {
                std::cerr << "error: " << e.what() << "\n";
                return 1;
            }
        }

        return 0;
    }

    if (benchmark->parsed()) {
        auto makeReport = [](const std::string& toolVer) {
            ltc::BenchmarkReport rep;
            rep.toolVersion = toolVer;
            rep.buildConfig = "Release";
            return rep;
        };

        if (benchFit->parsed()) {
            auto fitResults = ltc::runFitBenchmark(
                benchFitBrdf, benchFitAlpha, benchFitNdotV, benchFitCompare);
            for (const auto& r : fitResults) {
                std::cout << "Profile: " << (r.profile == ltc::FittingProfile::Quality
                                               ? "quality" : "reference") << "\n";
                if (!r.success) {
                    std::cout << "  error: " << r.errorMessage << "\n";
                    continue;
                }
                std::cout << "  converged:    " << (r.metrics.converged ? "yes" : "no") << "\n";
                std::cout << "  iterations:   " << r.metrics.iterations << "\n";
                std::cout << "  evaluations:  " << r.metrics.objectiveEvaluations << "\n";
                std::cout << "  finalObj:     " << r.metrics.finalObjective << "\n";
                std::cout << "  totalMs:      " << r.metrics.totalDurationMs << "\n";
            }
            if (!benchFitOutput.empty()) {
                ltc::BenchmarkReport rep = makeReport(std::string(ltc::Version::string()));
                rep.fitResults = fitResults;
                ltc::writeBenchmarkReport(rep, benchFitOutput);
                std::cout << "Report written: " << benchFitOutput << "\n";
            }
            return 0;
        }

        if (benchGen->parsed()) {
            ltc::FittingProfile prof = (benchGenProfile == "quality")
                ? ltc::FittingProfile::Quality : ltc::FittingProfile::Reference;
            auto r = ltc::runGenerationBenchmark(
                benchGenBrdf, benchGenWidth, benchGenHeight, benchGenThreads, prof);
            std::cout << "BRDF:         " << r.brdfName << "\n";
            std::cout << "Profile:      " << benchGenProfile << "\n";
            std::cout << "Resolution:   " << r.width << "x" << r.height << "\n";
            std::cout << "Threads:      " << r.threads << "\n";
            std::cout << "TotalMs:      " << r.totalDurationMs << "\n";
            std::cout << "MeanCellMs:   " << r.meanCellDurationMs << "\n";
            std::cout << "Converged:    " << r.convergedCells << "\n";
            std::cout << "MaxIter:      " << r.maxIterCells << "\n";
            std::cout << "MeanError:    " << r.meanFinalError << "\n";
            if (!benchGenOutput.empty()) {
                ltc::BenchmarkReport rep = makeReport(std::string(ltc::Version::string()));
                rep.generationResults.push_back(r);
                ltc::writeBenchmarkReport(rep, benchGenOutput);
                std::cout << "Report written: " << benchGenOutput << "\n";
            }
            return 0;
        }

        if (benchSuite->parsed()) {
            // Parse comma-separated BRDF list.
            std::vector<std::string> brdfList;
            std::string token;
            for (char c : benchSuiteBrdfs) {
                if (c == ',') { if (!token.empty()) brdfList.push_back(token); token.clear(); }
                else          { token += c; }
            }
            if (!token.empty()) brdfList.push_back(token);

            auto genResults = ltc::runSuiteBenchmark(brdfList, benchSuiteWidth, benchSuiteHeight);
            for (const auto& r : genResults) {
                std::cout << r.brdfName << " "
                          << (r.profile == ltc::FittingProfile::Quality ? "quality" : "reference")
                          << ": " << r.totalDurationMs << "ms"
                          << " err=" << r.meanFinalError << "\n";
            }
            if (!benchSuiteOutput.empty()) {
                ltc::BenchmarkReport rep = makeReport(std::string(ltc::Version::string()));
                rep.generationResults = genResults;
                std::filesystem::path outPath = std::filesystem::path(benchSuiteOutput) / "suite_report.json";
                std::filesystem::create_directories(benchSuiteOutput);
                ltc::writeBenchmarkReport(rep, outPath);
                std::cout << "Report written: " << outPath.string() << "\n";
            }
            return 0;
        }

        if (benchScaling->parsed()) {
            // Parse comma-separated thread counts.
            std::vector<uint32_t> threadList;
            std::string token2;
            for (char c : benchScalThreads) {
                if (c == ',') {
                    if (!token2.empty()) {
                        try { threadList.push_back(static_cast<uint32_t>(std::stoul(token2))); }
                        catch (...) {}
                        token2.clear();
                    }
                } else {
                    token2 += c;
                }
            }
            if (!token2.empty()) {
                try { threadList.push_back(static_cast<uint32_t>(std::stoul(token2))); }
                catch (...) {}
            }
            if (threadList.empty()) threadList = {1};

            auto sr = ltc::runScalingBenchmark(benchScalBrdf, benchScalWidth,
                                                benchScalHeight, threadList);
            for (size_t i = 0; i < sr.threadCounts.size(); ++i) {
                std::cout << "threads=" << sr.threadCounts[i]
                          << "  ms=" << (i < sr.durationMs.size() ? sr.durationMs[i] : 0.0)
                          << "  speedup=" << (i < sr.speedupRelativeToSingle.size()
                                                ? sr.speedupRelativeToSingle[i] : 0.0) << "\n";
            }
            if (!benchScalOutput.empty()) {
                ltc::BenchmarkReport rep = makeReport(std::string(ltc::Version::string()));
                rep.scalingResult = sr;
                ltc::writeBenchmarkReport(rep, benchScalOutput);
                std::cout << "Report written: " << benchScalOutput << "\n";
            }
            return 0;
        }

        return 0;
    }

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
