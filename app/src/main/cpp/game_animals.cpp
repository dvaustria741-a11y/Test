#include "game.h"
#include "platform.h"
#include <cmath>
#include <cstdio>
#include <algorithm>

namespace {
uint32_t g_rng = 0x1234abcdu;
float frand() {
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return (float)(g_rng >> 8) * (1.0f / 16777216.0f);
}
const float kPi2 = 6.2831853f;

float angleDiff(float a, float b) {
    float d = std::fmod(b - a + 3.14159265f, kPi2);
    if (d < 0) d += kPi2;
    return d - 3.14159265f;
}
}  // namespace

void Game::spawnAnimal(int type, float x, float z) {
    const AnimalDef& d = kAnimals[type];
    int variant = (int)(frand() * (float)d.nModels) % d.nModels;
    int m = pak_.find(d.models[variant]);
    if (m < 0) return;
    Animal a;
    a.type = type;
    a.model = m;
    a.x = x;
    a.z = z;
    a.y = world_.height(x, z);
    a.yaw = frand() * kPi2;
    a.hp = d.hp;
    a.state = 0;
    a.timer = 1.f + frand() * 3.f;
    a.scale = 0.92f + frand() * 0.16f;
    animals_.push_back(a);
}

void Game::trySpawn() {
    bool night = tod_ > 0.78f || tod_ < 0.2f;
    size_t cap = night ? 15 : 11;
    if (animals_.size() >= cap) return;
    float ang = frand() * kPi2;
    float dist = 55.f + frand() * 30.f;
    float x = pos_.x + std::cos(ang) * dist, z = pos_.z + std::sin(ang) * dist;
    Biome b = world_.biome(x, z);
    float r = frand();
    int type;
    if (night) {
        if (r < 0.50f) type = AN_WOLF;
        else if (r < 0.58f) type = AN_BEAR;
        else if (r < 0.70f) type = AN_BOAR;
        else if (r < 0.82f) type = AN_DEER;
        else if (r < 0.91f) type = AN_RABBIT;
        else type = AN_FOX;
    } else {
        if (r < 0.28f) type = AN_DEER;
        else if (r < 0.52f) type = AN_RABBIT;
        else if (r < 0.64f) type = AN_COW;
        else if (r < 0.74f) type = AN_BOAR;
        else if (r < 0.84f) type = AN_FOX;
        else if (r < 0.90f) type = AN_BEAR;
        else type = AN_WOLF;
    }
    if (b.desert > 0.5f && type != AN_RABBIT && type != AN_FOX && type != AN_WOLF) type = AN_RABBIT;
    if (type == AN_COW && b.autumn > 0.5f) type = AN_DEER;
    if (type == AN_BEAR && b.desert > 0.3f) type = AN_FOX;
    spawnAnimal(type, x, z);
    if (type == AN_WOLF && night) {
        int extra = frand() < 0.6f ? 2 : 1;
        for (int i = 0; i < extra; i++) spawnAnimal(AN_WOLF, x + (frand() - 0.5f) * 6.f, z + (frand() - 0.5f) * 6.f);
    }
}

