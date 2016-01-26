#pragma once

#include "gx_define.h"
#include "gx_math.h"

struct Input;
struct Renderer;

struct UIntHashPair
{
    uint32 key;
    uint32 value;
};

struct UIntHashMap
{
    struct UIntHashPair buckets[4096];
};

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

enum ShipTeam
{
    TEAM_ALLY,
    TEAM_ENEMY,
};

struct Ship
{
    uint32 id;
    uint8 team;

    vec2 position;
    float rotation;
    vec2 size;

    vec2 move_velocity;
    float rotation_velocity;

    bool move_order;
    vec2 target_position;

    int32 health;

    // TODO: expand on this
    float fire_cooldown;
    float fire_cooldown_timer;
};

struct AABB
{
    vec2 min;
    vec2 max;
};

struct Projectile
{
    uint32 owner;
    uint8 team;
    int32 damage;

    vec2 position;
    vec2 size;
    vec2 velocity;
};

struct DistanceField
{
    // TODO: decide whether this should be dynamically allocated or not
    uint16 values[32*32];
    uint32 width;
    uint32 height;
};

struct Building
{
    vec2 position;
    vec2 size;
};

struct GameState
{
    struct Camera camera;


    //
    // map
    //

    struct DistanceField distance_field;

    struct Building buildings[64];
    uint32 building_count;


    //
    // ship
    //

    struct Ship ships[64];
    uint32 ship_count;

    uint32 ship_ids;
    struct UIntHashMap ship_id_map;

    uint32 selected_ships[256];
    uint32 selected_ship_count;


    //
    // projectile
    //

    struct Projectile projectiles[256];
    uint32 projectile_count;
};

void init_game(struct GameMemory *memory);
void tick_game(struct GameMemory *memory, struct Input *input, uint32 screen_width, uint32 screen_height, float dt);
void render_game(struct GameMemory *memory, struct Renderer *renderer, uint32 screen_width, uint32 screen_height);
