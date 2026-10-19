#pragma once

struct Quality {
    const char* name;
    int tier;
    int terrainRes;
    float viewDist;
    float treeDist, rockDist, bushDist, detailDist, pebbleDist;
    float lod0, lod1;
    float density;
    float renderMin, renderMax;
    int shadows;
    int shadowRes;
    float shadowDist;
    int targetFps;
    int texSkip;
    int clouds;
    int trilinear;
};

inline Quality qualityPreset(int tier) {
    Quality q;
    if (tier <= 0) {
        q = {"Low", 0, 16, 150.f, 120.f, 110.f, 55.f, 26.f, 24.f, 16.f, 42.f, 0.55f, 0.55f, 0.80f, 0, 0, 0.f, 30, 2, 0, 0};
    } else if (tier == 1) {
        q = {"Medium", 1, 24, 280.f, 220.f, 200.f, 110.f, 42.f, 40.f, 34.f, 85.f, 0.80f, 0.65f, 0.90f, 0, 0, 0.f, 60, 1, 1, 1};
    } else {
        q = {"High", 2, 32, 450.f, 340.f, 300.f, 180.f, 62.f, 58.f, 58.f, 140.f, 1.00f, 0.80f, 1.00f, 1, 1024, 75.f, 60, 0, 2, 1};
    }
    return q;
}
