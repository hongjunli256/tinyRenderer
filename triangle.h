#pragma once
#include"model.h"
constexpr int width_obj = 800;
constexpr int height_obj = 800;
//const double PI = 3.1415926535;
//constexpr int width_obj_shadow = 2048;
//constexpr int height_obj_shadow = 2048;

constexpr TGAColor white = { 255, 255, 255, 255 }; // attention, BGRA order
constexpr TGAColor green = { 0, 255,   0, 255 };
constexpr TGAColor red = { 0,   0, 255, 255 };
constexpr TGAColor blue = { 255, 128,  64, 255 };
constexpr TGAColor yellow = { 0, 255, 255, 255 };
constexpr TGAColor orange = { 0, 165, 255, 255 };

struct RenderSettings {
    vec3 eye = { 0, 0, 4 };          // 相机
    vec3 light_vec = { 1, 1, 1 };    // 光线方向
    const vec3 center = { 0,0,0 };
    const vec3 up = { 0,1,0 };
};
void build_obj_triangle(const Model&model, TGAImage& framebuffer, TGAImage& zbuffer, TGAImage& framebuffer_toon, std::vector<double>& zbuffer_true, std::vector<double>& zbuffer_true_shadow, const RenderSettings &setting);



////划线第三代，基于第二代进行性能优化,把浮点数运算替换为整数运算
//void line(int ax, int ay, int bx, int by, TGAImage& framebuffer, TGAColor color) {
//    //取x或者y中比较长的计算
//    bool steep = std::abs(ax - bx) < std::abs(ay - by);
//    if (steep)
//    {
//        std::swap(ax, ay);
//        std::swap(bx, by);
//    }
//    //保证for循环进行哈哈
//    if (ax > bx)
//    {
//        std::swap(ax, bx);
//        std::swap(ay, by);
//    }
//    int y = ay;
//    int ierror = 0;
//    for (int x = ax; x <= bx; x++)
//    {
//        if (steep)
//        {
//            framebuffer.set(y, x, color);
//        }
//        else
//        {
//            framebuffer.set(x, y, color);
//        }
//        ierror += 2 * std::abs(by - ay);
//        y += (by > ay ? 1 : -1) * (ierror > bx - ax);
//        ierror -= 2 * (bx - ax) * (ierror > bx - ax);
//    }
//}

//暂时放着咯
//void draw_triangle_color(triangle& tri, const PhongShader& shader, const SSAOShader& ssaoShader, TGAImage& framebuffer, std::vector<double>& zbuffer_true, int width, int height) {
//
//    vec4 ndc[3] = { tri.dot[0] / tri.dot[0].w, tri.dot[1] / tri.dot[1].w, tri.dot[2] / tri.dot[2].w };
//    vec2 screen[3] = { (Viewport * ndc[0]).xy(), (Viewport * ndc[1]).xy(), (Viewport * ndc[2]).xy() };
//    double space = tri.signed_triangle_area(screen);
//    if (space < 0)
//    {
//        return;
//    }
//    int raw_bbminx = std::min(std::min((int)screen[0].x, (int)screen[1].x), (int)screen[2].x);
//    int raw_bbminy = std::min(std::min((int)screen[0].y, (int)screen[1].y), (int)screen[2].y);
//    int raw_bbmaxx = std::max(std::max((int)screen[0].x, (int)screen[1].x), (int)screen[2].x);
//    int raw_bbmaxy = std::max(std::max((int)screen[0].y, (int)screen[1].y), (int)screen[2].y);
//
//    int bbminx = std::max(0, raw_bbminx);
//    int bbminy = std::max(0, raw_bbminy);
//    int bbmaxx = std::min(width - 1, raw_bbmaxx);
//    int bbmaxy = std::min(height - 1, raw_bbmaxy);
//
//    if (bbminx > bbmaxx || bbminy > bbmaxy) {
//        return;
//    }
//    int w = bbmaxx - bbminx + 1;
//    int h = bbmaxy - bbminy + 1;
//
//    int ax = (int)screen[0].x, ay = (int)screen[0].y;
//    int bx = (int)screen[1].x, by = (int)screen[1].y;
//    int cx = (int)screen[2].x, cy = (int)screen[2].y;
//
//    int e0 = edge(ax, ay, bx, by, bbminx, bbminy);
//    int e1 = edge(bx, by, cx, cy, bbminx, bbminy);
//    int e2 = edge(cx, cy, ax, ay, bbminx, bbminy);
//
//    int de0x = -(by - ay);
//    int de1x = -(cy - by);
//    int de2x = -(ay - cy);
//
//    int de0y = (bx - ax);
//    int de1y = (cx - bx);
//    int de2y = (ax - cx);
//    mat<4, 4> model_ = ModelView.invert_transpose();
//#pragma omp parallel for private(ce0, ce1, ce2)
//    for (int j = 0; j < h; j++) {
//        int ce0 = e0 + de0y * j;
//        int ce1 = e1 + de1y * j;
//        int ce2 = e2 + de2y * j;
//        for (int i = 0; i < w; i++) {
//            bool inside =
//                (ce0 >= 0 && ce1 >= 0 && ce2 >= 0) ||
//                (ce0 <= 0 && ce1 <= 0 && ce2 <= 0);
//
//            if (inside) {
//                double for_c = (double)ce0 / tri.dot[2].w;
//                double for_a = (double)ce1 / tri.dot[0].w;
//                double for_b = (double)ce2 / tri.dot[1].w;
//                vec3 bar = { for_a,for_b ,for_c };
//                bar = bar / (for_a + for_b + for_c);
//
//                vec4 raw_n4 = tri.norm_gravity(bar[0], bar[1], bar[2]);
//                vec3 pixel_nor = { raw_n4.x, raw_n4.y, raw_n4.z };
//                pixel_nor = normalized(pixel_nor);
//
//                TGAColor color_more_real = shader.color(tri, bar, model_);
//                double z = (ndc[0].z * ce1 + ndc[1].z * ce2 + ndc[2].z * ce0) / (ce1 + ce2 + ce0);
//
//                int px = bbminx + i;
//                int py = bbminy + j;
//                int idx = px + py * width;
//
//                float ao = ssaoShader.AO(width, height, px, py, pixel_nor, zbuffer_true, z);
//                color_more_real[0] = std::min(255, (int)(color_more_real[0] * ao));
//                color_more_real[1] = std::min(255, (int)(color_more_real[1] * ao));
//                color_more_real[2] = std::min(255, (int)(color_more_real[2] * ao));
//
//                if (z >= zbuffer_true[idx])
//                {
//                    framebuffer.set(bbminx + i, bbminy + j, color_more_real);
//                    zbuffer_true[idx] = z;
//                }
//            }
//            ce0 += de0x;
//            ce1 += de1x;
//            ce2 += de2x;
//        }
//    }
//}

