#pragma once

enum Item {
    IT_WOOD = 0, IT_STONE, IT_MUSH, IT_COOKED, IT_AXE, IT_PICK, IT_TORCH, IT_CAMPFIRE, IT_BENCH, IT_BED, IT_CHEST,
    IT_SWORD, IT_MEAT, IT_COOKMEAT, IT_COUNT
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
    {"SWORD", true, false, false, 0, 0.85f, 0.80f, 0.55f},
    {"RAW MEAT", false, false, true, 14, 0.85f, 0.35f, 0.35f},
    {"COOKED MEAT", false, false, true, 48, 0.62f, 0.32f, 0.18f},
};

struct Recipe {
    int result;
    int c0, n0, c1, n1;
};

static const Recipe kRecipes[] = {
    {IT_AXE, IT_WOOD, 3, IT_STONE, 2},
    {IT_PICK, IT_WOOD, 3, IT_STONE, 3},
    {IT_SWORD, IT_WOOD, 4, IT_STONE, 6},
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
    float inset;
    int ramp;
};

static const PlaceDef kPlace[] = {
    {"FLOOR_W", "WOOD FLOOR", "Floor_WoodDark", nullptr, PK_FLOOR, 4, 0, -1, CK_NONE, 0, 0, 0, 0},
    {"FLOOR_L", "LIGHT FLOOR", "Floor_WoodLight", nullptr, PK_FLOOR, 4, 0, -1, CK_NONE, 0, 0, 0, 0},
    {"WALL", "WALL", "Wall_Plaster_Straight", nullptr, PK_EDGE, 5, 0, -1, CK_WALL, 0, 0, 0, 0},
    {"DOORW", "DOOR WALL", "Wall_Plaster_Door_Flat", nullptr, PK_EDGE, 6, 0, -1, CK_DOOR, 0, 0, 0, 0},
    {"WINW", "WINDOW WALL", "Wall_Plaster_Window_Wide_Flat", nullptr, PK_EDGE, 6, 0, -1, CK_WALL, 0, 0, 0, 0},
    {"SWALL", "STONE WALL", "Wall_UnevenBrick_Straight", nullptr, PK_EDGE, 0, 6, -1, CK_WALL, 0, 0, 0, 0},
    {"SDOOR", "STONE DOOR", "Wall_UnevenBrick_Door_Flat", nullptr, PK_EDGE, 0, 7, -1, CK_DOOR, 0, 0, 0, 0},
    {"SWIN", "STONE WINDOW", "Wall_UnevenBrick_Window_Wide_Flat", nullptr, PK_EDGE, 0, 7, -1, CK_WALL, 0, 0, 0, 0},
    {"FENCE", "FENCE", "Prop_WoodenFence_Single", nullptr, PK_EDGE, 2, 0, -1, CK_FENCE, 0, 0, 0, 0},
    {"ROOF", "ROOF", "Roof_Wooden_2x1", nullptr, PK_EDGE, 5, 0, -1, CK_NONE, 0, 2.95f, 1.5f, 0},
    {"RIDGE", "ROOF RIDGE", "Roof_Wooden_2x1_Middle", nullptr, PK_EDGE, 3, 0, -1, CK_NONE, 0, 2.95f, 0, 0},
    {"STAIRS", "STAIRS", "Stair_Interior_Solid", nullptr, PK_FREE, 10, 0, -1, CK_NONE, 0, 0, 0, 1},
    {"CAMPFIRE", "CAMPFIRE", "Bonfire", "Bonfire_Fire", PK_FREE, 0, 0, IT_CAMPFIRE, CK_CIRCLE, 0.55f, 0, 0, 0},
    {"BENCH", "WORKBENCH", "Workbench", nullptr, PK_FREE, 0, 0, IT_BENCH, CK_CIRCLE, 0.95f, 0, 0, 0},
    {"BED", "BED", "Bed_Twin1", nullptr, PK_FREE, 0, 0, IT_BED, CK_CIRCLE, 1.1f, 0, 0, 0},
    {"CHEST", "CHEST", "Chest_Wood", nullptr, PK_FREE, 0, 0, IT_CHEST, CK_CIRCLE, 0.65f, 0, 0, 0},
};
static const int kPlaceCount = (int)(sizeof(kPlace) / sizeof(kPlace[0]));
static const int kBuildPieces = 12;
static const int PD_CAMPFIRE = 12;
static const int PD_BED = 14;
static const float kLevelH = 3.12f;
static const float kStairRise = 3.03f;
static const float kStairRun = 4.3f;
static const float kStairHalfW = 0.86f;

inline int placeDefForItem(int item) {
    for (int i = 0; i < kPlaceCount; i++) if (kPlace[i].item == item) return i;
    return -1;
}

enum AnimalType { AN_DEER = 0, AN_RABBIT, AN_COW, AN_BOAR, AN_WOLF, AN_BEAR, AN_FOX, AN_COUNT };

struct AnimalDef {
    const char* name;
    const char* models[3];
    int nModels;
    float hp;
    float walk;
    float run;
    int mode;
    float dmg;
    float reach;
    int meat;
    float radius;
    float hitR;
    float hitH;
};

static const AnimalDef kAnimals[AN_COUNT] = {
    {"DEER", {"Animal_Deer", "Animal_Stag", "Animal_Doe"}, 3, 40, 1.6f, 7.5f, 0, 0, 0, 3, 0.6f, 0.8f, 1.0f},
    {"RABBIT", {"Animal_Rabbit", nullptr, nullptr}, 1, 10, 1.2f, 6.5f, 0, 0, 0, 1, 0.25f, 0.4f, 0.2f},
    {"COW", {"Animal_Cow", nullptr, nullptr}, 1, 60, 1.0f, 4.0f, 0, 0, 0, 4, 0.8f, 0.9f, 0.8f},
    {"BOAR", {"Animal_Boar", nullptr, nullptr}, 1, 50, 1.4f, 6.2f, 3, 12, 1.7f, 3, 0.6f, 0.7f, 0.5f},
    {"WOLF", {"Animal_Wolf", "Animal_WolfWhite", nullptr}, 2, 45, 2.0f, 7.4f, 2, 9, 1.8f, 2, 0.6f, 0.7f, 0.5f},
    {"BEAR", {"Animal_Bear", nullptr, nullptr}, 1, 120, 1.5f, 5.6f, 1, 22, 2.2f, 5, 0.9f, 1.0f, 0.8f},
    {"FOX", {"Animal_Fox", nullptr, nullptr}, 1, 20, 1.5f, 7.0f, 0, 0, 0, 1, 0.4f, 0.5f, 0.3f},
};
