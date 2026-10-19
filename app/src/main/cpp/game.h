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
    int targetFps() const { return screen_ == S_PLAY ? q_.targetFps : 60; }
    bool glReady() const { return glReady_; }

private:
    enum Screen { S_SPLASH = 0, S_MENU, S_SETTINGS, S_PLAY, S_PAUSE };
    struct Touch {
        int id = -1;
        int role = 0;
        float sx = 0, sy = 0, x = 0, y = 0, lx = 0, ly = 0;
    };
    enum { R_NONE = 0, R_JOY = 1, R_LOOK = 2, R_JUMP = 3, R_GEAR = 4 };

    void applyQuality(int tier);
    void loadSettings();
    void saveSettings();
    void updatePlay(float dt);
    void adaptQuality(float dt);
    void go(Screen s);
    void enterScreen();
    V3 streamPos() const;
    Camera menuCamera() const;
    Camera playCamera() const;
    void drawSplash(float dt);
    void drawMenu();
    void drawSettings();
    void drawPause();
    void drawHUD();
    void drawLeaves(float alpha);
    void drawSky();
    void drawLogoImage(float x, float y, float w, float a);
    void drawPanel(float x, float y, float w, float h, float a);
    bool button(float x, float y, float w, float h, const char* label, bool primary, float alpha);
    int segmented(float x, float y, float w, float h, const char* const* labels, int n, int sel);
    bool uiInside(float px, float py, float x, float y, float w, float h) const;
    bool clickIn(float x, float y, float w, float h) const;
    void uiTouch(int action, int id, float x, float y);
    void layout();
    void clearTouches();

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
    int joyMode_ = 0;
    int lookSens_ = 1;
    int detectedTier_ = 1;
    int curTier_ = 1;
    float renderScale_ = 0.8f;

    Screen screen_ = S_SPLASH;
    Screen nextScreen_ = S_MENU;
    Screen settingsFrom_ = S_MENU;
    int trans_ = 0;
    float transT_ = 0, fade_ = 0, screenT_ = 0, menuT_ = 0;
    float shownProg_ = 0;

    V3 spawn_{0, 0, 0};
    V3 pos_{0, 0, 0};
    float vy_ = 0, yaw_ = 0.6f, pitch_ = -0.05f;
    bool onGround_ = true;
    float bob_ = 0, bobT_ = 0;
    float velX_ = 0, velZ_ = 0;
    float joyX_ = 0, joyY_ = 0;
    bool jumpReq_ = false;
    bool showFps_ = true;

    Touch touches_[10];
    float joyBaseX_ = 0, joyBaseY_ = 0;
    float joyCX_ = 0, joyCY_ = 0, joyR_ = 0;
    float jumpCX_ = 0, jumpCY_ = 0, jumpR_ = 0;
    float gearCX_ = 0, gearCY_ = 0, gearR_ = 0;
    float u_ = 1.f;

    int uiPtr_ = -1;
    bool uiDown_ = false, uiClick_ = false;
    float uiX_ = 0, uiY_ = 0, uiSX_ = 0, uiSY_ = 0, uiCX_ = 0, uiCY_ = 0;

    double loadStart_ = 0;
    double lastTime_ = 0;
    float time_ = 0;
    float dtAcc_ = 0;
    int dtCount_ = 0;
    float atMinTimer_ = 0;
    int shownFps_ = 0;
    float fpsTimer_ = 0;
    int fpsFrames_ = 0;
    GLuint texJoyBase_ = 0, texJoyKnob_ = 0, texJump_ = 0, texGear_ = 0, texLogo_ = 0, texBtn_ = 0, texPanel_ = 0;
    int logoW_ = 0, logoH_ = 0, btnW_ = 0, btnH_ = 0;
};
