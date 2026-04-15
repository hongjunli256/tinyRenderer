#include <cmath>
//#include <vector>
#include<list>
#include "triangle.h"
#include "model.h"
#include "tgaimage.h"

#include "GLFW/glfw3.h"
#include "Imgui/imgui.h"
#include "Imgui/imgui_impl_glfw.h"
#include "Imgui/imgui_impl_opengl3.h"

//#include "hdr.h"       // 加
//#include "cubemap.h"   // 加

#pragma comment(lib, "glfw3.lib")
#pragma comment(lib, "opengl32.lib")

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif


struct RenderOutput {
    TGAImage color;
    TGAImage toon;
    TGAImage zbuf;
    std::vector<double> ztrue;
    std::vector<double> zshadow;
    GLuint tex_color = 0, tex_toon = 0, tex_zbuf = 0; // 缓存纹理

    RenderOutput(int w, int h)
        : color(w, h, 3, { 177,195,209,255 }),
        toon(w, h, 3, { 255,255,255,255 }),
        zbuf(w, h, 1, { 0,0,0,0 })
    {
        ztrue.assign(w * h, -511.0);
        zshadow.assign(width_obj * height_obj, -511.0);
    }

    // 清理纹理
    ~RenderOutput() {
        if (tex_color) glDeleteTextures(1, &tex_color);
        if (tex_toon)  glDeleteTextures(1, &tex_toon);
        if (tex_zbuf)  glDeleteTextures(1, &tex_zbuf);
    }
};

// 渲染函数（传入相机 & 光线）
void render_scene(const RenderSettings& s,std::list<Model>&models, RenderOutput& out) {
    // 清空深度缓冲（两个都清！）
    std::fill(out.ztrue.begin(), out.ztrue.end(), -511.0);
    std::fill(out.zshadow.begin(), out.zshadow.end(), -511.0);
    out.color.clear({ 177,195,205,255 });
    out.toon.clear({ 255,255,255,255 });
    out.zbuf.clear({ 0,0,0,0 });
    for (const Model& model : models) // 加 & 变成引用，0拷贝

    {
        build_obj_triangle(model, out.color, out.zbuf, out.toon, out.ztrue, out.zshadow,s);
    }
}

// 纹理工具（不变）
static GLuint texture_from_tga(const TGAImage& img) {
   

    int w = img.width();
    int h = img.height();
    int bpp = img.get_bpp();
    const uint8_t* data = img.get_data().data();

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);

    if (bpp == 3) {
        std::vector<uint8_t> rgb(w * h * 3);
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                int src_idx = (y * w + x) * 3;
                int dst_idx = ((h - 1 - y) * w + x) * 3; //翻转行
                rgb[dst_idx + 0] = data[src_idx + 2];
                rgb[dst_idx + 1] = data[src_idx + 1];
                rgb[dst_idx + 2] = data[src_idx + 0];
            }
        }
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());
    }
    else if (bpp == 1) {
        std::vector<uint8_t> gray(w * h);
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                int src_idx = y * w + x;
                int dst_idx = (h - 1 - y) * w + x; //翻转行
                gray[dst_idx] = data[src_idx];
            }
        }
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, gray.data());
        
    }
    return tex;
}

int main() {
    std::list<Model>models;
    models.emplace_back("obj/diablo3_pose.obj");
    models.emplace_back("obj/floor.obj");
    //models.emplace_back("obj/african_head_eye_inner.obj");
    //models.emplace_back("obj/african_head.obj");

    glfwInit();
    GLFWwindow* window = glfwCreateWindow(1600, 900, "SoftRenderer Viewer", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");
    ImGui::StyleColorsDark();

    RenderSettings settings;
    RenderOutput output(width_obj, height_obj); // 只创建一次
    render_scene(settings, models, output);     // 直接渲染进去，不拷贝

    output.tex_color = texture_from_tga(output.color);
    output.tex_toon = texture_from_tga(output.toon);
    output.tex_zbuf = texture_from_tga(output.zbuf);
    bool need_rerender = false;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        if (need_rerender) {
            render_scene(settings, models, output);
            // 删除旧纹理
            if (output.tex_color) glDeleteTextures(1, &output.tex_color);
            if (output.tex_toon)  glDeleteTextures(1, &output.tex_toon);
            if (output.tex_zbuf)  glDeleteTextures(1, &output.tex_zbuf);
            // 生成新纹理
            output.tex_color = texture_from_tga(output.color);
            output.tex_toon = texture_from_tga(output.toon);
            output.tex_zbuf = texture_from_tga(output.zbuf);
            need_rerender = false;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Render Controller");

        ImGui::Text("=== Camera (eye) ===");
        double min_eye = -5.0, max_eye = 5.0;
        ImGui::DragScalarN("Position", ImGuiDataType_Double, &settings.eye.x, 3, 0.05, &min_eye, &max_eye);

        ImGui::Separator();
        ImGui::Text("=== Light (light_vec) ===");
        double min_light = -5.0, max_light = 5.0;
        ImGui::DragScalarN("Direction", ImGuiDataType_Double, &settings.light_vec.x, 3, 0.05, &min_light, &max_light);


        ImGui::Separator();
        if (ImGui::Button("Render")) need_rerender = true;
        ImGui::SameLine();
        if (ImGui::Button("Save TGA")) {
            output.color.write_tga_file("output.tga");
            output.toon.write_tga_file("output_toon.tga");
            output.zbuf.write_tga_file("zbuffer.tga");
        }

        ImGui::Separator();

        // ImGui 显示
        ImGui::Image((ImTextureID)(intptr_t)output.tex_color, ImVec2(400, 400));
        ImGui::SameLine();
        ImGui::Image((ImTextureID)(intptr_t)output.tex_toon, ImVec2(400, 400));
        ImGui::Image((ImTextureID)(intptr_t)output.tex_zbuf, ImVec2(400, 400));

        ImGui::End();
        ImGui::Render();

        glClearColor(0.12f,0.12f,0.15f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return 0;
}