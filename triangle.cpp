#include"triangle.h"
#include "cubemap.h"
//明天看看怎么能够进一步提高效率，我感觉之前的代码里面还是存在莫名的拷贝存在，现在是draw卡通和原模型一起做4.3---提高效率好像没有尽头
//反正现在我是一定会做阴影的不如直接提前渲染出两个zbuffer顺便为以后优化ssao为3D做准备4.4---计划在PBR和IBL之后
//目前已知缺陷非3Dssao,然后阴影的处理变换大小暂时和原窗口一样大实际上这个不合理，但是这些都影响不大我都放在以后做4.4
//优化优化代码，周末对IBL展开写，我真没招了，越改越错，最后解决之后还是有白噪点，暂时用归一化函数，能用效果也可以，先上传一版吧，好久没更了
//接下来优化cubemap和hdr读取提高效率，再次检查重复拷贝问题，最后完全整理好IBL
class GlobalMat
{
private:
    mat<4, 4> Viewport, Perspective, ModelView,ModelView_for_Light;
public:
    void modelview(const vec3 eye, const vec3 center, const vec3 up) {
        vec3 n = normalized(eye - center);
        vec3 l = normalized(cross(up, n));
        vec3 m = normalized(cross(n, l));
        ModelView = mat<4, 4>{ {{l.x,l.y,l.z,0}, {m.x,m.y,m.z,0}, {n.x,n.y,n.z,0}, {0,0,0,1}} } *mat<4, 4>{{{1, 0, 0, -center.x}, { 0,1,0,-center.y }, { 0,0,1,-center.z }, { 0,0,0,1 }}};
    }
    void modelviewforLight(const vec3 light, const vec3 center, const vec3 up) {
        vec3 n = normalized(light - center);
        vec3 l = normalized(cross(up, n));
        vec3 m = normalized(cross(n, l));
        ModelView_for_Light = mat<4, 4>{ {{l.x,l.y,l.z,0}, {m.x,m.y,m.z,0}, {n.x,n.y,n.z,0}, {0,0,0,1}} };// *mat<4, 4>{{{1, 0, 0, -center.x}, { 0,1,0,-center.y }, { 0,0,1,-center.z }, { 0,0,0,1 }}};
    }
    void perspective(const double f) {
        Perspective = { {{1,0,0,0}, {0,1,0,0}, {0,0,1,0}, {0,0, -1 / f,1}} };
    }

    void viewport(const int x, const int y, const int width_screen, const int height_screen) {
        Viewport = { {{width_screen / 2., 0, 0, x + width_screen / 2.}, {0, height_screen / 2., 0, y + height_screen / 2.}, {0,0,1,0}, {0,0,0,1}} };
    }
    vec4 scale(const vec4& v)const {
         return Viewport * v;
    }
    vec4 rot(const vec4& v,bool isShadow)const {
        if (isShadow)
        {
            return ModelView_for_Light * v;
        }
        else
        {
            return ModelView * v;
        }
    }
    vec4 persp(const vec4& v)const
    {
        return Perspective * v;
    }
    mat<4, 4>M()
    {
        return (Viewport * Perspective * ModelView).invert();
    }
    mat<4, 4> modelview_invert_transpose()
    {
        return ModelView.invert_transpose();
    }
    mat<4, 4>N()
    {
        return Viewport* Perspective* ModelView_for_Light;
    }
    
};
class triangle {
public:

    vec4 dot[3];
    vec2 uv[3];
    vec4 norm[3];

    triangle(const Model& model, int face, const GlobalMat& gloMat,bool isShadow)
    {
        for (int i = 0; i < 3; i++)
        {
            //再三考虑下还是觉得，三角形处理的是透视矩阵乘下的点，不提前归一，也不用屏幕实际点,免得有时候要处理透视有时候不用，避免提前处理导致麻烦

            dot[i] = gloMat.persp(gloMat.rot(model.vert(face, i),isShadow));
            uv[i] = model.uv(face, i);
            norm[i] = model.normal(face, i);
        }
    }
    //计算是否为背面三角形
    double signed_triangle_area(vec2 screen[3]) {
        return (screen[1].x - screen[0].x) * (screen[2].y - screen[0].y) - (screen[1].y - screen[0].y) * (screen[2].x - screen[0].x);
    }
    //差值得到中间像素的uv，借用新的uv读取例如spec文件，diff文件等
    vec2 uv_gravity(double for_a, double for_b, double for_c)
    {
        return uv[2] * for_c + uv[0] * for_a + uv[1] * for_b;
    }
    //差值得到中间像素的法线
    vec4 norm_gravity(double for_a, double for_b, double for_c)
    {
        return norm[2] * for_c + norm[0] * for_a + norm[1] * for_b;
    }
private:

};
struct PBRShader {
private:
    const Model& model;
    vec4 l;
    const Cubemap* iblIrradiance = nullptr;
    const Cubemap* iblPrefilter = nullptr;
    const TGAImage* brdfLUT = nullptr;

