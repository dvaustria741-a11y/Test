#include "chunks.h"
#include "platform.h"
#include <cmath>
#include <algorithm>

ChunkManager::ChunkManager(const World& w, const ModelIds& ids, const Pak& pk) : world_(w), ids_(ids), pak_(pk) {
    kinds_.assign(pk.models.size(), 0);
    for (size_t i = 0; i < pk.models.size(); i++) {
        std::string n = pk.models[i].name;
        auto st = [&](const char* pre) { return n.rfind(pre, 0) == 0; };
        if (st("CommonTree") || st("Pine") || st("TwistedTree")) kinds_[i] = HK_TREE;
        else if (st("DeadTree")) kinds_[i] = HK_DEAD;
        else if (st("Rock_Medium")) kinds_[i] = HK_ROCK;
        else if (st("Pebble") || st("RockPath")) kinds_[i] = HK_PEBBLE;
        else if (st("Mushroom")) kinds_[i] = HK_MUSH;
    }
    thread_ = std::thread([this] { worker(); });
}

ChunkManager::~ChunkManager() {
    {
        std::lock_guard<std::mutex> l(mtx_);
        quit_ = true;
    }
    cv_.notify_all();
    if (thread_.joinable()) thread_.join();
}

void ChunkManager::worker() {
    for (;;) {
        Job j;
        {
            std::unique_lock<std::mutex> l(mtx_);
            cv_.wait(l, [this] { return quit_ || !jobs_.empty(); });
            if (quit_) return;
            size_t best = 0;
            float bd = 1e30f;
            for (size_t i = 0; i < jobs_.size(); i++) {
                float dx = (jobs_[i].cx + 0.5f) * CHUNK_SIZE - camX_;
                float dz = (jobs_[i].cz + 0.5f) * CHUNK_SIZE - camZ_;
                float d = dx * dx + dz * dz + (jobs_[i].kind == 1 ? -1.0f : 0.0f);
                if (d < bd) { bd = d; best = i; }
            }
            j = jobs_[best];
            jobs_.erase(jobs_.begin() + (long)best);
        }
        Result r;
        r.kind = j.kind;
        r.gen = j.gen;
        if (j.kind == 0) {
            r.chunk.reset(new ChunkCPU());
            buildChunk(world_, ids_, pak_, j.cx, j.cz, j.res, *r.chunk);
        } else {
            r.detail.reset(new DetailCPU());
            buildDetail(world_, ids_, j.cx, j.cz, *r.detail);
        }
        {
            std::lock_guard<std::mutex> l(mtx_);
            results_.push_back(std::move(r));
        }
    }
}

void ChunkManager::configure(int terrainRes, int viewR, int detailR) {
    bool resChanged = terrainRes != res_;
    res_ = terrainRes;
    viewR_ = viewR;
    detailR_ = detailR;
    if (resChanged) {
        releaseGL();
    }
}

void ChunkManager::destroyChunk(Chunk& c) {
    if (c.vao[0]) glDeleteVertexArrays(3, c.vao);
    if (c.vbo) glDeleteBuffers(1, &c.vbo);
    if (c.ibo) glDeleteBuffers(1, &c.ibo);
    c.vbo = c.ibo = 0;
    c.vao[0] = c.vao[1] = c.vao[2] = 0;
}

void ChunkManager::releaseGL() {
    for (auto& kv : map_) destroyChunk(kv.second);
    map_.clear();
    requested_.clear();
    backlog_.clear();
    gen_++;
    std::lock_guard<std::mutex> l(mtx_);
    jobs_.clear();
    results_.clear();
}

void ChunkManager::uploadChunk(ChunkCPU& c) {
    Chunk ch;
    ch.cx = c.cx;
    ch.cz = c.cz;
    ch.minY = c.terrain.minY;
    ch.maxY = c.terrain.maxY;
    ch.structs = std::move(c.structs);
    ch.colliders = std::move(c.colliders);
    applyRemovals(ch.structs, &ch.colliders);
    glGenBuffers(1, &ch.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, ch.vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(c.terrain.verts.size() * sizeof(TVert)), c.terrain.verts.data(), GL_STATIC_DRAW);
    glGenBuffers(1, &ch.ibo);
    glGenVertexArrays(3, ch.vao);
    for (int L = 0; L < 3; L++) {
        glBindVertexArray(ch.vao[L]);
        glBindBuffer(GL_ARRAY_BUFFER, ch.vbo);
        size_t base = (size_t)c.terrain.voff[L] * sizeof(TVert);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TVert), (const void*)(base + 0));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_BYTE, GL_TRUE, sizeof(TVert), (const void*)(base + 12));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(TVert), (const void*)(base + 16));
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ch.ibo);
        if (L == 0)
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(c.terrain.idx.size() * sizeof(uint16_t)), c.terrain.idx.data(), GL_STATIC_DRAW);
        ch.icount[L] = c.terrain.icount[L];
        ch.ioff[L] = c.terrain.ioff[L] * (uint32_t)sizeof(uint16_t);
    }
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    map_[chunkKey(c.cx, c.cz)] = std::move(ch);
}

