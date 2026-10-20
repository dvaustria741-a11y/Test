#include "game.h"
#include "platform.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <algorithm>
#include <sys/sysinfo.h>
#include <unistd.h>

namespace {

int detectGpuTier(const std::string& r) {
    std::string s;
    for (char c : r) s += (char)std::tolower((unsigned char)c);
    auto numAfter = [&](const char* key) {
        size_t p = s.find(key);
        if (p == std::string::npos) return -1;
        p += std::strlen(key);
        while (p < s.size() && !std::isdigit((unsigned char)s[p])) {
            if (s[p] == ',' ) return -1;
            p++;
        }
        int n = 0;
        bool any = false;
        while (p < s.size() && std::isdigit((unsigned char)s[p])) { n = n * 10 + (s[p] - '0'); p++; any = true; }
        return any ? n : -1;
    };
    if (s.find("adreno") != std::string::npos) {
        int n = numAfter("adreno");
        if (n >= 730) return 2;
        if (n == 702 || n == 710 || n == 720 || n == 722 || n == 721) return 1;
        if (n >= 640) return 2;
        if (n >= 610) return 1;
        return 0;
    }
    if (s.find("immortalis") != std::string::npos) return 2;
    if (s.find("mali") != std::string::npos) {
        int n = numAfter("mali-g");
        if (n < 0) return 0;
        if (n >= 77) return 2;
        if (n >= 52) return 1;
        return 0;
    }
    if (s.find("xclipse") != std::string::npos || s.find("apple") != std::string::npos || s.find("tegra") != std::string::npos) return 2;
    if (s.find("powervr") != std::string::npos) return 0;
    return 1;
}

int deviceCapTier() {
    int cap = 2;
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        double gb = (double)si.totalram * (double)si.mem_unit / (1024.0 * 1024.0 * 1024.0);
        if (gb < 3.2) cap = 0;
        else if (gb < 5.5) cap = std::min(cap, 1);
    }
    long cores = sysconf(_SC_NPROCESSORS_CONF);
    if (cores > 0 && cores <= 4) cap = 0;
    return cap;
}


}  // namespace




Game::Game() : world_(1337u) {
    std::vector<uint8_t> data;
    if (plat_read_asset("props.pak", data) && pakParse(std::move(data), pak_)) {
        ids_.fill(pak_);
        pakOk_ = true;
        cm_.reset(new ChunkManager(world_, ids_, pak_));
    } else {
        plat_log("failed to load props.pak");
    }
    float sh = world_.height(0.f, 0.f);
    pos_ = {0.f, sh, 0.f};
    spawn_ = pos_;
    loadSettings();
}

Game::~Game() { cm_.reset(); }

void Game::loadSettings() {
    std::string path = plat_data_dir() + "/settings.txt";
    FILE* f = std::fopen(path.c_str(), "r");
    if (!f) return;
    char line[128];
    while (std::fgets(line, sizeof(line), f)) {
        int v;
        if (std::sscanf(line, "mode=%d", &v) == 1) mode_ = std::max(0, std::min(3, v));
        if (std::sscanf(line, "fps=%d", &v) == 1) showFps_ = v != 0;
        if (std::sscanf(line, "joy=%d", &v) == 1) joyMode_ = std::max(0, std::min(1, v));
        if (std::sscanf(line, "look=%d", &v) == 1) lookSens_ = std::max(0, std::min(2, v));
    }
    std::fclose(f);
}

void Game::saveSettings() {
    std::string path = plat_data_dir() + "/settings.txt";
    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) return;
    std::fprintf(f, "mode=%d\nfps=%d\njoy=%d\nlook=%d\n", mode_, showFps_ ? 1 : 0, joyMode_, lookSens_);
    std::fclose(f);
}

void Game::applyQuality(int tier) {
    tier = std::max(0, std::min(2, tier));
    curTier_ = tier;
    q_ = qualityPreset(tier);
    rend_.setQuality(q_);
    int viewR = (int)std::ceil(q_.viewDist / CHUNK_SIZE) + 1;
    cm_->configure(q_.terrainRes, viewR, 1);
    renderScale_ = 0.5f * (q_.renderMin + q_.renderMax);
    if (q_.renderMax > 0.97f) renderScale_ = 1.0f;
    atMinTimer_ = 0;
}

