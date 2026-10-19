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

const unsigned char kSeg[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};

}  // namespace


namespace {
float easeOutCubic(float t) { t = clampf(t, 0.f, 1.f); float k = 1.f - t; return 1.f - k * k * k; }
float easeOutBack(float t) {
    t = clampf(t, 0.f, 1.f);
    const float c1 = 1.70158f, c3 = c1 + 1.f;
    float k = t - 1.f;
    return 1.f + c3 * k * k * k + c1 * k * k;
}
const float kGreen[3] = {0.36f, 0.70f, 0.20f};
const float kGreenDark[3] = {0.12f, 0.30f, 0.08f};
const float kCream[3] = {1.0f, 0.97f, 0.86f};
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

void Game::drawLeaves(float alpha) {
    for (int i = 0; i < 18; i++) {
        float ph = (float)i * 0.6180339f;
        float sp = 0.035f + 0.02f * std::fmod(ph * 7.f, 1.f);
        float fx = std::fmod(ph + time_ * sp * 0.4f, 1.f);
        float fy = std::fmod(ph * 3.7f + time_ * sp, 1.f);
        float x = fx * (float)W_ + std::sin(time_ * 1.1f + (float)i) * 40.f * u_;
        float y = fy * ((float)H_ + 60.f * u_) - 30.f * u_;
        float sz = (10.f + 12.f * std::fmod(ph * 5.f, 1.f)) * u_;
        int k = i % 3;
        float r = k == 0 ? 1.0f : (k == 1 ? 0.95f : 0.55f);
        float g = k == 0 ? 0.55f : (k == 1 ? 0.78f : 0.82f);
        float b = k == 0 ? 0.12f : (k == 1 ? 0.20f : 0.15f);
        rend_.uiShape(0, x, y, sz * 1.5f, sz, r, g, b, 0.85f * alpha);
    }
}

void Game::drawLogoImage(float x, float y, float w, float a) {
    float lh = w * (float)logoH_ / (float)std::max(1, logoW_);
    rend_.uiSprite(texLogo_, x, y, w, lh, 1, 1, 1, a, 0, 0, 1, 1);
    float th = w * 0.075f;
    rend_.uiText("NATURE GAME", x + w * 0.5f, y + lh * 0.64f - th * 0.5f, th, 0.36f, 0.20f, 0.07f, a, 1, false);
}

void Game::drawSky() {
    const int bands = 14;
    float bh = (float)H_ / (float)bands + 1.f;
    for (int i = 0; i < bands; i++) {
        float t = (float)i / (float)(bands - 1);
        rend_.uiShape(2, 0, bh * (float)i - (float)i, (float)W_, bh, lerpf(0.34f, 0.86f, t), lerpf(0.62f, 0.93f, t), lerpf(0.95f, 0.98f, t), 1.f, 0.f);
    }
}

void Game::drawSplash(float dt) {
    float prog = cm_->coreProgress(spawn_.x, spawn_.z, 2);
    shownProg_ += (prog - shownProg_) * std::min(1.f, dt * 4.f);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, W_, H_);
    glClearColor(0.8f, 0.9f, 0.97f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    rend_.uiBegin();
    drawSky();
    float W = (float)W_, H = (float)H_;
    float sunRise = easeOutCubic(screenT_ / 1.6f);
    float sunY = H * (0.62f - 0.30f * sunRise);
    rend_.uiShape(0, W * 0.72f - 230.f * u_, sunY - 230.f * u_, 460.f * u_, 460.f * u_, 1.0f, 0.95f, 0.75f, 0.35f);
    rend_.uiShape(0, W * 0.72f - 130.f * u_, sunY - 130.f * u_, 260.f * u_, 260.f * u_, 1.0f, 0.92f, 0.62f, 0.9f);
    float hillA = easeOutCubic(screenT_ / 1.2f);
    float hy1 = H * (1.25f - 0.34f * hillA), hy2 = H * (1.40f - 0.34f * hillA);
    rend_.uiShape(0, -W * 0.25f, hy1 - H * 0.55f, W * 1.1f, H * 1.1f, 0.30f, 0.58f, 0.22f, 1.f);
    rend_.uiShape(0, W * 0.30f, hy2 - H * 0.62f, W * 1.2f, H * 1.24f, 0.42f, 0.72f, 0.20f, 1.f);
    drawLeaves(1.f);

    float a = easeOutBack(screenT_ / 1.0f);
    float fadeIn = clampf(screenT_ / 0.6f, 0.f, 1.f);
    float cx = W * 0.5f, cy = H * 0.34f;
    if (texLogo_) {
        float lw = std::min(W * 0.5f, 820.f * u_) * (0.7f + 0.3f * a), lh = lw * (float)logoH_ / (float)std::max(1, logoW_);
        drawLogoImage(cx - lw * 0.5f, cy - lh * 0.5f, lw, fadeIn);
    } else {
        float th = 150.f * u_ * (0.6f + 0.4f * a);
        rend_.uiText("NATURE", cx, cy - th * 1.0f, th, 1.0f, 0.97f, 0.86f, fadeIn, 1);
        rend_.uiText("GAME", cx, cy + th * 0.05f, th, 1.0f, 0.78f, 0.25f, fadeIn, 1);
    }
    float sub = clampf((screenT_ - 0.9f) / 0.6f, 0.f, 1.f);
    rend_.uiText("EXPLORE THE WILD", cx, H * 0.66f, 38.f * u_, 1.f, 1.f, 1.f, sub * 0.95f, 1);

    float bw = 640.f * u_, bh = 20.f * u_;
    float bx = (W - bw) * 0.5f, by = H * 0.86f;
    rend_.uiShape(2, bx, by, bw, bh, 0.05f, 0.14f, 0.06f, 0.55f, 1.f);
    float fillW = std::max(bh, bw * shownProg_);
    rend_.uiShape(2, bx, by, fillW, bh, 0.55f, 0.88f, 0.25f, 1.f, 1.f);
    rend_.uiShape(2, bx + 3.f * u_, by + 3.f * u_, std::max(bh - 6.f * u_, fillW - 6.f * u_), bh * 0.4f, 1, 1, 1, 0.3f, 1.f);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "LOADING %d%%", (int)std::lround(shownProg_ * 100.f));
    rend_.uiText(buf, W * 0.5f, by + bh + 16.f * u_, 32.f * u_, 1, 1, 1, 0.95f, 1);

