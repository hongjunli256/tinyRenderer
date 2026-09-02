#include "cubemap.h"
#include <algorithm>
#include <cmath>

static vec3 sampleHDR(const HDRImage& hdr, vec3 dir)
{
    dir = normalized(dir);

    // atan2(y, x)
    float phi = atan2(dir.x, dir.z);
    float theta = acos(dir.y);

    float u = 0.5f - (phi / (2.0f * (float)M_PI));
    float v = theta / (float)M_PI;

    if (u < 0) u += 1.0f;
    if (v < 0) v += 1.0f;

    int x = (int)(u * hdr.width) % hdr.width;
    int y = (int)(v * hdr.height) % hdr.height;

    return vec3{
        hdr.get(x, y, 0),
        hdr.get(x, y, 1),
        hdr.get(x, y, 2)
    };
}

void Cubemap::fromHDR(const HDRImage& hdr)
{
    for (int side = 0; side < 6; side++)
    {
        for (int y = 0; y < size; y++)
        {
            for (int x = 0; x < size; x++)
            {
                float u = (x + 0.5f) / size;
                float v = (y + 0.5f) / size;
                vec3 dir;

                if (side == CUBE_POSITIVE_X) dir = { 1, -(v * 2 - 1), -(u * 2 - 1) };
                if (side == CUBE_NEGATIVE_X) dir = { -1, -(v * 2 - 1), u * 2 - 1 };
                if (side == CUBE_POSITIVE_Y) dir = { u * 2 - 1, 1, v * 2 - 1 };
                if (side == CUBE_NEGATIVE_Y) dir = { u * 2 - 1, -1, -(v * 2 - 1) };
                if (side == CUBE_POSITIVE_Z) dir = { u * 2 - 1, -(v * 2 - 1), 1 };
                if (side == CUBE_NEGATIVE_Z) dir = { -(u * 2 - 1), -(v * 2 - 1), -1 };

                dir = normalized(dir);
                vec3 color = sampleHDR(hdr, dir);

                int i = (y * size + x) * 3;
                face[side][i + 0] = color.x;
                face[side][i + 1] = color.y;
                face[side][i + 2] = color.z;
            }
        }
    }
}

void Cubemap::getFaceUV(vec3 dir, int& outFace, float& outU, float& outV) const
{
    vec3 v = normalized(dir);
    float x = v.x, y = v.y, z = v.z;
    float ax = fabsf(x), ay = fabsf(y), az = fabsf(z);

    if (ax >= ay && ax >= az && x > 0) outFace = CUBE_POSITIVE_X;
    else if (ax >= ay && ax >= az && x < 0) outFace = CUBE_NEGATIVE_X;
    else if (ay >= ax && ay >= az && y > 0) outFace = CUBE_POSITIVE_Y;
    else if (ay >= ax && ay >= az && y < 0) outFace = CUBE_NEGATIVE_Y;
    else if (az >= ax && az >= ay && z > 0) outFace = CUBE_POSITIVE_Z;
    else outFace = CUBE_NEGATIVE_Z;

    float u, vv;
    switch (outFace)
    {
    case CUBE_POSITIVE_X: u = -z; vv = -y; break;
    case CUBE_NEGATIVE_X: u = z; vv = -y; break;
    case CUBE_POSITIVE_Y: u = x; vv = z; break;
    case CUBE_NEGATIVE_Y: u = x; vv = -z; break;
    case CUBE_POSITIVE_Z: u = x; vv = -y; break;
    case CUBE_NEGATIVE_Z: u = -x; vv = -y; break;
    default: u = 0; vv = 0; break;
    }

    float maxAxis;
    if (outFace == CUBE_POSITIVE_Y || outFace == CUBE_NEGATIVE_Y)
        maxAxis = ay;
    else if (outFace == CUBE_POSITIVE_X || outFace == CUBE_NEGATIVE_X)
        maxAxis = ax;
    else
        maxAxis = az;

    outU = (u / maxAxis + 1.0f) * 0.5f;
    outV = (vv / maxAxis + 1.0f) * 0.5f;
}

vec3 Cubemap::sample(vec3 dir) const
{
    int faceIdx;
    float u, v;
    getFaceUV(dir, faceIdx, u, v);

    int x = (int)(u * size);
    int y = (int)(v * size);
    x = std::clamp(x, 0, size - 1);
    y = std::clamp(y, 0, size - 1);

    int i = (y * size + x) * 3;
    vec3 color = {
        face[faceIdx][i + 0],
        face[faceIdx][i + 1],
        face[faceIdx][i + 2]
    };

    color.x = std::max(0.0, color.x);
    color.y = std::max(0.0, color.y);
    color.z = std::max(0.0, color.z);
    return color;
}

