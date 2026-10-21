#include "game.h"
#include "platform.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace {
const float kPi = 3.14159265f;
}

void Game::initSurvival() {
    mAxe_ = pak_.find("Axe_Bronze");
    mPick_ = pak_.find("Pickaxe_Bronze");
    mSword_ = pak_.find("Sword_Bronze");
    mTorch_ = pak_.find("WoodenTorch");
    mTorchFire_ = pak_.find("WoodenTorch_Fire");
    std::memset(inv_, 0, sizeof(inv_));
    rebuildHotbar();
}

void Game::addPopup(const char* text, float r, float g, float b) {
    if (popups_.size() > 5) popups_.erase(popups_.begin());
    Popup p;
    std::snprintf(p.text, sizeof(p.text), "%s", text);
    p.t = 0.f;
    p.r = r; p.g = g; p.b = b;
    popups_.push_back(p);
}

void Game::giveItem(int item, int n) {
    inv_[item] += n;
    hotbarDirty_ = true;
}

int Game::currentItem() const {
    if (sel_ < 0 || sel_ >= (int)hotbar_.size()) return -1;
    return hotbar_[(size_t)sel_];
}

void Game::rebuildHotbar() {
    int keep = currentItem();
    hotbar_.clear();
    hotbar_.push_back(-1);
    static const int order[] = {IT_AXE, IT_PICK, IT_SWORD, IT_TORCH, IT_CAMPFIRE, IT_BENCH, IT_BED, IT_CHEST, IT_MUSH, IT_COOKED, IT_MEAT, IT_COOKMEAT};
    for (int it : order) if (inv_[it] > 0) hotbar_.push_back(it);
    sel_ = 0;
    for (size_t i = 0; i < hotbar_.size(); i++) if (hotbar_[i] == keep) sel_ = (int)i;
    hotbarDirty_ = false;
}

bool Game::canAfford(const PlaceDef& pd) const {
    if (pd.item >= 0) return inv_[pd.item] > 0;
    return inv_[IT_WOOD] >= pd.wood && inv_[IT_STONE] >= pd.stone;
}

bool Game::craft(int r) {
    if (r < 0 || r >= kRecipeCount) return false;
    const Recipe& rc = kRecipes[r];
    if (inv_[rc.c0] < rc.n0) return false;
    if (rc.c1 >= 0 && inv_[rc.c1] < rc.n1) return false;
    inv_[rc.c0] -= rc.n0;
    if (rc.c1 >= 0) inv_[rc.c1] -= rc.n1;
    giveItem(rc.result, 1);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "CRAFTED %s", kItems[rc.result].name);
    addPopup(buf, 0.6f, 1.f, 0.5f);
    return true;
}

bool Game::nearCampfire(float radius) const {
    for (const Placed& p : placed_) {
        if (p.def != PD_CAMPFIRE) continue;
        float dx = p.x - pos_.x, dz = p.z - pos_.z;
        if (dx * dx + dz * dz <= radius * radius) return true;
    }
    return false;
}

bool Game::cook() {
    if (!nearCampfire(4.5f)) return false;
    if (inv_[IT_MEAT] > 0) {
        inv_[IT_MEAT]--;
        giveItem(IT_COOKMEAT, 1);
        addPopup("COOKED MEAT", 1.f, 0.7f, 0.3f);
        return true;
    }
    if (inv_[IT_MUSH] > 0) {
        inv_[IT_MUSH]--;
        giveItem(IT_COOKED, 1);
        addPopup("COOKED", 1.f, 0.7f, 0.3f);
        return true;
    }
    return false;
}

void Game::damagePlayer(float amt) {
    health_ -= amt;
    if (amt > 0.5f) hurtT_ = 0.45f;
}

void Game::respawn() {
    pos_ = spawn_;
    pos_.y = world_.height(pos_.x, pos_.z);
    health_ = 70.f;
    hunger_ = 70.f;
    inv_[IT_WOOD] /= 2;
    inv_[IT_STONE] /= 2;
    if (tod_ > 0.75f || tod_ < 0.22f) tod_ = 0.30f;
    animals_.clear();
    hotbarDirty_ = true;
    clearTouches();
    screen_ = S_PLAY;
    screenT_ = 0;
}

