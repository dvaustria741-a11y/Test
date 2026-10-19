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
    GLint uVP = -1, uLightVP = -1, uTime = -1, uSway = -1, uTex = -1, uShadow = -1, uMask = -1, uTintBoost = -1;
    GLint uSunDir = -1, uSunCol = -1, uAmbTop = -1, uAmbBot = -1, uFogCol = -1, uCam = -1, uFogK = -1, uShadowOn = -1, uShadowTexel = -1;
};

struct ProgTerrain {
    GLuint prog = 0;
    GLint uVP = -1, uLightVP = -1, uShadow = -1, uSunDir = -1, uSunCol = -1, uAmbTop = -1, uAmbBot = -1;
    GLint uFogCol = -1, uCam = -1, uFogK = -1, uShadowOn = -1, uShadowTexel = -1;
};

struct ProgSky {
    GLuint prog = 0;
    GLint uF = -1, uR = -1, uU = -1, uTanY = -1, uAspect = -1, uTime = -1, uSunDir = -1, uClouds = -1, uFogCol = -1;
};

struct ProgUI {
    GLuint prog = 0;
    GLint uRect = -1, uScreen = -1, uColor = -1, uShape = -1, uParam = -1, uTex = -1;
};

class Renderer {
public:
    bool init(const Pak& pak, const Quality& q, int w, int h);
    void destroy();
    void resize(int w, int h);
    void setQuality(const Quality& q);
    void render(const Camera& cam, ChunkManager& cm, float time, float renderScale);
    void uiBegin();
    void uiShape(int shape, float x, float y, float w, float h, float r, float g, float b, float a, float param = 0.f, GLuint tex = 0);
    void uiEnd();
    GLuint loadUITexture(const char* path);
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
    std::vector<float> matSway_, matBoost_;
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
