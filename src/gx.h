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
    vec2 position;
    float rotation;
    vec2 size;

    vec2 move_velocity;
    float rotation_velocity;
};

struct GameState
{
    struct Camera camera;

    struct Ship ships[8];
    uint32 ship_count;
};

void init_game(struct GameMemory *memory);
void tick_game(struct GameMemory *memory, struct Input *input, float dt);
void render_game(struct GameMemory *memory, struct Renderer *renderer, uint32 viewport_width, uint32 viewport_height);