//void draw_triangle_toon(triangle& tri, const ToonShader& shader, TGAImage& framebuffer, std::vector<double>& zbuffer_true, int width, int height) {
//
//    vec4 ndc[3] = { tri.dot[0] / tri.dot[0].w, tri.dot[1] / tri.dot[1].w, tri.dot[2] / tri.dot[2].w };
//    vec2 screen[3] = { (Viewport * ndc[0]).xy(), (Viewport * ndc[1]).xy(), (Viewport * ndc[2]).xy() };
//    double space = tri.signed_triangle_area(screen);
//    if (space < 0)
//    {
//        return;
//    }
//    int raw_bbminx = std::min(std::min((int)screen[0].x, (int)screen[1].x), (int)screen[2].x);
//    int raw_bbminy = std::min(std::min((int)screen[0].y, (int)screen[1].y), (int)screen[2].y);
//    int raw_bbmaxx = std::max(std::max((int)screen[0].x, (int)screen[1].x), (int)screen[2].x);
//    int raw_bbmaxy = std::max(std::max((int)screen[0].y, (int)screen[1].y), (int)screen[2].y);
//
//    int bbminx = std::max(0, raw_bbminx);
//    int bbminy = std::max(0, raw_bbminy);
//    int bbmaxx = std::min(width - 1, raw_bbmaxx);
//    int bbmaxy = std::min(height - 1, raw_bbmaxy);
//
//    if (bbminx > bbmaxx || bbminy > bbmaxy) {
//        return;
//    }
//    int w = bbmaxx - bbminx + 1;
//    int h = bbmaxy - bbminy + 1;
//
//    int ax = (int)screen[0].x, ay = (int)screen[0].y;
//    int bx = (int)screen[1].x, by = (int)screen[1].y;
//    int cx = (int)screen[2].x, cy = (int)screen[2].y;
//
//    int e0 = edge(ax, ay, bx, by, bbminx, bbminy);
//    int e1 = edge(bx, by, cx, cy, bbminx, bbminy);
//    int e2 = edge(cx, cy, ax, ay, bbminx, bbminy);
//
//    int de0x = -(by - ay);
//    int de1x = -(cy - by);
//    int de2x = -(ay - cy);
//
//    int de0y = (bx - ax);
//    int de1y = (cx - bx);
//    int de2y = (ax - cx);
//    mat<4, 4> model_ = ModelView.invert_transpose();
//#pragma omp parallel for private(ce0, ce1, ce2)
//    for (int j = 0; j < h; j++) {
//        int ce0 = e0 + de0y * j;
//        int ce1 = e1 + de1y * j;
//        int ce2 = e2 + de2y * j;
//        for (int i = 0; i < w; i++) {
//            bool inside =
//                (ce0 >= 0 && ce1 >= 0 && ce2 >= 0) ||
//                (ce0 <= 0 && ce1 <= 0 && ce2 <= 0);
//
//            if (inside) {
//                double for_c = (double)ce0 / tri.dot[2].w;
//                double for_a = (double)ce1 / tri.dot[0].w;
//                double for_b = (double)ce2 / tri.dot[1].w;
//                vec3 bar = { for_a,for_b ,for_c };
//                bar = bar / (for_a + for_b + for_c);
//                TGAColor color_more_real = shader.color(tri, bar, model_);
//                double z = (ndc[0].z * ce1 + ndc[1].z * ce2 + ndc[2].z * ce0) / (ce1 + ce2 + ce0);
//                int idx = bbminx + i + (bbminy + j) * width;
//                if (z >= zbuffer_true[idx])
//                {
//
//                    framebuffer.set(bbminx + i, bbminy + j, color_more_real);
//                    zbuffer_true[idx] = z;
//
//                }
//            }
//            ce0 += de0x;
//            ce1 += de1x;
//            ce2 += de2x;
//        }
//    }
//}