void Game::sleepInBed() {
    bool night = tod_ > 0.76f || tod_ < 0.2f;
    if (!night) { addPopup("NOT TIRED YET", 1.f, 0.9f, 0.5f); return; }
    if (tod_ > 0.5f) day_++;
    tod_ = 0.27f;
    health_ = std::min(100.f, health_ + 30.f);
    addPopup("GOOD MORNING", 1.f, 0.95f, 0.6f);
    saveGame();
}

void Game::updateSurvival(float dt) {
    if (hotbarDirty_) rebuildHotbar();
    float mv = std::sqrt(velX_ * velX_ + velZ_ * velZ_);
    hunger_ -= dt * (100.f / 1500.f) * (1.f + mv * 0.05f);
    if (hunger_ < 0.f) hunger_ = 0.f;
    if (hunger_ <= 0.f) damagePlayer(dt * 1.6f);
    else if (hunger_ > 55.f && health_ < 100.f) health_ = std::min(100.f, health_ + dt * 1.2f);
    hurtT_ = std::max(0.f, hurtT_ - dt);
    for (size_t i = 0; i < popups_.size();) {
        popups_[i].t += dt;
        if (popups_[i].t > 1.6f) popups_.erase(popups_.begin() + (long)i); else i++;
    }
    if (health_ <= 0.f) {
        health_ = 0.f;
        deathCount_++;
        clearTouches();
        screen_ = S_DEAD;
        screenT_ = 0;
        saveGame();
    }
}

void Game::computeAtmos(Atmos& a) const {
    float ang = (tod_ - 0.25f) * 6.2831853f;
    V3 sun = vnorm({std::cos(ang) * 0.9f, std::sin(ang), 0.35f});
    V3 moon = vnorm({-sun.x, -sun.y, -sun.z});
    float elev = sun.y;
    float day = smoothstepf(-0.06f, 0.22f, elev);
    float warm = (1.f - smoothstepf(0.0f, 0.35f, elev)) * smoothstepf(-0.14f, 0.04f, elev);
    a.night = 1.f - smoothstepf(-0.18f, 0.05f, elev);
    auto mix3 = [](V3 x, V3 y, float t) { return V3{lerpf(x.x, y.x, t), lerpf(x.y, y.y, t), lerpf(x.z, y.z, t)}; };
    a.sunCol = mix3(V3{0.14f, 0.18f, 0.34f}, mix3(V3{0.78f, 0.68f, 0.50f}, V3{0.95f, 0.50f, 0.22f}, warm), day);
    a.ambTop = mix3(mix3(V3{0.10f, 0.14f, 0.28f}, V3{0.56f, 0.66f, 0.80f}, day), V3{0.66f, 0.52f, 0.58f}, warm * 0.6f);
    a.ambBot = mix3(V3{0.07f, 0.08f, 0.11f}, V3{0.40f, 0.38f, 0.30f}, day);
    a.fogCol = mix3(mix3(V3{0.04f, 0.07f, 0.15f}, V3{0.84f, 0.92f, 0.98f}, day), V3{0.96f, 0.62f, 0.42f}, warm * 0.75f);
    a.zenith = mix3(mix3(V3{0.015f, 0.03f, 0.09f}, V3{0.36f, 0.62f, 0.95f}, day), V3{0.30f, 0.38f, 0.70f}, warm * 0.6f);
    a.lightDir = vnorm(sun * day + moon * (1.f - day) + V3{0.f, 0.3f, 0.f});
    a.sunDisc = sun;
    a.moonDir = moon;
}

void Game::updateLights(const Camera& cam) {
    PLight L[2];
    int n = 0;
    float best = 60.f * 60.f;
    const Placed* bp = nullptr;
    for (const Placed& p : placed_) {
        if (p.def != PD_CAMPFIRE) continue;
        float dx = p.x - cam.pos.x, dz = p.z - cam.pos.z;
        float d2 = dx * dx + dz * dz;
        if (d2 < best) { best = d2; bp = &p; }
    }
    if (bp) {
        float fl = 0.88f + 0.12f * std::sin(time_ * 17.f) * std::sin(time_ * 7.3f) + 0.05f * std::sin(time_ * 31.f);
        L[n].pos = {bp->x, bp->y + 0.9f, bp->z};
        L[n].radius = 17.f;
        L[n].col = {1.0f * fl * 1.3f, 0.55f * fl * 1.3f, 0.20f * fl * 1.3f};
        n++;
    }
    if (screen_ == S_PLAY && currentItem() == IT_TORCH) {
        float fl = 0.9f + 0.1f * std::sin(time_ * 19.f);
        V3 f = camForward();
        L[n].pos = cam.pos + f * 0.8f + V3{0, -0.1f, 0};
        L[n].radius = 14.f;
        L[n].col = {1.0f * fl * 1.1f, 0.62f * fl * 1.1f, 0.28f * fl * 1.1f};
        n++;
    }
    rend_.setLights(L, n);
}

