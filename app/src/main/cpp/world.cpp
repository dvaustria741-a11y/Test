#include "world.h"
#include "vmath.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>

void ModelIds::fill(const Pak& pk) {
    char buf[64];
    auto f = [&](const char* n) { return pk.find(n); };
    for (int i = 0; i < 5; i++) {
        std::snprintf(buf, sizeof(buf), "CommonTree_%d", i + 1); commonTree[i] = f(buf);
        std::snprintf(buf, sizeof(buf), "Pine_%d", i + 1); pine[i] = f(buf);
        std::snprintf(buf, sizeof(buf), "TwistedTree_%d", i + 1); twisted[i] = f(buf);
        std::snprintf(buf, sizeof(buf), "DeadTree_%d", i + 1); dead[i] = f(buf);
    }
    for (int i = 0; i < 3; i++) { std::snprintf(buf, sizeof(buf), "Rock_Medium_%d", i + 1); rockMed[i] = f(buf); }
    bush[0] = f("Bush_Common"); bush[1] = f("Bush_Common_Flowers");
    grass[0] = f("Grass_Common_Short"); grass[1] = f("Grass_Common_Tall");
    grass[2] = f("Grass_Wispy_Short"); grass[3] = f("Grass_Wispy_Tall");
    flowers[0] = f("Flower_3_Group"); flowers[1] = f("Flower_4_Group");
    flowers[2] = f("Flower_3_Single"); flowers[3] = f("Flower_4_Single");
    fern = f("Fern_1");
    clover[0] = f("Clover_1"); clover[1] = f("Clover_2");
    plantBig[0] = f("Plant_1_Big"); plantBig[1] = f("Plant_7_Big");
    plantSmall[0] = f("Plant_1"); plantSmall[1] = f("Plant_7");
    mushroom[0] = f("Mushroom_Common"); mushroom[1] = f("Mushroom_Laetiporus");
    for (int i = 0; i < 5; i++) { std::snprintf(buf, sizeof(buf), "Pebble_Round_%d", i + 1); pebble[i] = f(buf); }
    for (int i = 0; i < 6; i++) { std::snprintf(buf, sizeof(buf), "Pebble_Square_%d", i + 1); pebble[5 + i] = f(buf); }
    pathStone[0] = f("RockPath_Round_Small_1"); pathStone[1] = f("RockPath_Round_Small_2");
    pathStone[2] = f("RockPath_Round_Small_3"); pathStone[3] = f("RockPath_Round_Thin");
}

Biome World::biome(float x, float z) const {
    float d = fbm(x * 0.0013f + 11.3f, z * 0.0013f - 7.1f, 3, seed_ + 101);
    float a = fbm(x * 0.0017f - 3.7f, z * 0.0017f + 9.9f, 3, seed_ + 202);
    float p = fbm(x * 0.0026f + 21.f, z * 0.0026f + 4.f, 2, seed_ + 303);
    Biome b;
    b.desert = smoothstepf(0.10f, 0.28f, d);
    b.autumn = (1.f - b.desert) * smoothstepf(-0.02f, 0.18f, a);
    b.pine = smoothstepf(-0.08f, 0.20f, p);
    return b;
}

float World::density(float x, float z) const {
    float n = fbm(x * 0.0065f + 3.f, z * 0.0065f - 5.f, 3, seed_ + 505);
    return smoothstepf(-0.28f, 0.22f, n);
}

float World::pathField(float x, float z, int k) const {
    static const float sc[2] = {0.0031f, 0.0023f};
    float wx = fbm(x * 0.007f + 13.f, z * 0.007f, 2, seed_ + 700 + k) * 55.f;
    float wz = fbm(x * 0.007f - 8.f, z * 0.007f + 31.f, 2, seed_ + 710 + k) * 55.f;
    return fbm((x + wx) * sc[k], (z + wz) * sc[k], 3, seed_ + 720 + k);
}

