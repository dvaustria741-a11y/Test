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
    loadSettings();
}

Game::~Game() {
    cm_.reset();
}

void Game::loadSettings() {
    std::string path = plat_data_dir() + "/settings.txt";
    FILE* f = std::fopen(path.c_str(), "r");
    if (!f) return;
    char line[128];
    while (std::fgets(line, sizeof(line), f)) {
        int v;
        if (std::sscanf(line, "mode=%d", &v) == 1) mode_ = std::max(0, std::min(3, v));
        if (std::sscanf(line, "fps=%d", &v) == 1) showFps_ = v != 0;
    }
    std::fclose(f);
}

void Game::saveSettings() {
    std::string path = plat_data_dir() + "/settings.txt";
    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) return;
    std::fprintf(f, "mode=%d\nfps=%d\n", mode_, showFps_ ? 1 : 0);
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
    lowTimer_ = highTimer_ = atMinTimer_ = 0;
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
    loading_ = true;
    loadStart_ = plat_time();
    lastTime_ = loadStart_;
    texJoyBase_ = rend_.loadUITexture("ui/joy_base.png");
    texJoyKnob_ = rend_.loadUITexture("ui/joy_knob.png");
    texJump_ = rend_.loadUITexture("ui/btn_jump.png");
    texGear_ = rend_.loadUITexture("ui/btn_gear.png");
    texTitle_ = rend_.loadUITexture("ui/title.png");
    layout();
    return true;
}

void Game::termGL() {
    if (!glReady_) return;
    if (cm_) cm_->releaseGL();
    GLuint* texs[] = {&texJoyBase_, &texJoyKnob_, &texJump_, &texGear_, &texTitle_};
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
    joyCX_ = 230.f * u_;
    joyCY_ = (float)H_ - 230.f * u_;
    jumpR_ = 88.f * u_;
    jumpCX_ = (float)W_ - 190.f * u_;
    jumpCY_ = (float)H_ - 220.f * u_;
    gearR_ = 48.f * u_;
    gearCX_ = (float)W_ - 80.f * u_;
    gearCY_ = 80.f * u_;
}

bool Game::inCircle(float x, float y, float cx, float cy, float r) const {
    float dx = x - cx, dy = y - cy;
    return dx * dx + dy * dy <= r * r;
}

int Game::menuHit(float x, float y) const {
    float pw = 900.f * u_, ph = 440.f * u_;
    float px = ((float)W_ - pw) * 0.5f, py = ((float)H_ - ph) * 0.5f;
    float bw = 170.f * u_, bh = 170.f * u_, gap = 34.f * u_;
    float total = bw * 4 + gap * 3;
    float bx = px + (pw - total) * 0.5f;
    float by = py + 70.f * u_;
    for (int i = 0; i < 4; i++) {
        float x0 = bx + (bw + gap) * (float)i;
        if (x >= x0 && x <= x0 + bw && y >= by && y <= by + bh) return i;
    }
    float fx = px + pw * 0.5f, fy = py + ph - 90.f * u_;
    if (inCircle(x, y, fx, fy, 50.f * u_)) return 10;
    if (x >= px && x <= px + pw && y >= py && y <= py + ph) return 99;
    return -1;
}