    vec3 mix(const vec3& a, const vec3& b, double t) const {
        return a * (1.0 - t) + b * t;
    }

    vec3 fresnel(const vec3& F0, double cosTheta) const {
        float a = 1.0 - cosTheta;
        float a2 = a * a;
        float a4 = a2 * a2;
        float a5 = a4 * a;
        return F0 + (vec3{ 1.0,1.0,1.0 } - F0) * a5;
    }

    double D_GGX(double NdotH, double roughness) const {
        double a = roughness * roughness;
        double a2 = a * a;
        double NdotH2 = NdotH * NdotH;
        double denom = NdotH2 * (a2 - 1.0) + 1.0;
        return a2 / (M_PI * denom * denom);
    }

    double G_Schlick(double NdotV, double roughness) const {
        double r = roughness + 1.0;
        double k = (r * r) / 8.0;
        return NdotV / (NdotV * (1.0 - k) + k);
    }

    double G_Smith(double NdotV, double NdotL, double roughness) const {
        return G_Schlick(NdotV, roughness) * G_Schlick(NdotL, roughness);
    }

public:
    PBRShader(const vec3& light, const Model& m, const GlobalMat& gloMat,
        const Cubemap* irradiance = nullptr,
        const Cubemap* prefilter = nullptr,
        const TGAImage* brdf = nullptr) : model(m), iblIrradiance(irradiance), iblPrefilter(prefilter), brdfLUT(brdf) {
        l = normalized(gloMat.persp(vec4{ light.x, light.y, light.z, 0.0 }));
    }

    TGAColor color(triangle& tri, const vec3 bar, const mat<4, 4>& modelView_invert_transpose,mat<2,4>&T) const {
        //mat<2, 4> E = { tri.dot[1] - tri.dot[0], tri.dot[2] - tri.dot[0] };
        //mat<2, 2> U = { tri.uv[1] - tri.uv[0], tri.uv[2] - tri.uv[0] };
        //mat<2, 4> T = U.invert() * E;

        vec4 t0 = normalized(T[0]);
        vec4 t1 = normalized(T[1]);
        vec4 n_t = normalized(modelView_invert_transpose * tri.norm_gravity(bar[0], bar[1], bar[2]));
        mat<4, 4> D_mat = { t0, t1, n_t, vec4{0,0,0,1} };

        vec2 uv = tri.uv_gravity(bar[0], bar[1], bar[2]);
        vec4 N = normalized(D_mat.transpose() * model.normal(uv));
        vec4 V = { 0, 0, 1, 0 };
        //vec4 L = l;
        vec4 H = normalized(V + l);

        TGAColor dif = model.diffuse(uv);
        TGAColor sp = model.specular(uv);

        vec3 albedo = {
            dif[2] / 255.0,
            dif[1] / 255.0,
            dif[0] / 255.0
        };

        double roughness = 1.0 - std::max(sp[2] / 255.0, 0.0);
        roughness = std::max(roughness, 0.05);
        double metallic = sp[1] / 255.0;

        // ==============================================================================================
        // 【正确标准PBR代码 —— 已注释】
        // ==============================================================================================
        /*
        double metallic = metallicMap.r;          // 必须有：标准金属度贴图
        double roughness = roughnessMap.g;        // 必须有：标准粗糙度贴图
        roughness = std::max(roughness, 0.05);
        */

        double NdotV = std::max(N * V, 0.001);
        double NdotL = std::max(N * l, 0.001);
        double NdotH = std::max(N * H, 0.001);
        double HdotV = std::max(H * V, 0.001);

        vec3 F0 = { 0.04, 0.04, 0.04 };

        F0 = mix(F0, albedo, metallic);

        vec3 F = fresnel(F0, HdotV);

        double D = D_GGX(NdotH, roughness);
        double G = G_Smith(NdotV, NdotL, roughness);
        vec3 specular = (F * D * G) / (4.0 * NdotV * NdotL + 0.3);
        specular = {
            std::min(specular.x, 1.0),
            std::min(specular.y, 1.0),
            std::min(specular.z, 1.0)
        };

        double Ks = (F.x + F.y + F.z) / 3.0;
        double Kd = (1.0 - Ks) * (1.0 - metallic);

        vec3 diffuse = albedo * (Kd / M_PI);
        vec3 finalRGB = (diffuse + specular) * NdotL * 5.5;

        //IBL
        if (iblIrradiance != nullptr)
        {
            vec3 N3 = { N.x, N.y, N.z };
            vec3 V3 = { V.x, V.y, V.z };
            vec3 Nn = normalized(N3);
            vec3 Vn = normalized(V3);

            // 反射方向
            vec3 R = normalized(2.0f *(Nn*Vn) * Nn - Vn);

            // 1) 漫反射：irradiance
            vec3 env_diff = iblIrradiance->sample(Nn);
            vec3 ibl_diff = env_diff * albedo * vec3({ 0.22f });

            // 2) 高光反射：prefilter
            vec3 env_spec = iblPrefilter->sample(R);
            vec3 ibl_spec = env_spec * 0.5f;

            // 3) 混合
            vec3 ibl = ibl_diff * (1.0 - metallic) + ibl_spec * metallic;

            // 4) 最终叠加
            finalRGB = finalRGB + ibl * 0.8f;
        }

        TGAColor res;
        res[2] = (uint8_t)std::min(std::max(finalRGB.x * 255.0, 0.0), 255.0);
        res[1] = (uint8_t)std::min(std::max(finalRGB.y * 255.0, 0.0), 255.0);
        res[0] = (uint8_t)std::min(std::max(finalRGB.z * 255.0, 0.0), 255.0);
        res[3] = 255;
        return res;
    }
};