bool Game::initGL(int w, int h, const char* glRenderer) {
    if (!pakOk_) return false;
    W_ = w;
    H_ = h;
    detectedTier_ = std::min(detectGpuTier(glRenderer ? glRenderer : ""), deviceCapTier());
    plat_log("GL renderer: %s -> tier %d", glRenderer ? glRenderer : "?", detectedTier_);
    int tier = mode_ == 0 ? detectedTier_ : mode_ - 1;
    Quality q = qualityPreset(tier);
    if (!rend_.init(pak_, q, w, h)) {
        plat_log("renderer init failed");
        return false;
    }
    glReady_ = true;
    applyQuality(tier);
    screen_ = S_SPLASH;
    screenT_ = 0;
    trans_ = 0;
    fade_ = 0;
    shownProg_ = 0;
    loadStart_ = plat_time();
    lastTime_ = loadStart_;
    texJoyBase_ = rend_.loadUITexture("ui/joy_base.png");
    texJoyKnob_ = rend_.loadUITexture("ui/joy_knob.png");
    texJump_ = rend_.loadUITexture("ui/btn_jump.png");
    texGear_ = rend_.loadUITexture("ui/btn_gear.png");
    texLogo_ = rend_.loadUITexture("ui/title.png", &logoW_, &logoH_);
    texBtn_ = rend_.loadUITexture("ui/btn_wide.png", &btnW_, &btnH_);
    texPanel_ = rend_.loadUITexture("ui/panel.png");
    layout();
    return true;
}

void Game::termGL() {
    if (!glReady_) return;
    if (cm_) cm_->releaseGL();
    GLuint* texs[] = {&texJoyBase_, &texJoyKnob_, &texJump_, &texGear_, &texLogo_, &texBtn_, &texPanel_};
    for (GLuint* t : texs) if (*t) { glDeleteTextures(1, t); *t = 0; }
    rend_.destroy();
    glReady_ = false;
}

void Game::resize(int w, int h) {
    W_ = w;
    H_ = h;
    if (glReady_) rend_.resize(w, h);
    layout();
}

void Game::layout() {
    u_ = std::max(0.5f, (float)H_ / 1080.f);
    joyR_ = 150.f * u_;
    joyCX_ = 250.f * u_;
    joyCY_ = (float)H_ - 250.f * u_;
    jumpR_ = 88.f * u_;
    jumpCX_ = (float)W_ - 190.f * u_;
    jumpCY_ = (float)H_ - 220.f * u_;
    gearR_ = 48.f * u_;
    gearCX_ = (float)W_ - 80.f * u_;
    gearCY_ = 80.f * u_;
}

void Game::clearTouches() {
    for (auto& t : touches_) { t.id = -1; t.role = R_NONE; }
    joyX_ = joyY_ = 0;
    uiPtr_ = -1;
    uiDown_ = false;
    uiClick_ = false;
}

void Game::go(Screen s) {
    if (trans_ != 0) return;
    nextScreen_ = s;
    trans_ = 1;
    transT_ = 0;
}

void Game::enterScreen() {
    clearTouches();
    if (screen_ == S_MENU) menuT_ = 0;
    if (screen_ == S_PLAY) { velX_ = velZ_ = 0; }
}

V3 Game::streamPos() const {
    bool fromPlay = screen_ == S_PLAY || screen_ == S_PAUSE || (screen_ == S_SETTINGS && settingsFrom_ == S_PAUSE);
    return fromPlay ? pos_ : spawn_;
}

Camera Game::menuCamera() const {
    float a = menuT_ * 0.11f + 0.4f;
    float R = 30.f;
    float cx = spawn_.x + std::cos(a) * R, cz = spawn_.z + std::sin(a) * R;
    float cy = world_.height(cx, cz) + 7.5f;
    V3 tgt = {spawn_.x, spawn_.y + 3.5f, spawn_.z};
    V3 d = tgt - V3{cx, cy, cz};
    float l = vlen(d);
    Camera c;
    c.pos = {cx, cy, cz};
    c.yaw = std::atan2(d.x, -d.z);
    c.pitch = std::asin(clampf(d.y / std::max(l, 1e-3f), -1.f, 1.f));
    c.fov = 1.05f;
    c.aspect = (float)W_ / (float)H_;
    return c;
}

Camera Game::playCamera() const {
    Camera cam;
    cam.pos = {pos_.x, pos_.y + 1.72f + bob_, pos_.z};
    cam.yaw = yaw_;
    cam.pitch = pitch_;
    cam.fov = 1.22f;
    cam.aspect = (float)W_ / (float)H_;
    return cam;
}

bool Game::uiInside(float px, float py, float x, float y, float w, float h) const {
    return px >= x && px <= x + w && py >= y && py <= y + h;
}

bool Game::clickIn(float x, float y, float w, float h) const {
    return uiClick_ && trans_ == 0 && uiInside(uiCX_, uiCY_, x, y, w, h) && uiInside(uiSX_, uiSY_, x, y, w, h);
}

