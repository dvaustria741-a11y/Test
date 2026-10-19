#pragma once
#include <memory>
#include <string>
#include "gl.h"
#include "vmath.h"
#include "pak.h"
#include "world.h"
#include "chunks.h"
#include "renderer.h"
#include "quality.h"

class Game {
public:
    Game();
    ~Game();
    bool initGL(int w, int h, const char* glRenderer);
    void termGL();
    void resize(int w, int h);
    void frame();
    void touch(int action, int id, float x, float y);
    int targetFps() const { return q_.targetFps; }
    bool glReady() const { return glReady_; }
    bool loaded() const { return pakOk_; }

private:
    struct Touch {
        int id = -1;
        int role = 0;
        float sx = 0, sy = 0, x = 0, y = 0, lx = 0, ly = 0;
    };
    enum { R_NONE = 0, R_JOY = 1, R_LOOK = 2, R_JUMP = 3, R_GEAR = 4, R_MENU = 5 };

    void applyQuality(int tier);
    void loadSettings();
    void saveSettings();
    void update(float dt);
    void adaptQuality(float dt);
    void drawUI();
    void drawLoading();
    void drawDigit(int d, float x, float y, float h, float r, float g, float b, float a);
    void drawNumber(int n, float x, float y, float h);
    bool inCircle(float x, float y, float cx, float cy, float r) const;
    void layout();
    int menuHit(float x, float y) const;

    World world_;
    Pak pak_;
    ModelIds ids_;
    bool pakOk_ = false;
    std::unique_ptr<ChunkManager> cm_;
    Renderer rend_;
    bool glReady_ = false;
    int W_ = 0, H_ = 0;
    Quality q_ = qualityPreset(1);
    int mode_ = 0;
    int detectedTier_ = 1;
    int curTier_ = 1;
    float renderScale_ = 0.8f;

    V3 pos_{0, 0, 0};
    float vy_ = 0, yaw_ = 0.6f, pitch_ = -0.05f;
    bool onGround_ = true;
    float bob_ = 0, bobT_ = 0;
    float velX_ = 0, velZ_ = 0;
    float joyX_ = 0, joyY_ = 0;
    bool jumpReq_ = false;
    bool menuOpen_ = false;
    bool showFps_ = true;

    Touch touches_[10];
    float joyBaseX_ = 0, joyBaseY_ = 0;
    float joyCX_ = 0, joyCY_ = 0, joyR_ = 0;
    float jumpCX_ = 0, jumpCY_ = 0, jumpR_ = 0;
    float gearCX_ = 0, gearCY_ = 0, gearR_ = 0;
    float u_ = 1.f;

    bool loading_ = true;
    double loadStart_ = 0;
    double lastTime_ = 0;
    float time_ = 0;
    float fpsAvg_ = 60.f;
    float dtAcc_ = 0;
    int dtCount_ = 0;
    float lowTimer_ = 0, highTimer_ = 0, atMinTimer_ = 0;
    int shownFps_ = 0;
    float fpsTimer_ = 0;
    int fpsFrames_ = 0;
    GLuint texJoyBase_ = 0, texJoyKnob_ = 0, texJump_ = 0, texGear_ = 0, texTitle_ = 0;
    bool uiTexTried_ = false;
};
