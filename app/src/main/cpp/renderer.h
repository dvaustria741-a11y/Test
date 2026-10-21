#pragma once
#include <vector>
#include "gl.h"
#include "vmath.h"
#include "pak.h"
#include "quality.h"
#include "chunks.h"

struct Camera {
    V3 pos;
    float yaw, pitch, fov, aspect;
};

struct Atmos {
    V3 lightDir{0.42f, 0.78f, 0.46f};
    V3 sunCol{0.78f, 0.68f, 0.50f};
    V3 ambTop{0.56f, 0.66f, 0.80f};
    V3 ambBot{0.40f, 0.38f, 0.30f};
    V3 fogCol{0.84f, 0.92f, 0.98f};
    V3 zenith{0.36f, 0.62f, 0.95f};
    V3 sunDisc{0.42f, 0.78f, 0.46f};
    V3 moonDir{-0.42f, 0.78f, -0.46f};
    float night = 0.f;
};

struct PLight {
    V3 pos{0, 0, 0};
    float radius = 1.f;
    V3 col{0, 0, 0};
};

struct ViewModelState {
    bool on = false;
    int model = -1, fire = -1;
    V3 pos{0, 0, 0};
    float yaw = 0, pitch = 0, roll = 0, scale = 1;
};

struct GPUInst {
    float x, y, z, s, yaw;
    uint32_t tint;
};

struct InstanceLists {
    std::vector<std::vector<GPUInst>> g;
    std::vector<int> active;
    std::vector<size_t> off;
    size_t total = 0;
    void init(size_t n) { g.assign(n, {}); off.assign(n, 0); active.clear(); total = 0; }
    void clear() {
        for (int i : active) g[(size_t)i].clear();
        active.clear();
        total = 0;
    }
    void add(size_t idx, const GPUInst& in) {
        if (g[idx].empty()) active.push_back((int)idx);
        g[idx].push_back(in);
        total++;
    }
};

struct ProgProps {
    GLuint prog = 0;
    GLint uVP = -1, uLightVP = -1, uTime = -1, uSway = -1, uTex = -1, uShadow = -1, uMask = -1, uTintBoost = -1, uAOMin = -1, uSoft = -1;
    GLint uPitch = -1, uRoll = -1, uVCol = -1, uGhost = -1, uLP = -1, uLC = -1;
    GLint uSunDir = -1, uSunCol = -1, uAmbTop = -1, uAmbBot = -1, uFogCol = -1, uCam = -1, uFogK = -1, uShadowOn = -1, uShadowTexel = -1;
};

struct ProgTerrain {
    GLuint prog = 0;
    GLint uVP = -1, uLightVP = -1, uShadow = -1, uSunDir = -1, uSunCol = -1, uAmbTop = -1, uAmbBot = -1;
    GLint uFogCol = -1, uCam = -1, uFogK = -1, uShadowOn = -1, uShadowTexel = -1, uLP = -1, uLC = -1;
};

struct ProgSky {
    GLuint prog = 0;
    GLint uF = -1, uR = -1, uU = -1, uTanY = -1, uAspect = -1, uTime = -1, uSunDir = -1, uClouds = -1, uFogCol = -1, uZen = -1, uMoonDir = -1, uNight = -1;
};

struct ProgUI {
    GLuint prog = 0;
    GLint uRect = -1, uScreen = -1, uColor = -1, uShape = -1, uParam = -1, uTex = -1, uUV = -1, uSize = -1;
};

class Renderer {
public:
    bool init(const Pak& pak, const Quality& q, int w, int h);
    void destroy();
    void resize(int w, int h);
    void setQuality(const Quality& q);
    void render(const Camera& cam, ChunkManager& cm, float time, float renderScale);
    void setAtmos(const Atmos& a) { atmos_ = a; }
    void setLights(const PLight* l, int n) { nLights_ = n > 2 ? 2 : n; for (int i = 0; i < 2; i++) lights_[i] = i < nLights_ ? l[i] : PLight(); }
    void setExtras(const std::vector<PropInst>* v) { extras_ = v; }
    void setDynamic(const std::vector<PropInst>* v) { dynamic_ = v; }
    void setGhost(bool on, const PropInst& p, float r, float g, float b) { ghostOn_ = on; ghost_ = p; ghostCol_[0] = r; ghostCol_[1] = g; ghostCol_[2] = b; }
    void setViewModel(const ViewModelState& v) { vm_ = v; }
    void uiBegin();
    void uiShape(int shape, float x, float y, float w, float h, float r, float g, float b, float a, float param = 0.f, GLuint tex = 0);
    void uiEnd();
    GLuint loadUITexture(const char* path, int* outW = nullptr, int* outH = nullptr);
    void uiSprite(GLuint tex, float x, float y, float w, float h, float r, float g, float b, float a, float u0, float v0, float u1, float v1);
    void uiText(const char* s, float x, float y, float h, float r, float g, float b, float a, int align, bool shadow = true);
    float textWidth(const char* s, float h) const;
    int width() const { return W_; }
    int height() const { return H_; }
    const Quality& quality() const { return q_; }
    int lastDrawCalls() const { return lastDrawCalls_; }
    int lastInstances() const { return lastInstances_; }

private:
    bool buildPrograms();
    bool uploadPak();
    void loadTextures();
    void freeTextures();
    void createTargets();
    void freeTargets();
    void collect(const Camera& cam, const Frustum& fr, ChunkManager& cm);
    void collectShadow(const Camera& cam, ChunkManager& cm);
    void drawGroups(const InstanceLists& L, bool shadow);

    const Pak* pak_ = nullptr;
    Quality q_{};
    int W_ = 0, H_ = 0;
    ProgProps props_, propsShadow_;
    ProgTerrain terrain_;
    ProgSky sky_;
    ProgUI ui_;
    GLuint emptyVao_ = 0;
    GLuint vbo_ = 0, ibo_ = 0;
    std::vector<std::vector<std::vector<GLuint>>> vaos_;
    GLuint instVbo_[2] = {0, 0};
    int instRing_ = 0;
    size_t instCap_ = 90000;
    std::vector<GLuint> tex_;
    std::vector<float> matSway_, matBoost_, matAO_, matSoft_, matVcol_;
    Atmos atmos_;
    PLight lights_[2];
    int nLights_ = 0;
    const std::vector<PropInst>* extras_ = nullptr;
    const std::vector<PropInst>* dynamic_ = nullptr;
    bool ghostOn_ = false;
    PropInst ghost_{};
    float ghostCol_[3] = {0, 1, 0};
    ViewModelState vm_;
    InstanceLists vmLists_, ghostLists_;
    GLuint fontTex_ = 0;
    void buildFont();
    std::vector<int> matMask_, matDouble_;
    int rocksMat_ = -1, rocksDesertMat_ = -1;
    GLuint sceneFbo_ = 0, sceneColor_ = 0, sceneDepth_ = 0;
    int sceneW_ = 0, sceneH_ = 0;
    GLuint shadowFbo_ = 0, shadowTex_ = 0;
    int shadowRes_ = 0;
    InstanceLists lists_, shadowLists_;
    std::vector<GPUInst> scratch_;
    Mat4 lightVP_{};
    float time_ = 0;
    V3 camPos_{};
    float fogK_ = 0.01f;
    bool shadowActive_ = false;
    Mat4 lightRaw_{};
    int lastDrawCalls_ = 0;
    int lastInstances_ = 0;
    int rocksIdxDummy_ = 0;
};