float World::path(float x, float z) const {
    float zone = smoothstepf(-0.20f, 0.05f, fbm(x * 0.0016f + 40.f, z * 0.0016f, 2, seed_ + 730));
    if (zone <= 0.001f) return 0.f;
    float best = 0.f;
    const float e = 1.5f;
    for (int k = 0; k < 2; k++) {
        float f0 = pathField(x, z, k);
        if (std::fabs(f0) > 0.06f) continue;
        float fx = pathField(x + e, z, k);
        float fz = pathField(x, z + e, k);
        float gx = (fx - f0) / e, gz = (fz - f0) / e;
        float g = std::sqrt(gx * gx + gz * gz) + 1e-4f;
        float dist = std::fabs(f0) / g;
        float m = 1.f - smoothstepf(1.3f, 3.2f, dist);
        best = std::max(best, m);
    }
    return best * zone;
}

float World::baseHeight(float x, float z, const Biome& b) const {
    float hills = fbm(x * 0.0042f, z * 0.0042f, 4, seed_ + 1) * 30.f;
    float rolling = fbm(x * 0.013f, z * 0.013f, 3, seed_ + 2) * 3.5f;
    float green = hills + rolling;
    float rid = 1.f - std::fabs(fbm(x * 0.007f + 5.f, z * 0.007f, 2, seed_ + 3));
    float dune = hills * 0.35f + rid * rid * 5.0f + fbm(x * 0.03f, z * 0.03f, 2, seed_ + 4) * 0.6f;
    return lerpf(green, dune, b.desert);
}

float World::height(float x, float z) const {
    Biome b = biome(x, z);
    return baseHeight(x, z, b) - 0.28f * path(x, z);
}

void World::terrainColor(float x, float z, const Biome& b, float slope, float out[3]) const {
    const float green[3] = {0.40f, 0.72f, 0.10f};
    const float autumn[3] = {0.72f, 0.55f, 0.10f};
    const float desert[3] = {0.93f, 0.76f, 0.40f};
    float v = fbm(x * 0.02f, z * 0.02f, 2, seed_ + 909);
    for (int i = 0; i < 3; i++) {
        float c = lerpf(green[i], autumn[i], b.autumn);
        c = lerpf(c, desert[i], b.desert);
        out[i] = c * (1.f + 0.12f * v);
    }
    float rk = smoothstepf(0.22f, 0.45f, slope);
    const float rockG[3] = {0.52f, 0.50f, 0.40f};
    const float rockD[3] = {0.80f, 0.56f, 0.30f};
    for (int i = 0; i < 3; i++) {
        float rc = lerpf(rockG[i], rockD[i], b.desert);
        out[i] = lerpf(out[i], rc, rk);
    }
}

