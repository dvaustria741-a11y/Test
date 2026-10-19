#pragma once
#include <cmath>
#include <cstring>
#include <cstdint>
#include <algorithm>

struct V3 { float x, y, z; };
inline V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline V3 operator*(V3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline float vlen(V3 a) { return std::sqrt(dot(a, a)); }
inline V3 vnorm(V3 a) { float l = vlen(a); return l > 1e-8f ? a * (1.0f / l) : V3{0, 1, 0}; }

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float smoothstepf(float e0, float e1, float x) {
    float t = clampf((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

struct Mat4 { float m[16]; };

inline Mat4 mat4Mul(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int c = 0; c < 4; c++)
        for (int row = 0; row < 4; row++) {
            float s = 0;
            for (int k = 0; k < 4; k++) s += a.m[k * 4 + row] * b.m[c * 4 + k];
            r.m[c * 4 + row] = s;
        }
    return r;
}

inline Mat4 mat4Perspective(float fovy, float aspect, float zn, float zf) {
    Mat4 r; std::memset(r.m, 0, sizeof(r.m));
    float f = 1.0f / std::tan(fovy * 0.5f);
    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = (zf + zn) / (zn - zf);
    r.m[11] = -1.0f;
    r.m[14] = 2.0f * zf * zn / (zn - zf);
    return r;
}

inline Mat4 mat4LookAt(V3 eye, V3 at, V3 up) {
    V3 f = vnorm(at - eye);
    V3 s = vnorm(cross(f, up));
    V3 u = cross(s, f);
    Mat4 r; std::memset(r.m, 0, sizeof(r.m));
    r.m[0] = s.x; r.m[4] = s.y; r.m[8] = s.z;
    r.m[1] = u.x; r.m[5] = u.y; r.m[9] = u.z;
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
    r.m[12] = -dot(s, eye); r.m[13] = -dot(u, eye); r.m[14] = dot(f, eye);
    r.m[15] = 1.0f;
    return r;
}

inline Mat4 mat4Ortho(float l, float r_, float b, float t, float n, float f) {
    Mat4 r; std::memset(r.m, 0, sizeof(r.m));
    r.m[0] = 2.0f / (r_ - l);
    r.m[5] = 2.0f / (t - b);
    r.m[10] = -2.0f / (f - n);
    r.m[12] = -(r_ + l) / (r_ - l);
    r.m[13] = -(t + b) / (t - b);
    r.m[14] = -(f + n) / (f - n);
    r.m[15] = 1.0f;
    return r;
}

struct Frustum { float p[6][4]; };

inline void frustumFromVP(const Mat4& vp, Frustum& fr) {
    const float* m = vp.m;
    for (int i = 0; i < 4; i++) {
        float r0 = m[0 + i], r1 = m[4 + i], r2 = m[8 + i], r3 = m[12 + i];
        (void)r0; (void)r1; (void)r2; (void)r3;
    }
    auto row = [&](int i, float* o) { o[0] = m[0 * 4 + i]; o[1] = m[1 * 4 + i]; o[2] = m[2 * 4 + i]; o[3] = m[3 * 4 + i]; };
    float r0[4], r1[4], r2[4], r3[4];
    row(0, r0); row(1, r1); row(2, r2); row(3, r3);
    for (int k = 0; k < 4; k++) {
        fr.p[0][k] = r3[k] + r0[k];
        fr.p[1][k] = r3[k] - r0[k];
        fr.p[2][k] = r3[k] + r1[k];
        fr.p[3][k] = r3[k] - r1[k];
        fr.p[4][k] = r3[k] + r2[k];
        fr.p[5][k] = r3[k] - r2[k];
    }
    for (int i = 0; i < 6; i++) {
        float l = std::sqrt(fr.p[i][0] * fr.p[i][0] + fr.p[i][1] * fr.p[i][1] + fr.p[i][2] * fr.p[i][2]);
        if (l > 1e-8f) for (int k = 0; k < 4; k++) fr.p[i][k] /= l;
    }
}

inline bool frustumSphere(const Frustum& f, float x, float y, float z, float r) {
    for (int i = 0; i < 6; i++)
        if (f.p[i][0] * x + f.p[i][1] * y + f.p[i][2] * z + f.p[i][3] < -r) return false;
    return true;
}

inline bool frustumAABB(const Frustum& f, float x0, float y0, float z0, float x1, float y1, float z1) {
    for (int i = 0; i < 6; i++) {
        float px = f.p[i][0] >= 0 ? x1 : x0;
        float py = f.p[i][1] >= 0 ? y1 : y0;
        float pz = f.p[i][2] >= 0 ? z1 : z0;
        if (f.p[i][0] * px + f.p[i][1] * py + f.p[i][2] * pz + f.p[i][3] < 0) return false;
    }
    return true;
}