void Game::updateViewModel(const Camera& cam) {
    ViewModelState vm;
    int it = currentItem();
    if (!buildActive_ && (it == IT_AXE || it == IT_PICK || it == IT_TORCH || it == IT_SWORD)) {
        float sw = swingT_ >= 0.f ? std::sin(swingT_ * kPi) : 0.f;
        float scale = 1.f, yawOff = 0.f, roll = 0.f, pitchOff = -0.3f;
        if (it == IT_AXE) { vm.model = mAxe_; scale = 0.75f; yawOff = -kPi * 0.5f; roll = -kPi * 0.5f; pitchOff = -0.35f; }
        else if (it == IT_PICK) { vm.model = mPick_; scale = 0.7f; yawOff = kPi * 0.5f; roll = 0.f; pitchOff = -0.35f; }
        else if (it == IT_SWORD) { vm.model = mSword_; scale = 0.8f; yawOff = 0.f; roll = 0.f; pitchOff = -0.6f; }
        else { vm.model = mTorch_; vm.fire = mTorchFire_; scale = 1.0f; pitchOff = -0.25f; }
        if (vm.model >= 0) {
            float cp = std::cos(cam.pitch), sp = std::sin(cam.pitch);
            V3 f = {std::sin(cam.yaw) * cp, sp, -std::cos(cam.yaw) * cp};
            V3 r = vnorm(cross(f, V3{0, 1, 0}));
            V3 u = cross(r, f);
            vm.pos = cam.pos + r * 0.30f + u * (-0.30f + bob_ * 0.5f) + f * (0.58f + sw * 0.12f);
            vm.yaw = -cam.yaw + yawOff;
            vm.pitch = cam.pitch + pitchOff - sw * 1.15f;
            vm.roll = roll;
            vm.scale = scale;
            vm.on = true;
        }
    }
    rend_.setViewModel(vm);
}

void Game::updateTarget() {
    target_ = PropHit();
    if (buildActive_) return;
    Camera c = playCamera();
    target_ = cm_->raycast(c.pos, camForward(), 3.6f);
}

static int hpFor(int kind) {
    switch (kind) {
        case HK_TREE: return 100;
        case HK_DEAD: return 70;
        case HK_ROCK: return 100;
        default: return 1;
    }
}

static int damageFor(int kind, int tool) {
    switch (kind) {
        case HK_TREE: return tool == IT_AXE ? 34 : (tool == IT_PICK ? 10 : (tool == IT_SWORD ? 8 : (tool == IT_TORCH ? 5 : 12)));
        case HK_DEAD: return tool == IT_AXE ? 50 : (tool == IT_PICK ? 16 : (tool == IT_SWORD ? 10 : 20));
        case HK_ROCK: return tool == IT_PICK ? 28 : (tool == IT_AXE ? 6 : (tool == IT_SWORD ? 3 : 8));
        default: return 100;
    }
}

