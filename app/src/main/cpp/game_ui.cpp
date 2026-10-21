#include "game.h"
#include "platform.h"
#include "survival.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>

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

void Game::bar(float x, float y, float w, float h, float frac, float r, float g, float b) {
    rend_.uiShape(2, x, y, w, h, 0.03f, 0.06f, 0.05f, 0.65f, 0.5f);
    float fw = std::max(h, (w - 6.f * u_) * clampf(frac, 0.f, 1.f));
    if (frac > 0.005f) rend_.uiShape(2, x + 3.f * u_, y + 3.f * u_, fw, h - 6.f * u_, r, g, b, 1.f, 0.5f);
}

bool Game::smallButton(float cx, float cy, float r, const char* label, float alpha) {
    bool pressed = uiDown_ && trans_ == 0 && uiInside(uiX_, uiY_, cx - r, cy - r, r * 2, r * 2);
    bool clicked = clickIn(cx - r, cy - r, r * 2, r * 2);
    float k = pressed ? 0.93f : 1.f;
    rend_.uiShape(0, cx - r * k, cy - r * k + 5.f * u_, r * 2 * k, r * 2 * k, 0.05f, 0.14f, 0.07f, 0.6f * alpha);
    rend_.uiShape(0, cx - r * k, cy - r * k, r * 2 * k, r * 2 * k, 0.16f, 0.32f, 0.20f, 0.92f * alpha);
    rend_.uiShape(1, cx - r * k, cy - r * k, r * 2 * k, r * 2 * k, 1.f, 0.85f, 0.4f, 0.9f * alpha, 0.06f);
    float th = r * 0.55f;
    rend_.uiText(label, cx, cy - th * 0.5f, th, 1.f, 0.97f, 0.86f, alpha, 1);
    return clicked;
}