    if (trans_ == 0 && ((prog >= 0.999f && screenT_ > 2.8f) || plat_time() - loadStart_ > 25.0)) go(S_MENU);
}

void Game::drawPanel(float x, float y, float w, float h, float a) {
    if (texPanel_) { rend_.uiSprite(texPanel_, x, y, w, h, 1, 1, 1, a, 0, 0, 1, 1); return; }
    rend_.uiShape(2, x, y + 10.f * u_, w, h, 0.02f, 0.06f, 0.04f, 0.35f * a, 0.10f);
    rend_.uiShape(2, x, y, w, h, 0.08f, 0.17f, 0.12f, 0.92f * a, 0.10f);
    rend_.uiShape(2, x + 5.f * u_, y + 5.f * u_, w - 10.f * u_, h * 0.12f, 1, 1, 1, 0.08f * a, 1.f);
}

bool Game::button(float x, float y, float w, float h, const char* label, bool primary, float alpha) {
    bool pressed = uiDown_ && trans_ == 0 && uiInside(uiX_, uiY_, x, y, w, h) && uiInside(uiSX_, uiSY_, x, y, w, h);
    bool clicked = clickIn(x, y, w, h);
    float sc = pressed ? 0.95f : 1.f;
    float bw = w * sc, bh = h * sc;
    float bx = x + (w - bw) * 0.5f, by = y + (h - bh) * 0.5f + (pressed ? 4.f * u_ : 0.f);
    float cr = primary ? kGreen[0] : 0.20f, cg = primary ? kGreen[1] : 0.38f, cb = primary ? kGreen[2] : 0.30f;
    if (pressed) { cr *= 0.88f; cg *= 0.88f; cb *= 0.88f; }
    if (texBtn_) {
        rend_.uiSprite(texBtn_, bx, by, bw, bh, primary ? 1.f : 0.85f, 1.f, primary ? 1.f : 0.9f, alpha, 0, 0, 1, 1);
    } else {
        rend_.uiShape(2, bx, by + 9.f * u_, bw, bh, kGreenDark[0], kGreenDark[1], kGreenDark[2], 0.85f * alpha, 0.45f);
        rend_.uiShape(2, bx, by, bw, bh, cr, cg, cb, alpha, 0.45f);
        rend_.uiShape(2, bx + 6.f * u_, by + 5.f * u_, bw - 12.f * u_, bh * 0.44f, 1, 1, 1, 0.20f * alpha, 0.5f);
    }
    float th = std::min(bh * 0.40f, 56.f * u_);
    rend_.uiText(label, bx + bw * 0.5f, by + (bh - th) * 0.5f - 2.f * u_, th, kCream[0], kCream[1], kCream[2], alpha, 1);
    return clicked;
}

