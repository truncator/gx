#include "gx.h"
#include "gx_io.h"
#include "gx_renderer.h"

#define NULL_UINT_HASH_KEY UINT32_MAX

static struct UIntHashMap create_uint_hash_map()
{
    struct UIntHashMap map = {0};

    uint32 size = ARRAY_SIZE(map.buckets);
    for (uint32 i = 0; i < size; ++i)
    {
        struct UIntHashPair *pair = &map.buckets[i];
        pair->key = NULL_UINT_HASH_KEY;
    }

    return map;
}

static struct UIntHashPair *find_pair(struct UIntHashMap *map, uint32 key)
{
    uint32 hash_map_size = ARRAY_SIZE(map->buckets);

    // TODO: actually hash this?
    uint32 hash = key;
    uint32 bucket = hash % hash_map_size;

    struct UIntHashPair *pair = &map->buckets[bucket];
    if (pair->key == NULL_UINT_HASH_KEY)
        return NULL;

    while (pair->key != key)
    {
        bucket = (bucket + 1) % hash_map_size;
        pair = &map->buckets[bucket];

        if (pair->key == NULL_UINT_HASH_KEY)
            return NULL;
    }

    return pair;
}

static void emplace(struct UIntHashMap *map, uint32 key, uint32 value)
{
    struct UIntHashPair *pair = find_pair(map, key);
    ASSERT(pair == NULL);

    uint32 hash_map_size = ARRAY_SIZE(map->buckets);

    uint32 hash = key;
    uint32 bucket = hash % hash_map_size;

    pair = &map->buckets[bucket];
    while ((pair->key != NULL_UINT_HASH_KEY) && (pair->key != key))
    {
        bucket = (bucket + 1) % hash_map_size;
        pair = &map->buckets[bucket];

        if (pair->key == NULL_UINT_HASH_KEY)
        {
            pair->key = key;
            pair->value = value;
            return;
        }
    }

    pair->key = key;
    pair->value = value;
}

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

static uint32 generate_ship_id(struct GameState *game_state)
{
    ASSERT(game_state->ship_ids < UINT32_MAX);

    uint32 id = game_state->ship_ids;
    ++game_state->ship_ids;

    return id;
}

static struct Ship *new_ship(struct GameState *game_state)
{
    ASSERT(game_state->ship_count < ARRAY_SIZE(game_state->ships));

    uint32 array_index = game_state->ship_count;
    ++game_state->ship_count;

    struct Ship *ship = &game_state->ships[array_index];
    ship->id = generate_ship_id(game_state);

    emplace(&game_state->ship_id_map, ship->id, array_index);

    return ship;
}