void Game::drawHUD() {
    float W = (float)W_, H = (float)H_;
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

    bool jp = false, ap = false;
    for (auto& t : touches_) { if (t.id >= 0 && t.role == R_JUMP) jp = true; if (t.id >= 0 && t.role == R_ACT) ap = true; }
    if (texJump_) rend_.uiSprite(texJump_, jumpCX_ - jumpR_, jumpCY_ - jumpR_, jumpR_ * 2, jumpR_ * 2, 1, 1, 1, jp ? 1.f : 0.8f, 0, 0, 1, 1);
    else {
        rend_.uiShape(0, jumpCX_ - jumpR_, jumpCY_ - jumpR_, jumpR_ * 2, jumpR_ * 2, 1, 1, 1, jp ? 0.5f : 0.25f);
        rend_.uiShape(1, jumpCX_ - jumpR_, jumpCY_ - jumpR_, jumpR_ * 2, jumpR_ * 2, 1, 1, 1, 0.8f, 0.06f);
        rend_.uiText("JUMP", jumpCX_, jumpCY_ - 14.f * u_, 28.f * u_, 1, 1, 1, 0.95f, 1);
    }

    int it = currentItem();
    const char* actLabel = "HIT";
    if (buildActive_) actLabel = "PLACE";
    else if (it >= 0 && kItems[it].placeable) actLabel = "PLACE";
    else if (it >= 0 && kItems[it].food) actLabel = "EAT";
    float ak = ap ? 0.94f : 1.f;
    rend_.uiShape(0, actCX_ - actR_ * ak, actCY_ - actR_ * ak + 7.f * u_, actR_ * 2 * ak, actR_ * 2 * ak, 0.1f, 0.25f, 0.06f, 0.6f);
    rend_.uiShape(0, actCX_ - actR_ * ak, actCY_ - actR_ * ak, actR_ * 2 * ak, actR_ * 2 * ak, 0.36f, 0.70f, 0.20f, 0.92f);
    rend_.uiShape(1, actCX_ - actR_ * ak, actCY_ - actR_ * ak, actR_ * 2 * ak, actR_ * 2 * ak, 1.f, 0.88f, 0.45f, 0.95f, 0.05f);
    rend_.uiText(actLabel, actCX_, actCY_ - 21.f * u_, 42.f * u_, 1.f, 0.97f, 0.86f, 1.f, 1);

    if (texGear_) rend_.uiSprite(texGear_, gearCX_ - gearR_, gearCY_ - gearR_, gearR_ * 2, gearR_ * 2, 1, 1, 1, 0.9f, 0, 0, 1, 1);
    else {
        rend_.uiShape(0, gearCX_ - gearR_, gearCY_ - gearR_, gearR_ * 2, gearR_ * 2, 0, 0, 0, 0.35f);
        rend_.uiShape(2, gearCX_ - gearR_ * 0.42f, gearCY_ - gearR_ * 0.45f, gearR_ * 0.84f, gearR_ * 0.2f, 1, 1, 1, 0.95f, 1.f);
        rend_.uiShape(2, gearCX_ - gearR_ * 0.42f, gearCY_ - gearR_ * 0.10f, gearR_ * 0.84f, gearR_ * 0.2f, 1, 1, 1, 0.95f, 1.f);
        rend_.uiShape(2, gearCX_ - gearR_ * 0.42f, gearCY_ + gearR_ * 0.25f, gearR_ * 0.84f, gearR_ * 0.2f, 1, 1, 1, 0.95f, 1.f);
    }
    rend_.uiShape(0, bagCX_ - topR_, bagCY_ - topR_, topR_ * 2, topR_ * 2, 0.08f, 0.18f, 0.12f, 0.85f);
    rend_.uiShape(1, bagCX_ - topR_, bagCY_ - topR_, topR_ * 2, topR_ * 2, 1.f, 0.85f, 0.4f, 0.9f, 0.06f);
    rend_.uiText("BAG", bagCX_, bagCY_ - 13.f * u_, 26.f * u_, 1.f, 0.97f, 0.86f, 1.f, 1);
    rend_.uiShape(0, buildCX_ - topR_, buildCY_ - topR_, topR_ * 2, topR_ * 2, 0.08f, 0.18f, 0.12f, 0.85f);
    rend_.uiShape(1, buildCX_ - topR_, buildCY_ - topR_, topR_ * 2, topR_ * 2, 1.f, 0.85f, 0.4f, 0.9f, 0.06f);
    rend_.uiText("MAKE", buildCX_, buildCY_ - 12.f * u_, 24.f * u_, 1.f, 0.97f, 0.86f, 1.f, 1);

    if (buildActive_) {
        struct B { float x, y; const char* l; } bs[5] = {{rotCX_, rotCY_, "TURN"}, {delCX_, delCY_, "DEL"}, {cancelCX_, cancelCY_, "X"}, {lvlUpCX_, lvlUpCY_, "UP"}, {lvlDnCX_, lvlDnCY_, "DOWN"}};
        for (auto& b : bs) {
            rend_.uiShape(0, b.x - subR_, b.y - subR_, subR_ * 2, subR_ * 2, 0.08f, 0.18f, 0.12f, 0.85f);
            rend_.uiShape(1, b.x - subR_, b.y - subR_, subR_ * 2, subR_ * 2, 1.f, 0.85f, 0.4f, 0.9f, 0.06f);
            rend_.uiText(b.l, b.x, b.y - 12.f * u_, 24.f * u_, 1.f, 0.97f, 0.86f, 1.f, 1);
        }
    }

    float cx = W * 0.5f, cy = H * 0.5f;
    bool animalT = animalIdx_ >= 0 && (!target_.valid || animalDist_ <= target_.t + 0.2f);
    bool reach = target_.valid || animalT;
    float cs = (reach ? 12.f : 7.f) * u_;
    rend_.uiShape(0, cx - cs, cy - cs, cs * 2, cs * 2, reach ? 1.f : 1.f, reach ? 0.95f : 1.f, reach ? 0.5f : 1.f, reach ? 0.95f : 0.7f);
    rend_.uiShape(0, cx - cs * 0.45f, cy - cs * 0.45f, cs * 0.9f, cs * 0.9f, 0.f, 0.f, 0.f, 0.35f);
    if (animalT) {
        const Animal& an = animals_[(size_t)animalIdx_];
        const AnimalDef& ad = kAnimals[an.type];
        rend_.uiText(ad.name, cx, cy + 34.f * u_, 30.f * u_, 1.f, 0.7f, 0.6f, 0.95f, 1);
        bar(cx - 90.f * u_, cy + 72.f * u_, 180.f * u_, 18.f * u_, an.hp / ad.hp, 0.9f, 0.3f, 0.3f);
    } else if (reach) {
        const char* nm = target_.kind == HK_TREE || target_.kind == HK_DEAD ? "TREE" : (target_.kind == HK_ROCK ? "ROCK" : (target_.kind == HK_MUSH ? "MUSHROOM" : "STONE"));
        rend_.uiText(nm, cx, cy + 34.f * u_, 30.f * u_, 1, 1, 1, 0.95f, 1);
        uint64_t key = propKey(target_.p.x, target_.p.z, target_.p.model);
        auto f = hp_.find(key);
        int full = (target_.kind == HK_TREE) ? 100 : (target_.kind == HK_DEAD ? 70 : (target_.kind == HK_ROCK ? 100 : 1));
        if (f != hp_.end()) bar(cx - 90.f * u_, cy + 72.f * u_, 180.f * u_, 18.f * u_, (float)f->second / (float)full, 0.95f, 0.75f, 0.25f);
    }
    if (buildActive_ && ghostOn_) {
        const PlaceDef& pd = kPlace[buildPiece_];
        char buf[80];
        if (pd.wood || pd.stone) std::snprintf(buf, sizeof(buf), "%s  W%d S%d  LEVEL %d", pd.name, pd.wood, pd.stone, buildLevel_ + 1);
        else std::snprintf(buf, sizeof(buf), "%s  LEVEL %d", pd.name, buildLevel_ + 1);
        rend_.uiText(buf, cx, H * 0.14f, 36.f * u_, ghostValid_ ? 0.7f : 1.f, ghostValid_ ? 1.f : 0.5f, ghostValid_ ? 0.6f : 0.4f, 1.f, 1);
    }

    float vx = 30.f * u_, vy = 24.f * u_;
    if (showFps_) {
        char fb[24];
        std::snprintf(fb, sizeof(fb), "FPS %d", shownFps_);
        rend_.uiText(fb, vx, vy, 30.f * u_, 1, 1, 1, 0.85f, 0);
        vy += 46.f * u_;
    }
    rend_.uiText("HP", vx, vy - 2.f * u_, 26.f * u_, 1.f, 0.55f, 0.5f, 1.f, 0);
    bar(vx + 70.f * u_, vy, 300.f * u_, 26.f * u_, health_ / 100.f, 0.88f, 0.22f, 0.22f);
    vy += 40.f * u_;
    rend_.uiText("FOOD", vx, vy - 2.f * u_, 26.f * u_, 1.f, 0.8f, 0.45f, 1.f, 0);
    bar(vx + 70.f * u_ + 40.f * u_, vy, 260.f * u_, 26.f * u_, hunger_ / 100.f, 0.95f, 0.62f, 0.18f);
    char clock[48];
    float hrs = tod_ * 24.f;
    std::snprintf(clock, sizeof(clock), "DAY %d  %02d:%02d", day_, (int)hrs % 24, (int)((hrs - std::floor(hrs)) * 60.f));
    rend_.uiText(clock, W * 0.5f, 24.f * u_, 34.f * u_, 1, 1, 1, 0.92f, 1);

    int n = (int)hotbar_.size();
    for (int i = 0; i < n; i++) {
        float sx, sy, sw, sh;
        slotRect(i, sx, sy, sw, sh);
        bool selSlot = i == sel_ && !buildActive_;
        rend_.uiShape(2, sx, sy + 4.f * u_, sw, sh, 0.02f, 0.05f, 0.04f, 0.45f, 0.18f);
        rend_.uiShape(2, sx, sy, sw, sh, selSlot ? 0.30f : 0.10f, selSlot ? 0.55f : 0.20f, selSlot ? 0.18f : 0.14f, 0.88f, 0.18f);
        if (selSlot) rend_.uiShape(2, sx - 3.f * u_, sy - 3.f * u_, sw + 6.f * u_, sh + 6.f * u_, 1.f, 0.85f, 0.4f, 0.0f, 0.18f);
        int itm = hotbar_[(size_t)i];
        if (itm < 0) {
            rend_.uiText("HAND", sx + sw * 0.5f, sy + sh * 0.5f - 11.f * u_, 22.f * u_, 1, 1, 1, 0.9f, 1);
        } else {
            const ItemInfo& inf = kItems[itm];
            rend_.uiShape(0, sx + sw * 0.5f - 18.f * u_, sy + 10.f * u_, 36.f * u_, 36.f * u_, inf.r, inf.g, inf.b, 1.f);
            char nb[12];
            std::snprintf(nb, sizeof(nb), "%.5s", inf.name);
            rend_.uiText(nb, sx + sw * 0.5f, sy + sh - 32.f * u_, 18.f * u_, 1, 1, 1, 0.95f, 1);
            if (!inf.tool) {
                char cb[12];
                std::snprintf(cb, sizeof(cb), "%d", inv_[itm]);
                rend_.uiText(cb, sx + sw - 8.f * u_, sy + 6.f * u_, 22.f * u_, 1, 0.95f, 0.6f, 1.f, 2);
            }
        }
    }
    float py = H * 0.62f;
    for (const Popup& p : popups_) {
        float a = clampf(1.6f - p.t, 0.f, 1.f);
        float rise = p.t * 60.f * u_;
        rend_.uiText(p.text, W * 0.5f, py - rise, 34.f * u_, p.r, p.g, p.b, a, 1);
        py += 40.f * u_;
    }
    if (hurtT_ > 0.f) rend_.uiShape(2, 0, 0, W, H, 0.8f, 0.05f, 0.05f, clampf(hurtT_ * 0.5f, 0.f, 0.3f), 0.f);
    float night = 0.f;
    (void)night;
}

