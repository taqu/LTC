#include "fit/brdf.h"
#include <cmath>

namespace
{
float lambda_beckman(const float alpha, const float cosTheta)
{
    const float a = 1.0f / alpha / tanf(acosf(cosTheta));
    return (cosTheta < 1.0f) ? (1.0f - 1.259f * a + 0.396f * a * a) / (3.535f * a + 2.181f * a * a) : 0.0f;
}

float lambda_ggx(const float alpha, const float cosTheta)
{
    const float a = 1.0f / alpha / tanf(acosf(cosTheta));
    return (cosTheta < 1.0f) ? 0.5f * (-1.0f + sqrtf(1.0f + 1.0f / a / a)) : 0.0f;
}
} // namespace

float BrdfBeckmann::eval(const glm::vec3& V, const glm::vec3& L, const float alpha, float& pdf) const
{
    using namespace glm;
    if(V.z <= 0) {
        pdf = 0;
        return 0;
    }

    // masking
    const float LambdaV = lambda_beckman(alpha, V.z);

    // shadowing
    float G2;
    if(L.z <= 0.0f)
        G2 = 0;
    else {
        const float LambdaL = lambda_beckman(alpha, L.z);
        G2 = 1.0f / (1.0f + LambdaV + LambdaL);
    }

    // D
    const vec3 H = normalize(V + L);
    const float slopex = H.x / H.z;
    const float slopey = H.y / H.z;
    float D = expf(-(slopex * slopex + slopey * slopey) / (alpha * alpha)) / (3.14159f * alpha * alpha * H.z * H.z * H.z * H.z);

    pdf = fabsf(D * H.z / 4.0f / dot(V, H));
    float res = D * G2 / 4.0f / V.z;

    return res;
}

glm::vec3 BrdfBeckmann::sample(const glm::vec3& V, const float alpha, const float U1, const float U2) const
{
    using namespace glm;
    const float phi = 2.0f * 3.14159f * U1;
    const float r = alpha * sqrtf(-logf(U2));
    const vec3 N = normalize(vec3(r * cosf(phi), r * sinf(phi), 1.0f));
    const vec3 L = -V + 2.0f * N * dot(N, V);
    return L;
}

float BrdfGGX::eval(const glm::vec3& V, const glm::vec3& L, const float alpha, float& pdf) const
{
    using namespace glm;
    if(V.z <= 0) {
        pdf = 0;
        return 0;
    }

    // masking
    const float LambdaV = lambda_ggx(alpha, V.z);

    // shadowing
    float G2;
    if(L.z <= 0.0f)
        G2 = 0;
    else {
        const float LambdaL = lambda_ggx(alpha, L.z);
        G2 = 1.0f / (1.0f + LambdaV + LambdaL);
    }

    // D
    const vec3 H = normalize(V + L);
    const float slopex = H.x / H.z;
    const float slopey = H.y / H.z;
    float D = 1.0f / (1.0f + (slopex * slopex + slopey * slopey) / alpha / alpha);
    D = D * D;
    D = D / (3.14159f * alpha * alpha * H.z * H.z * H.z * H.z);

    pdf = fabsf(D * H.z / 4.0f / dot(V, H));
    float res = D * G2 / 4.0f / V.z;

    return res;
}

glm::vec3 BrdfGGX::sample(const glm::vec3& V, const float alpha, const float U1, const float U2) const
{
    using namespace glm;
    const float phi = 2.0f * 3.14159f * U1;
    const float r = alpha * sqrtf(U2 / (1.0f - U2));
    const vec3 N = normalize(vec3(r * cosf(phi), r * sinf(phi), 1.0f));
    const vec3 L = -V + 2.0f * N * dot(N, V);
    return L;
}

float BrdfDisneyDiffuse::eval(const glm::vec3& V, const glm::vec3& L, const float alpha, float& pdf) const
{
    using namespace glm;
    if(V.z <= 0 || L.z <= 0) {
        pdf = 0;
        return 0;
    }

    pdf = L.z / 3.14159f;

    float NdotV = V.z;
    float NdotL = L.z;
    float LdotH = dot(L, normalize(V + L));
    float perceptualRoughness = sqrtf(alpha);
    float fd90 = 0.5f + 2 * LdotH * LdotH * perceptualRoughness;
    float lightScatter = (1 + (fd90 - 1) * powf(1 - NdotL, 5.0f));
    float viewScatter = (1 + (fd90 - 1) * powf(1 - NdotV, 5.0f));
    return lightScatter * viewScatter * L.z / 3.14159f;
}

glm::vec3 BrdfDisneyDiffuse::sample(const glm::vec3& /*V*/, const float /*alpha*/, const float U1, const float U2) const
{
    using namespace glm;
    const float r = sqrtf(U1);
    const float phi = 2.0f * 3.14159f * U2;
    const vec3 L = vec3(r * cosf(phi), r * sinf(phi), sqrtf(1.0f - r * r));
    return L;
}