struct PhongShader {
private:
    const Model& model;
    vec4 l;
public:
    PhongShader(const vec3 light, const Model& m, const GlobalMat& gloMat) : model(m) {
        l = normalized((gloMat.persp(vec4{ light.x, light.y, light.z, 0. })));
    }

    TGAColor color(triangle& tri, const vec3 bar, mat<4, 4>modelView_invert_transpose, mat<2, 4>&T) const {
        vec4 t0 = normalized(T[0]);
        vec4 t1 = normalized(T[1]);
        vec4 n_t = normalized((modelView_invert_transpose * tri.norm_gravity(bar[0], bar[1], bar[2])));
        mat<4, 4>D = { t0,t1,n_t,{0,0,0,1} };

        vec2 uv = tri.uv_gravity(bar[0], bar[1], bar[2]);

        vec4 n = normalized(D.transpose() * model.normal(uv));
        vec4 r = normalized(n * (n * l) * 2 - l);

        double ambient = .8;
        double diff = std::max(0., n * l);
        double spec = 3. * std::pow(std::max(r.z, 0.), 35) * static_cast<double>(model.specular(uv)[1]) / 255.0;

        TGAColor gl_FragColor = model.diffuse(uv);
        double all_light = (ambient + diff + spec);
        for (int channel : {0, 1, 2})
            gl_FragColor[channel] = std::min<int>(255, gl_FragColor[channel] * all_light);

        return gl_FragColor;
    }
};

class ToonShader{
private:
    TGAColor mColor;
    const Model& model;
    vec4 l; 
public:
    ToonShader(TGAColor color, const vec3 light, const Model& m, const GlobalMat& gloMat) : model(m) {
        this->mColor = color;
        l = normalized((gloMat.persp( vec4{ light.x, light.y, light.z, 0. }))); // transform the light vector to view coordinates
    }

    TGAColor color(triangle& tri, const vec3 bar, mat<4, 4>modelView_invert_transpose) const {
        vec4 n = normalized(tri.norm_gravity(bar[0], bar[1], bar[2])); // per-vertex normal interpolation
        double diffuse = std::max(0., n * l);
        double intensity = .15 + diffuse;
        if (intensity > .66) intensity = 1;
        else if (intensity > .33) intensity = .66;
        else intensity = .33;

        TGAColor gl_FragColor;
        for (int channel : {0, 1, 2})
            gl_FragColor[channel] = std::min<int>(255, mColor[channel] * intensity);
        return gl_FragColor;                           
    }
};