void Game::uiTouch(int action, int id, float x, float y) {
    if (action == 0) {
        if (uiPtr_ >= 0) return;
        uiPtr_ = id;
        uiDown_ = true;
        uiX_ = uiSX_ = x;
        uiY_ = uiSY_ = y;
    } else if (action == 1) {
        if (id == uiPtr_) { uiX_ = x; uiY_ = y; }
    } else {
        if (id == uiPtr_) {
            uiClick_ = true;
            uiCX_ = x;
            uiCY_ = y;
            uiDown_ = false;
            uiPtr_ = -1;
        }
    }
}

void Game::touch(int action, int id, float x, float y) {
    if (!glReady_ || screen_ == S_SPLASH) return;
    if (screen_ != S_PLAY) { uiTouch(action, id, x, y); return; }
    if (trans_ != 0) return;
    Touch* t = nullptr;
    for (auto& tt : touches_) if (tt.id == id) { t = &tt; break; }
    if (action == 0) {
        if (t) return;
        for (auto& tt : touches_) if (tt.id < 0) { t = &tt; break; }
        if (!t) return;
        t->id = id; t->sx = t->x = t->lx = x; t->sy = t->y = t->ly = y; t->role = R_NONE;
        float dx = x - gearCX_, dy = y - gearCY_;
        if (dx * dx + dy * dy <= gearR_ * gearR_ * 1.7f) {
            clearTouches();
            screen_ = S_PAUSE;
            screenT_ = 0;
            return;
        }
        dx = x - jumpCX_; dy = y - jumpCY_;
        if (dx * dx + dy * dy <= jumpR_ * jumpR_ * 1.3f) { t->role = R_JUMP; jumpReq_ = true; return; }
        bool joyActive = false;
        for (auto& tt : touches_) if (tt.id >= 0 && tt.role == R_JOY) joyActive = true;
        if (!joyActive) {
            if (joyMode_ == 0) {
                dx = x - joyCX_; dy = y - joyCY_;
                if (dx * dx + dy * dy <= joyR_ * joyR_ * 1.8f) {
                    t->role = R_JOY;
                    joyBaseX_ = joyCX_; joyBaseY_ = joyCY_;
                    return;
                }
            } else if (x < (float)W_ * 0.45f) {
                t->role = R_JOY;
                joyBaseX_ = x; joyBaseY_ = y;
                return;
            }
        }
        t->role = R_LOOK;
    } else if (action == 1) {
        if (!t) return;
        t->x = x; t->y = y;
        if (t->role == R_LOOK) {
            static const float sens[3] = {0.7f, 1.0f, 1.5f};
            float k = 0.0026f * (1080.f / (float)H_) * sens[lookSens_];
            yaw_ += (x - t->lx) * k;
            pitch_ -= (y - t->ly) * k;
            pitch_ = clampf(pitch_, -1.35f, 1.35f);
            t->lx = x; t->ly = y;
        }
    } else {
        if (!t) return;
        if (t->role == R_JOY) { joyX_ = joyY_ = 0; }
        t->id = -1; t->role = R_NONE;
    }
}

void Game::updatePlay(float dt) {
    for (auto& t : touches_) {
        if (t.id >= 0 && t.role == R_JOY) {
            float dx = t.x - joyBaseX_, dy = t.y - joyBaseY_;
            float r = joyR_;
            float m = std::sqrt(dx * dx + dy * dy);
            if (m > r && joyMode_ == 1) { joyBaseX_ += dx / m * (m - r); joyBaseY_ += dy / m * (m - r); dx = t.x - joyBaseX_; dy = t.y - joyBaseY_; m = r; }
            float mag = std::min(1.f, m / r);
            if (mag < 0.10f) { joyX_ = joyY_ = 0; }
            else {
                float k = (mag - 0.10f) / 0.90f / std::max(m / r, 1e-4f);
                joyX_ = dx / r * k;
                joyY_ = -dy / r * k;
            }
        }
    }
    float mag = std::min(1.f, std::sqrt(joyX_ * joyX_ + joyY_ * joyY_));
    float speed = 5.2f * mag;
    if (mag > 0.92f) speed = 8.6f;
    float fx = std::sin(yaw_), fz = -std::cos(yaw_);
    float rx = std::cos(yaw_), rz = std::sin(yaw_);
    float tvx = (fx * joyY_ + rx * joyX_), tvz = (fz * joyY_ + rz * joyX_);
    float tl = std::sqrt(tvx * tvx + tvz * tvz);
    if (tl > 1e-4f) { tvx = tvx / tl * speed; tvz = tvz / tl * speed; } else { tvx = tvz = 0; }
    float a = 1.f - std::exp(-12.f * dt);
    velX_ += (tvx - velX_) * a;
    velZ_ += (tvz - velZ_) * a;
    float nx = pos_.x + velX_ * dt, nz = pos_.z + velZ_ * dt;
    cm_->collide(nx, nz, 0.45f);
    pos_.x = nx; pos_.z = nz;
    float ground = world_.height(pos_.x, pos_.z);
    if (jumpReq_ && onGround_) { vy_ = 7.2f; onGround_ = false; }
    jumpReq_ = false;
    if (onGround_) {
        pos_.y = ground;
        vy_ = 0;
    } else {
        vy_ -= 22.f * dt;
        pos_.y += vy_ * dt;
        if (pos_.y <= ground) { pos_.y = ground; vy_ = 0; onGround_ = true; }
    }
    float hv = std::sqrt(velX_ * velX_ + velZ_ * velZ_);
    if (onGround_ && hv > 0.3f) bobT_ += dt * (hv * 1.5f);
    float targetBob = onGround_ ? std::sin(bobT_) * 0.035f * std::min(1.f, hv / 5.f) : 0.f;
    bob_ += (targetBob - bob_) * (1.f - std::exp(-14.f * dt));
}

