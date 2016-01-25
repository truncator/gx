#pragma once

#include "gx_define.h"
#include "gx_math.h"

struct Input;
struct Renderer;

struct GameMemory
{
    void *game_memory;
    size_t game_memory_size;

    void *render_memory;
    size_t render_memory_size;
};

struct Camera
{
    vec2 position;
    float rotation;
    float zoom;

    vec2 move_velocity;
    float zoom_velocity;
};

struct Ship
{
    uint32 id;

    vec2 position;
    float rotation;
    vec2 size;

    vec2 move_velocity;
    float rotation_velocity;

    bool move_order;
    vec2 target_position;
};

struct AABB
{
    vec2 min;
    vec2 max;
};

struct UIntHashPair
{
    uint32 key;
    uint32 value;
};

struct UIntHashMap
{
    struct UIntHashPair buckets[4096];
};

struct GameState
{
    struct Camera camera;

    struct Ship ships[8];
    uint32 ship_count;

    uint32 ship_ids;
    struct UIntHashMap ship_id_map;

    uint32 selected_ships[256];
    uint32 selected_ship_count;
};

void init_game(struct GameMemory *memory);
void tick_game(struct GameMemory *memory, struct Input *input, uint32 screen_width, uint32 screen_height, float dt);
void render_game(struct GameMemory *memory, struct Renderer *renderer, uint32 screen_width, uint32 screen_height);