namespace {

struct Sample { float h; float pm; Biome b; };

void buildTerrain(const World& w, int cx, int cz, int res, TerrainCPU& t) {
    const float cell = (float)CHUNK_SIZE / (float)res;
    const int G = res + 3;
    const float x0 = (float)(cx * CHUNK_SIZE), z0 = (float)(cz * CHUNK_SIZE);
    std::vector<Sample> S((size_t)G * G);
    for (int j = 0; j < G; j++)
        for (int i = 0; i < G; i++) {
            float x = x0 + (float)(i - 1) * cell, z = z0 + (float)(j - 1) * cell;
            Sample s;
            s.b = w.biome(x, z);
            s.pm = w.path(x, z);
            s.h = w.height(x, z);
            S[(size_t)j * G + i] = s;
        }
    const int R1 = res + 1;
    std::vector<TVert> base((size_t)R1 * R1);
    float minY = 1e9f, maxY = -1e9f;
    for (int j = 0; j <= res; j++)
        for (int i = 0; i <= res; i++) {
            int gi = i + 1, gj = j + 1;
            const Sample& s = S[(size_t)gj * G + gi];
            float hl = S[(size_t)gj * G + gi - 1].h, hr = S[(size_t)gj * G + gi + 1].h;
            float hd = S[(size_t)(gj - 1) * G + gi].h, hu = S[(size_t)(gj + 1) * G + gi].h;
            V3 n = vnorm({hl - hr, 2.f * cell, hd - hu});
            float x = x0 + (float)i * cell, z = z0 + (float)j * cell;
            float col[3];
            w.terrainColor(x, z, s.b, 1.f - n.y, col);
            TVert v;
            v.x = x; v.y = s.h; v.z = z;
            v.nx = (int8_t)std::lround(n.x * 127.f);
            v.ny = (int8_t)std::lround(n.y * 127.f);
            v.nz = (int8_t)std::lround(n.z * 127.f);
            v.nw = 0;
            v.r = (uint8_t)clampf(col[0] * 255.f, 0.f, 255.f);
            v.g = (uint8_t)clampf(col[1] * 255.f, 0.f, 255.f);
            v.b = (uint8_t)clampf(col[2] * 255.f, 0.f, 255.f);
            v.a = (uint8_t)clampf(s.pm * 255.f, 0.f, 255.f);
            base[(size_t)j * R1 + i] = v;
            minY = std::min(minY, s.h);
            maxY = std::max(maxY, s.h);
        }
    t.minY = minY - 4.f;
    t.maxY = maxY + 1.f;
    t.verts.clear();
    t.idx.clear();
    for (int L = 0; L < 3; L++) {
        int step = 1 << L;
        int n = res / step;
        if (n < 2 || res % step != 0) { n = std::max(2, n); step = res / n; }
        t.voff[L] = (uint32_t)t.verts.size();
        t.ioff[L] = (uint32_t)t.idx.size();
        const int N1 = n + 1;
        for (int j = 0; j <= n; j++)
            for (int i = 0; i <= n; i++) t.verts.push_back(base[(size_t)(j * step) * R1 + i * step]);
        const uint16_t gridCount = (uint16_t)(N1 * N1);
        for (int j = 0; j < n; j++)
            for (int i = 0; i < n; i++) {
                uint16_t v00 = (uint16_t)(j * N1 + i), v10 = (uint16_t)(j * N1 + i + 1);
                uint16_t v01 = (uint16_t)((j + 1) * N1 + i), v11 = (uint16_t)((j + 1) * N1 + i + 1);
                t.idx.push_back(v00); t.idx.push_back(v01); t.idx.push_back(v10);
                t.idx.push_back(v10); t.idx.push_back(v01); t.idx.push_back(v11);
            }
        std::vector<uint16_t> edges[4];
        for (int i = 0; i <= n; i++) {
            edges[0].push_back((uint16_t)(0 * N1 + i));
            edges[1].push_back((uint16_t)(n * N1 + i));
            edges[2].push_back((uint16_t)(i * N1 + 0));
            edges[3].push_back((uint16_t)(i * N1 + n));
        }
        uint16_t next = gridCount;
        for (int e = 0; e < 4; e++) {
            uint16_t first = next;
            for (size_t k = 0; k < edges[e].size(); k++) {
                TVert v = t.verts[t.voff[L] + edges[e][k]];
                v.y -= 4.0f;
                t.verts.push_back(v);
                next++;
            }
            for (size_t k = 0; k + 1 < edges[e].size(); k++) {
                uint16_t a = edges[e][k], b = edges[e][k + 1];
                uint16_t a2 = (uint16_t)(first + k), b2 = (uint16_t)(first + k + 1);
                t.idx.push_back(a); t.idx.push_back(b); t.idx.push_back(b2);
                t.idx.push_back(a); t.idx.push_back(b2); t.idx.push_back(a2);
            }
        }
        t.vcount[L] = (uint32_t)t.verts.size() - t.voff[L];
        t.icount[L] = (uint32_t)t.idx.size() - t.ioff[L];
    }
}

uint32_t packTint(float r, float g, float b, float a = 1.f) {
    auto c = [](float v) { return (uint32_t)clampf(v * 255.f + 0.5f, 0.f, 255.f); };
    return c(r) | (c(g) << 8) | (c(b) << 16) | (c(a) << 24);
}

void leafColor(const Biome& b, Rng& r, bool pine, float out[3]) {
    if (pine) {
        out[0] = r.range(0.14f, 0.26f); out[1] = r.range(0.48f, 0.62f); out[2] = r.range(0.08f, 0.16f);
        return;
    }
    float gr[3] = {r.range(0.30f, 0.46f), r.range(0.70f, 0.86f), r.range(0.08f, 0.16f)};
    float t = r.f();
    float au[3];
    if (t < 0.40f) { au[0] = 1.00f; au[1] = r.range(0.38f, 0.52f); au[2] = 0.08f; }
    else if (t < 0.70f) { au[0] = r.range(0.80f, 0.92f); au[1] = r.range(0.10f, 0.18f); au[2] = 0.07f; }
    else { au[0] = 1.00f; au[1] = r.range(0.72f, 0.84f); au[2] = 0.14f; }
    float k = smoothstepf(0.35f, 0.65f, b.autumn);
    for (int i = 0; i < 3; i++) out[i] = lerpf(gr[i], au[i], k);
}

void grassColor(const Biome& b, Rng& r, float out[3]) {
    float gr[3] = {0.48f, 0.82f, 0.14f};
    float au[3] = {0.88f, 0.62f, 0.12f};
    float de[3] = {0.84f, 0.70f, 0.28f};
    float j = r.range(0.88f, 1.10f);
    for (int i = 0; i < 3; i++) {
        float c = lerpf(gr[i], au[i], b.autumn);
        c = lerpf(c, de[i], b.desert);
        out[i] = c * j;
    }
}

void addTree(std::vector<PropInst>& v, std::vector<Collider>& cols, int model, float x, float y, float z, float s,
             float yaw, uint32_t tint, float colR, Rng& r) {
    if (model < 0) return;
    PropInst p;
    p.x = x; p.y = y; p.z = z; p.s = s; p.yaw = yaw; p.tint = tint;
    p.model = (uint16_t)model; p.var = 0; p.rnd = (uint8_t)(r.u() & 255);
    v.push_back(p);
    cols.push_back({x, z, colR * s});
}

}  // namespace