void Game::adaptQuality(float dt) {
    dtAcc_ += dt;
    dtCount_++;
    if (dtCount_ < 20) return;
    float avg = dtAcc_ / (float)dtCount_;
    dtAcc_ = 0;
    dtCount_ = 0;
    float target = 1.f / (float)q_.targetFps;
    if (avg > target * 1.14f) {
        renderScale_ = std::max(q_.renderMin, renderScale_ * 0.93f);
        if (renderScale_ <= q_.renderMin + 0.001f) atMinTimer_ += avg * 20.f; else atMinTimer_ = 0;
    } else if (avg < target * 0.82f) {
        renderScale_ = std::min(q_.renderMax, renderScale_ * 1.04f);
        atMinTimer_ = 0;
    } else {
        atMinTimer_ = std::max(0.f, atMinTimer_ - avg * 20.f);
    }
    if (mode_ == 0 && atMinTimer_ > 5.0f && curTier_ > 0 && avg > target * 1.3f) {
        applyQuality(curTier_ - 1);
        plat_log("auto quality dropped to tier %d", curTier_);
    }
}

void Game::frame() {
    if (!glReady_) return;
    double now = plat_time();
    float dt = clampf((float)(now - lastTime_), 0.0005f, 0.1f);
    lastTime_ = now;
    time_ += dt;
    screenT_ += dt;
    fpsFrames_++;
    fpsTimer_ += dt;
    if (fpsTimer_ >= 0.5f) {
        shownFps_ = (int)std::lround((float)fpsFrames_ / fpsTimer_);
        fpsFrames_ = 0;
        fpsTimer_ = 0;
    }

    if (trans_ == 1) {
        transT_ += dt;
        fade_ = clampf(transT_ / 0.28f, 0.f, 1.f);
        if (transT_ >= 0.28f) {
            if (nextScreen_ == S_SETTINGS) settingsFrom_ = screen_;
            screen_ = nextScreen_;
            screenT_ = 0;
            trans_ = 2;
            transT_ = 0;
            enterScreen();
        }
    } else if (trans_ == 2) {
        transT_ += dt;
        fade_ = 1.f - clampf(transT_ / 0.40f, 0.f, 1.f);
        if (transT_ >= 0.40f) { trans_ = 0; fade_ = 0; }
    }

    V3 sp = streamPos();
    cm_->update(sp.x, sp.z, screen_ == S_SPLASH ? 6 : 2);

    switch (screen_) {
        case S_SPLASH:
            drawSplash(dt);
            break;
        case S_MENU:
        case S_SETTINGS: {
            bool fromPlay = screen_ == S_SETTINGS && settingsFrom_ == S_PAUSE;
            if (!fromPlay) menuT_ += dt;
            Camera cam = fromPlay ? playCamera() : menuCamera();
            rend_.render(cam, *cm_, time_, renderScale_);
            rend_.uiBegin();
            if (screen_ == S_MENU) drawMenu(); else drawSettings();
            break;
        }
        case S_PLAY:
            updatePlay(dt);
            adaptQuality(dt);
            rend_.render(playCamera(), *cm_, time_, renderScale_);
            rend_.uiBegin();
            drawHUD();
            break;
        case S_PAUSE:
            rend_.render(playCamera(), *cm_, time_, renderScale_);
            rend_.uiBegin();
            drawPause();
            break;
    }
    if (fade_ > 0.001f) rend_.uiShape(2, 0, 0, (float)W_, (float)H_, 0.03f, 0.08f, 0.05f, fade_, 0.f);
    rend_.uiEnd();
    uiClick_ = false;
}