int Game::segmented(float x, float y, float w, float h, const char* const* labels, int n, int sel) {
    float sw = w / (float)n;
    int clicked = -1;
    rend_.uiShape(2, x, y, w, h, 0.04f, 0.09f, 0.07f, 0.75f, 0.5f);
    for (int i = 0; i < n; i++) {
        float sx = x + sw * (float)i;
        if (clickIn(sx, y, sw, h)) clicked = i;
        bool pressed = uiDown_ && uiInside(uiX_, uiY_, sx, y, sw, h) && uiInside(uiSX_, uiSY_, sx, y, sw, h);
        if (i == sel) {
            rend_.uiShape(2, sx + 4.f * u_, y + 4.f * u_, sw - 8.f * u_, h - 8.f * u_, kGreen[0], kGreen[1], kGreen[2], 1.f, 0.5f);
            rend_.uiShape(2, sx + 8.f * u_, y + 7.f * u_, sw - 16.f * u_, (h - 8.f * u_) * 0.4f, 1, 1, 1, 0.22f, 0.5f);
        } else if (pressed) {
            rend_.uiShape(2, sx + 4.f * u_, y + 4.f * u_, sw - 8.f * u_, h - 8.f * u_, 1, 1, 1, 0.12f, 0.5f);
        }
        float th = std::min(h * 0.42f, 34.f * u_);
        rend_.uiText(labels[i], sx + sw * 0.5f, y + (h - th) * 0.5f, th, kCream[0], kCream[1], kCream[2], i == sel ? 1.f : 0.75f, 1);
    }
    return clicked;
}

void Game::drawMenu() {
    float W = (float)W_, H = (float)H_;
    for (int i = 0; i < 10; i++)
        rend_.uiShape(2, 0, 0, W * 0.62f * (1.f - (float)i / 10.f), H, 0.02f, 0.07f, 0.05f, 0.06f, 0.f);
    drawLeaves(0.9f);
    float tIn = easeOutBack(screenT_ / 0.9f);
    float bob = std::sin(time_ * 1.2f) * 6.f * u_;
    float x0 = 120.f * u_;
    if (texLogo_) {
        float lw = std::min(W * 0.40f, 700.f * u_) * (0.8f + 0.2f * tIn);
        drawLogoImage(x0, H * 0.04f + bob, lw, clampf(screenT_ / 0.5f, 0.f, 1.f));
    } else {
        float th = 130.f * u_;
        float al = clampf(screenT_ / 0.5f, 0.f, 1.f);
        rend_.uiText("NATURE", x0, H * 0.08f + bob, th * (0.7f + 0.3f * tIn), 1.0f, 0.97f, 0.86f, al, 0);
        rend_.uiText("GAME", x0, H * 0.08f + th * 1.02f + bob, th * (0.7f + 0.3f * tIn), 1.0f, 0.78f, 0.25f, al, 0);
    }
    const char* labels[3] = {"PLAY", "SETTINGS", "QUIT"};
    float bwid = 560.f * u_;
    float bhs[3] = {150.f * u_, 120.f * u_, 120.f * u_};
    float y = H * 0.52f;
    for (int i = 0; i < 3; i++) {
        float p = easeOutCubic((screenT_ - 0.25f - 0.12f * (float)i) / 0.55f);
        float xo = -(1.f - p) * 700.f * u_;
        float yy = y + (i == 0 ? 0.f : 0.f);
        bool c = button(x0 + xo, yy, bwid, bhs[i], labels[i], i == 0, p);
        if (c && p > 0.9f) {
            if (i == 0) go(S_PLAY);
            else if (i == 1) go(S_SETTINGS);
            else plat_quit();
        }
        y += bhs[i] + 26.f * u_;
    }
    rend_.uiText("V0.1", W - 30.f * u_, H - 60.f * u_, 30.f * u_, 1, 1, 1, 0.7f, 2);
}