void Game::drawBag() {
    float W = (float)W_, H = (float)H_;
    rend_.uiShape(2, 0, 0, W, H, 0.02f, 0.05f, 0.04f, 0.5f, 0.f);
    float pw = 1500.f * u_, ph = 860.f * u_;
    float pp = easeOutCubic(screenT_ / 0.3f);
    float px = (W - pw) * 0.5f, py = (H - ph) * 0.5f + (1.f - pp) * 50.f * u_;
    drawPanel(px, py, pw, ph, pp);
    rend_.uiText("BACKPACK", px + pw * 0.5f, py + 34.f * u_, 56.f * u_, 1.f, 0.97f, 0.86f, pp, 1);
    float lx = px + 80.f * u_, ly = py + 130.f * u_;
    rend_.uiText("ITEMS", lx, ly, 34.f * u_, 1.f, 0.85f, 0.4f, pp, 0);
    ly += 52.f * u_;
    int shown = 0;
    for (int i = 0; i < IT_COUNT; i++) {
        if (inv_[i] <= 0) continue;
        char buf[40];
        std::snprintf(buf, sizeof(buf), "%s  %d", kItems[i].name, inv_[i]);
        rend_.uiShape(0, lx, ly + 6.f * u_, 22.f * u_, 22.f * u_, kItems[i].r, kItems[i].g, kItems[i].b, pp);
        rend_.uiText(buf, lx + 40.f * u_, ly, 30.f * u_, 1, 1, 1, pp, 0);
        ly += 46.f * u_;
        shown++;
    }
    if (!shown) rend_.uiText("EMPTY", lx, ly, 30.f * u_, 1, 1, 1, pp * 0.6f, 0);
    float rx = px + 560.f * u_, ry = py + 130.f * u_;
    rend_.uiText("CRAFT", rx, ry, 34.f * u_, 1.f, 0.85f, 0.4f, pp, 0);
    ry += 52.f * u_;
    float rw = 860.f * u_, rh = 74.f * u_;
    for (int r = 0; r < kRecipeCount; r++) {
        const Recipe& rc = kRecipes[r];
        bool ok = inv_[rc.c0] >= rc.n0 && (rc.c1 < 0 || inv_[rc.c1] >= rc.n1);
        char cost[64];
        if (rc.c1 >= 0) std::snprintf(cost, sizeof(cost), "%d %s  %d %s", rc.n0, kItems[rc.c0].name, rc.n1, kItems[rc.c1].name);
        else std::snprintf(cost, sizeof(cost), "%d %s", rc.n0, kItems[rc.c0].name);
        bool pressed = uiDown_ && uiInside(uiX_, uiY_, rx, ry, rw, rh) && uiInside(uiSX_, uiSY_, rx, ry, rw, rh);
        rend_.uiShape(2, rx, ry, rw, rh, ok ? (pressed ? 0.26f : 0.34f) : 0.12f, ok ? (pressed ? 0.55f : 0.66f) : 0.2f, ok ? 0.2f : 0.16f, 0.92f * pp, 0.3f);
        rend_.uiText(kItems[rc.result].name, rx + 24.f * u_, ry + 18.f * u_, 36.f * u_, 1.f, 0.97f, 0.86f, ok ? pp : pp * 0.5f, 0);
        rend_.uiText(cost, rx + rw - 24.f * u_, ry + 24.f * u_, 26.f * u_, 1, 1, 1, ok ? pp * 0.95f : pp * 0.5f, 2);
        if (clickIn(rx, ry, rw, rh) && ok) { craft(r); }
        ry += rh + 10.f * u_;
    }
    if (nearCampfire(4.5f)) {
        bool ok = inv_[IT_MUSH] > 0 || inv_[IT_MEAT] > 0;
        const char* cookLabel = inv_[IT_MEAT] > 0 ? "COOK MEAT" : "COOK MUSHROOM";
        bool pressed = uiDown_ && uiInside(uiX_, uiY_, rx, ry, rw, rh) && uiInside(uiSX_, uiSY_, rx, ry, rw, rh);
        rend_.uiShape(2, rx, ry, rw, rh, ok ? (pressed ? 0.7f : 0.85f) : 0.3f, ok ? (pressed ? 0.35f : 0.45f) : 0.2f, 0.12f, 0.92f * pp, 0.3f);
        rend_.uiText(cookLabel, rx + 24.f * u_, ry + 18.f * u_, 36.f * u_, 1.f, 0.97f, 0.86f, ok ? pp : pp * 0.5f, 0);
        if (clickIn(rx, ry, rw, rh) && ok) cook();
    } else {
        rend_.uiText("STAND NEAR A CAMPFIRE TO COOK", rx, ry + 16.f * u_, 26.f * u_, 1, 1, 1, pp * 0.55f, 0);
    }
    if (button(px + (pw - 380.f * u_) * 0.5f, py + ph - 130.f * u_, 380.f * u_, 96.f * u_, "CLOSE", true, pp)) {
        hotbarDirty_ = true;
        screen_ = S_PLAY;
        screenT_ = 0;
        clearTouches();
    }
}

