#include "gx.h"
#include "gx_io.h"
#include "gx_renderer.h"

static void tick_camera(struct Input *input, struct Camera *camera, float dt)
{
    //
    // move
    //

    vec2 camera_up = vec2_direction(camera->rotation);
    vec2 camera_right = vec2_direction(camera->rotation - PI_OVER_TWO);

    vec2 move_acceleration = vec2_zero();
    if (key_down(GLFW_KEY_W, input))
        move_acceleration = vec2_add(move_acceleration, camera_up);
    if (key_down(GLFW_KEY_A, input))
        move_acceleration = vec2_add(move_acceleration, vec2_negate(camera_right));
    if (key_down(GLFW_KEY_S, input))
        move_acceleration = vec2_add(move_acceleration, vec2_negate(camera_up));
    if (key_down(GLFW_KEY_D, input))
        move_acceleration = vec2_add(move_acceleration, camera_right);

    if (vec2_length2(move_acceleration) > FLOAT_EPSILON)
    {
        const float acceleration_magnitude = camera->zoom * 10.0f;
        move_acceleration = vec2_normalize(move_acceleration);
        move_acceleration = vec2_mul(move_acceleration, acceleration_magnitude);

        // v = v0 + (a*t)
        camera->move_velocity = vec2_add(camera->move_velocity, vec2_mul(move_acceleration, dt));

        // Clamp move velocity.
        const float max_move_speed = 5.0f * sqrtf(camera->zoom);
        if (vec2_length2(camera->move_velocity) > (max_move_speed * max_move_speed))
            camera->move_velocity = vec2_mul(vec2_normalize(camera->move_velocity), max_move_speed);
    }

    // Number of seconds required for friction to stop movement completely.
    // (because velocity is sampled each frame, actual stop time longer than this)
    float stop_time = 0.1f;
    float inv_stop_time = 1.0f / stop_time;

    if (move_acceleration.x == 0.0f)
        camera->move_velocity.x -= camera->move_velocity.x * inv_stop_time * dt;
    if (move_acceleration.y == 0.0f)
        camera->move_velocity.y -= camera->move_velocity.y * inv_stop_time * dt;

    // r = r0 + (v0*t) + (a*t^2)/2
    camera->position = vec2_add(vec2_add(camera->position, vec2_mul(camera->move_velocity, dt)), vec2_div(vec2_mul(move_acceleration, dt * dt), 2.0f));


    //
    // zoom
    //

    float zoom_acceleration = 0.0f;
    if (key_down(GLFW_KEY_SPACE, input))
        zoom_acceleration += 1.0f;
    if (key_down(GLFW_KEY_LEFT_CONTROL, input))
        zoom_acceleration -= 1.0f;
    zoom_acceleration *= camera->zoom * 50.0f;

    if (zoom_acceleration == 0.0f)
        camera->zoom_velocity -= camera->zoom_velocity * (1.0f / 0.1f) * dt;

    // v = v0 + (a*t)
    camera->zoom_velocity += zoom_acceleration * dt;

    // r = r0 + (v0*t) + (a*t^2)/2
    camera->zoom += (camera->zoom_velocity * dt) + (zoom_acceleration * dt * dt) / 2.0f;

    // Clamp zoom velocity.
    const float max_zoom_speed = 3.0f * camera->zoom;
    camera->zoom_velocity = clamp_float(camera->zoom_velocity, -max_zoom_speed, max_zoom_speed);

    float unclamped_zoom = camera->zoom;
    camera->zoom = clamp_float(camera->zoom, 1.0f, 1000.0f);
    if (camera->zoom != unclamped_zoom)
        camera->zoom_velocity = 0.0f;
}

static struct Ship *new_ship(struct GameState *game_state)
{
    ASSERT(game_state->ship_count < ARRAY_SIZE(game_state->ships));

    struct Ship *ship = &game_state->ships[game_state->ship_count++];
    return ship;
}

void init_game(struct GameMemory *memory)
{
    ASSERT(memory->game_memory_size >= sizeof(struct GameState));
    struct GameState *game_state = (struct GameState *)memory->game_memory;

    struct Camera *camera = &game_state->camera;
    camera->zoom = 10.0f;

    struct Ship *ship = new_ship(game_state);
    ship->size = vec2_new(1, 1);
    ship->move_velocity = vec2_new(0, 0);
}