void Game::doAct() {
    if (screen_ != S_PLAY || cooldown_ > 0.f) return;
    if (buildActive_) {
        if (ghostValid_) placeGhost();
        cooldown_ = 0.3f;
        return;
    }
    int it = currentItem();
    if (it >= 0 && kItems[it].placeable) {
        if (ghostValid_) placeGhost();
        else addPopup("CANT PLACE HERE", 1.f, 0.5f, 0.4f);
        cooldown_ = 0.35f;
        return;
    }
    if (it >= 0 && kItems[it].food) {
        hunger_ = std::min(100.f, hunger_ + kItems[it].hunger);
        inv_[it]--;
        hotbarDirty_ = true;
        char buf[32];
        std::snprintf(buf, sizeof(buf), "+%d FOOD", (int)kItems[it].hunger);
        addPopup(buf, 1.f, 0.8f, 0.4f);
        cooldown_ = 0.6f;
        return;
    }
    for (const Placed& p : placed_) {
        if (p.def != PD_BED) continue;
        float dx = p.x - pos_.x, dz = p.z - pos_.z;
        if (dx * dx + dz * dz < 2.8f * 2.8f && !target_.valid && animalIdx_ < 0) {
            sleepInBed();
            cooldown_ = 1.0f;
            return;
        }
    }
    swingT_ = 0.f;
    cooldown_ = it == IT_SWORD ? 0.34f : 0.42f;
    bool animalFirst = animalIdx_ >= 0 && (!target_.valid || animalDist_ <= target_.t + 0.2f);
    if (animalFirst) {
        float dmg = it == IT_SWORD ? 36.f : (it == IT_AXE ? 22.f : (it == IT_PICK ? 14.f : (it == IT_TORCH ? 10.f : 7.f)));
        kick_ += 0.03f;
        hitAnimal(animalIdx_, dmg);
        return;
    }
    if (!target_.valid) return;
    int kind = target_.kind;
    int dmg = damageFor(kind, it);
    kick_ += 0.035f;
    uint64_t key = propKey(target_.p.x, target_.p.z, target_.p.model);
    int cur;
    auto f = hp_.find(key);
    cur = f == hp_.end() ? hpFor(kind) : f->second;
    cur -= dmg;
    if (cur > 0) { hp_[key] = cur; return; }
    hp_.erase(key);
    uint32_t h = hash32((uint32_t)(key >> 7));
    int item = IT_WOOD, n = 1;
    if (kind == HK_TREE) { item = IT_WOOD; n = 3 + (int)(h % 3); }
    else if (kind == HK_DEAD) { item = IT_WOOD; n = 2 + (int)(h % 2); }
    else if (kind == HK_ROCK) { item = IT_STONE; n = 3 + (int)(h % 4); }
    else if (kind == HK_PEBBLE) { item = IT_STONE; n = 1; }
    else if (kind == HK_MUSH) { item = IT_MUSH; n = 1; }
    cm_->removeProp(target_.p);
    giveItem(item, n);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "+%d %s", n, kItems[item].name);
    addPopup(buf, kItems[item].r, kItems[item].g, kItems[item].b);
    target_ = PropHit();
}

float Game::floorTop(float x, float z, float ref) const {
    float best = -1e9f;
    for (const Placed& p : placed_) {
        const PlaceDef& pd = kPlace[p.def];
        if (pd.kind == PK_FLOOR) {
            if (std::fabs(p.x - x) > 1.0f || std::fabs(p.z - z) > 1.0f) continue;
            float top = p.y + 0.02f;
            if (top <= ref + 0.6f) best = std::max(best, top);
        } else if (pd.ramp) {
            float dx = x - p.x, dz = z - p.z;
            float c = std::cos(p.yaw), s = std::sin(p.yaw);
            float lx = c * dx - s * dz, lz = s * dx + c * dz;
            if (std::fabs(lx) > kStairHalfW || lz > 0.36f || lz < -4.35f) continue;
            float t = clampf((0.3f - lz) / kStairRun, 0.f, 1.f);
            float top = p.y + kStairRise * t;
            if (top <= ref + 0.6f) best = std::max(best, top);
        }
    }
    return best;
}

float Game::groundAt(float x, float z, float ref) const {
    return std::max(world_.height(x, z), floorTop(x, z, ref));
}

bool Game::aimGround(V3& out) const {
    Camera c = playCamera();
    V3 d = camForward();
    float ref = pos_.y;
    float prev = 0.6f;
    for (float t = 0.6f; t <= 8.f; t += 0.2f) {
        V3 p = c.pos + d * t;
        float g = groundAt(p.x, p.z, ref);
        if (p.y <= g) {
            float lo = prev, hi = t;
            for (int i = 0; i < 6; i++) {
                float mid = 0.5f * (lo + hi);
                V3 q = c.pos + d * mid;
                if (q.y <= groundAt(q.x, q.z, ref)) hi = mid; else lo = mid;
            }
            V3 q = c.pos + d * hi;
            out = {q.x, groundAt(q.x, q.z, ref), q.z};
            return true;
        }
        prev = t;
    }
    return false;
}

