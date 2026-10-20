#pragma once
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include "gl.h"
#include "vmath.h"
#include "pak.h"
#include "world.h"
#include "chunks.h"
#include "renderer.h"
#include "quality.h"
#include "survival.h"

struct Placed {
    int def;
    float x, y, z, yaw;
};

struct Popup {
    char text[32];
    float t;
    float r, g, b;
};

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
    enum Screen { S_SPLASH = 0, S_MENU, S_SETTINGS, S_PLAY, S_PAUSE, S_BAG, S_BUILD, S_DEAD };
    struct Touch {
        int id = -1;
        int role = 0;
        float sx = 0, sy = 0, x = 0, y = 0, lx = 0, ly = 0;
    };
    enum { R_NONE = 0, R_JOY = 1, R_LOOK = 2, R_JUMP = 3, R_ACT = 4 };

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
    V3 camForward() const;
    bool hitCircle(float x, float y, float cx, float cy, float r) const;
    void layout();
    void clearTouches();
    void slotRect(int i, float& x, float& y, float& w, float& h) const;

    void drawSplash(float dt);
    void drawMenu();
    void drawSettings();
    void drawPause();
    void drawHUD();
    void drawBag();
    void drawBuildMenu();
    void drawDead();
    void drawLeaves(float alpha);
    void drawSky();
    void drawLogoImage(float x, float y, float w, float a);
    void drawPanel(float x, float y, float w, float h, float a);
    bool button(float x, float y, float w, float h, const char* label, bool primary, float alpha);
    bool smallButton(float cx, float cy, float r, const char* label, float alpha);
    int segmented(float x, float y, float w, float h, const char* const* labels, int n, int sel);
    bool uiInside(float px, float py, float x, float y, float w, float h) const;
    bool clickIn(float x, float y, float w, float h) const;
    void uiTouch(int action, int id, float x, float y);
    void bar(float x, float y, float w, float h, float frac, float r, float g, float b);

    void initSurvival();
    void updateSurvival(float dt);
    void computeAtmos(Atmos& a) const;
    void updateLights(const Camera& cam);
    void updateViewModel(const Camera& cam);
    void rebuildHotbar();
    int currentItem() const;
    void doAct();
    void updateTarget();
    void addPopup(const char* text, float r, float g, float b);
    void giveItem(int item, int n);
    bool canAfford(const PlaceDef& pd) const;
    bool craft(int recipe);
    bool cook();
    bool nearCampfire(float radius) const;
    float groundAt(float x, float z) const;
    float floorTop(float x, float z) const;
    bool aimGround(V3& out) const;
    void computeGhost();
    bool placeGhost();
    bool removeAimed();
    void collidePlaced(float& x, float& z, float radius) const;
    void rebuildExtras();
    void flickerFires();
    void damagePlayer(float amt);
    void respawn();
    void sleepInBed();
    void saveGame();
    void loadGame();
    int modelOf(const char* name) const { return name ? pak_.find(name) : -1; }

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
    bool actHeld_ = false;
    bool showFps_ = true;

    Touch touches_[10];
    float joyBaseX_ = 0, joyBaseY_ = 0;
    float joyCX_ = 0, joyCY_ = 0, joyR_ = 0;
    float jumpCX_ = 0, jumpCY_ = 0, jumpR_ = 0;
    float actCX_ = 0, actCY_ = 0, actR_ = 0;
    float gearCX_ = 0, gearCY_ = 0, gearR_ = 0;
    float bagCX_ = 0, bagCY_ = 0, buildCX_ = 0, buildCY_ = 0, topR_ = 0;
    float rotCX_ = 0, rotCY_ = 0, delCX_ = 0, delCY_ = 0, cancelCX_ = 0, cancelCY_ = 0, subR_ = 0;
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

    int inv_[IT_COUNT] = {0};
    std::vector<int> hotbar_;
    int sel_ = 0;
    float health_ = 100.f, hunger_ = 100.f, hurtT_ = 0.f;
    float tod_ = 0.32f;
    int day_ = 1;
    float dayLen_ = 840.f;
    std::vector<Placed> placed_;
    std::vector<PropInst> extras_;
    std::vector<int> fireIdx_;
    bool buildActive_ = false;
    int buildPiece_ = 0;
    int buildFlip_ = 0;
    PropInst ghost_{};
    bool ghostOn_ = false, ghostValid_ = false;
    int ghostDef_ = -1;
    PropHit target_;
    float swingT_ = -1.f, cooldown_ = 0.f, kick_ = 0.f;
    std::unordered_map<uint64_t, int> hp_;
    std::vector<Popup> popups_;
    float saveTimer_ = 0.f, hungerTick_ = 0.f, regenTick_ = 0.f;
    int mAxe_ = -1, mPick_ = -1, mTorch_ = -1, mTorchFire_ = -1;
    bool hotbarDirty_ = true;
    int bagTab_ = 0;
    int deathCount_ = 0;
};