static void tick_physics(struct GameState *game_state, float dt)
{
    for (uint32 i = 0; i < game_state->ship_count; ++i)
    {
        struct Ship *ship = &game_state->ships[i];

        vec2 move_acceleration = vec2_zero();

        // v = v0 + (a*t)
        ship->move_velocity = vec2_add(ship->move_velocity, vec2_mul(move_acceleration, dt));

        // r = r0 + (v*t) + (a*t^2)/2
        ship->position = vec2_add(vec2_add(ship->position, vec2_mul(ship->move_velocity, dt)), vec2_div(vec2_mul(move_acceleration, dt * dt), 2.0f));
    }
}

void tick_game(struct GameMemory *memory, struct Input *input, float dt)
{
    struct GameState *game_state = (struct GameState *)memory->game_memory;
    struct RenderBuffer *render_buffer = (struct RenderBuffer *)memory->render_memory;

    clear_quad_buffer(&render_buffer->quads);
    clear_quad_buffer(&render_buffer->screen_quads);

    if (mouse_down(MOUSE_LEFT, input))
    {
        vec2 origin = input->mouse_down_positions[MOUSE_LEFT];
        vec2 end = input->mouse_position;

        vec2 min = min_vec2(origin, end);
        vec2 max = max_vec2(origin, end);

        vec2 bottom_left  = vec2_new(min.x, max.y);
        vec2 bottom_right = vec2_new(max.x, max.y);
        vec2 top_left     = vec2_new(min.x, min.y);
        vec2 top_right    = vec2_new(max.x, min.y);

        vec2 size = vec2_sub(max, min);

        const uint32 outline_thickness = 1;

        draw_screen_quad_buffered(&render_buffer->screen_quads, top_left, vec2_new(size.x, outline_thickness), vec4_zero(), vec3_new(1, 1, 0));
        draw_screen_quad_buffered(&render_buffer->screen_quads, bottom_left, vec2_new(size.x, outline_thickness), vec4_zero(), vec3_new(1, 1, 0));
        draw_screen_quad_buffered(&render_buffer->screen_quads, top_left, vec2_new(outline_thickness, size.y), vec4_zero(), vec3_new(1, 1, 0));
        draw_screen_quad_buffered(&render_buffer->screen_quads, top_right, vec2_new(outline_thickness, size.y), vec4_zero(), vec3_new(1, 1, 0));
    }

    tick_camera(input, &game_state->camera, dt);
    tick_physics(game_state, dt);
}

void render_game(struct GameMemory *memory, struct Renderer *renderer, uint32 viewport_width, uint32 viewport_height)
{
    struct GameState *game_state = (struct GameState *)memory->game_memory;
    struct RenderBuffer *render_buffer = (struct RenderBuffer *)memory->render_memory;

    float aspect_ratio = (float)viewport_width / (float)viewport_height;
    mat4 projection_matrix = mat4_orthographic(-game_state->camera.zoom/2.0f, game_state->camera.zoom/2.0f,
                                               -game_state->camera.zoom/2.0f / aspect_ratio, game_state->camera.zoom/2.0f / aspect_ratio,
                                               0.0f, 1.0f);
    mat4 view_matrix = mat4_look_at(vec3_new(game_state->camera.position.x, game_state->camera.position.y, 0.0f), vec3_new(game_state->camera.position.x, game_state->camera.position.y, -1.0f), vec3_new(0, 1, 0));
    mat4 view_projection_matrix = mat4_mul(projection_matrix, view_matrix);

    update_ubo(renderer->camera_ubo, sizeof(mat4), &view_projection_matrix);

    bind_program(renderer->quad_program);
    begin_sprite_batch(&renderer->sprite_batch);

    for (uint32 i = 0; i < game_state->ship_count; ++i)
    {
        struct Ship *ship = &game_state->ships[i];
        draw_quad(&renderer->sprite_batch, ship->position, ship->size, vec3_new(0.5f, 0.5f, 0.5f));
    }

    end_sprite_batch(&renderer->sprite_batch);
    bind_program(0);

    bind_program(renderer->quad_program);
    draw_quad_buffer(&renderer->sprite_batch, &render_buffer->quads);
    bind_program(0);

    view_projection_matrix = mat4_orthographic(0, viewport_width, viewport_height, 0, 0, 1);
    update_ubo(renderer->camera_ubo, sizeof(mat4), &view_projection_matrix);

    bind_program(renderer->quad_program);
    draw_screen_quad_buffer(&renderer->sprite_batch, &render_buffer->screen_quads);
    bind_program(0);
}