void Game::updateAnimals(float dt) {
    spawnTimer_ -= dt;
    if (spawnTimer_ <= 0.f) {
        spawnTimer_ = 1.8f;
        trySpawn();
    }
    bool night = tod_ > 0.78f || tod_ < 0.2f;
    bool torch = currentItem() == IT_TORCH;
    bool alerted = false;
    for (size_t i = 0; i < animals_.size();) {
        Animal& a = animals_[i];
        const AnimalDef& d = kAnimals[a.type];
        float dx = pos_.x - a.x, dz = pos_.z - a.z;
        float dist = std::sqrt(dx * dx + dz * dz);
        if (dist > 135.f) { animals_.erase(animals_.begin() + (long)i); continue; }
        a.hurt = std::max(0.f, a.hurt - dt);
        a.atkCd -= dt;
        a.timer -= dt;
        bool nearFire = false;
        for (const Placed& p : placed_) {
            if (p.def != PD_CAMPFIRE) continue;
            float fx = p.x - a.x, fz = p.z - a.z;
            if (fx * fx + fz * fz < 10.f * 10.f) { nearFire = true; break; }
        }
        bool scared = (d.mode == 2 || d.mode == 1) && ((torch && dist < 9.f) || nearFire);
        float tx = a.tx, tz = a.tz;
        float spd = 0.f;
        int prev = a.state;
        if (d.mode == 0) {
            float trig = a.type == AN_RABBIT ? 9.f : (a.type == AN_COW ? 6.f : 16.f);
            if (a.state != 2 && (dist < trig || a.hurt > 0.f)) { a.state = 2; a.timer = 4.f + frand() * 2.f; }
        } else {
            float aggroDist = d.mode == 2 ? (night ? 46.f : 11.f) : (d.mode == 1 ? 14.f : 5.f);
            if (a.angry) aggroDist = 40.f;
            if (scared) { if (a.state != 2) { a.state = 2; a.timer = 5.f; } }
            else if (a.state != 3 && dist < aggroDist) {
                a.state = 3;
                if (d.mode == 2 && night && !alerted && prev != 3) {
                    alerted = true;
                    addPopup("WOLVES NEARBY", 1.f, 0.55f, 0.45f);
                }
            } else if (a.state == 3 && dist > aggroDist + 22.f) { a.state = 0; a.timer = 2.f; }
        }
        if (a.state == 2) {
            if (a.timer <= 0.f && dist > 24.f) { a.state = 0; a.timer = 2.f; }
            else if (a.timer <= 0.f && d.mode != 0) a.timer = 1.f;
            tx = a.x - dx; tz = a.z - dz;
            spd = d.run;
        } else if (a.state == 3) {
            tx = pos_.x; tz = pos_.z;
            spd = d.run;
            if (dist < d.reach) {
                spd = 0.f;
                if (a.atkCd <= 0.f) {
                    a.atkCd = 1.2f;
                    damagePlayer(d.dmg * (0.85f + frand() * 0.3f));
                    float l = std::max(dist, 0.1f);
                    pos_.x += dx / l * 0.35f;
                    pos_.z += dz / l * 0.35f;
                    kick_ += 0.05f;
                }
            }
        } else if (a.state == 1) {
            tx = a.tx; tz = a.tz;
            float wx = tx - a.x, wz = tz - a.z;
            if (wx * wx + wz * wz < 1.5f * 1.5f || a.timer <= 0.f) { a.state = 0; a.timer = 2.f + frand() * 5.f; }
            spd = d.walk;
        } else {
            if (a.timer <= 0.f) {
                float an = frand() * kPi2, r = 6.f + frand() * 10.f;
                a.tx = a.x + std::cos(an) * r;
                a.tz = a.z + std::sin(an) * r;
                a.state = 1;
                a.timer = 8.f + frand() * 6.f;
            }
            spd = 0.f;
        }
        if (spd > 0.f) {
            float want = std::atan2(tx - a.x, tz - a.z);
            float df = angleDiff(a.yaw, want);
            a.yaw += df * (1.f - std::exp(-7.f * dt));
            float vx = std::sin(a.yaw) * spd * dt, vz = std::cos(a.yaw) * spd * dt;
            float nx = a.x + vx, nz = a.z + vz;
            cm_->collide(nx, nz, d.radius);
            collidePlaced(nx, nz, d.radius);
            a.x = nx;
            a.z = nz;
            a.phase += spd * dt * (a.state == 1 ? 3.0f : 1.6f);
        }
        float gy = world_.height(a.x, a.z);
        a.y += (gy - a.y) * std::min(1.f, dt * 12.f);
        i++;
    }
    animalIdx_ = -1;
    animalDist_ = 99.f;
    int idx;
    float t;
    if (!buildActive_ && animalRay(idx, t)) { animalIdx_ = idx; animalDist_ = t; }
}

bool Game::animalRay(int& idx, float& tout) const {
    Camera c = playCamera();
    V3 d = camForward();
    float best = 2.9f;
    idx = -1;
    for (size_t i = 0; i < animals_.size(); i++) {
        const Animal& a = animals_[i];
        const AnimalDef& df = kAnimals[a.type];
        V3 ctr = {a.x, a.y + df.hitH * a.scale, a.z};
        float r = df.hitR * a.scale + 0.25f;
        V3 oc = c.pos - ctr;
        float b = dot(oc, d);
        float cc = dot(oc, oc) - r * r;
        float disc = b * b - cc;
        if (disc < 0.f) continue;
        float t = -b - std::sqrt(disc);
        if (t < 0.f) t = cc < 0.f ? 0.f : -1.f;
        if (t < 0.f) continue;
        if (t < best) { best = t; idx = (int)i; }
    }
    tout = best;
    return idx >= 0;
}

bool Game::hitAnimal(int idx, float dmg) {
    if (idx < 0 || idx >= (int)animals_.size()) return false;
    Animal& a = animals_[(size_t)idx];
    const AnimalDef& d = kAnimals[a.type];
    a.hp -= dmg;
    a.hurt = 0.3f;
    a.angry = d.mode != 0;
    if (d.mode == 0) { a.state = 2; a.timer = 5.f; }
    else a.state = 3;
    if (a.hp <= 0.f) { killAnimal(idx); return true; }
    return false;
}

void Game::killAnimal(int idx) {
    Animal a = animals_[(size_t)idx];
    const AnimalDef& d = kAnimals[a.type];
    int n = d.meat + (int)(frand() * 2.f);
    giveItem(IT_MEAT, n);
    char buf[40];
    std::snprintf(buf, sizeof(buf), "+%d RAW MEAT", n);
    addPopup(buf, 0.95f, 0.45f, 0.45f);
    animals_.erase(animals_.begin() + idx);
    animalIdx_ = -1;
}

void Game::updateDynamic() {
    dyn_.clear();
    for (const Animal& a : animals_) {
        PropInst p{};
        float moving = a.state == 0 ? 0.f : 1.f;
        float bob = moving * std::fabs(std::sin(a.phase * 2.f)) * 0.05f * a.scale;
        p.x = a.x;
        p.z = a.z;
        p.y = a.y + bob;
        p.s = a.scale * (1.f + a.hurt * 0.12f);
        p.yaw = a.yaw + moving * std::sin(a.phase * 2.f) * 0.05f;
        p.tint = 0xFFFFFFFFu;
        p.model = (uint16_t)a.model;
        dyn_.push_back(p);
    }
}
