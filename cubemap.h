#pragma once
#define _USE_MATH_DEFINES
#include "geometry.h"
#include "hdr.h"
#include <cmath>
#include <cstring>
#include <algorithm>

// 立方体6个面
#define CUBE_POSITIVE_X 0
#define CUBE_NEGATIVE_X 1
#define CUBE_POSITIVE_Y 2
#define CUBE_NEGATIVE_Y 3
#define CUBE_POSITIVE_Z 4
#define CUBE_NEGATIVE_Z 5

struct Cubemap
{
    int size = 128;
    float* face[6] = { nullptr };

    Cubemap(int cubeSize) : size(cubeSize)
    {
        for (int i = 0; i < 6; i++)
        {
            face[i] = new float[size * size * 3];
            memset(face[i], 0, sizeof(float) * size * size * 3);
        }
    }

    ~Cubemap()
    {
        for (int i = 0; i < 6; i++)
        {
            delete[] face[i];
        }
    }

    // 禁止拷贝，防止双重释放；允许移动
    Cubemap(const Cubemap&) = delete;
    Cubemap& operator=(const Cubemap&) = delete;
    Cubemap(Cubemap&&) noexcept = default;
    Cubemap& operator=(Cubemap&&) noexcept = default;

    void fromHDR(const HDRImage& hdr);
    void generatePrefilter(const Cubemap& source, int roughnessLevel);
    void generateIrradiance(const Cubemap& source);
    vec3 sample(vec3 dir) const;

private:
    void getFaceUV(vec3 dir, int& outFace, float& outU, float& outV) const;
};