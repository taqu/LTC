#include <cassert>
#include "fit/ltc.h"
#include "fit/brdf.h"
#include "fit/nelder_mead.h"
#include "fit/dds.h"
#include "fit/float_to_half.h"
#include <CLI/CLI.hpp>

namespace
{
// minimal roughness (avoid singularities)
const float MIN_ALPHA = 0.00001f;

const float pi = 3.1415927f;

// computes
// * the norm (albedo) of the BRDF
// * the average Schlick Fresnel value
// * the average direction of the BRDF
void computeAvgTerms(const Brdf& brdf, const vec3& V, const float alpha,
    float& norm, float& fresnel, vec3& averageDir, int32_t numSamples)
{
    norm = 0.0f;
    fresnel = 0.0f;
    averageDir = vec3(0, 0, 0);

    for (int32_t j = 0; j < numSamples; ++j){
    for (int32_t i = 0; i < numSamples; ++i)
    {
        const float U1 = (i + 0.5f)/numSamples;
        const float U2 = (j + 0.5f)/numSamples;

        // sample
        const vec3 L = brdf.sample(V, alpha, U1, U2);

        // eval
        float pdf;
        float eval = brdf.eval(V, L, alpha, pdf);

        if (pdf > 0)
        {
            float weight = eval / pdf;

            vec3 H = normalize(V+L);

            // accumulate
            norm       += weight;
            fresnel    += weight * pow(1.0f - glm::max(dot(V, H), 0.0f), 5.0f);
            averageDir += weight * L;
        }
    }
    }

    norm    /= (float)(numSamples*numSamples);
    fresnel /= (float)(numSamples*numSamples);

    // clear y component, which should be zero with isotropic BRDFs
    averageDir.y = 0.0f;

    averageDir = normalize(averageDir);
}

// compute the error between the BRDF and the LTC
// using Multiple Importance Sampling
float computeError(const LTC& ltc, const Brdf& brdf, const vec3& V, const float alpha, int32_t numSamples)
{
    double error = 0.0;

    for (int32_t j = 0; j < numSamples; ++j){
    for (int32_t i = 0; i < numSamples; ++i)
    {
        const float U1 = (i + 0.5f)/numSamples;
        const float U2 = (j + 0.5f)/numSamples;

        // importance sample LTC
        {
            // sample
            const vec3 L = ltc.sample(U1, U2);

            float pdf_brdf;
            float eval_brdf = brdf.eval(V, L, alpha, pdf_brdf);
            float eval_ltc = ltc.eval(L);
            float pdf_ltc = eval_ltc/ltc.magnitude;

            // error with MIS weight
            double error_ = fabsf(eval_brdf - eval_ltc);
            error_ = error_*error_*error_;
            error += error_/(pdf_ltc + pdf_brdf);
        }

        // importance sample BRDF
        {
            // sample
            const vec3 L = brdf.sample(V, alpha, U1, U2);

            float pdf_brdf;
            float eval_brdf = brdf.eval(V, L, alpha, pdf_brdf);
            float eval_ltc = ltc.eval(L);
            float pdf_ltc = eval_ltc/ltc.magnitude;

            // error with MIS weight
            double error_ = fabsf(eval_brdf - eval_ltc);
            error_ = error_*error_*error_;
            error += error_/(pdf_ltc + pdf_brdf);
        }
    }
    }

    return (float)error / (float)(numSamples*numSamples);
}

struct FitLTC
{
    FitLTC(LTC& ltc_, const Brdf& brdf, bool isotropic_, const vec3& V_, float alpha_, int32_t numSamples_) :
        ltc(ltc_), brdf(brdf), isotropic(isotropic_), V(V_), alpha(alpha_), numSamples(numSamples_)
    {
    }

    void update(const float* params)
    {
        float m11 = std::max<float>(params[0], 1e-7f);
        float m22 = std::max<float>(params[1], 1e-7f);
        float m13 = params[2];

        if (isotropic)
        {
            ltc.m11 = m11;
            ltc.m22 = m11;
            ltc.m13 = 0.0f;
        }
        else
        {
            ltc.m11 = m11;
            ltc.m22 = m22;
            ltc.m13 = m13;
        }
        ltc.update();
    }

    float operator()(const float* params)
    {
        update(params);
        return computeError(ltc, brdf, V, alpha, numSamples);
    }

    const Brdf& brdf;
    LTC& ltc;
    bool isotropic;

