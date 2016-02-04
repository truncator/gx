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

enum UnitFlags
{
    UNIT_MOVE_ORDER = 0x01,
};

struct Path
{
    // TODO: optimize space?
    struct VisibilityNode *nodes[32];
    uint32 node_count;
    uint32 current_node_index;

    vec2 start;
    vec2 end;
};

struct Ship
{
    uint32 id;
    uint8 team;

    uint32 flags;

    vec2 position;
    float rotation;
    vec2 size;

    vec2 move_velocity;
    float rotation_velocity;

    int32 health;

    // TODO: expand on this
    float fire_cooldown;
    float fire_cooldown_timer;

    struct Path path;
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

struct WorkingPathNode
{
    struct VisibilityNode *node;
    struct WorkingPathNode *parent;

    float f_cost;
    float g_cost;
    float h_cost;
};

struct VisibilityNode
{
    uint32 vertex_index;

    // TODO: linked list if memory becomes an issue?
    uint32 neighbor_indices[64];
    uint32 neighbor_index_count;
};

struct VisibilityGraph
{
    vec2 vertices[4096];
    uint32 vertex_count;

    struct VisibilityNode nodes[4096];
    uint32 node_count;
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

    struct VisibilityGraph visibility_graph;

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
