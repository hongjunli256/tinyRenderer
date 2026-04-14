#pragma once

#include <cstdio>
#include <vector>
#include <string>
#include <cstring>

struct HDRImage
{
    int width = 0, height = 0;
    std::vector<float> data;

    bool load(const std::string& path);

    float get(int x, int y, int c) const
    {
        x = (x % width + width) % width;
        y = (y % height + height) % height;
        return data[(y * width + x) * 3 + c];
    }
};