void Cubemap::generateIrradiance(const Cubemap& source)
{
    const int numSamples = 16; // 16足够，不要32

    for (int side = 0; side < 6; side++)
    {
        for (int y = 0; y < size; y++)
        {
            for (int x = 0; x < size; x++)
            {
                float u = (x + 0.5f) / size;
                float v = (y + 0.5f) / size;
                vec3 dir;
                if (side == CUBE_POSITIVE_X) dir = { 1, -(v * 2 - 1), -(u * 2 - 1) };
                if (side == CUBE_NEGATIVE_X) dir = { -1, -(v * 2 - 1), u * 2 - 1 };
                if (side == CUBE_POSITIVE_Y) dir = { u * 2 - 1, 1, v * 2 - 1 };
                if (side == CUBE_NEGATIVE_Y) dir = { u * 2 - 1, -1, -(v * 2 - 1) };
                if (side == CUBE_POSITIVE_Z) dir = { u * 2 - 1, -(v * 2 - 1), 1 };
                if (side == CUBE_NEGATIVE_Z) dir = { -(u * 2 - 1), -(v * 2 - 1), -1 };
                dir = normalized(dir);

                vec3 irradiance{ 0,0,0 };

                for (int i = 0; i < numSamples; i++)
                {
                    for (int j = 0; j < numSamples; j++)
                    {
                        float u1 = (i + 0.5f) / numSamples;
                        float u2 = (j + 0.5f) / numSamples;

                        float phi = 2.0f * (float)M_PI * u1;
                        float cosTheta = sqrtf(u2);
                        float sinTheta = sqrtf(1.0f - u2);

                        vec3 t{ cosf(phi) * sinTheta, sinf(phi) * sinTheta, cosTheta };

                        vec3 up;
                        if (fabs(dir.y) < 0.999f)
                            up = { 0,1,0 };
                        else
                            up = { 1,0,0 };

                        vec3 tangent = cross(up, dir);
                        float tLen = sqrtf(tangent.x * tangent.x + tangent.y * tangent.y + tangent.z * tangent.z);
                        if (tLen < 1e-6f)
                        {
                            tangent = { 1,0,0 };
                        }
                        else
                        {
                            tangent = tangent / tLen;
                        }
                        vec3 bitangent = cross(tangent, dir);
                        bitangent = normalized(bitangent);

                        vec3 sampleDir = tangent * t.x + bitangent * t.y + dir * t.z;
                        sampleDir = normalized(sampleDir);

                        float ndotwi = sampleDir* dir;
                        if (ndotwi <= 0.0f)
                            continue;

                        vec3 col = source.sample(sampleDir);
                        // Reinhard软高光压缩，无硬截断，压制HDR尖峰
                        col.x = col.x / (1.0f + col.x);
                        col.y = col.y / (1.0f + col.y);
                        col.z = col.z / (1.0f + col.z);

                        irradiance = irradiance + col;
                    }
                }
                // 余弦重要性采样正确缩放
                float scale = (float)M_PI / float(numSamples * numSamples);
                irradiance = irradiance * scale;

                // 全部使用三元表达式，不使用std::max / std::min
                irradiance.x = (irradiance.x < 0.0f) ? 0.0f : ((irradiance.x > 100.0f) ? 100.0f : irradiance.x);
                irradiance.y = (irradiance.y < 0.0f) ? 0.0f : ((irradiance.y > 100.0f) ? 100.0f : irradiance.y);
                irradiance.z = (irradiance.z < 0.0f) ? 0.0f : ((irradiance.z > 100.0f) ? 100.0f : irradiance.z);

                int idx = (y * size + x) * 3;
                face[side][idx + 0] = irradiance.x;
                face[side][idx + 1] = irradiance.y;
                face[side][idx + 2] = irradiance.z;
            }
        }
    }
}

// 注意：当前只是简单随机抖动模糊，不是GGX重要性采样，仅做环境模糊，不适合PBR高光IBL
void Cubemap::generatePrefilter(const Cubemap& source, int roughnessLevel)
{
    int maxMip = 5;
    roughnessLevel = std::clamp(roughnessLevel, 0, maxMip);
    float blur = (float)roughnessLevel / (float)maxMip;

    for (int side = 0; side < 6; side++)
    {
        for (int y = 0; y < size; y++)
        {
            for (int x = 0; x < size; x++)
            {
                float u = (x + 0.5f) / size;
                float v = (y + 0.5f) / size;
                vec3 dir;

                if (side == CUBE_POSITIVE_X) dir = { 1, -(v * 2 - 1), -(u * 2 - 1) };
                if (side == CUBE_NEGATIVE_X) dir = { -1, -(v * 2 - 1), u * 2 - 1 };
                if (side == CUBE_POSITIVE_Y) dir = { u * 2 - 1, 1, v * 2 - 1 };
                if (side == CUBE_NEGATIVE_Y) dir = { u * 2 - 1, -1, -(v * 2 - 1) };
                if (side == CUBE_POSITIVE_Z) dir = { u * 2 - 1, -(v * 2 - 1), 1 };
                if (side == CUBE_NEGATIVE_Z) dir = { -(u * 2 - 1), -(v * 2 - 1), -1 };

                dir = normalized(dir);

                vec3 finalColor = { 0,0,0 };
                int samples = 16;
                for (int i = 0; i < samples; i++)
                {
                    vec3 jitter = {
                        (rand() / (float)RAND_MAX - 0.5f) * blur,
                        (rand() / (float)RAND_MAX - 0.5f) * blur,
                        (rand() / (float)RAND_MAX - 0.5f) * blur
                    };
                    vec3 sampleDir = normalized(dir + jitter);
                    finalColor = finalColor + source.sample(sampleDir);
                }
                finalColor = finalColor * (1.0f / samples);

                int i = (y * size + x) * 3;
                face[side][i + 0] = finalColor.x;
                face[side][i + 1] = finalColor.y;
                face[side][i + 2] = finalColor.z;
            }
        }
    }
}