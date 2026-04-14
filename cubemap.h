#pragma once
#include "geometry.h"       // 你自己的 vec3 类
#include "hdr.h"      // 你刚创建的 HDR 读取类
#include <cmath>

// 立方体6个面
#define CUBE_POSITIVE_X 0
#define CUBE_NEGATIVE_X 1
#define CUBE_POSITIVE_Y 2
#define CUBE_NEGATIVE_Y 3
#define CUBE_POSITIVE_Z 4
#define CUBE_NEGATIVE_Z 5

struct Cubemap
{
    int size = 128;                // 立方图大小（128~256足够IBL）
    float* face[6] = { nullptr };   // 6个面，每个面是 float RGB 数据

    // 构造：创建6个面的内存
    Cubemap(int cubeSize) : size(cubeSize)
    {
        for (int i = 0; i < 6; i++)
        {
            face[i] = new float[size * size * 3];
        }
    }

    // 析构：释放内存
    ~Cubemap()
    {
        for (int i = 0; i < 6; i++)
        {
            delete[] face[i];
        }
    }

    // ------------------------------
    // 核心函数1：从HDR全景图创建立方体
    // ------------------------------
    void fromHDR(const HDRImage& hdr);

    // ------------------------------
    // 核心函数2：用方向向量采样颜色
    // ------------------------------
    vec3 sample(vec3 dir) const;

private:
    // 内部工具：从方向获取面+UV坐标
    void getFaceUV(vec3 dir, int& outFace, float& outU, float& outV) const;
};