#pragma once
#include <vector>
#include <string>
#include <cstdint>

enum ModelCat { CAT_TREE = 0, CAT_ROCK = 1, CAT_BUSH = 2, CAT_DETAIL = 3, CAT_PEBBLE = 4 };

struct MatDef {
    char name[33];
    char tex[33];
    uint32_t size;
    uint32_t alpha;
    uint32_t vcol;
    uint32_t dbl;
    float boost;
    float sway;
    float aoMin;
    float soft;
};

struct SubMesh {
    uint32_t mat;
    uint32_t voff;
    uint32_t vcount;
    uint32_t ioff;
    uint32_t icount;
};

struct ModelDef {
    char name[33];
    float mn[3];
    float mx[3];
    std::vector<std::vector<SubMesh>> lods;
    int cat;
    float radius;
    float cy;
};

struct Pak {
    std::vector<MatDef> mats;
    std::vector<ModelDef> models;
    std::vector<uint8_t> blob;
    size_t vblobOff = 0, vblobSize = 0;
    size_t iblobOff = 0, iblobSize = 0;
    int find(const char* name) const {
        std::string want = std::string(name).substr(0, 32);
        for (size_t i = 0; i < models.size(); i++)
            if (std::string(models[i].name) == want) return (int)i;
        return -1;
    }
};

bool pakParse(std::vector<uint8_t>&& data, Pak& out);