bool Game::aimPlane(float y, V3& out) const {
    Camera c = playCamera();
    V3 d = camForward();
    if (std::fabs(d.y) < 0.04f) return false;
    float t = (y - c.pos.y) / d.y;
    if (t < 0.8f || t > 10.f) return false;
    V3 p = c.pos + d * t;
    out = {p.x, y, p.z};
    return true;
}

void Game::computeGhost() {
    ghostOn_ = false;
    ghostValid_ = false;
    ghostDef_ = -1;
    int def = -1;
    if (buildActive_) def = buildPiece_;
    else {
        int it = currentItem();
        if (it >= 0 && kItems[it].placeable) def = placeDefForItem(it);
    }
    if (def < 0) return;
    const PlaceDef& pd = kPlace[def];
    int model = modelOf(pd.model);
    if (model < 0) return;
    const int level = buildActive_ ? buildLevel_ : 0;
    const float ref = pos_.y;
    const float planeY = ref + (float)level * kLevelH;
    V3 P;
    bool hit = level > 0 ? aimPlane(planeY, P) : aimGround(P);
    if (!hit) {
        V3 f = camForward();
        float l = std::sqrt(f.x * f.x + f.z * f.z);
        if (l < 1e-3f) return;
        P = {pos_.x + f.x / l * 4.f, 0.f, pos_.z + f.z / l * 4.f};
        P.y = level > 0 ? planeY : groundAt(P.x, P.z, ref);
        if (level > 0) return;
    }
    PropInst g{};
    g.model = (uint16_t)model;
    g.s = 1.f;
    g.tint = 0xFFFFFFFFu;
    bool ok = canAfford(pd);
    float pdx = P.x - pos_.x, pdz = P.z - pos_.z;
    if (pdx * pdx + pdz * pdz > 10.f * 10.f) ok = false;
    if (pd.kind == PK_FLOOR) {
        float cx = std::round(P.x / 2.f) * 2.f, cz = std::round(P.z / 2.f) * 2.f;
        float hmax = -1e9f, hmin = 1e9f;
        for (int i = 0; i < 5; i++) {
            float hx = cx + (i == 1 ? -1.f : (i == 2 ? 1.f : 0.f)), hz = cz + (i == 3 ? -1.f : (i == 4 ? 1.f : 0.f));
            float h = world_.height(hx, hz);
            hmax = std::max(hmax, h);
            hmin = std::min(hmin, h);
        }
        float y;
        bool supported = false;
        if (level == 0) {
            float ty = hmax + 0.2f;
            y = ty;
            for (const Placed& p : placed_) {
                if (kPlace[p.def].kind != PK_FLOOR) continue;
                if (std::fabs(p.x - cx) <= 2.05f && std::fabs(p.z - cz) <= 2.05f && std::fabs(p.y - ty) < 1.6f) { y = p.y; break; }
            }
            supported = true;
            if (hmax - hmin > 1.6f) ok = false;
        } else {
            y = planeY;
            float ex[4] = {cx, cx, cx - 1.f, cx + 1.f}, ez[4] = {cz - 1.f, cz + 1.f, cz, cz};
            for (const Placed& p : placed_) {
                const PlaceDef& q = kPlace[p.def];
                if (q.kind == PK_EDGE && q.coll != CK_NONE) {
                    for (int i = 0; i < 4; i++)
                        if (std::fabs(p.x - ex[i]) < 0.4f && std::fabs(p.z - ez[i]) < 0.4f && std::fabs(p.y + 3.1f - planeY) < 1.2f) { y = p.y + 3.1f; supported = true; }
                } else if (q.kind == PK_FLOOR && std::fabs(p.x - cx) <= 2.05f && std::fabs(p.z - cz) <= 2.05f && std::fabs(p.y - planeY) < 1.6f) {
                    y = p.y;
                    supported = true;
                }
            }
        }
        if (!supported) ok = false;
        for (const Placed& p : placed_) {
            if (kPlace[p.def].kind != PK_FLOOR) continue;
            if (std::fabs(p.x - cx) < 0.5f && std::fabs(p.z - cz) < 0.5f && std::fabs(p.y - y) < 1.0f) ok = false;
        }
        g.x = cx; g.y = y; g.z = cz; g.yaw = 0.f;
    } else if (pd.kind == PK_EDGE) {
        float tx = std::round(P.x / 2.f) * 2.f, tz = std::round(P.z / 2.f) * 2.f;
        float ex[4] = {tx, tx, tx - 1.f, tx + 1.f}, ez[4] = {tz - 1.f, tz + 1.f, tz, tz};
        bool ax[4] = {true, true, false, false};
        int bi = 0;
        float bd = 1e9f;
        for (int i = 0; i < 4; i++) {
            float d2 = (ex[i] - P.x) * (ex[i] - P.x) + (ez[i] - P.z) * (ez[i] - P.z);
            if (d2 < bd) { bd = d2; bi = i; }
        }
        float x = ex[bi], z = ez[bi];
        bool alongX = ax[bi];
        float yaw = alongX ? 0.f : kPi * 0.5f;
        if (buildFlip_ & 1) yaw += kPi;
        float refY = level > 0 ? planeY : world_.height(x, z) + 0.2f;
        float fy = -1e9f, fd = 1e9f;
        for (const Placed& p : placed_) {
            if (kPlace[p.def].kind != PK_FLOOR) continue;
            if (std::fabs(p.x - x) <= 1.05f && std::fabs(p.z - z) <= 1.05f) {
                float d = std::fabs(p.y - refY);
                if (d < 1.6f && d < fd) { fd = d; fy = p.y; }
            }
        }
        float base = 0.f;
        bool haveBase = false;
        if (fy > -1e8f) { base = fy; haveBase = true; }
        else if (level == 0) {
            float h0 = world_.height(x, z);
            float h1 = world_.height(x + (alongX ? 1.f : 0.f), z + (alongX ? 0.f : 1.f));
            float h2 = world_.height(x - (alongX ? 1.f : 0.f), z - (alongX ? 0.f : 1.f));
            base = std::max(h0, std::max(h1, h2));
            if (base - std::min(h0, std::min(h1, h2)) > 1.4f) ok = false;
            haveBase = true;
        } else {
            for (const Placed& p : placed_) {
                const PlaceDef& q = kPlace[p.def];
                if (q.kind != PK_EDGE || q.coll == CK_NONE) continue;
                bool pAlongX = std::fabs(std::sin(p.yaw)) < 0.5f;
                if (pAlongX == alongX && std::fabs(p.x - x) < 0.3f && std::fabs(p.z - z) < 0.3f && std::fabs(p.y + 3.1f - planeY) < 1.2f) { base = p.y + 3.1f; haveBase = true; }
            }
        }
        if (!haveBase) { ok = false; base = planeY; }
        float px = x, pz = z;
        if (pd.inset > 0.f) {
            px = x - pd.inset * std::sin(yaw);
            pz = z - pd.inset * std::cos(yaw);
        }
        g.x = px; g.z = pz; g.y = base + pd.yOff; g.yaw = yaw;
        for (const Placed& p : placed_) {
            if (kPlace[p.def].kind != PK_EDGE) continue;
            bool pAlongX = std::fabs(std::sin(p.yaw)) < 0.5f;
            if (pAlongX == alongX && std::fabs(p.x - px) < 0.3f && std::fabs(p.z - pz) < 0.3f && std::fabs(p.y - g.y) < 0.5f) ok = false;
        }
    } else {
        float x = P.x, z = P.z;
        float yaw = -yaw_ + (float)(buildFlip_ & 3) * kPi * 0.5f;
        if (pd.ramp) {
            x = std::round(P.x);
            z = std::round(P.z);
            yaw = std::round(-yaw_ / (kPi * 0.5f)) * kPi * 0.5f + (float)(buildFlip_ & 3) * kPi * 0.5f;
        }
        float y = level > 0 ? planeY : groundAt(x, z, ref + 0.6f);
        if (level == 0) {
            float sx = std::fabs(world_.height(x + 0.7f, z) - world_.height(x - 0.7f, z));
            float sz = std::fabs(world_.height(x, z + 0.7f) - world_.height(x, z - 0.7f));
            if ((sx > 0.8f || sz > 0.8f) && !pd.ramp) ok = false;
        }
        for (const Placed& p : placed_) {
            const PlaceDef& q = kPlace[p.def];
            if (q.kind != PK_FREE) continue;
            float dx = p.x - x, dz = p.z - z;
            float rr = pd.ramp || q.ramp ? 2.2f : q.colR + pd.colR;
            if (std::fabs(p.y - y) < 1.5f && dx * dx + dz * dz < rr * rr) ok = false;
        }
        g.x = x; g.y = y; g.z = z; g.yaw = yaw;
    }
    ghost_ = g;
    ghostOn_ = true;
    ghostValid_ = ok;
    ghostDef_ = def;
}

