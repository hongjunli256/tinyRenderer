#include"triangle.h"
#include "cubemap.h"

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
        ModelView_for_Light = mat<4, 4>{ {{l.x,l.y,l.z,0}, {m.x,m.y,m.z,0}, {n.x,n.y,n.z,0}, {0,0,0,1}} };//*mat<4, 4>{{{1, 0, 0, -center.x}, { 0,1,0,-center.y }, { 0,0,1,-center.z }, { 0,0,0,1 }}};
        //我现在用的是方向面光
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
class MyShader {
public:
    virtual TGAColor color(triangle& tri, const vec3& bar, const mat<4, 4>& modelView_invert_transpose, const mat<2, 4>& T)const = 0;
    virtual ~MyShader() = default; // 虚析构，多态delete安全
};
class PBRShader :public MyShader{
private:
    const Model& model;
    vec4 l; // 视图空间光源方向
    const Cubemap* iblIrradiance = nullptr;
    const Cubemap* iblPrefilter = nullptr;
    const TGAImage* brdfLUT = nullptr;

    vec3 mix(const vec3& a, const vec3& b, double t) const {
        return a * (1.0 - t) + b * t;
    }

    // Schlick‑Fresnel F0 -> F(cosTheta)
    vec3 fresnel(const vec3& F0, double cosTheta) const {
        float a = static_cast<float>(1.0 - cosTheta);
        float a2 = a * a;
        float a4 = a2 * a2;
        float a5 = a4 * a;
        return F0 + (vec3{ 1.0,1.0,1.0 } - F0) * a5;
    }

    // GGX NDF
    double D_GGX(double NdotH, double roughness) const {
        double a = roughness * roughness;
        double a2 = a * a;
        double NdotH2 = NdotH * NdotH;
        double denom = NdotH2 * (a2 - 1.0) + 1.0;
        return a2 / (M_PI * denom * denom);
    }