class SSAOShader {
    float mSSAO_RADIUS = 10.0f;
    float mSSAO_BIAS = 0.005f;
    int mSSAO_SAMPLE_COUNT=16;
    std::vector<vec3> g_ssao_samples;

    void InitSSAOSamples()
    {
        srand(12345);
        for (int i = 0; i < mSSAO_SAMPLE_COUNT; i++)
        {
            // 随机方向 x(-1~1) y(-1~1) z(0~1) → 上半球
            float x = (rand() % 1000) / 500.0f - 1.0f;
            float y = (rand() % 1000) / 500.0f - 1.0f;
            float z = (rand() % 1000) / 1000.0f;

            // 归一化 = 变成单位向量
            float len = sqrt(x * x + y * y + z * z);
            x /= len; y /= len; z /= len;

            // 越靠近中心采样越密 → 效果更自然
            float scale = (float)i / 16.0f;
            scale *= scale;
            g_ssao_samples.push_back ({ x * scale, y * scale, z * scale });
        }
    }
private:
public:
    SSAOShader(float SSAO_RADIUS,float SSAO_BIAS, int SSAO_SAMPLE_COUNT)  {
        mSSAO_RADIUS = SSAO_RADIUS;
        mSSAO_BIAS = SSAO_BIAS;
        mSSAO_SAMPLE_COUNT=SSAO_SAMPLE_COUNT;
        this->InitSSAOSamples();
    }
    //目标，给我一张可知width和height的图片上的zbuffer_ture,以及当前像素我来看看需不需要遮挡
    float AO(int width,int height,int px, int py, const vec3& pixel_nor, std::vector<double>& zbuffer_true,double z)const
    {
        float occlusion = 0.0f;
        for (int s = 0; s < mSSAO_SAMPLE_COUNT; s++)
        {
            vec3 sample_dir = g_ssao_samples[s];
            // 翻转到法线半球
            if (sample_dir * pixel_nor < 0)
                sample_dir = { -sample_dir.x, -sample_dir.y, -sample_dir.z };

            // 屏幕邻域偏移采样(软渲染极简适配，复用屏幕空间，暂时跳过世界重算哈哈我先提升一下效率再说)
            int off_x = (int)(sample_dir.x * mSSAO_RADIUS);
            int off_y = (int)(sample_dir.y * mSSAO_RADIUS);
            int spx = px + off_x;
            int spy = py + off_y;

            // 边界裁剪
            if (spx < 0 || spx >= width || spy < 0 || spy >= height) continue;

            // 直接读深度缓冲(你现成全局zbuffer)
            float sample_z = zbuffer_true[spx + spy * width];
            // 深度比对遮挡
            if ((sample_z - z) > mSSAO_BIAS)
                occlusion += 1.0f;
        }
        float ao = 1.0f - (occlusion / (float)mSSAO_SAMPLE_COUNT);
        return ao;
    }
};
inline int edge(int ax, int ay, int bx, int by, int px, int py) {
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}