    const vec3& V;
    float alpha;
    int32_t numSamples;
};

// fit brute force
// refine first guess by exploring parameter space
void fit(LTC& ltc, const Brdf& brdf, const vec3& V, const float alpha, const int32_t numSamples, const int32_t maxIterations, const float epsilon = 0.05f, const bool isotropic = false)
{
    float startFit[3] = { ltc.m11, ltc.m22, ltc.m13 };
    float resultFit[3];

    FitLTC fitter(ltc, brdf, isotropic, V, alpha, numSamples);

    // Find best-fit LTC lobe (scale, alphax, alphay)
    static_cast<void>(NelderMead<3>(resultFit, startFit, epsilon, 1e-5f, maxIterations, fitter));

    // Update LTC with best fitting values
    fitter.update(resultFit);
}

// fit data
void fitTab(mat3* tab, vec2* tabMagFresnel, const int32_t N, const Brdf& brdf, int32_t numSamples, int32_t maxIterations)
{
    LTC ltc;

    // loop over theta and alpha
    for (int32_t a = N - 1; a >=     0; --a){
    for (int32_t t =     0; t <= N - 1; ++t)
    {
        // parameterised by sqrt(1 - cos(theta))
        float x = t/float(N - 1);
        float ct = 1.0f - x*x;
        float theta = std::min<float>(1.57f, acosf(ct));
        const vec3 V = vec3(sinf(theta), 0, cosf(theta));

        // alpha = roughness^2
        float roughness = a/float(N - 1);
        float alpha = std::max<float>(roughness*roughness, MIN_ALPHA);

        cout << "a = " << a << "\t t = " << t  << endl;
        cout << "alpha = " << alpha << "\t theta = " << theta << endl;
        cout << endl;

        vec3 averageDir;
        computeAvgTerms(brdf, V, alpha, ltc.magnitude, ltc.fresnel, averageDir, numSamples);

        bool isotropic;

        // 1. first guess for the fit
        // init the hemisphere in which the distribution is fitted
        // if theta == 0 the lobe is rotationally symmetric and aligned with Z = (0 0 1)
        if (t == 0)
        {
            ltc.X = vec3(1, 0, 0);
            ltc.Y = vec3(0, 1, 0);
            ltc.Z = vec3(0, 0, 1);

            if (a == N - 1) // roughness = 1
            {
                ltc.m11 = 1.0f;
                ltc.m22 = 1.0f;
            }
            else // init with roughness of previous fit
            {
                ltc.m11 = tab[a + 1 + t*N][0][0];
                ltc.m22 = tab[a + 1 + t*N][1][1];
            }

            ltc.m13 = 0;
            ltc.update();

            isotropic = true;
        }
        // otherwise use previous configuration as first guess
        else
        {
            vec3 L = averageDir;
            vec3 T1(L.z, 0, -L.x);
            vec3 T2(0, 1, 0);
            ltc.X = T1;
            ltc.Y = T2;
            ltc.Z = L;

            ltc.update();

            isotropic = false;
        }

        // 2. fit (explore parameter space and refine first guess)
        float epsilon = 0.05f;
        fit(ltc, brdf, V, alpha, numSamples, maxIterations, epsilon, isotropic);

        // copy data
        tab[a + t*N] = ltc.M;
        tabMagFresnel[a + t*N][0] = ltc.magnitude;
        tabMagFresnel[a + t*N][1] = ltc.fresnel;

        // kill useless coefs in matrix
        tab[a+t*N][0][1] = 0;
        tab[a+t*N][1][0] = 0;
        tab[a+t*N][2][1] = 0;
        tab[a+t*N][1][2] = 0;

        cout << tab[a+t*N][0][0] << "\t " << tab[a+t*N][1][0] << "\t " << tab[a+t*N][2][0] << endl;
        cout << tab[a+t*N][0][1] << "\t " << tab[a+t*N][1][1] << "\t " << tab[a+t*N][2][1] << endl;
        cout << tab[a+t*N][0][2] << "\t " << tab[a+t*N][1][2] << "\t " << tab[a+t*N][2][2] << endl;
        cout << endl;
    }}
}

float sqr(float x)
{
    return x*x;
}

float G(float w, float s, float g)
{
    return -2.0f*sinf(w)*cosf(s)*cosf(g) + pi/2.0f - g + sinf(g)*cosf(g);
}

float H(float w, float s, float g)
{
    float sinsSq = sqr(sin(s));
    float cosgSq = sqr(cos(g));

    return cosf(w)*(cosf(g)*sqrtf(sinsSq - cosgSq) + sinsSq*asinf(cosf(g)/sinf(s)));
}

float ihemi(float w, float s)
{
    float g = asinf(cosf(s)/sinf(w));
    float sinsSq = sqr(sinf(s));

    if (w >= 0.0f && w <= (pi/2.0f - s))
        return pi*cosf(w)*sinsSq;

    if (w >= (pi/2.0f - s) && w < pi/2.0f)
        return pi*cosf(w)*sinsSq + G(w, s, g) - H(w, s, g);

    if (w >= pi/2.0f && w < (pi/2.0f + s))
        return G(w, s, g) + H(w, s, g);

    return 0.0f;
}

void genSphereTab(float* tabSphere, int32_t N)
{
    for (int32_t j = 0; j < N; ++j)
    for (int32_t i = 0; i < N; ++i)
    {
        const float U1 = float(i)/(N - 1);
        const float U2 = float(j)/(N - 1);

        // z = cos(elevation angle)
        float z = 2.0f*U1 - 1.0f;

        // length of average dir., proportional to sin(sigma)^2
        float len = U2;

        float sigma = asinf(sqrtf(len));
        float omega = acosf(z);

        // compute projected (cosine-weighted) solid angle of spherical cap
        float value = 0.0f;

        if (sigma > 0.0f)
            value = ihemi(omega, sigma)/(pi*len);
        else
            value = std::max<float>(z, 0.0f);

        if (value != value)
            printf("nan!\n");

        tabSphere[i + j*N] = value;
    }
}

void packTab(
    vec4* tex1, vec4* tex2,
    const mat3*  tab,
    const vec2*  tabMagFresnel,
    const float* tabSphere,
    int32_t N)
{
    for (int32_t i = 0; i < N*N; ++i)
    {
        const mat3& m = tab[i];

        mat3 invM = inverse(m);

        // normalize by the middle element
        invM /= invM[1][1];

        // store the variable terms
        tex1[i].x = invM[0][0];
        tex1[i].y = invM[0][2];
        tex1[i].z = invM[2][0];
        tex1[i].w = invM[2][2];

        tex2[i].x = tabMagFresnel[i][0];
        tex2[i].y = tabMagFresnel[i][1];
        tex2[i].z = 0.0f; // unused
        tex2[i].w = tabSphere[i];
    }
}

void write_dds(const char* path, const float* data, int N)
{
    int numTerms = N*N*4;

    uint16_t* half = new uint16_t[numTerms];

    for (int i = 0; i < numTerms; ++i){
        half[i] = float_to_half(data[i]);
    }

    save_dds(path, DDS_FORMAT_R16G16B16A16_FLOAT, sizeof(uint16_t)*4, N, N, half);

    delete[] half;
}
}

