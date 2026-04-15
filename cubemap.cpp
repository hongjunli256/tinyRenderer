#include "cubemap.h"
#include <algorithm>
#include <cmath>

static vec3 sampleHDR(const HDRImage& hdr, vec3 dir)
{
    dir = normalized(dir);

    float phi = atan2(dir.z, dir.x);
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

    float maxAxis = ax;
    if (outFace == 2 || outFace == 3) maxAxis = ay;
    else maxAxis = az;

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


    color.x = color.x / (color.x + 1.0);
    color.y = color.y / (color.y + 1.0);
    color.z = color.z / (color.z + 1.0);


    color.x = std::max(0.0, std::min(color.x, 1.0));
    color.y = std::max(0.0, std::min(color.y, 1.0));
    color.z = std::max(0.0, std::min(color.z, 1.0));

    return color;
}

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