void ChunkManager::update(float px, float pz, int maxUploads) {
    std::vector<Result> fresh;
    {
        std::lock_guard<std::mutex> l(mtx_);
        camX_ = px;
        camZ_ = pz;
        fresh.swap(results_);
    }
    for (auto& r : fresh) backlog_.push_back(std::move(r));
    int pcx = (int)std::floor(px / CHUNK_SIZE), pcz = (int)std::floor(pz / CHUNK_SIZE);
    int uploads = 0;
    std::vector<Result> keep;
    for (auto& r : backlog_) {
        if (r.gen != gen_) continue;
        if (r.kind == 0 && r.chunk) {
            int64_t key = chunkKey(r.chunk->cx, r.chunk->cz);
            if (std::abs(r.chunk->cx - pcx) > viewR_ + 1 || std::abs(r.chunk->cz - pcz) > viewR_ + 1) {
                requested_.erase(key);
                continue;
            }
            if (uploads >= maxUploads) { keep.push_back(std::move(r)); continue; }
            uploadChunk(*r.chunk);
            uploads++;
        } else if (r.kind == 1 && r.detail) {
            auto it = map_.find(chunkKey(r.detail->cx, r.detail->cz));
            if (it != map_.end()) {
                it->second.detail = std::move(r.detail->items);
                applyRemovals(it->second.detail, nullptr);
                it->second.detailState = 2;
            }
        }
    }
    backlog_.swap(keep);

    std::vector<Job> newJobs;
    for (int dz = -viewR_; dz <= viewR_; dz++)
        for (int dx = -viewR_; dx <= viewR_; dx++) {
            if (dx * dx + dz * dz > viewR_ * viewR_ + viewR_) continue;
            int cx = pcx + dx, cz = pcz + dz;
            int64_t key = chunkKey(cx, cz);
            if (requested_.count(key)) continue;
            requested_.insert(key);
            newJobs.push_back({cx, cz, 0, gen_, res_});
        }
    for (auto& kv : map_) {
        Chunk& c = kv.second;
        int dx = c.cx - pcx, dz = c.cz - pcz;
        if (c.detailState == 0 && std::max(std::abs(dx), std::abs(dz)) <= detailR_) {
            c.detailState = 1;
            newJobs.push_back({c.cx, c.cz, 1, gen_, res_});
        }
    }
    if (!newJobs.empty()) {
        std::lock_guard<std::mutex> l(mtx_);
        for (auto& j : newJobs) jobs_.push_back(j);
        cv_.notify_one();
    }

    for (auto it = map_.begin(); it != map_.end();) {
        Chunk& c = it->second;
        int dx = std::abs(c.cx - pcx), dz = std::abs(c.cz - pcz);
        if (dx > viewR_ + 1 || dz > viewR_ + 1) {
            requested_.erase(it->first);
            destroyChunk(c);
            it = map_.erase(it);
            continue;
        }
        if (c.detailState == 2 && (dx > detailR_ + 1 || dz > detailR_ + 1)) {
            std::vector<PropInst>().swap(c.detail);
            c.detailState = 0;
        }
        ++it;
    }
}

bool ChunkManager::coreReady(float px, float pz, int ring) const {
    return coreProgress(px, pz, ring) >= 0.999f;
}

float ChunkManager::coreProgress(float px, float pz, int ring) const {
    int pcx = (int)std::floor(px / CHUNK_SIZE), pcz = (int)std::floor(pz / CHUNK_SIZE);
    int total = 0, ok = 0;
    for (int dz = -ring; dz <= ring; dz++)
        for (int dx = -ring; dx <= ring; dx++) {
            total += 2;
            auto it = map_.find(chunkKey(pcx + dx, pcz + dz));
            if (it != map_.end()) {
                ok++;
                if (it->second.detailState == 2) ok++;
            }
        }
    return total ? (float)ok / (float)total : 1.f;
}

void ChunkManager::collide(float& x, float& z, float radius) const {
    int pcx = (int)std::floor(x / CHUNK_SIZE), pcz = (int)std::floor(z / CHUNK_SIZE);
    for (int dz = -1; dz <= 1; dz++)
        for (int dx = -1; dx <= 1; dx++) {
            auto it = map_.find(chunkKey(pcx + dx, pcz + dz));
            if (it == map_.end()) continue;
            for (const Collider& c : it->second.colliders) {
                float ox = x - c.x, oz = z - c.z;
                float rr = c.r + radius;
                float d2 = ox * ox + oz * oz;
                if (d2 < rr * rr && d2 > 1e-6f) {
                    float d = std::sqrt(d2);
                    float push = rr - d;
                    x += ox / d * push;
                    z += oz / d * push;
                }
            }
        }
}