void init_game(struct GameMemory *memory)
{
    ASSERT(memory->game_memory_size >= sizeof(struct GameState));
    struct GameState *game_state = (struct GameState *)memory->game_memory;

    struct Camera *camera = &game_state->camera;
    camera->zoom = 20.0f;

    game_state->ship_id_map = create_uint_hash_map();

    for (uint32 i = 0; i < 4; ++i)
    {
        struct Ship *ship = new_ship(game_state);
        ship->position = vec2_new(random_float(-5.0f, 5.0f), random_float(-5.0f, 5.0f));
        ship->size = vec2_new(1, 1);
        ship->move_velocity = vec2_new(0, 0);
    }
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

static struct AABB calc_mouse_selection_box(struct Input *input, uint32 mouse_button)
{
    vec2 origin = input->mouse_down_positions[mouse_button];
    vec2 end = input->mouse_position;

    struct AABB aabb = {0};
    aabb.min = min_vec2(origin, end);
    aabb.max = max_vec2(origin, end);
    return aabb;
}

static struct AABB aabb_from_transform(vec2 center, vec2 size)
{
    vec2 half_size = vec2_div(size, 2.0f);

    struct AABB aabb;
    aabb.min = vec2_sub(center, half_size);
    aabb.max = vec2_add(center, half_size);
    return aabb;
}

static bool aabb_intersection(struct AABB a, struct AABB b)
{
    if ((a.max.x > b.min.x) && (a.min.x < b.max.x))
    {
        if ((a.max.y > b.min.y) && (a.min.y < b.max.y))
            return true;
    }

    return false;
}

static vec2 screen_to_world_coords(vec2 screen_coords, struct Camera *camera, uint32 screen_width, uint32 screen_height)
{
    float aspect_ratio = (float)screen_width / (float)screen_height;

    vec2 world_coords;
    world_coords.x = ((screen_coords.x / (float)screen_width) * 2.0f - 1.0f) * camera->zoom/2.0f;
    world_coords.y = ((((float)screen_height - screen_coords.y) / (float)screen_height) * 2.0f - 1.0f) * camera->zoom/2.0f / aspect_ratio;
    world_coords = vec2_add(world_coords, camera->position);
    return world_coords;
}

void tick_game(struct GameMemory *memory, struct Input *input, uint32 screen_width, uint32 screen_height, float dt)
{
    struct GameState *game_state = (struct GameState *)memory->game_memory;
    struct RenderBuffer *render_buffer = (struct RenderBuffer *)memory->render_memory;

    clear_quad_buffer(&render_buffer->quads);
    clear_quad_buffer(&render_buffer->screen_quads);

    if (mouse_down(MOUSE_LEFT, input))
    {
        struct AABB box = calc_mouse_selection_box(input, MOUSE_LEFT);

        vec2 size = vec2_sub(box.max, box.min);
        vec2 bottom_left  = vec2_new(box.min.x, box.max.y);
        vec2 bottom_right = vec2_new(box.max.x, box.max.y);
        vec2 top_left     = vec2_new(box.min.x, box.min.y);
        vec2 top_right    = vec2_new(box.max.x, box.min.y);

        const uint32 outline_thickness = 1;

        draw_screen_quad_buffered(&render_buffer->screen_quads, top_left, vec2_new(size.x, outline_thickness), vec4_zero(), vec3_new(1, 1, 0));
        draw_screen_quad_buffered(&render_buffer->screen_quads, bottom_left, vec2_new(size.x, outline_thickness), vec4_zero(), vec3_new(1, 1, 0));
        draw_screen_quad_buffered(&render_buffer->screen_quads, top_left, vec2_new(outline_thickness, size.y), vec4_zero(), vec3_new(1, 1, 0));
        draw_screen_quad_buffered(&render_buffer->screen_quads, top_right, vec2_new(outline_thickness, size.y), vec4_zero(), vec3_new(1, 1, 0));
    }

    if (mouse_released(MOUSE_LEFT, input))
    {
        // Clear previous selection.
        game_state->selected_ship_count = 0;

        struct AABB screen_selection_box = calc_mouse_selection_box(input, MOUSE_LEFT);

        // Because screen space y-values are inverted, these two results have conflicting
        // y-values, i.e. screen_to_world_min.y is actually the maximum y. These values
        // are then properly min/maxed and stored correctly in world_selection_box.
        vec2 screen_to_world_min = screen_to_world_coords(screen_selection_box.min, &game_state->camera, screen_width, screen_height);
        vec2 screen_to_world_max = screen_to_world_coords(screen_selection_box.max, &game_state->camera, screen_width, screen_height);

        struct AABB world_selection_box;
        world_selection_box.min = min_vec2(screen_to_world_min, screen_to_world_max);
        world_selection_box.max = max_vec2(screen_to_world_min, screen_to_world_max);

        // Add colliding ships to the selection list.
        // TODO: optimize, spatial grid hash?
        for (uint32 i = 0; i < game_state->ship_count; ++i)
        {
            struct Ship *ship = &game_state->ships[i];
            struct AABB ship_aabb = aabb_from_transform(ship->position, ship->size);

            vec2 center = vec2_div(vec2_add(ship_aabb.min, ship_aabb.max), 2.0f);

            if (aabb_intersection(world_selection_box, ship_aabb))
                game_state->selected_ships[game_state->selected_ship_count++] = ship->id;
        }

        if (game_state->selected_ship_count > 0)
            fprintf(stderr, "selected %u ships\n", game_state->selected_ship_count);
    }

    if (mouse_down(MOUSE_RIGHT, input))
    {
        for (uint32 i = 0; i < game_state->selected_ship_count; ++i)
        {
            uint32 id = game_state->selected_ships[i];

            struct UIntHashPair *id_pair = find_pair(&game_state->ship_id_map, id);
            ASSERT_NOT_NULL(id_pair);

            ASSERT(id_pair->value < game_state->ship_count);
            struct Ship *ship = &game_state->ships[id_pair->value];

            ship->target_position = screen_to_world_coords(input->mouse_position, &game_state->camera, screen_width, screen_height);
            ship->move_order = true;
        }
    }

    // Outline selected ships.
    for (uint32 i = 0; i < game_state->selected_ship_count; ++i)
    {
        uint32 id = game_state->selected_ships[i];

        struct UIntHashPair *id_pair = find_pair(&game_state->ship_id_map, id);
        ASSERT_NOT_NULL(id_pair);

        ASSERT(id_pair->value < game_state->ship_count);
        struct Ship *ship = &game_state->ships[id_pair->value];

        draw_quad_buffered(&render_buffer->quads, ship->position, vec2_mul(ship->size, 1.1f), vec4_zero(), vec3_new(0, 1, 0));

        if (ship->move_order)
            draw_quad_buffered(&render_buffer->quads, ship->target_position, vec2_new(0.5f, 0.5f), vec4_zero(), vec3_new(0, 1, 0));
    }

    // Handle move orders.
    for (uint32 i = 0; i < game_state->ship_count; ++i)
    {
        struct Ship *ship = &game_state->ships[i];
        if (ship->move_order)
        {
            vec2 direction = vec2_normalize(vec2_sub(ship->target_position, ship->position));
            ship->move_velocity = vec2_mul(direction, 2.0f);

            if (vec2_distance2(ship->position, ship->target_position) < 0.1f)
            {
                ship->move_velocity = vec2_zero();
                ship->move_order = false;
            }
        }
    }

    tick_camera(input, &game_state->camera, dt);
    tick_physics(game_state, dt);
}

void render_game(struct GameMemory *memory, struct Renderer *renderer, uint32 screen_width, uint32 screen_height)
{
    struct GameState *game_state = (struct GameState *)memory->game_memory;
    struct RenderBuffer *render_buffer = (struct RenderBuffer *)memory->render_memory;

    float aspect_ratio = (float)screen_width / (float)screen_height;
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

    view_projection_matrix = mat4_orthographic(0, screen_width, screen_height, 0, 0, 1);
    update_ubo(renderer->camera_ubo, sizeof(mat4), &view_projection_matrix);

    bind_program(renderer->quad_program);
    draw_screen_quad_buffer(&renderer->sprite_batch, &render_buffer->screen_quads);
    bind_program(0);
}
