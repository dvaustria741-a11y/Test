#include "pak.h"
#include "platform.h"
#include <cstring>
#include <cmath>
#include <algorithm>

namespace {
struct Reader {
    const uint8_t* d;
    size_t n;
    size_t p = 0;
    bool ok = true;
    template <class T> T get() {
        T v{};
        if (p + sizeof(T) > n) { ok = false; return v; }
        std::memcpy(&v, d + p, sizeof(T));
        p += sizeof(T);
        return v;
    }
    void bytes(void* dst, size_t c) {
        if (p + c > n) { ok = false; std::memset(dst, 0, c); return; }
        std::memcpy(dst, d + p, c);
        p += c;
    }
};

int categorize(const char* n) {
    std::string s(n);
    auto starts = [&](const char* pre) { return s.rfind(pre, 0) == 0; };
    if (starts("CommonTree") || starts("Pine") || starts("TwistedTree") || starts("DeadTree")) return CAT_TREE;
    if (starts("Rock_Medium")) return CAT_ROCK;
    if (starts("Bush")) return CAT_BUSH;
    if (starts("RockPath") || starts("Pebble")) return CAT_PEBBLE;
    return CAT_DETAIL;
}
}

bool pakParse(std::vector<uint8_t>&& data, Pak& out) {
    out.blob = std::move(data);
    Reader r{out.blob.data(), out.blob.size()};
    char magic[4];
    r.bytes(magic, 4);
    if (std::memcmp(magic, "NPK3", 4) != 0) { plat_log("pak: bad magic"); return false; }
    uint32_t modelCount = r.get<uint32_t>();
    uint32_t matCount = r.get<uint32_t>();
    if (modelCount > 1024 || matCount > 256) return false;
    out.mats.resize(matCount);
    for (auto& m : out.mats) {
        std::memset(&m, 0, sizeof(m));
        r.bytes(m.name, 32);
        r.bytes(m.tex, 32);
        m.size = r.get<uint32_t>();
        m.alpha = r.get<uint32_t>();
        m.vcol = r.get<uint32_t>();
        m.dbl = r.get<uint32_t>();
        m.boost = r.get<float>();
        m.sway = r.get<float>();
        m.aoMin = r.get<float>();
        m.soft = r.get<float>();
    }
    out.models.resize(modelCount);
    for (auto& md : out.models) {
        std::memset(md.name, 0, sizeof(md.name));
        r.bytes(md.name, 32);
        float b[6];
        r.bytes(b, sizeof(b));
        for (int i = 0; i < 3; i++) { md.mn[i] = b[i]; md.mx[i] = b[3 + i]; }
        uint32_t lodCount = r.get<uint32_t>();
        if (lodCount > 8) return false;
        md.lods.resize(lodCount);
        for (auto& lod : md.lods) {
            uint32_t sc = r.get<uint32_t>();
            if (sc > 16) return false;
            lod.resize(sc);
            for (auto& s : lod) {
                s.mat = r.get<uint32_t>();
                s.voff = r.get<uint32_t>();
                s.vcount = r.get<uint32_t>();
                s.ioff = r.get<uint32_t>();
                s.icount = r.get<uint32_t>();
            }
        }
        md.cat = categorize(md.name);
        float ex = std::max(std::fabs(md.mn[0]), std::fabs(md.mx[0]));
        float ez = std::max(std::fabs(md.mn[2]), std::fabs(md.mx[2]));
        float ey = std::max(std::fabs(md.mn[1]), std::fabs(md.mx[1]));
        md.radius = std::sqrt(ex * ex + ez * ez + ey * ey * 0.25f) * 0.9f;
        md.cy = (md.mn[1] + md.mx[1]) * 0.5f;
    }
    uint32_t vs = r.get<uint32_t>();
    uint32_t is = r.get<uint32_t>();
    if (!r.ok) return false;
    out.vblobOff = r.p;
    out.vblobSize = vs;
    out.iblobOff = r.p + vs;
    out.iblobSize = is;
    if (out.iblobOff + is > out.blob.size()) { plat_log("pak: truncated"); return false; }
    return true;
}