int32_t main(int32_t argc, char* argv[])
{
    CLI::App app{"LTC LUT Generator", "ltcgen"};
    app.set_version_flag("--version", "1.0.0");

        // fit subcommand
    auto* fit = app.add_subcommand("fit", "Fit an LTC to a BRDF at one (alpha, NdotV) point");
    std::string brdfName = "ggx";
    uint32_t fitSize = 64;
    uint32_t fitSamples = 32;
    uint32_t fitMaxIter = 100;
    fit->add_option("--brdf",            brdfName,  "BRDF model (ggx, beckmann, disney-diffuse)")->capture_default_str();
    fit->add_option("--size",         fitSize,"size of precomputed table (theta, alpha)")->capture_default_str();
    fit->add_option("--samples",         fitSamples,"number of samples used to compute the error during fitting")->capture_default_str();
    fit->add_option("--max-iterations",  fitMaxIter,"Nelder–Mead maximum iteration count")->capture_default_str();

    CLI11_PARSE(app, argc, argv);

    // BRDF to fit
    BrdfGGX brdf_ggx;
    BrdfBeckmann brdf_beckmann;
    BrdfDisneyDiffuse brdf_disney;

    Brdf* brdf = nullptr;
    if(brdfName == "ggx"){
        brdf = &brdf_ggx;
    }else if(brdfName == "beckmann"){
        brdf = &brdf_beckmann;
    }else if(brdfName == "disney-diffuse"){
        brdf = &brdf_disney;
    }else{
        return -1;
    }

    // allocate data
    int32_t N2 = fitSize*fitSize;
    mat3*  tab = new mat3[N2];
    vec2*  tabMagFresnel = new vec2[N2];
    float* tabSphere = new float[N2];

    // fit
    fitTab(tab, tabMagFresnel, fitSize, *brdf, fitSamples, fitMaxIter);

    // projected solid angle of a spherical cap, clipped to the horizon
    genSphereTab(tabSphere, fitSize);

    // pack tables (texture representation)
    vec4* tex1 = new vec4[N2];
    vec4* tex2 = new vec4[N2];
    packTab(tex1, tex2, tab, tabMagFresnel, tabSphere, fitSize);

    write_dds("GGX_LTCMat.dds", reinterpret_cast<const float*>(tex1), fitSize);
    write_dds("GGX_LTCAmp.dds", reinterpret_cast<const float*>(tex2), fitSize);

    // spherical plots
    // make_spherical_plots(brdf, tab, N);

    // delete data
    delete[] tab;
    delete[] tabMagFresnel;
    delete[] tabSphere;
    delete[] tex1;
    delete[] tex2;

    return 0;
}