void Game::touch(int action, int id, float x, float y) {
    if (!glReady_ || loading_) return;
    Touch* t = nullptr;
    for (auto& tt : touches_) if (tt.id == id) { t = &tt; break; }
    if (action == 0) {
        if (t) return;
        for (auto& tt : touches_) if (tt.id < 0) { t = &tt; break; }
        if (!t) return;
        t->id = id; t->sx = t->x = t->lx = x; t->sy = t->y = t->ly = y; t->role = R_NONE;
        if (menuOpen_) {
            int h = menuHit(x, y);
            if (h >= 0 && h <= 3) {
                mode_ = h;
                int tier = mode_ == 0 ? detectedTier_ : mode_ - 1;
                applyQuality(tier);
                saveSettings();
            } else if (h == 10) {
                showFps_ = !showFps_;
                saveSettings();
            } else if (h < 0) {
                menuOpen_ = false;
            }
            t->role = R_MENU;
            return;
        }
        if (inCircle(x, y, gearCX_, gearCY_, gearR_ * 1.3f)) { menuOpen_ = true; t->role = R_MENU; return; }
        if (inCircle(x, y, jumpCX_, jumpCY_, jumpR_ * 1.15f)) { t->role = R_JUMP; jumpReq_ = true; return; }
        bool joyActive = false;
        for (auto& tt : touches_) if (tt.id >= 0 && tt.role == R_JOY) joyActive = true;
        if (x < (float)W_ * 0.45f && !joyActive) {
            t->role = R_JOY;
            joyBaseX_ = x; joyBaseY_ = y;
            return;
        }
        t->role = R_LOOK;
    } else if (action == 1) {
        if (!t) return;
        t->x = x; t->y = y;
        if (t->role == R_LOOK) {
            float k = 0.0026f * (1080.f / (float)H_);
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

void Game::update(float dt) {
    for (auto& t : touches_) {
        if (t.id >= 0 && t.role == R_JOY) {
            float dx = t.x - joyBaseX_, dy = t.y - joyBaseY_;
            float r = joyR_;
            float m = std::sqrt(dx * dx + dy * dy);
            if (m > r) { joyBaseX_ += dx / m * (m - r); joyBaseY_ += dy / m * (m - r); dx = t.x - joyBaseX_; dy = t.y - joyBaseY_; m = r; }
            float mag = m / r;
            if (mag < 0.12f) { joyX_ = joyY_ = 0; }
            else {
                float k = (mag - 0.12f) / 0.88f / std::max(mag, 1e-4f);
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
        highTimer_ = 0;
        lowTimer_ += avg * 20.f;
        if (renderScale_ <= q_.renderMin + 0.001f) atMinTimer_ += avg * 20.f; else atMinTimer_ = 0;
    } else if (avg < target * 0.82f) {
        renderScale_ = std::min(q_.renderMax, renderScale_ * 1.04f);
        lowTimer_ = 0;
        atMinTimer_ = 0;
    } else {
        lowTimer_ = std::max(0.f, lowTimer_ - avg * 20.f);
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
    float dt = (float)(now - lastTime_);
    lastTime_ = now;
    dt = clampf(dt, 0.0005f, 0.1f);
    time_ += dt;

    fpsFrames_++;
    fpsTimer_ += dt;
    if (fpsTimer_ >= 0.5f) {
        shownFps_ = (int)std::lround((float)fpsFrames_ / fpsTimer_);
        fpsFrames_ = 0;
        fpsTimer_ = 0;
    }

    cm_->update(pos_.x, pos_.z, loading_ ? 6 : 2);
    if (loading_) {
        float prog = cm_->coreProgress(pos_.x, pos_.z, 2);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, W_, H_);
        glClearColor(0.80f, 0.90f, 0.97f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        rend_.uiBegin();
        drawLoading();
        float bw = 520.f * u_, bh = 16.f * u_;
        float bx = ((float)W_ - bw) * 0.5f, by = (float)H_ * 0.82f;
        rend_.uiShape(2, bx, by, bw, bh, 1, 1, 1, 0.5f, 1.0f);
        rend_.uiShape(2, bx, by, std::max(bh, bw * prog), bh, 0.35f, 0.75f, 0.12f, 1.f, 1.0f);
        rend_.uiEnd();
        if (prog >= 0.999f || now - loadStart_ > 25.0) {
            loading_ = false;
            lastTime_ = plat_time();
        }
        return;
    }

    update(dt);
    adaptQuality(dt);

    Camera cam;
    cam.pos = {pos_.x, pos_.y + 1.72f + bob_, pos_.z};
    cam.yaw = yaw_;
    cam.pitch = pitch_;
    cam.fov = 1.22f;
    cam.aspect = (float)W_ / (float)H_;
    rend_.render(cam, *cm_, time_, renderScale_);
    rend_.uiBegin();
    drawUI();
    rend_.uiEnd();
}

void Game::drawLoading() {
    if (texTitle_) {
        float tw = std::min((float)W_ * 0.7f, 900.f * u_), th = tw * 0.35f;
        rend_.uiShape(3, ((float)W_ - tw) * 0.5f, (float)H_ * 0.22f, tw, th, 1, 1, 1, 1, 0, texTitle_);
    } else {
        float cx = (float)W_ * 0.5f, cy = (float)H_ * 0.42f;
        rend_.uiShape(0, cx - 90.f * u_, cy - 90.f * u_, 180.f * u_, 180.f * u_, 1.0f, 0.85f, 0.45f, 0.95f);
        rend_.uiShape(0, cx - 60.f * u_, cy - 60.f * u_, 120.f * u_, 120.f * u_, 0.45f, 0.78f, 0.18f, 1.0f);
    }
}

void Game::drawDigit(int d, float x, float y, float h, float r, float g, float b, float a) {
    float w = h * 0.55f, t = h * 0.13f;
    unsigned char m = kSeg[d % 10];
    if (m & 1) rend_.uiShape(2, x + t * 0.5f, y, w - t, t, r, g, b, a, 0.3f);
    if (m & 2) rend_.uiShape(2, x + w - t, y + t * 0.5f, t, h * 0.5f - t, r, g, b, a, 0.3f);
    if (m & 4) rend_.uiShape(2, x + w - t, y + h * 0.5f, t, h * 0.5f - t * 0.5f, r, g, b, a, 0.3f);
    if (m & 8) rend_.uiShape(2, x + t * 0.5f, y + h - t, w - t, t, r, g, b, a, 0.3f);
    if (m & 16) rend_.uiShape(2, x, y + h * 0.5f, t, h * 0.5f - t * 0.5f, r, g, b, a, 0.3f);
    if (m & 32) rend_.uiShape(2, x, y + t * 0.5f, t, h * 0.5f - t, r, g, b, a, 0.3f);
    if (m & 64) rend_.uiShape(2, x + t * 0.5f, y + h * 0.5f - t * 0.5f, w - t, t, r, g, b, a, 0.3f);
}

void Game::drawNumber(int n, float x, float y, float h) {
    char buf[12];
    std::snprintf(buf, sizeof(buf), "%d", std::max(0, std::min(999, n)));
    float w = h * 0.55f, gap = h * 0.25f;
    for (size_t i = 0; buf[i]; i++) {
        drawDigit(buf[i] - '0', x + (float)i * (w + gap) + 2.f, y + 2.f, h, 0, 0, 0, 0.45f);
        drawDigit(buf[i] - '0', x + (float)i * (w + gap), y, h, 1, 1, 1, 0.95f);
    }
}

void Game::drawUI() {
    bool joyOn = false;
    float jx = 0, jy = 0;
    for (auto& t : touches_) if (t.id >= 0 && t.role == R_JOY) { joyOn = true; jx = t.x; jy = t.y; }
    float bx = joyOn ? joyBaseX_ : joyCX_, by = joyOn ? joyBaseY_ : joyCY_;
    float baseA = joyOn ? 0.55f : 0.28f;
    if (texJoyBase_) rend_.uiShape(3, bx - joyR_, by - joyR_, joyR_ * 2, joyR_ * 2, 1, 1, 1, baseA / 0.55f, 0, texJoyBase_);
    else {
        rend_.uiShape(0, bx - joyR_, by - joyR_, joyR_ * 2, joyR_ * 2, 1, 1, 1, baseA * 0.35f);
        rend_.uiShape(1, bx - joyR_, by - joyR_, joyR_ * 2, joyR_ * 2, 1, 1, 1, baseA, 0.05f);
    }
    float kr = 58.f * u_;
    float kx = bx, ky = by;
    if (joyOn) {
        float dx = jx - bx, dy = jy - by, m = std::sqrt(dx * dx + dy * dy);
        if (m > joyR_) { dx = dx / m * joyR_; dy = dy / m * joyR_; }
        kx = bx + dx; ky = by + dy;
    }
    if (texJoyKnob_) rend_.uiShape(3, kx - kr, ky - kr, kr * 2, kr * 2, 1, 1, 1, joyOn ? 0.95f : 0.5f, 0, texJoyKnob_);
    else rend_.uiShape(0, kx - kr, ky - kr, kr * 2, kr * 2, 1, 1, 1, joyOn ? 0.7f : 0.35f);

    bool jp = false;
    for (auto& t : touches_) if (t.id >= 0 && t.role == R_JUMP) jp = true;
    if (texJump_) rend_.uiShape(3, jumpCX_ - jumpR_, jumpCY_ - jumpR_, jumpR_ * 2, jumpR_ * 2, 1, 1, 1, jp ? 1.f : 0.75f, 0, texJump_);
    else {
        rend_.uiShape(0, jumpCX_ - jumpR_, jumpCY_ - jumpR_, jumpR_ * 2, jumpR_ * 2, 1, 1, 1, jp ? 0.5f : 0.25f);
        rend_.uiShape(1, jumpCX_ - jumpR_, jumpCY_ - jumpR_, jumpR_ * 2, jumpR_ * 2, 1, 1, 1, 0.8f, 0.06f);
        float aw = 14.f * u_, ah = 46.f * u_;
        rend_.uiShape(2, jumpCX_ - aw * 0.5f, jumpCY_ - ah * 0.5f, aw, ah, 1, 1, 1, 0.9f, 0.5f);
        rend_.uiShape(0, jumpCX_ - 20.f * u_, jumpCY_ - ah * 0.5f - 6.f * u_, 40.f * u_, 40.f * u_, 1, 1, 1, 0.9f);
    }

    if (texGear_) rend_.uiShape(3, gearCX_ - gearR_, gearCY_ - gearR_, gearR_ * 2, gearR_ * 2, 1, 1, 1, 0.85f, 0, texGear_);
    else {
        rend_.uiShape(0, gearCX_ - gearR_, gearCY_ - gearR_, gearR_ * 2, gearR_ * 2, 0, 0, 0, 0.35f);
        rend_.uiShape(1, gearCX_ - gearR_ * 0.62f, gearCY_ - gearR_ * 0.62f, gearR_ * 1.24f, gearR_ * 1.24f, 1, 1, 1, 0.9f, 0.28f);
        rend_.uiShape(0, gearCX_ - gearR_ * 0.2f, gearCY_ - gearR_ * 0.2f, gearR_ * 0.4f, gearR_ * 0.4f, 1, 1, 1, 0.9f);
    }
    if (showFps_) drawNumber(shownFps_, 30.f * u_, 28.f * u_, 38.f * u_);

    if (menuOpen_) {
        rend_.uiShape(2, 0, 0, (float)W_, (float)H_, 0, 0, 0, 0.35f, 0.0f);
        float pw = 900.f * u_, ph = 440.f * u_;
        float px = ((float)W_ - pw) * 0.5f, py = ((float)H_ - ph) * 0.5f;
        rend_.uiShape(2, px, py, pw, ph, 0.08f, 0.10f, 0.14f, 0.88f, 0.12f);
        float bw = 170.f * u_, bh = 170.f * u_, gap = 34.f * u_;
        float total = bw * 4 + gap * 3;
        float x0 = px + (pw - total) * 0.5f, y0 = py + 70.f * u_;
        for (int i = 0; i < 4; i++) {
            float x = x0 + (bw + gap) * (float)i;
            bool sel = mode_ == i;
            rend_.uiShape(2, x, y0, bw, bh, sel ? 0.30f : 0.18f, sel ? 0.62f : 0.22f, sel ? 0.18f : 0.28f, 0.95f, 0.18f);
            if (i == 0) {
                drawDigit(0, x + bw * 0.5f - 0.5f * 0.55f * 80.f * u_ - 5.f * u_, y0 + (bh - 80.f * u_) * 0.5f, 80.f * u_, 1, 1, 1, 1);
                rend_.uiShape(2, x + bw * 0.5f - 12.f * u_, y0 + bh - 34.f * u_, 24.f * u_, 6.f * u_, 1, 1, 1, 0.9f, 0.5f);
            } else {
                float barW = 22.f * u_, barGap = 12.f * u_;
                float bt = barW * 3 + barGap * 2;
                float bx0 = x + (bw - bt) * 0.5f;
                for (int k = 0; k < 3; k++) {
                    float hh = (34.f + 28.f * (float)k) * u_;
                    bool on = k < i;
                    rend_.uiShape(2, bx0 + (barW + barGap) * (float)k, y0 + bh - 36.f * u_ - hh, barW, hh, 1, 1, 1, on ? 1.f : 0.22f, 0.3f);
                }
            }
        }
        float fx = px + pw * 0.5f, fy = py + ph - 90.f * u_;
        float fr = 50.f * u_;
        rend_.uiShape(0, fx - fr, fy - fr, fr * 2, fr * 2, showFps_ ? 0.30f : 0.18f, showFps_ ? 0.62f : 0.22f, showFps_ ? 0.18f : 0.28f, 0.95f);
        drawNumber(60, fx - 28.f * u_, fy - 14.f * u_, 28.f * u_);
    }
}