void buildChunk(const World& w, const ModelIds& id, const Pak& pk, int cx, int cz, int terrainRes, ChunkCPU& out) {
    (void)pk;
    out.cx = cx; out.cz = cz;
    out.structs.clear();
    out.colliders.clear();
    buildTerrain(w, cx, cz, terrainRes, out.terrain);
    const float x0 = (float)(cx * CHUNK_SIZE), z0 = (float)(cz * CHUNK_SIZE);
    const uint32_t seed = w.seed();

    const float tcell = 8.f;
    for (int j = 0; j < 8; j++)
        for (int i = 0; i < 8; i++) {
            int gx = cx * 8 + i, gz = cz * 8 + j;
            Rng r(hash2i(gx, gz, seed + 11));
            float x = x0 + ((float)i + 0.12f + 0.76f * r.f()) * tcell;
            float z = z0 + ((float)j + 0.12f + 0.76f * r.f()) * tcell;
            Biome b = w.biome(x, z);
            float dens = w.density(x, z);
            float p = dens * (0.30f * (1.f - b.desert) + 0.075f * b.desert);
            if (r.f() > p) continue;
            if (w.path(x, z) > 0.08f) continue;
            float y = w.height(x, z) - 0.12f;
            float yaw = r.range(0.f, 6.2831853f);
            float rt = r.f();
            float lc[3];
            if (b.desert > 0.5f) {
                int m = id.dead[r.u() % 5];
                addTree(out.structs, out.colliders, m, x, y, z, r.range(1.1f, 1.9f), yaw, packTint(1, 1, 1), 0.38f, r);
            } else if (b.pine > 0.55f && b.autumn < 0.5f && rt < 0.82f) {
                int m = id.pine[r.u() % 5];
                leafColor(b, r, true, lc);
                addTree(out.structs, out.colliders, m, x, y, z, r.range(1.5f, 2.5f), yaw, packTint(lc[0], lc[1], lc[2]), 0.30f, r);
            } else if (rt < 0.80f) {
                int m = id.commonTree[r.u() % 5];
                leafColor(b, r, false, lc);
                addTree(out.structs, out.colliders, m, x, y, z, r.range(1.2f, 2.0f), yaw, packTint(lc[0], lc[1], lc[2]), 0.32f, r);
            } else {
                int m = id.twisted[r.u() % 5];
                leafColor(b, r, false, lc);
                addTree(out.structs, out.colliders, m, x, y, z, r.range(0.7f, 1.1f), yaw, packTint(lc[0], lc[1], lc[2]), 0.42f, r);
            }
        }

    const float rcell = 32.f;
    for (int j = 0; j < 2; j++)
        for (int i = 0; i < 2; i++) {
            int gx = cx * 2 + i, gz = cz * 2 + j;
            Rng r(hash2i(gx, gz, seed + 23));
            float cxm = x0 + ((float)i + 0.2f + 0.6f * r.f()) * rcell;
            float czm = z0 + ((float)j + 0.2f + 0.6f * r.f()) * rcell;
            Biome b = w.biome(cxm, czm);
            float pr = 0.26f + 0.24f * b.desert;
            if (r.f() > pr) continue;
            if (w.path(cxm, czm) > 0.1f) continue;
            int n = 2 + (int)(r.u() % 4);
            float s0 = r.range(1.5f, 3.2f) * (1.f + 0.55f * b.desert);
            for (int k = 0; k < n; k++) {
                float ang = r.range(0.f, 6.2831853f), rad = r.range(0.f, 5.5f);
                float x = cxm + std::cos(ang) * rad, z = czm + std::sin(ang) * rad;
                if (w.path(x, z) > 0.1f) continue;
                float s = s0 * r.range(0.55f, 1.15f);
                PropInst p;
                p.x = x; p.z = z; p.y = w.height(x, z) - 0.28f * s;
                p.s = s; p.yaw = r.range(0.f, 6.2831853f); p.tint = packTint(1, 1, 1);
                p.model = (uint16_t)std::max(0, id.rockMed[r.u() % 3]);
                p.var = (uint8_t)(b.desert > 0.5f ? 1 : 0);
                p.rnd = (uint8_t)(r.u() & 255);
                out.structs.push_back(p);
                out.colliders.push_back({x, z, 1.15f * s});
            }
        }

    const float scell = 12.f;
    for (int j = 0; j < 5; j++)
        for (int i = 0; i < 5; i++) {
            float fx = x0 + (float)i * scell, fz = z0 + (float)j * scell;
            int gx = cx * 5 + i, gz = cz * 5 + j;
            Rng r(hash2i(gx, gz, seed + 37));
            if (r.f() > 0.28f) continue;
            float x = fx + r.f() * scell, z = fz + r.f() * scell;
            if (x >= x0 + CHUNK_SIZE || z >= z0 + CHUNK_SIZE) continue;
            Biome b = w.biome(x, z);
            if (w.path(x, z) > 0.1f) continue;
            float s = r.range(0.35f, 0.9f);
            PropInst p;
            p.x = x; p.z = z; p.y = w.height(x, z) - 0.2f * s;
            p.s = s; p.yaw = r.range(0.f, 6.2831853f); p.tint = packTint(1, 1, 1);
            p.model = (uint16_t)std::max(0, id.rockMed[r.u() % 3]);
            p.var = (uint8_t)(b.desert > 0.5f ? 1 : 0);
            p.rnd = (uint8_t)(r.u() & 255);
            out.structs.push_back(p);
            out.colliders.push_back({x, z, 1.1f * s});
        }

    const float bcell = 10.f;
    for (int j = 0; j < 6; j++)
        for (int i = 0; i < 6; i++) {
            int gx = cx * 6 + i, gz = cz * 6 + j;
            Rng r(hash2i(gx, gz, seed + 53));
            float x = x0 + ((float)i + r.f()) * bcell, z = z0 + ((float)j + r.f()) * bcell;
            if (x >= x0 + CHUNK_SIZE || z >= z0 + CHUNK_SIZE) continue;
            Biome b = w.biome(x, z);
            float dens = w.density(x, z);
            float p = 0.30f * (1.f - b.desert) * (0.3f + 0.7f * dens);
            if (r.f() > p) continue;
            if (w.path(x, z) > 0.1f) continue;
            float lc[3];
            leafColor(b, r, false, lc);
            PropInst pi;
            pi.x = x; pi.z = z; pi.y = w.height(x, z) - 0.05f;
            pi.s = r.range(1.0f, 1.9f); pi.yaw = r.range(0.f, 6.2831853f);
            pi.tint = packTint(lc[0], lc[1], lc[2]);
            pi.model = (uint16_t)std::max(0, id.bush[r.f() < 0.45f ? 1 : 0]);
            pi.var = 0; pi.rnd = (uint8_t)(r.u() & 255);
            out.structs.push_back(pi);
        }
}