void Game::drawSettings() {
    float W = (float)W_, H = (float)H_;
    rend_.uiShape(2, 0, 0, W, H, 0.02f, 0.05f, 0.04f, 0.45f, 0.f);
    float pw = 1240.f * u_, ph = 800.f * u_;
    float pp = easeOutCubic(screenT_ / 0.35f);
    float px = (W - pw) * 0.5f, py = (H - ph) * 0.5f + (1.f - pp) * 60.f * u_;
    drawPanel(px, py, pw, ph, pp);
    rend_.uiText("SETTINGS", px + pw * 0.5f, py + 36.f * u_, 62.f * u_, kCream[0], kCream[1], kCream[2], pp, 1);
    float lx = px + 70.f * u_, cx = px + 470.f * u_, cw = pw - 540.f * u_, rh = 84.f * u_;
    float y = py + 160.f * u_;
    float th = 40.f * u_;
    static const char* qL[4] = {"AUTO", "LOW", "MED", "HIGH"};
    static const char* jL[2] = {"FIXED", "FLOATING"};
    static const char* sL[3] = {"LOW", "MED", "HIGH"};
    static const char* fL[2] = {"ON", "OFF"};
    rend_.uiText("GRAPHICS", lx, y + (rh - th) * 0.5f, th, 1, 1, 1, pp, 0);
    int c = segmented(cx, y, cw, rh, qL, 4, mode_);
    if (c >= 0 && c != mode_) { mode_ = c; applyQuality(mode_ == 0 ? detectedTier_ : mode_ - 1); saveSettings(); }
    y += rh + 40.f * u_;
    rend_.uiText("JOYSTICK", lx, y + (rh - th) * 0.5f, th, 1, 1, 1, pp, 0);
    c = segmented(cx, y, cw, rh, jL, 2, joyMode_);
    if (c >= 0 && c != joyMode_) { joyMode_ = c; saveSettings(); }
    y += rh + 40.f * u_;
    rend_.uiText("LOOK SPEED", lx, y + (rh - th) * 0.5f, th, 1, 1, 1, pp, 0);
    c = segmented(cx, y, cw, rh, sL, 3, lookSens_);
    if (c >= 0 && c != lookSens_) { lookSens_ = c; saveSettings(); }
    y += rh + 40.f * u_;
    rend_.uiText("FPS COUNTER", lx, y + (rh - th) * 0.5f, th, 1, 1, 1, pp, 0);
    c = segmented(cx, y, cw, rh, fL, 2, showFps_ ? 0 : 1);
    if (c >= 0) { showFps_ = c == 0; saveSettings(); }
    if (button(px + (pw - 420.f * u_) * 0.5f, py + ph - 150.f * u_, 420.f * u_, 100.f * u_, "BACK", true, pp)) {
        if (settingsFrom_ == S_PAUSE) { screen_ = S_PAUSE; screenT_ = 0; clearTouches(); }
        else go(S_MENU);
    }
}

void Game::drawPause() {
    float W = (float)W_, H = (float)H_;
    float pp = easeOutCubic(screenT_ / 0.25f);
    rend_.uiShape(2, 0, 0, W, H, 0.02f, 0.05f, 0.04f, 0.5f * pp, 0.f);
    float pw = 1000.f * u_, ph = 640.f * u_;
    float px = (W - pw) * 0.5f, py = (H - ph) * 0.5f + (1.f - pp) * 50.f * u_;
    drawPanel(px, py, pw, ph, pp);
    rend_.uiText("PAUSED", px + pw * 0.5f, py + 34.f * u_, 60.f * u_, kCream[0], kCream[1], kCream[2], pp, 1);
    float bw = pw - 380.f * u_, bh = 110.f * u_, bx = px + 190.f * u_, y = py + 150.f * u_;
    if (button(bx, y, bw, bh, "RESUME", true, pp)) { screen_ = S_PLAY; screenT_ = 0; clearTouches(); return; }
    y += bh + 24.f * u_;
    if (button(bx, y, bw, bh, "SETTINGS", false, pp)) { settingsFrom_ = S_PAUSE; screen_ = S_SETTINGS; screenT_ = 0; clearTouches(); return; }
    y += bh + 24.f * u_;
    if (button(bx, y, bw, bh, "MAIN MENU", false, pp)) go(S_MENU);
}