bool Game::placeGhost() {
    if (!ghostOn_ || !ghostValid_ || ghostDef_ < 0) return false;
    const PlaceDef& pd = kPlace[ghostDef_];
    if (pd.item >= 0) { inv_[pd.item]--; hotbarDirty_ = true; }
    else { inv_[IT_WOOD] -= pd.wood; inv_[IT_STONE] -= pd.stone; }
    placed_.push_back({ghostDef_, ghost_.x, ghost_.y, ghost_.z, ghost_.yaw});
    rebuildExtras();
    saveGame();
    return true;
}

bool Game::removeAimed() {
    Camera c = playCamera();
    V3 d = camForward();
    int best = -1;
    float bp = 1e9f;
    for (size_t i = 0; i < placed_.size(); i++) {
        const Placed& p = placed_[i];
        V3 w = V3{p.x, p.y + 1.0f, p.z} - c.pos;
        float t = dot(w, d);
        if (t < 0.3f || t > 6.f) continue;
        float perp2 = dot(w, w) - t * t;
        float r = kPlace[p.def].kind == PK_FLOOR ? 1.1f : 1.4f;
        if (perp2 < r * r && perp2 < bp) { bp = perp2; best = (int)i; }
    }
    if (best < 0) { addPopup("NOTHING TO REMOVE", 1.f, 0.7f, 0.4f); return false; }
    const PlaceDef& pd = kPlace[placed_[(size_t)best].def];
    if (pd.item >= 0) inv_[pd.item]++;
    else { inv_[IT_WOOD] += pd.wood; inv_[IT_STONE] += pd.stone; }
    hotbarDirty_ = true;
    placed_.erase(placed_.begin() + best);
    rebuildExtras();
    addPopup("REMOVED", 0.9f, 0.9f, 0.9f);
    return true;
}