    // Schlick‑GGX几何遮蔽
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
        const TGAImage* brdf = nullptr)
        : model(m), iblIrradiance(irradiance), iblPrefilter(prefilter), brdfLUT(brdf)
    {
        // light:世界空间方向，旋转到视图空间
        l = normalized(gloMat.rot(vec4{ light.x, light.y, light.z, 0.0 }, false));
    }
    TGAColor color(triangle& tri, const vec3& bar, const mat<4, 4>& modelView_invert_transpose,const mat<2, 4>& T) const
    {

        vec4 t0 = normalized(T[0]);
        vec4 t1 = normalized(T[1]);
        vec4 n_t = normalized(modelView_invert_transpose * tri.norm_gravity(bar[0], bar[1], bar[2]));

        // Gram‑Schmidt 正交化 TBN
        t0 = normalized(t0 - (t0 * n_t) * n_t);
        t1 = normalized(t1 - (t1 * n_t) * n_t - (t1 * t0) * t0);

        // D_mat: 切线空间 -> 视图空间，列向量：T,B,N
        mat<4, 4> D_mat = { t0, t1, n_t, vec4{0,0,0,1} };

        vec2 uv = tri.uv_gravity(bar[0], bar[1], bar[2]);

        // 法线贴图采样 [0‑255]
        vec4 normalTex = model.normal(uv);
        vec4 N_tangent = vec4{
            normalTex.x / 255.0 * 2.0 - 1.0,
            normalTex.y / 255.0 * 2.0 - 1.0,
            normalTex.z / 255.0 * 2.0 - 1.0,
            0.0
        };
        // 如果你的法线贴图Y是翻转的，打开下面这行
        //N_tangent.y = -N_tangent.y;

        N_tangent = normalized(N_tangent);
        vec4 N = normalized(D_mat * N_tangent);

        // ========== 视图空间视线向量：表面指向相机，看向‑Z ==========
        vec4 V = { 0, 0, -1, 0 };
        vec4 H = normalized(V + l);

        TGAColor dif = model.diffuse(uv);
        TGAColor sp = model.specular(uv);

        // albedo BGR→RGB
        vec3 albedo = {
            dif[2] / 255.0,
            dif[1] / 255.0,
            dif[0] / 255.0
        };

        // spec贴图约定: G=metallic, B=1‑roughness
        double roughness = 1.0 - std::max(sp[2] / 255.0, 0.0);
        roughness = std::max(roughness, 0.05);
        double metallic = sp[1] / 255.0;

        // 限制下界，避免除0
        double NdotV = std::max(N * V, 0.0001);
        double NdotL = std::max(N * l, 0.0);
        double NdotH = std::max(N * H, 0.0001);
        double HdotV = std::max(H * V, 0.0001);


        vec3 F0 = { 0.04, 0.04, 0.04 };
        F0 = mix(F0, albedo, metallic);

        vec3 F = fresnel(F0, HdotV);
        double D = D_GGX(NdotH, roughness);
        double G = G_Smith(NdotV, NdotL, roughness);

        // 物理正确分母，移除+0.3错误偏移
        double denom = 4.0 * NdotV * NdotL;
        vec3 specular = F * (D * G) / std::max(denom, 0.0001);

        // 限制高光数值，防止爆炸
        specular.x = std::min(specular.x, 2.0);
        specular.y = std::min(specular.y, 2.0);
        specular.z = std::min(specular.z, 2.0);

        double Ks = (F.x + F.y + F.z) / 3.0;
        double Kd = (1.0 - Ks) * (1.0 - metallic);
        vec3 diffuse = albedo * (Kd / M_PI);

        // 直接光照
        vec3 finalRGB = (diffuse + specular) * NdotL*5.5;

        if (iblIrradiance != nullptr)
        {
            vec3 N3 = { N.x, N.y, N.z };
            vec3 env_diff = iblIrradiance->sample(N3);

            const float envExposure = 2.5f;
            vec3 ibl_diff = mul(env_diff, albedo) * (1.0f / (float)M_PI) * envExposure * (1.0f - metallic);
            finalRGB = finalRGB + ibl_diff;
        }

        // 简单clamp，暂时关闭Reinhard，方便调试
        finalRGB.x = finalRGB.x < 0.0 ? 0.0 : (finalRGB.x > 1.0 ? 1.0 : finalRGB.x);
        finalRGB.y = finalRGB.y < 0.0 ? 0.0 : (finalRGB.y > 1.0 ? 1.0 : finalRGB.y);
        finalRGB.z = finalRGB.z < 0.0 ? 0.0 : (finalRGB.z > 1.0 ? 1.0 : finalRGB.z);

        TGAColor res;
        res[2] = static_cast<uint8_t>(finalRGB.x * 255.0);
        res[1] = static_cast<uint8_t>(finalRGB.y * 255.0);
        res[0] = static_cast<uint8_t>(finalRGB.z * 255.0);
        res[3] = 255;

        return res;
    }
};

