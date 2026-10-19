#pragma once
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>
#include "gl.h"
#include "world.h"

struct Chunk {
    int cx = 0, cz = 0;
    GLuint vbo = 0, ibo = 0;
    GLuint vao[3] = {0, 0, 0};
    uint32_t icount[3] = {0, 0, 0};
    uint32_t ioff[3] = {0, 0, 0};
    float minY = 0, maxY = 0;
    std::vector<PropInst> structs;
    std::vector<Collider> colliders;
    std::vector<PropInst> detail;
    int detailState = 0;
};

inline int64_t chunkKey(int cx, int cz) { return ((int64_t)(uint32_t)cx << 32) | (uint32_t)cz; }

class ChunkManager {
public:
    ChunkManager(const World& w, const ModelIds& ids, const Pak& pk);
    ~ChunkManager();
    void configure(int terrainRes, int viewR, int detailR);
    void update(float px, float pz, int maxUploads);
    void releaseGL();
    bool coreReady(float px, float pz, int ring) const;
    float coreProgress(float px, float pz, int ring) const;
    void collide(float& x, float& z, float radius) const;
    const std::unordered_map<int64_t, Chunk>& chunks() const { return map_; }

private:
    struct Job { int cx, cz, kind, gen, res; };
    struct Result {
        int kind = 0, gen = 0;
        std::unique_ptr<ChunkCPU> chunk;
        std::unique_ptr<DetailCPU> detail;
    };
    void worker();
    void uploadChunk(ChunkCPU& c);
    void destroyChunk(Chunk& c);

    const World& world_;
    const ModelIds& ids_;
    const Pak& pak_;
    std::unordered_map<int64_t, Chunk> map_;
    std::unordered_set<int64_t> requested_;
    std::vector<Result> backlog_;
    int res_ = 16, viewR_ = 3, detailR_ = 1, gen_ = 0;
    std::thread thread_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::vector<Job> jobs_;
    std::vector<Result> results_;
    float camX_ = 0, camZ_ = 0;
    bool quit_ = false;
};