void draw_shadow_zbuffer(triangle& tri,std::vector<double>& zbuffer_true, int width, int height, const GlobalMat& gloMat) {

    vec4 ndc[3] = { tri.dot[0] / tri.dot[0].w, tri.dot[1] / tri.dot[1].w, tri.dot[2] / tri.dot[2].w };
    vec2 screen[3] = { (gloMat.scale(ndc[0])).xy(), (gloMat.scale(ndc[1])).xy(), (gloMat.scale(ndc[2])).xy() };
    double space = tri.signed_triangle_area(screen);
    if (space < 0)
    {
        return;
    }
    int raw_bbminx = std::min(std::min((int)screen[0].x, (int)screen[1].x), (int)screen[2].x);
    int raw_bbminy = std::min(std::min((int)screen[0].y, (int)screen[1].y), (int)screen[2].y);
    int raw_bbmaxx = std::max(std::max((int)screen[0].x, (int)screen[1].x), (int)screen[2].x);
    int raw_bbmaxy = std::max(std::max((int)screen[0].y, (int)screen[1].y), (int)screen[2].y);

    int bbminx = std::max(0, raw_bbminx);
    int bbminy = std::max(0, raw_bbminy);
    int bbmaxx = std::min(width - 1, raw_bbmaxx);
    int bbmaxy = std::min(height - 1, raw_bbmaxy);

    if (bbminx > bbmaxx || bbminy > bbmaxy) {
        return;
    }
    int w = bbmaxx - bbminx + 1;
    int h = bbmaxy - bbminy + 1;

    int ax = (int)screen[0].x, ay = (int)screen[0].y;
    int bx = (int)screen[1].x, by = (int)screen[1].y;
    int cx = (int)screen[2].x, cy = (int)screen[2].y;

    int e0 = edge(ax, ay, bx, by, bbminx, bbminy);
    int e1 = edge(bx, by, cx, cy, bbminx, bbminy);
    int e2 = edge(cx, cy, ax, ay, bbminx, bbminy);

    int de0x = -(by - ay);
    int de1x = -(cy - by);
    int de2x = -(ay - cy);

    int de0y = (bx - ax);
    int de1y = (cx - bx);
    int de2y = (ax - cx);

#pragma omp parallel for private(ce0, ce1, ce2)
    for (int j = 0; j < h; j++) {
        int ce0 = e0 + de0y * j;
        int ce1 = e1 + de1y * j;
        int ce2 = e2 + de2y * j;
        for (int i = 0; i < w; i++) {
            bool inside =
                (ce0 >= 0 && ce1 >= 0 && ce2 >= 0) ||
                (ce0 <= 0 && ce1 <= 0 && ce2 <= 0);

            if (inside) {
                double z = (ndc[0].z * ce1 + ndc[1].z * ce2 + ndc[2].z * ce0) / (ce1 + ce2 + ce0);
                int idx = bbminx + i + (bbminy + j) * width;
                if (z >= zbuffer_true[idx])
                {
                   
                    zbuffer_true[idx] = z;

                }
            }
            ce0 += de0x;
            ce1 += de1x;
            ce2 += de2x;
        }
    }
}

