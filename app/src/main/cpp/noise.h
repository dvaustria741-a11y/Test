#pragma once
#include <cstdint>
#include <cmath>

inline uint32_t hash32(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x;
}
inline uint32_t hash2i(int x, int y, uint32_t seed) {
    return hash32((uint32_t)x * 0x9E3779B1u ^ hash32((uint32_t)y * 0x85EBCA77u + seed));
}
inline float hashf(int x, int y, uint32_t seed) {
    return (hash2i(x, y, seed) >> 8) * (1.0f / 16777216.0f);
}

struct Rng {
    uint32_t s;
    explicit Rng(uint32_t a) : s(a ? a : 0x9E3779B9u) {}
    uint32_t u() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float f() { return (u() >> 8) * (1.0f / 16777216.0f); }
    float range(float a, float b) { return a + (b - a) * f(); }
};

inline float gnoise(float x, float y, uint32_t seed) {
    static const float gx[16] = {1.f, .9239f, .7071f, .3827f, 0.f, -.3827f, -.7071f, -.9239f, -1.f, -.9239f, -.7071f, -.3827f, 0.f, .3827f, .7071f, .9239f};
    static const float gy[16] = {0.f, .3827f, .7071f, .9239f, 1.f, .9239f, .7071f, .3827f, 0.f, -.3827f, -.7071f, -.9239f, -1.f, -.9239f, -.7071f, -.3827f};
    int xi = (int)std::floor(x), yi = (int)std::floor(y);
    float xf = x - (float)xi, yf = y - (float)yi;
    auto g = [&](int ix, int iy, float dx, float dy) {
        uint32_t h = hash2i(ix, iy, seed) & 15u;
        return gx[h] * dx + gy[h] * dy;
    };
    float n00 = g(xi, yi, xf, yf);
    float n10 = g(xi + 1, yi, xf - 1.f, yf);
    float n01 = g(xi, yi + 1, xf, yf - 1.f);
    float n11 = g(xi + 1, yi + 1, xf - 1.f, yf - 1.f);
    float u = xf * xf * xf * (xf * (xf * 6.f - 15.f) + 10.f);
    float v = yf * yf * yf * (yf * (yf * 6.f - 15.f) + 10.f);
    float a = n00 + (n10 - n00) * u;
    float b = n01 + (n11 - n01) * u;
    return (a + (b - a) * v) * 1.45f;
}

inline float fbm(float x, float y, int oct, uint32_t seed) {
    float sum = 0.f, amp = 1.f, norm = 0.f;
    for (int i = 0; i < oct; i++) {
        sum += gnoise(x, y, seed + (uint32_t)i * 977u) * amp;
        norm += amp;
        amp *= 0.5f;
        x = x * 2.03f + 17.3f;
        y = y * 2.03f - 9.1f;
    }
    return sum / norm;
}