void Game::drawHUD() {
    bool joyOn = false;
    float jx = 0, jy = 0;
    for (auto& t : touches_) if (t.id >= 0 && t.role == R_JOY) { joyOn = true; jx = t.x; jy = t.y; }
    bool fixed = joyMode_ == 0;
    float bx = (fixed || !joyOn) ? joyCX_ : joyBaseX_, by = (fixed || !joyOn) ? joyCY_ : joyBaseY_;
    float baseA = joyOn ? 0.60f : (fixed ? 0.40f : 0.25f);
    if (texJoyBase_) rend_.uiSprite(texJoyBase_, bx - joyR_, by - joyR_, joyR_ * 2, joyR_ * 2, 1, 1, 1, baseA / 0.6f, 0, 0, 1, 1);
    else {
        rend_.uiShape(0, bx - joyR_, by - joyR_, joyR_ * 2, joyR_ * 2, 1, 1, 1, baseA * 0.30f);
        rend_.uiShape(1, bx - joyR_, by - joyR_, joyR_ * 2, joyR_ * 2, 1, 1, 1, baseA, 0.05f);
    }
    float kr = 58.f * u_, kx = bx, ky = by;
    if (joyOn) {
        float dx = jx - bx, dy = jy - by, m = std::sqrt(dx * dx + dy * dy);
        if (m > joyR_) { dx = dx / m * joyR_; dy = dy / m * joyR_; }
        kx = bx + dx; ky = by + dy;
    }
    if (texJoyKnob_) rend_.uiSprite(texJoyKnob_, kx - kr, ky - kr, kr * 2, kr * 2, 1, 1, 1, joyOn ? 0.95f : 0.6f, 0, 0, 1, 1);
    else rend_.uiShape(0, kx - kr, ky - kr, kr * 2, kr * 2, 1, 1, 1, joyOn ? 0.75f : 0.40f);

    bool jp = false;
    for (auto& t : touches_) if (t.id >= 0 && t.role == R_JUMP) jp = true;
    if (texJump_) rend_.uiSprite(texJump_, jumpCX_ - jumpR_, jumpCY_ - jumpR_, jumpR_ * 2, jumpR_ * 2, 1, 1, 1, jp ? 1.f : 0.8f, 0, 0, 1, 1);
    else {
        rend_.uiShape(0, jumpCX_ - jumpR_, jumpCY_ - jumpR_, jumpR_ * 2, jumpR_ * 2, 1, 1, 1, jp ? 0.5f : 0.25f);
        rend_.uiShape(1, jumpCX_ - jumpR_, jumpCY_ - jumpR_, jumpR_ * 2, jumpR_ * 2, 1, 1, 1, 0.8f, 0.06f);
        rend_.uiText("JUMP", jumpCX_, jumpCY_ - 16.f * u_, 32.f * u_, 1, 1, 1, 0.95f, 1);
    }
    if (texGear_) rend_.uiSprite(texGear_, gearCX_ - gearR_, gearCY_ - gearR_, gearR_ * 2, gearR_ * 2, 1, 1, 1, 0.9f, 0, 0, 1, 1);
    else {
        rend_.uiShape(0, gearCX_ - gearR_, gearCY_ - gearR_, gearR_ * 2, gearR_ * 2, 0, 0, 0, 0.35f);
        rend_.uiShape(2, gearCX_ - gearR_ * 0.42f, gearCY_ - gearR_ * 0.45f, gearR_ * 0.84f, gearR_ * 0.2f, 1, 1, 1, 0.95f, 1.f);
        rend_.uiShape(2, gearCX_ - gearR_ * 0.42f, gearCY_ - gearR_ * 0.10f, gearR_ * 0.84f, gearR_ * 0.2f, 1, 1, 1, 0.95f, 1.f);
        rend_.uiShape(2, gearCX_ - gearR_ * 0.42f, gearCY_ + gearR_ * 0.25f, gearR_ * 0.84f, gearR_ * 0.2f, 1, 1, 1, 0.95f, 1.f);
    }
    if (showFps_) {
        char buf[24];
        std::snprintf(buf, sizeof(buf), "FPS %d", shownFps_);
        rend_.uiText(buf, 30.f * u_, 24.f * u_, 36.f * u_, 1, 1, 1, 0.95f, 0);
    }
}
