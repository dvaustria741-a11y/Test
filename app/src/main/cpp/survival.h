#pragma once

enum Item {
    IT_WOOD = 0, IT_STONE, IT_MUSH, IT_COOKED, IT_AXE, IT_PICK, IT_TORCH, IT_CAMPFIRE, IT_BENCH, IT_BED, IT_CHEST, IT_COUNT
};

struct ItemInfo {
    const char* name;
    bool tool;
    bool placeable;
    bool food;
    float hunger;
    float r, g, b;
};

static const ItemInfo kItems[IT_COUNT] = {
    {"WOOD", false, false, false, 0, 0.62f, 0.42f, 0.22f},
    {"STONE", false, false, false, 0, 0.62f, 0.64f, 0.68f},
    {"MUSHROOM", false, false, true, 10, 0.92f, 0.62f, 0.28f},
    {"COOKED", false, false, true, 30, 0.80f, 0.45f, 0.20f},
    {"AXE", true, false, false, 0, 0.75f, 0.78f, 0.82f},
    {"PICKAXE", true, false, false, 0, 0.75f, 0.78f, 0.82f},
    {"TORCH", true, false, false, 0, 1.00f, 0.65f, 0.25f},
    {"CAMPFIRE", false, true, false, 0, 0.95f, 0.45f, 0.18f},
    {"WORKBENCH", false, true, false, 0, 0.60f, 0.42f, 0.24f},
    {"BED", false, true, false, 0, 0.70f, 0.30f, 0.30f},
    {"CHEST", false, true, false, 0, 0.55f, 0.38f, 0.20f},
};

struct Recipe {
    int result;
    int c0, n0, c1, n1;
};

static const Recipe kRecipes[] = {
    {IT_AXE, IT_WOOD, 3, IT_STONE, 2},
    {IT_PICK, IT_WOOD, 3, IT_STONE, 3},
    {IT_TORCH, IT_WOOD, 2, -1, 0},
    {IT_CAMPFIRE, IT_WOOD, 6, IT_STONE, 4},
    {IT_BENCH, IT_WOOD, 12, IT_STONE, 4},
    {IT_BED, IT_WOOD, 14, -1, 0},
    {IT_CHEST, IT_WOOD, 10, -1, 0},
};
static const int kRecipeCount = (int)(sizeof(kRecipes) / sizeof(kRecipes[0]));

enum PlaceKind { PK_FLOOR = 0, PK_EDGE = 1, PK_FREE = 2 };
enum CollKind { CK_NONE = 0, CK_WALL = 1, CK_DOOR = 2, CK_CIRCLE = 3, CK_FENCE = 4 };

struct PlaceDef {
    const char* id;
    const char* name;
    const char* model;
    const char* fire;
    int kind;
    int wood;
    int stone;
    int item;
    int coll;
    float colR;
    float yOff;
};

static const PlaceDef kPlace[] = {
    {"FLOOR_W", "WOOD FLOOR", "Floor_WoodDark", nullptr, PK_FLOOR, 4, 0, -1, CK_NONE, 0, 0},
    {"FLOOR_L", "LIGHT FLOOR", "Floor_WoodLight", nullptr, PK_FLOOR, 4, 0, -1, CK_NONE, 0, 0},
    {"WALL", "WALL", "Wall_Plaster_Straight", nullptr, PK_EDGE, 5, 0, -1, CK_WALL, 0, 0},
    {"DOORW", "DOOR WALL", "Wall_Plaster_Door_Flat", nullptr, PK_EDGE, 6, 0, -1, CK_DOOR, 0, 0},
    {"WINW", "WINDOW WALL", "Wall_Plaster_Window_Wide_Flat", nullptr, PK_EDGE, 6, 0, -1, CK_WALL, 0, 0},
    {"SWALL", "STONE WALL", "Wall_UnevenBrick_Straight", nullptr, PK_EDGE, 0, 6, -1, CK_WALL, 0, 0},
    {"SDOOR", "STONE DOOR", "Wall_UnevenBrick_Door_Flat", nullptr, PK_EDGE, 0, 7, -1, CK_DOOR, 0, 0},
    {"SWIN", "STONE WINDOW", "Wall_UnevenBrick_Window_Wide_Flat", nullptr, PK_EDGE, 0, 7, -1, CK_WALL, 0, 0},
    {"FENCE", "FENCE", "Prop_WoodenFence_Single", nullptr, PK_EDGE, 2, 0, -1, CK_FENCE, 0, 0},
    {"ROOF", "ROOF", "Roof_Wooden_2x1", nullptr, PK_EDGE, 5, 0, -1, CK_NONE, 0, 3.1f},
    {"CAMPFIRE", "CAMPFIRE", "Bonfire", "Bonfire_Fire", PK_FREE, 0, 0, IT_CAMPFIRE, CK_CIRCLE, 0.55f, 0},
    {"BENCH", "WORKBENCH", "Workbench", nullptr, PK_FREE, 0, 0, IT_BENCH, CK_CIRCLE, 0.95f, 0},
    {"BED", "BED", "Bed_Twin1", nullptr, PK_FREE, 0, 0, IT_BED, CK_CIRCLE, 1.1f, 0},
    {"CHEST", "CHEST", "Chest_Wood", nullptr, PK_FREE, 0, 0, IT_CHEST, CK_CIRCLE, 0.65f, 0},
};
static const int kPlaceCount = (int)(sizeof(kPlace) / sizeof(kPlace[0]));
static const int kBuildPieces = 10;
static const int PD_CAMPFIRE = 10;
static const int PD_BED = 12;

inline int placeDefForItem(int item) {
    for (int i = 0; i < kPlaceCount; i++) if (kPlace[i].item == item) return i;
    return -1;
}
