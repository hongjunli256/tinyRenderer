#include "hdr.h"

bool HDRImage::load(const std::string& path)
{
    FILE* f;
    errno_t err = fopen_s(&f, path.c_str(), "rb");
    if (err != 0 || !f) return false;

    char magic[256];
    if (!fgets(magic, sizeof(magic), f))
    {
        fclose(f);
        return false;
    }

    char line[256];
    while (true)
    {
        if (!fgets(line, sizeof(line), f)) break;
        if (line[0] == '\n') break;
    }

    int w, h;
    fscanf_s(f, "-Y %d +X %d\n", &h, &w);
    width = w;
    height = h;

    data.resize(width * height * 3, 0.0f);

    unsigned char buf[4];
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            if (fread(buf, 1, 4, f) != 4) break;

            float exp = powf(2.0f, (float)buf[3] - 128.0f);
            float r = (float)buf[0] / 255.0f * exp;
            float g = (float)buf[1] / 255.0f * exp;
            float b = (float)buf[2] / 255.0f * exp;

            int i = (y * width + x) * 3;
            data[i + 0] = r;
            data[i + 1] = g;
            data[i + 2] = b;
        }
    }

    fclose(f);
    return true;
}