void ChunkManager::applyRemovals(std::vector<PropInst>& v, std::vector<Collider>* cols) {
    if (removed_.empty()) return;
    for (PropInst& p : v) {
        if (p.s <= 0.f) continue;
        if (removed_.count(propKey(p.x, p.z, p.model))) {
            p.s = 0.f;
            if (cols) {
                for (size_t i = 0; i < cols->size(); i++)
                    if (std::fabs((*cols)[i].x - p.x) < 0.05f && std::fabs((*cols)[i].z - p.z) < 0.05f) {
                        cols->erase(cols->begin() + (long)i);
                        break;
                    }
            }
        }
    }
}

void ChunkManager::removeProp(const PropInst& pi) {
    removed_.insert(propKey(pi.x, pi.z, pi.model));
    auto it = map_.find(chunkKey((int)std::floor(pi.x / CHUNK_SIZE), (int)std::floor(pi.z / CHUNK_SIZE)));
    if (it == map_.end()) return;
    applyRemovals(it->second.structs, &it->second.colliders);
    applyRemovals(it->second.detail, nullptr);
}

std::vector<uint64_t> ChunkManager::removedList() const {
    return std::vector<uint64_t>(removed_.begin(), removed_.end());
}

void ChunkManager::setRemoved(const std::vector<uint64_t>& v) {
    removed_.clear();
    for (uint64_t k : v) removed_.insert(k);
}

PropHit ChunkManager::raycast(V3 o, V3 d, float maxDist) const {
    PropHit best;
    best.t = maxDist;
    int pcx = (int)std::floor(o.x / CHUNK_SIZE), pcz = (int)std::floor(o.z / CHUNK_SIZE);
    for (int dz = -1; dz <= 1; dz++)
        for (int dx = -1; dx <= 1; dx++) {
            auto it = map_.find(chunkKey(pcx + dx, pcz + dz));
            if (it == map_.end()) continue;
            for (int pass = 0; pass < 2; pass++) {
                const std::vector<PropInst>& v = pass == 0 ? it->second.structs : it->second.detail;
                for (const PropInst& p : v) {
                    if (p.s <= 0.f) continue;
                    int kind = kinds_[p.model];
                    if (kind == HK_NONE) continue;
                    float ddx = p.x - o.x, ddz = p.z - o.z;
                    if (ddx * ddx + ddz * ddz > (maxDist + 4.f) * (maxDist + 4.f)) continue;
                    float t = -1.f;
                    if (kind == HK_TREE || kind == HK_DEAD) {
                        float r = (kind == HK_TREE ? 0.55f : 0.45f) * p.s;
                        float ymax = p.y + (kind == HK_TREE ? 3.6f : 3.0f) * p.s + 1.0f;
                        float ocx = o.x - p.x, ocz = o.z - p.z;
                        float a = d.x * d.x + d.z * d.z;
                        if (a < 1e-6f) continue;
                        float b = 2.f * (ocx * d.x + ocz * d.z);
                        float cc = ocx * ocx + ocz * ocz - r * r;
                        float disc = b * b - 4.f * a * cc;
                        if (disc < 0.f) continue;
                        float tt = cc < 0.f ? 0.f : (-b - std::sqrt(disc)) / (2.f * a);
                        if (tt < 0.f) continue;
                        float y = o.y + d.y * tt;
                        if (y < p.y - 0.2f || y > ymax) continue;
                        t = tt;
                    } else {
                        const ModelDef& md = pak_.models[p.model];
                        float r = kind == HK_ROCK ? md.radius * p.s * 0.6f : std::max(0.5f, md.radius * p.s * 1.1f);
                        V3 c = {p.x, p.y + md.cy * p.s, p.z};
                        V3 oc = o - c;
                        float b = dot(oc, d);
                        float cc = dot(oc, oc) - r * r;
                        float disc = b * b - cc;
                        if (disc < 0.f) continue;
                        float tt = -b - std::sqrt(disc);
                        if (tt < 0.f) tt = cc < 0.f ? 0.f : -1.f;
                        if (tt < 0.f) continue;
                        t = tt;
                    }
                    if (t >= 0.f && t < best.t) {
                        best.t = t;
                        best.p = p;
                        best.kind = kind;
                        best.valid = true;
                    }
                }
            }
        }
    return best;
}
