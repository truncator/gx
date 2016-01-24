#pragma once

#include "gx_define.h"
#include "gx_math.h"

struct Input;
struct Renderer;

struct GameMemory
{
    void *game_memory;
    size_t game_memory_size;
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
    vec2 size;

    float rotation;
};

struct GameState
{
    struct Camera camera;
};

void init_game(struct GameMemory *memory);
void tick_game(struct GameMemory *memory, struct Input *input, float dt);
void render_game(struct GameMemory *memory, struct Renderer *renderer, uint32 viewport_width, uint32 viewport_height);