//void draw_both_together(triangle& tri, const PhongShader& shader1, const ToonShader& shader2, const SSAOShader& ssaoShader, TGAImage& framebuffer, TGAImage& framebuffer_toon, std::vector<double>& zbuffer_true, int width, int height, mat<4, 4>& model_, const GlobalMat& gloMat)
void draw_both_together(triangle& tri, const PBRShader& shader1, const ToonShader& shader2, const SSAOShader& ssaoShader, TGAImage& framebuffer, TGAImage& framebuffer_toon, std::vector<double>& zbuffer_true, int width, int height, mat<4, 4>& model_, const GlobalMat& gloMat)
{

    vec4 ndc[3] = { tri.dot[0] / tri.dot[0].w, tri.dot[1] / tri.dot[1].w, tri.dot[2] / tri.dot[2].w };
    vec2 screen[3] = { (gloMat.scale(ndc[0])).xy(), (gloMat.scale(ndc[1])).xy(), (gloMat.scale(ndc[2])).xy() };
    double space = tri.signed_triangle_area(screen);
    if (space < 0)
    {
        return;
    }
    int raw_bbminx = std::min(std::min((int)screen[0].x, (int)screen[1].x), (int)screen[2].x);
    int raw_bbminy = std::min(std::min((int)screen[0].y, (int)screen[1].y), (int)screen[2].y);
    int raw_bbmaxx = std::max(std::max((int)screen[0].x, (int)screen[1].x), (int)screen[2].x);
    int raw_bbmaxy = std::max(std::max((int)screen[0].y, (int)screen[1].y), (int)screen[2].y);

    int bbminx = std::max(0, raw_bbminx);
    int bbminy = std::max(0, raw_bbminy);
    int bbmaxx = std::min(width - 1, raw_bbmaxx);
    int bbmaxy = std::min(height - 1, raw_bbmaxy);

    if (bbminx > bbmaxx || bbminy > bbmaxy) {
        return;
    }
    int w = bbmaxx - bbminx + 1;
    int h = bbmaxy - bbminy + 1;

    int ax = (int)screen[0].x, ay = (int)screen[0].y;
    int bx = (int)screen[1].x, by = (int)screen[1].y;
    int cx = (int)screen[2].x, cy = (int)screen[2].y;

    int e0 = edge(ax, ay, bx, by, bbminx, bbminy);
    int e1 = edge(bx, by, cx, cy, bbminx, bbminy);
    int e2 = edge(cx, cy, ax, ay, bbminx, bbminy);

    int de0x = -(by - ay);
    int de1x = -(cy - by);
    int de2x = -(ay - cy);

    int de0y = (bx - ax);
    int de1y = (cx - bx);
    int de2y = (ax - cx);

    mat<2, 4> E = { tri.dot[1] - tri.dot[0], tri.dot[2] - tri.dot[0] };
    mat<2, 2> U = { tri.uv[1] - tri.uv[0], tri.uv[2] - tri.uv[0] };
    mat<2, 4> T = U.invert() * E;
#pragma omp parallel for private(ce0, ce1, ce2)
    for (int j = 0; j < h; j++) {
        int ce0 = e0 + de0y * j;
        int ce1 = e1 + de1y * j;
        int ce2 = e2 + de2y * j;
        for (int i = 0; i < w; i++)
        {
            bool inside =
            (ce0 >= 0 && ce1 >= 0 && ce2 >= 0) ||
            (ce0 <= 0 && ce1 <= 0 && ce2 <= 0);
            if (inside) {
                double z = (ndc[0].z * ce1 + ndc[1].z * ce2 + ndc[2].z * ce0) / (ce1 + ce2 + ce0);
                int px = bbminx + i;
                int py = bbminy + j;
                int idx = px + py * width;
                if (z >= zbuffer_true[idx])
                {
                    //这里如果没有透视矫正模型由于不那么规律看不出来，但是地上的平面会很明显
                    double for_c = (double)ce0 / tri.dot[2].w;
                    double for_a = (double)ce1/ tri.dot[0].w;
                    double for_b = (double)ce2/ tri.dot[1].w;
                    vec3 bar = { for_a,for_b ,for_c };
                    bar = bar / (for_a + for_b + for_c);

                    vec4 raw_n4 = tri.norm_gravity(bar[0], bar[1], bar[2]);
                    vec3 pixel_nor = { raw_n4.x, raw_n4.y, raw_n4.z };
                    pixel_nor = normalized(pixel_nor);

                    TGAColor color_more_real = shader1.color(tri, bar, model_,T);

                    float ao = ssaoShader.AO(width, height, px, py, pixel_nor, zbuffer_true, z);
                    color_more_real[0] = std::min(255, (int)(color_more_real[0] * ao));
                    color_more_real[1] = std::min(255, (int)(color_more_real[1] * ao));
                    color_more_real[2] = std::min(255, (int)(color_more_real[2] * ao));

                    TGAColor color_more_real_toon = shader2.color(tri, bar, model_);

                    framebuffer.set(px, py, color_more_real);
                    framebuffer_toon.set(px, py, color_more_real_toon);
                    zbuffer_true[idx] = z;
                 }
            }
            ce0 += de0x;
            ce1 += de1x;
            ce2 += de2x;
        }
    }
}
void create_zbuffer_img(TGAImage& zbuffer_img, std::vector<double>& zbuffer_true, int width, int height) {
    double minz = +1000;
    double maxz = -1000;
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            double z = zbuffer_true[x + y * width];
            if (z < -100) continue;
            minz = std::min(z, minz);
            maxz = std::max(z, maxz);
        }
    }
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            double z = zbuffer_true[x + y * width];
            if (z < -100) continue;
            z = (z - minz) / (maxz - minz) * 255;
            unsigned char c = (unsigned char)z;
            zbuffer_img.set(x, y, { c, 255, 255, 255 });
        }
    }
}
void build_obj_triangle(const Model &model, TGAImage& framebuffer, TGAImage& zbuffer_img, TGAImage& framebuffer_toon, std::vector<double>& zbuffer_true, std::vector<double>& zbuffer_true_shadow,const RenderSettings& setting)
{
    // 1. 加载 HDR 并创建 Cubemap
    HDRImage hdr;
    hdr.load("HDR/lebombo_1k.hdr");

    Cubemap irradiance(64);
    irradiance.fromHDR(hdr); // 环境光贴图

    Cubemap prefilter(64);
    prefilter.generatePrefilter(irradiance,3); // 反射贴图

    SSAOShader ssaoShader(10.0f, 0.005f, 16);
    GlobalMat glomat;
    glomat.modelview(setting.eye, setting.center, setting.up);
    glomat.modelviewforLight(setting.light_vec, setting.center, setting.up);
    glomat.perspective(norm(setting.eye - setting.center));
    glomat.viewport(width_obj/16, height_obj/16, width_obj*7/8, height_obj*7/8);
    mat<4, 4> modelview_invert_transpose = glomat.modelview_invert_transpose();//ModelView.invert_transpose();
    
    //PhongShader shader(setting.light_vec,model,glomat);
    PBRShader shader(
        setting.light_vec,
        model,
        glomat,
        &irradiance,
        &prefilter
    );
    ToonShader toonShader(orange, setting.light_vec, model,glomat);
    //我们需要提前zbuffer让SSAO可以正确计算AO系数
    for (int i = 0; i < model.nfaces(); i++)
    {
        triangle tri(model, i,glomat,false);
        draw_shadow_zbuffer(tri, zbuffer_true, width_obj, height_obj,glomat);
        
    }
    //正式渲染整个模型(基础颜色与SSAO)
    for (int i = 0; i < model.nfaces(); i++)
    {
        triangle tri(model, i,glomat,false);
        //draw_triangle_color(tri,shader,ssaoShader,framebuffer,zbuffer_true,width_obj,height_obj);
        draw_both_together(tri, shader, toonShader, ssaoShader, framebuffer, framebuffer_toon, zbuffer_true, width_obj, height_obj,modelview_invert_transpose,glomat);
    }
    create_zbuffer_img(zbuffer_img, zbuffer_true, width_obj, height_obj);
    
    //M、N的组合技来实现位置变换
    mat<4, 4> M = glomat.M();
    //视角变换后的阴影
    for (int i = 0; i < model.nfaces(); i++)
    {
        triangle tri(model, i,glomat,true);
        draw_shadow_zbuffer(tri,  zbuffer_true_shadow, width_obj, height_obj,glomat);
    }

    mat<4, 4> N = glomat.N();//Viewport * Perspective * ModelView;
    //计算阴影区域
    std::vector<bool> mask(width_obj * height_obj, false);
    for (int x = 0; x < width_obj; x++) {
        for (int y = 0; y < height_obj; y++) {
            vec4 fragment = M * vec4{ (double)x, (double)y, zbuffer_true[x + y * width_obj], 1. };
            vec4 q = N * fragment;
            vec3 p = q.xyz() / q.w;
            bool lit = (fragment.z < -100 ||                                   // it's the background or
                (p.x < 0 || p.x >= width_obj || p.y < 0 || p.y >= height_obj) ||   // it is out of bounds of the shadow buffer
                (p.z > zbuffer_true_shadow[int(p.x) + int(p.y) * height_obj] - .03));  // it is visible
            mask[x + y * width_obj] = lit;
        }
    }
    //基于mask正式上色
    for (int x = 0; x < width_obj; x++) {
        for (int y = 0; y < height_obj; y++) {
            if (mask[x + y * width_obj]) continue;
            TGAColor c = framebuffer.get(x, y);
            vec3 a = { c[0], c[1], c[2] };
            if (norm(a) < 80) continue;
            a = normalized(a) * 80;
            framebuffer.set(x, y, { (unsigned char)a[0], (unsigned char)a[1], (unsigned char)a[2], 255 });
        }
    }

    ////基于z的简单边缘检测
    constexpr double threshold = .15;
    constexpr int Gx[3][3] = { {-1,  0,  1}, {-2, 0, 2}, {-1, 0, 1} };
    constexpr int Gy[3][3] = { {-1, -2, -1}, { 0, 0, 0}, { 1, 2, 1} };
    for (int y = 1; y < framebuffer.height() - 1; ++y) {
        for (int x = 1; x < framebuffer.width() - 1; ++x) {
            vec2 sum;
            for (int j = -1; j <= 1; ++j) {
                for (int i = -1; i <= 1; ++i) {

                    sum = sum + vec2{
                        Gx[j + 1][i + 1] * zbuffer_true[x + i + (y + j) * width_obj],
                        Gy[j + 1][i + 1] * zbuffer_true[x + i + (y + j) * width_obj]
                    };
                }
            }
            if (norm(sum) > threshold)
            {
                framebuffer_toon.set(x, y, TGAColor{ 0, 0, 0, 255 });
            }
                
        }
    }
    return;
}