void buildDetail(const World& w, const ModelIds& id, int cx, int cz, DetailCPU& out) {
    out.cx = cx; out.cz = cz;
    out.items.clear();
    out.items.reserve(900);
    const float x0 = (float)(cx * CHUNK_SIZE), z0 = (float)(cz * CHUNK_SIZE);
    const uint32_t seed = w.seed();
    const int N = 40;
    const float cell = (float)CHUNK_SIZE / (float)N;
    auto push = [&](int model, float x, float y, float z, float s, float yaw, uint32_t tint, uint8_t var, Rng& r) {
        if (model < 0) return;
        PropInst p;
        p.x = x; p.y = y; p.z = z; p.s = s; p.yaw = yaw; p.tint = tint;
        p.model = (uint16_t)model; p.var = var; p.rnd = (uint8_t)(r.u() & 255);
        out.items.push_back(p);
    };
    for (int j = 0; j < N; j++)
        for (int i = 0; i < N; i++) {
            int gx = cx * N + i, gz = cz * N + j;
            Rng r(hash2i(gx, gz, seed + 31));
            float x = x0 + ((float)i + r.f()) * cell, z = z0 + ((float)j + r.f()) * cell;
            Biome b = w.biome(x, z);
            float pm = w.path(x, z);
            float gp = lerpf(0.95f, 0.10f, b.desert);
            float patch = smoothstepf(-0.45f, 0.05f, fbm(x * 0.02f, z * 0.02f, 2, seed + 606));
            gp *= (0.35f + 0.65f * patch) * clampf(1.f - pm * 1.4f, 0.f, 1.f);
            float y = w.height(x, z);
            float rr = r.f();
            if (rr < gp) {
                float c[3];
                grassColor(b, r, c);
                float t = r.f();
                int m = t < 0.38f ? id.grass[0] : (t < 0.68f ? id.grass[1] : (t < 0.84f ? id.grass[2] : id.grass[3]));
                push(m, x, y - 0.03f, z, r.range(0.55f, 0.95f), r.range(0.f, 6.2831853f), packTint(c[0], c[1], c[2]), 0, r);
                continue;
            }
            if (pm > 0.02f && pm < 0.8f && b.desert < 0.5f && r.f() < 0.30f) {
                int m = id.pebble[r.u() % 11];
                push(m, x, y - 0.02f, z, r.range(1.0f, 2.4f), r.range(0.f, 6.2831853f), packTint(1, 1, 1), 0, r);
                continue;
            }
            if (pm > 0.45f && b.desert >= 0.3f && r.f() < 0.20f) {
                int m = id.pathStone[r.u() % 4];
                push(m, x, y + 0.005f, z, r.range(0.8f, 1.3f), r.range(0.f, 6.2831853f), packTint(1, 1, 1), 0, r);
                continue;
            }
            if (pm > 0.35f && b.desert < 0.3f && r.f() < 0.04f) {
                int m = id.pathStone[r.u() % 4];
                push(m, x, y + 0.005f, z, r.range(0.7f, 1.2f), r.range(0.f, 6.2831853f), packTint(1, 1, 1), 0, r);
            }
        }

    const float fcell = 3.2f;
    const int FN = 20;
    for (int j = 0; j < FN; j++)
        for (int i = 0; i < FN; i++) {
            int gx = cx * FN + i, gz = cz * FN + j;
            Rng r(hash2i(gx, gz, seed + 41));
            float x = x0 + ((float)i + r.f()) * fcell, z = z0 + ((float)j + r.f()) * fcell;
            Biome b = w.biome(x, z);
            if (b.desert > 0.3f) continue;
            float pm = w.path(x, z);
            if (pm > 0.1f) continue;
            float fn = fbm(x * 0.03f + 7.f, z * 0.03f, 2, seed + 707);
            float y = w.height(x, z);
            if (fn > 0.12f && r.f() < 0.55f) {
                push(id.flowers[r.u() % 4], x, y - 0.02f, z, r.range(0.7f, 1.25f), r.range(0.f, 6.2831853f), packTint(1, 1, 1), 0, r);
            } else if (r.f() < 0.07f) {
                push(id.clover[r.u() % 2], x, y - 0.02f, z, r.range(0.8f, 1.4f), r.range(0.f, 6.2831853f), packTint(0.55f, 0.85f, 0.2f), 0, r);
            }
        }

    const float pcell = 6.4f;
    const int PN = 10;
    for (int j = 0; j < PN; j++)
        for (int i = 0; i < PN; i++) {
            int gx = cx * PN + i, gz = cz * PN + j;
            Rng r(hash2i(gx, gz, seed + 67));
            float x = x0 + ((float)i + r.f()) * pcell, z = z0 + ((float)j + r.f()) * pcell;
            Biome b = w.biome(x, z);
            float pm = w.path(x, z);
            if (pm > 0.1f) continue;
            float dens = w.density(x, z);
            float y = w.height(x, z);
            if (b.desert > 0.4f) {
                if (r.f() < 0.30f) {
                    int m = id.plantBig[r.u() % 2];
                    float t = r.f();
                    uint32_t tint = t < 0.6f ? packTint(1.0f, r.range(0.25f, 0.4f), 0.15f) : packTint(0.55f, 0.8f, 0.2f);
                    push(m, x, y - 0.03f, z, r.range(0.9f, 1.5f), r.range(0.f, 6.2831853f), tint, 0, r);
                }
            } else {
                float lc[3];
                if (r.f() < 0.22f * (0.4f + dens)) {
                    leafColor(b, r, false, lc);
                    float au = b.autumn;
                    float pr = lerpf(0.30f, 0.90f, au), pg = lerpf(0.72f, 0.45f, au), pb = lerpf(0.12f, 0.10f, au);
                    push(id.fern, x, y - 0.05f, z, r.range(0.28f, 0.48f), r.range(0.f, 6.2831853f), packTint(pr, pg, pb), 0, r);
                } else if (r.f() < 0.10f) {
                    float au = b.autumn;
                    push(id.plantBig[r.u() % 2], x, y - 0.03f, z, r.range(0.8f, 1.3f), r.range(0.f, 6.2831853f),
                         packTint(lerpf(0.38f, 0.95f, au), lerpf(0.78f, 0.42f, au), 0.12f), 0, r);
                } else if (dens > 0.5f && r.f() < 0.12f) {
                    int m = r.f() < 0.7f ? id.mushroom[0] : id.mushroom[1];
                    push(m, x, y - 0.02f, z, r.range(1.2f, 2.3f), r.range(0.f, 6.2831853f), packTint(1, 1, 1), 0, r);
                }
            }
        }
}