void Game::rebuildExtras() {
    extras_.clear();
    fireIdx_.clear();
    for (const Placed& p : placed_) {
        const PlaceDef& pd = kPlace[p.def];
        int m = modelOf(pd.model);
        if (m < 0) continue;
        PropInst e{};
        e.x = p.x; e.y = p.y; e.z = p.z; e.s = 1.f; e.yaw = p.yaw; e.tint = 0xFFFFFFFFu; e.model = (uint16_t)m;
        extras_.push_back(e);
        if (pd.fire) {
            int fm = modelOf(pd.fire);
            if (fm >= 0) {
                e.model = (uint16_t)fm;
                fireIdx_.push_back((int)extras_.size());
                extras_.push_back(e);
            }
        }
    }
}

void Game::flickerFires() {
    for (int idx : fireIdx_) {
        PropInst& e = extras_[(size_t)idx];
        e.s = 1.0f + 0.10f * std::sin(time_ * 14.f + (float)idx) + 0.06f * std::sin(time_ * 27.f + (float)idx * 2.f);
        e.yaw += 0.02f;
    }
}

void Game::collidePlaced(float& x, float& z, float radius) const {
    for (const Placed& p : placed_) {
        const PlaceDef& pd = kPlace[p.def];
        if (pd.coll == CK_NONE) continue;
        float dx = x - p.x, dz = z - p.z;
        if (dx * dx + dz * dz > 6.f * 6.f) continue;
        if (pd.coll == CK_CIRCLE) {
            float rr = pd.colR + radius;
            float d2 = dx * dx + dz * dz;
            if (d2 < rr * rr && d2 > 1e-6f) {
                float d = std::sqrt(d2);
                x += dx / d * (rr - d);
                z += dz / d * (rr - d);
            }
            continue;
        }
        float c = std::cos(p.yaw), s = std::sin(p.yaw);
        float lx = c * dx - s * dz, lz = s * dx + c * dz;
        struct Box { float cx, cz, hx, hz; };
        Box boxes[2];
        int nb = 0;
        if (pd.coll == CK_WALL) boxes[nb++] = {0.f, -0.005f, 1.0f, 0.31f};
        else if (pd.coll == CK_DOOR) { boxes[nb++] = {-0.81f, -0.005f, 0.19f, 0.31f}; boxes[nb++] = {0.81f, -0.005f, 0.19f, 0.31f}; }
        else boxes[nb++] = {0.f, 0.f, 1.0f, 0.12f};
        for (int i = 0; i < nb; i++) {
            const Box& b = boxes[i];
            float qx = clampf(lx, b.cx - b.hx, b.cx + b.hx), qz = clampf(lz, b.cz - b.hz, b.cz + b.hz);
            float ox = lx - qx, oz = lz - qz;
            float d2 = ox * ox + oz * oz;
            float px = 0.f, pz = 0.f;
            if (d2 > 1e-8f) {
                if (d2 < radius * radius) {
                    float d = std::sqrt(d2);
                    px = ox / d * (radius - d);
                    pz = oz / d * (radius - d);
                } else continue;
            } else {
                float ex = b.hx - std::fabs(lx - b.cx), ez = b.hz - std::fabs(lz - b.cz);
                if (ex < ez) px = (lx >= b.cx ? 1.f : -1.f) * (ex + radius);
                else pz = (lz >= b.cz ? 1.f : -1.f) * (ez + radius);
            }
            float wx = c * px + s * pz, wz = -s * px + c * pz;
            x += wx;
            z += wz;
            dx = x - p.x;
            dz = z - p.z;
            lx = c * dx - s * dz;
            lz = s * dx + c * dz;
        }
    }
}