void Game::drawBuildMenu() {
    float W = (float)W_, H = (float)H_;
    rend_.uiShape(2, 0, 0, W, H, 0.02f, 0.05f, 0.04f, 0.5f, 0.f);
    float pw = 1500.f * u_, ph = 860.f * u_;
    float pp = easeOutCubic(screenT_ / 0.3f);
    float px = (W - pw) * 0.5f, py = (H - ph) * 0.5f + (1.f - pp) * 50.f * u_;
    drawPanel(px, py, pw, ph, pp);
    rend_.uiText("BUILD", px + pw * 0.5f, py + 34.f * u_, 56.f * u_, 1.f, 0.97f, 0.86f, pp, 1);
    char have[64];
    std::snprintf(have, sizeof(have), "WOOD %d   STONE %d", inv_[IT_WOOD], inv_[IT_STONE]);
    rend_.uiText(have, px + pw * 0.5f, py + 110.f * u_, 30.f * u_, 1.f, 0.85f, 0.4f, pp, 1);
    float cw = 330.f * u_, ch = 128.f * u_, gx = 26.f * u_, gy = 18.f * u_;
    int cols = 4;
    float x0 = px + (pw - ((float)cols * cw + (float)(cols - 1) * gx)) * 0.5f, y0 = py + 160.f * u_;
    for (int i = 0; i < kBuildPieces; i++) {
        const PlaceDef& pd = kPlace[i];
        float x = x0 + (float)(i % cols) * (cw + gx), y = y0 + (float)(i / cols) * (ch + gy);
        bool ok = canAfford(pd);
        bool pressed = uiDown_ && uiInside(uiX_, uiY_, x, y, cw, ch) && uiInside(uiSX_, uiSY_, x, y, cw, ch);
        rend_.uiShape(2, x, y, cw, ch, ok ? (pressed ? 0.26f : 0.34f) : 0.12f, ok ? (pressed ? 0.55f : 0.66f) : 0.2f, ok ? 0.2f : 0.16f, 0.92f * pp, 0.2f);
        rend_.uiText(pd.name, x + cw * 0.5f, y + 24.f * u_, 30.f * u_, 1.f, 0.97f, 0.86f, ok ? pp : pp * 0.5f, 1);
        char cost[32];
        if (pd.wood && pd.stone) std::snprintf(cost, sizeof(cost), "W%d S%d", pd.wood, pd.stone);
        else if (pd.wood) std::snprintf(cost, sizeof(cost), "WOOD %d", pd.wood);
        else std::snprintf(cost, sizeof(cost), "STONE %d", pd.stone);
        rend_.uiText(cost, x + cw * 0.5f, y + 74.f * u_, 28.f * u_, 1, 1, 1, ok ? pp * 0.95f : pp * 0.5f, 1);
        if (clickIn(x, y, cw, ch)) {
            buildActive_ = true;
            buildPiece_ = i;
            buildFlip_ = 0;
            buildLevel_ = 0;
            screen_ = S_PLAY;
            screenT_ = 0;
            clearTouches();
            return;
        }
    }
    rend_.uiText("PIECES SNAP TO A 2 METER GRID. TURN FLIPS. UP AND DOWN CHANGE LEVEL.", px + pw * 0.5f, py + ph - 175.f * u_, 24.f * u_, 1, 1, 1, pp * 0.7f, 1);
    if (button(px + (pw - 380.f * u_) * 0.5f, py + ph - 130.f * u_, 380.f * u_, 96.f * u_, "CLOSE", true, pp)) {
        screen_ = S_PLAY;
        screenT_ = 0;
        clearTouches();
    }
}

void Game::drawDead() {
    float W = (float)W_, H = (float)H_;
    float pp = easeOutCubic(screenT_ / 0.6f);
    rend_.uiShape(2, 0, 0, W, H, 0.35f, 0.03f, 0.03f, 0.55f * pp, 0.f);
    rend_.uiText("YOU DIED", W * 0.5f, H * 0.30f, 120.f * u_, 1.f, 0.85f, 0.8f, pp, 1);
    rend_.uiText("HALF OF YOUR WOOD AND STONE WAS LOST", W * 0.5f, H * 0.30f + 150.f * u_, 32.f * u_, 1, 1, 1, pp * 0.9f, 1);
    if (screenT_ > 0.8f && button((W - 520.f * u_) * 0.5f, H * 0.62f, 520.f * u_, 130.f * u_, "RESPAWN", true, 1.f)) respawn();
}
