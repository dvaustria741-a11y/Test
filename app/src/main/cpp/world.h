#pragma once
#include <vector>
#include <cstdint>
#include "noise.h"
#include "pak.h"

constexpr int CHUNK_SIZE = 64;

struct Biome { float desert, autumn, pine; };

struct TVert {
    float x, y, z;
    int8_t nx, ny, nz, nw;
    uint8_t r, g, b, a;
};

struct PropInst {
    float x, y, z, s, yaw;
    uint32_t tint;
    uint16_t model;
    uint8_t var;
    uint8_t rnd;
};

struct Collider { float x, z, r; };

struct TerrainCPU {
    std::vector<TVert> verts;
    std::vector<uint16_t> idx;
    uint32_t vcount[3];
    uint32_t voff[3];
    uint32_t icount[3];
    uint32_t ioff[3];
    float minY, maxY;
};

struct ChunkCPU {
    int cx, cz;
    TerrainCPU terrain;
    std::vector<PropInst> structs;
    std::vector<Collider> colliders;
};

struct DetailCPU {
    int cx, cz;
    std::vector<PropInst> items;
};

struct ModelIds {
    int commonTree[5], pine[5], twisted[5], dead[5];
    int rockMed[3];
    int bush[2];
    int grass[4];
    int flowers[4];
    int fern, clover[2], plantBig[2], plantSmall[2];
    int mushroom[2];
    int pebble[11];
    int pathStone[4];
    void fill(const Pak& pk);
};

class World {
public:
    explicit World(uint32_t seed = 1337u) : seed_(seed) {}
    float height(float x, float z) const;
    Biome biome(float x, float z) const;
    float path(float x, float z) const;
    float density(float x, float z) const;
    void terrainColor(float x, float z, const Biome& b, float slope, float out[3]) const;
    uint32_t seed() const { return seed_; }
private:
    float pathField(float x, float z, int k) const;
    float baseHeight(float x, float z, const Biome& b) const;
    uint32_t seed_;
};

void buildChunk(const World& w, const ModelIds& ids, const Pak& pk, int cx, int cz, int terrainRes, ChunkCPU& out);
void buildDetail(const World& w, const ModelIds& ids, int cx, int cz, DetailCPU& out);