void Game::saveGame() {
    std::string path = plat_data_dir() + "/save.txt";
    std::string tmp = path + ".tmp";
    FILE* f = std::fopen(tmp.c_str(), "w");
    if (!f) return;
    std::fprintf(f, "T %.5f %d\n", tod_, day_);
    std::fprintf(f, "P %.3f %.3f %.3f %.4f %.2f %.2f\n", pos_.x, pos_.y, pos_.z, yaw_, health_, hunger_);
    std::fprintf(f, "I");
    for (int i = 0; i < IT_COUNT; i++) std::fprintf(f, " %d", inv_[i]);
    std::fprintf(f, "\n");
    for (const Placed& p : placed_) std::fprintf(f, "S %s %.3f %.3f %.3f %.4f\n", kPlace[p.def].id, p.x, p.y, p.z, p.yaw);
    if (cm_) {
        std::vector<uint64_t> rl = cm_->removedList();
        for (uint64_t k : rl) std::fprintf(f, "R %llu\n", (unsigned long long)k);
    }
    std::fclose(f);
    std::rename(tmp.c_str(), path.c_str());
}

void Game::loadGame() {
    std::string path = plat_data_dir() + "/save.txt";
    FILE* f = std::fopen(path.c_str(), "r");
    if (!f) return;
    char line[256];
    std::vector<uint64_t> removed;
    while (std::fgets(line, sizeof(line), f)) {
        if (line[0] == 'T') {
            float t; int d;
            if (std::sscanf(line, "T %f %d", &t, &d) == 2) { tod_ = clampf(t, 0.f, 0.9999f); day_ = std::max(1, d); }
        } else if (line[0] == 'P') {
            float x, y, z, yw, hp, hg;
            if (std::sscanf(line, "P %f %f %f %f %f %f", &x, &y, &z, &yw, &hp, &hg) == 6) {
                pos_ = {x, y, z};
                yaw_ = yw;
                health_ = clampf(hp, 1.f, 100.f);
                hunger_ = clampf(hg, 0.f, 100.f);
            }
        } else if (line[0] == 'I') {
            const char* p = line + 1;
            for (int i = 0; i < IT_COUNT; i++) {
                int n = 0, used = 0;
                if (std::sscanf(p, " %d%n", &n, &used) != 1) break;
                inv_[i] = std::max(0, n);
                p += used;
            }
        } else if (line[0] == 'S') {
            char id[32];
            float x, y, z, yw;
            if (std::sscanf(line, "S %31s %f %f %f %f", id, &x, &y, &z, &yw) == 5) {
                for (int i = 0; i < kPlaceCount; i++)
                    if (std::strcmp(kPlace[i].id, id) == 0) { placed_.push_back({i, x, y, z, yw}); break; }
            }
        } else if (line[0] == 'R') {
            unsigned long long k;
            if (std::sscanf(line, "R %llu", &k) == 1) removed.push_back((uint64_t)k);
        }
    }
    std::fclose(f);
    if (cm_) cm_->setRemoved(removed);
    rebuildExtras();
    hotbarDirty_ = true;
}