struct PhongShader : public MyShader {
private:
    const Model& model;
    vec4 l;
public:
    PhongShader(const vec3 light, const Model& m, const GlobalMat& gloMat) : model(m) {
        l = normalized(gloMat.rot(vec4{ light.x, light.y, light.z, 0.0 }, false));
    }
    TGAColor color(triangle& tri, const vec3& bar, const mat<4, 4>& modelView_invert_transpose, const mat<2, 4>& T) const{
        vec4 t0 = normalized(T[0]);
        vec4 t1 = normalized(T[1]);
        vec4 n_t = normalized((modelView_invert_transpose * tri.norm_gravity(bar[0], bar[1], bar[2])));

        // ========== 新增 Gram‑Schmidt 正交化 ==========
        t0 = normalized(t0 - (t0 * n_t) * n_t);
        // t1：既要垂直 n_t，又要垂直已经修正后的 t0，再归一化
        t1 = normalized(t1 - (t1 * n_t) * n_t - (t1 * t0) * t0);

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

class ToonShader :public MyShader {
private:
    TGAColor mColor;
    const Model& model;
    vec4 l; 
public:
    ToonShader(TGAColor color, const vec3 light, const Model& m, const GlobalMat& gloMat) : model(m) {
        this->mColor = color;
        l = normalized(gloMat.rot(vec4{ light.x, light.y, light.z, 0.0 }, false));
    }

    TGAColor color(triangle& tri, const vec3& bar, const mat<4, 4>& model_inv_tp, const mat<2, 4>& T) const {
        vec4 raw_n = tri.norm_gravity(bar[0], bar[1], bar[2]);
        vec4 n_view = normalized(model_inv_tp * raw_n);
        double diffuse = std::max(0., n_view * l);
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
    float mSSAO_RADIUS = 1.0f;     // !!!现在是view空间物理半径，不再是像素
    float mSSAO_BIAS = 0.02f;      // view‑space物理bias，米单位
    int mSSAO_SAMPLE_COUNT = 16;
    std::vector<vec3> g_ssao_samples;

    mat<4,4> m_proj;
    mat<4,4> m_invProj;
    bool m_hasProj = false;

    static float fractf(float v) { return v - floorf(v); }
    static float hash(float x, float y)
    {
        return fractf(sin(x * 12.9898f + y * 78.233f) * 43758.5453f);
    }

    void InitSSAOSamples()
    {
        srand(12345);
        g_ssao_samples.clear();
        for (int i = 0; i < mSSAO_SAMPLE_COUNT; i++)
        {
            float x = (rand() % 1000) / 500.0f - 1.0f;
            float y = (rand() % 1000) / 500.0f - 1.0f;
            float z = (rand() % 1000) / 1000.0f;

            float len = sqrt(x * x + y * y + z * z);
            x /= len; y /= len; z /= len;

            float scale = (float)i / (float)mSSAO_SAMPLE_COUNT;
            scale *= scale;
            g_ssao_samples.push_back({ x * scale, y * scale, z * scale });
        }
    }

    // px,py:屏幕像素; width,height:分辨率; ndc_z:[-1,1] 输出view空间位置
    vec3 ndcToView(int px, int py, int width, int height, double ndc_z) const
    {
        float nx = 2.0f * float(px) / float(width) - 1.0f;
        float ny = 1.0f - 2.0f * float(py) / float(height);
        vec4 ndc{ nx, ny, (float)ndc_z, 1.0f };
        vec4 clip = m_invProj * ndc;
        float invW = 1.0f / clip.w;
        return { clip.x * invW, clip.y * invW, clip.z * invW };
    }

    // view空间点投影回屏幕像素
    bool viewToNdc(const vec3& viewPt, int width, int height, int& outPx, int& outPy) const
    {
        vec4 clip = m_proj * vec4{ viewPt.x,viewPt.y,viewPt.z,1.0f };
        if (fabs(clip.w) < 1e-6f) return false;
        float invW = 1.0f / clip.w;
        float nx = clip.x * invW;
        float ny = clip.y * invW;
        if (nx < -1.0f || nx > 1.0f || ny < -1.0f || ny > 1.0f)
            return false;

        outPx = int(((nx + 1.0f) * 0.5f) * float(width));
        outPy = int(((1.0f - ny) * 0.5f) * float(height));
        return true;
    }

private:
public:
    SSAOShader(float SSAO_RADIUS, float SSAO_BIAS, int SSAO_SAMPLE_COUNT)
    {
        mSSAO_RADIUS = SSAO_RADIUS;
        mSSAO_BIAS = SSAO_BIAS;
        mSSAO_SAMPLE_COUNT = SSAO_SAMPLE_COUNT;
        InitSSAOSamples();
    }

    // 每一帧设置投影矩阵，必须调用！
    void SetProjection(const mat<4,4>& proj, const mat<4,4>& invProj)
    {
        m_proj = proj;
        m_invProj = invProj;
        m_hasProj = true;
    }

    // AO对外接口完全不变！
    double AO(int width, int height, int px, int py, const vec3& pixel_nor, const std::vector<double>& zbuffer_true, double z)const
    {
        if (!m_hasProj) return 1.0;

        // 当前像素：NDC → view空间位置
        vec3 viewPos = ndcToView(px, py, width, height, z);

        // 像素随机旋转扰动
        float rnd = hash((float)px, (float)py);
        float rotAngle = rnd * 2.0f * (float)M_PI;
        float rotCos = cosf(rotAngle);
        float rotSin = sinf(rotAngle);

        float occlusion = 0.0f;

        for (int s = 0; s < mSSAO_SAMPLE_COUNT; s++)
        {
            vec3 sample_dir = g_ssao_samples[s];

            // 样本在切线平面旋转
            float rx = sample_dir.x * rotCos - sample_dir.y * rotSin;
            float ry = sample_dir.x * rotSin + sample_dir.y * rotCos;
            sample_dir.x = rx;
            sample_dir.y = ry;

            // 翻转到法线半球
            if (sample_dir* pixel_nor< 0.0f)
            {
                sample_dir = { -sample_dir.x, -sample_dir.y, -sample_dir.z };
            }

            // view‑space采样点：物理半径偏移
            vec3 sampleViewPos = viewPos + sample_dir * mSSAO_RADIUS;

            // 投影回屏幕，得到采样像素坐标
            int spx, spy;
            if (!viewToNdc(sampleViewPos, width, height, spx, spy))
                continue;
            if (spx < 0 || spx >= width || spy < 0 || spy >= height)
                continue;

            int idx_sp = spx + spy * width;
            double sampleNdcZ = zbuffer_true[idx_sp];
            if (sampleNdcZ < -100.0)
                continue;

            // 采样点NDC再次反算得到采样点viewPos
            vec3 sampleViewReal = ndcToView(spx, spy, width, height, sampleNdcZ);

            // 真实view空间深度差
            float deltaViewZ = sampleViewReal.z - viewPos.z;

            // 平滑权重，物理bias，不再和NDC耦合
            float weight = std::clamp((deltaViewZ - mSSAO_BIAS) / (mSSAO_RADIUS * 0.25f), 0.0f, 1.0f);

            // 三维空间距离衰减
            vec3 diff = sampleViewReal - viewPos;
            float distSq = diff*diff;
            float falloff = 1.0f / (1.0f + distSq);

            occlusion += weight * falloff;
        }

        double avgOcc = occlusion / (double)mSSAO_SAMPLE_COUNT;
        double ao = 1.0 - std::clamp(avgOcc * 1.3, 0.0, 1.0);
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
                double denom = ce1 + ce2 + ce0;
                if (std::fabs(denom) < 1e-12)
                    continue;
                double z = (ndc[0].z * ce1 + ndc[1].z * ce2 + ndc[2].z * ce0) / (ce1 + ce2 + ce0);
                int idx = bbminx + i + (bbminy + j) * width;
                if (z > zbuffer_true[idx])
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
//加载一次模型同时渲染toon和普通模型
//void draw_both_together(triangle& tri, const PhongShader& shader1, const ToonShader& shader2, TGAImage& framebuffer, TGAImage& framebuffer_toon, std::vector<double>& zbuffer_true, std::vector<vec3>& norm_buf, int width, int height, mat<4, 4>& model_, const GlobalMat& gloMat)
void draw_both_together(triangle& tri, const MyShader& shader1, const MyShader& shader2, TGAImage& framebuffer, TGAImage& framebuffer_toon, std::vector<double>& zbuffer_true, std::vector<vec3>& norm_buf, int width, int height, mat<4, 4>& model_, const GlobalMat& gloMat)
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
                    double for_a = (double)ce1 / tri.dot[0].w;
                    double for_b = (double)ce2 / tri.dot[1].w;
                    double for_c = (double)ce0 / tri.dot[2].w;
                    vec3 bar = { for_a,for_b ,for_c };
                    double sum = for_a + for_b + for_c;
                    if (sum < 1e-12) sum = 1e-12;
                    bar = bar / sum;

                    vec4 raw_n4 = tri.norm_gravity(bar[0], bar[1], bar[2]);
                    vec4 view_n4 = normalized(model_ * raw_n4);
                    vec3 pixel_nor = { view_n4.x, view_n4.y, view_n4.z };
                    norm_buf[idx] = pixel_nor;

                    TGAColor color_more_real = shader1.color(tri, bar, model_, T);
                    TGAColor color_more_real_toon = shader2.color(tri, bar,model_,T);

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
void build_obj_triangle(const Model &model, TGAImage& framebuffer, TGAImage& zbuffer_img, TGAImage& framebuffer_toon, std::vector<double>& zbuffer_true, std::vector<double>& zbuffer_true_shadow, std::vector<vec3>& norm_buf,const RenderSettings& setting)
{

    GlobalMat glomat;
    glomat.modelview(setting.eye, setting.center, setting.up);
    glomat.modelviewforLight(setting.light_vec, setting.center, setting.up);
    glomat.perspective(norm(setting.eye - setting.center));
    glomat.viewport(width_obj/16, height_obj/16, width_obj*7/8, height_obj*7/8);
    mat<4, 4> modelview_invert_transpose = glomat.modelview_invert_transpose();
    //三shader
    PhongShader shader_phong(setting.light_vec,model,glomat);
    PBRShader shader_pbr(
        setting.light_vec,
        model,
        glomat,
        &setting.irradiance,
        &setting.prefilter
    );
    ToonShader shader_toon(orange, setting.light_vec, model,glomat);
    //多态哈哈
    MyShader& shader = shader_phong;
    MyShader& toonShader = shader_toon;

    for (int i = 0; i < model.nfaces(); i++)
    {
        triangle tri(model, i,glomat,false);
        draw_both_together(tri, shader, toonShader, framebuffer, framebuffer_toon, zbuffer_true, norm_buf,width_obj, height_obj,modelview_invert_transpose,glomat);
    }

    SSAOShader ssaoShader(10.0f, 0.005f, 16);
#pragma omp parallel for
    for (int py = 0; py < height_obj; py++)
    {
        for (int px = 0; px < width_obj; px++)
        {
            int idx = px + py * width_obj;
            double z_val = zbuffer_true[idx];
            if (z_val < -100.0) continue;

            const vec3& n = norm_buf[idx];
            float ao = ssaoShader.AO(width_obj, height_obj, px, py, n, zbuffer_true, z_val);

            TGAColor c = framebuffer.get(px, py);
            c[0] = static_cast<uint8_t>(std::clamp((float)c[0] * ao, 0.0f, 255.0f));
            c[1] = static_cast<uint8_t>(std::clamp((float)c[1] * ao, 0.0f, 255.0f));
            c[2] = static_cast<uint8_t>(std::clamp((float)c[2] * ao, 0.0f, 255.0f));
            framebuffer.set(px, py, c);
        }
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
            int idx_zbuffer = x + y * width_obj;
            vec4 fragment = M * vec4{ (double)x, (double)y, zbuffer_true[idx_zbuffer], 1. };
            vec4 q = N * fragment;
            vec3 p = q.xyz() / q.w;
            bool lit;
            if (fragment.z < -100)
            {
                lit = true;
            }
            else if (p.x < 0.0 || p.x >= width_obj || p.y < 0.0 || p.y >= height_obj)
            {
                lit = false; //正交面光，超出shadowmap视为阴影
            }
            else
            {
                int sx = static_cast<int>(p.x);
                int sy = static_cast<int>(p.y);
                lit = (p.z > zbuffer_true_shadow[sx + sy * width_obj] - 0.03);
            }
            mask[idx_zbuffer] = lit;
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
                    int idx_zbuffer = (x + i) + (y + j) * width_obj;
                    sum = sum + vec2{
                         
                        Gx[j + 1][i + 1] * zbuffer_true[idx_zbuffer],
                        Gy[j + 1][i + 1] * zbuffer_true[idx_zbuffer]
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