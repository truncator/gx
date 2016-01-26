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

static void remove_pair(struct UIntHashMap *map, uint32 key)
{
    struct UIntHashPair *pair = find_pair(map, key);
    ASSERT(pair != NULL);

    // Mark the pair as null.
    pair->key = NULL_UINT_HASH_KEY;
    pair->value = 0;
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

static struct Ship *get_ship_by_id(struct GameState *game_state, uint32 id)
{
    struct UIntHashPair *pair = find_pair(&game_state->ship_id_map, id);
    if (pair == NULL)
        return NULL;

    uint32 array_index = pair->value;
    ASSERT(array_index < game_state->ship_count);

    struct Ship *ship = &game_state->ships[array_index];
    return ship;
}

static uint32 generate_ship_id(struct GameState *game_state)
{
    ASSERT(game_state->ship_ids < UINT32_MAX);

    uint32 id = game_state->ship_ids;
    ++game_state->ship_ids;

    return id;
}

static struct Ship *create_ship(struct GameState *game_state)
{
    ASSERT(game_state->ship_count < ARRAY_SIZE(game_state->ships));

    uint32 array_index = game_state->ship_count;
    ++game_state->ship_count;

    struct Ship *ship = &game_state->ships[array_index];
    ship->id = generate_ship_id(game_state);

    emplace(&game_state->ship_id_map, ship->id, array_index);

    return ship;
}

static void destroy_ship(struct GameState *game_state, struct Ship *ship)
{
    struct UIntHashPair *pair = find_pair(&game_state->ship_id_map, ship->id);
    ASSERT_NOT_NULL(pair);

    uint32 array_index = pair->value;
    ASSERT(array_index < game_state->ship_count);

    remove_pair(&game_state->ship_id_map, ship->id);

    // Ship is already at the end of the array.
    if (array_index == game_state->ship_count - 1)
    {
        --game_state->ship_count;
        return;
    }

    // Swap the destroyed ship with the last active ship in the array.
    struct Ship last = game_state->ships[game_state->ship_count - 1];
    game_state->ships[array_index] = last;

    --game_state->ship_count;

    // Update the ship ID map with the swapped ship's new array index.
    struct UIntHashPair *last_pair = find_pair(&game_state->ship_id_map, last.id);
    ASSERT_NOT_NULL(last_pair);
    last_pair->value = array_index;
}

static void damage_ship(struct GameState *game_state, struct Ship *ship, int32 damage)
{
    ship->health -= damage;

    if (ship->health <= 0)
        destroy_ship(game_state, ship);
}

static struct Ship *find_nearest_enemy(struct GameState *game_state, struct Ship *ship)
{
    struct Ship *nearest = NULL;
    float min_distance = FLOAT_MAX;

    for (uint32 i = 0; i < game_state->ship_count; ++i)
    {
        struct Ship *candidate = &game_state->ships[i];
        if (candidate->team == ship->team)
            continue;

        float distance = vec2_distance2(candidate->position, ship->position);
        if (distance < min_distance)
        {
            nearest = candidate;
            min_distance = distance;
        }
    }

    return nearest;
}

static struct Projectile *create_projectile(struct GameState *game_state)
{
    ASSERT(game_state->projectile_count < ARRAY_SIZE(game_state->projectiles));
    struct Projectile *projectile = &game_state->projectiles[game_state->projectile_count++];
    return projectile;
}

static void destroy_projectile(struct GameState *game_state, struct Projectile *projectile)
{
    ASSERT(game_state->projectile_count > 0);

    // TODO: optimize if needed!
    for (uint32 i = 0; i < game_state->projectile_count; ++i)
    {
        if (&game_state->projectiles[i] == projectile)
        {
            // Swap the projectile with the last active item in the array.
            struct Projectile last = game_state->projectiles[game_state->projectile_count - 1];
            game_state->projectiles[i] = last;

            --game_state->projectile_count;

            return;
        }
    }
}

static void fire_projectile(struct GameState *game_state, struct Ship *source, struct Ship *target, int32 damage)
{
    ASSERT(source != target);

    source->fire_cooldown_timer = source->fire_cooldown;

    struct Projectile *projectile = create_projectile(game_state);

    projectile->owner = source->id;
    projectile->team = source->team;
    projectile->damage = damage;

    projectile->position = source->position;
    projectile->size = vec2_new(0.1f, 0.1f);

    vec2 direction = vec2_normalize(vec2_sub(target->position, projectile->position));
    projectile->velocity = vec2_mul(direction, 5.0f);
}

static struct Building *create_building(struct GameState *game_state)
{
    ASSERT(game_state->building_count < ARRAY_SIZE(game_state->buildings));

    uint32 array_index = game_state->building_count;
    ++game_state->building_count;

    struct Building *building = &game_state->buildings[array_index];
    return building;
}

static uint16 *get_distance_field_value(struct DistanceField *distance_field, vec2 position)
{
    float x = position.x + (distance_field->width/2.0f * distance_field->cell_size) - distance_field->cell_size/2.0f;
    float y = position.y + (distance_field->height/2.0f * distance_field->cell_size) - distance_field->cell_size/2.0f;

    struct AABB aabb = aabb_from_transform(vec2_zero(), vec2_mul(vec2_new(distance_field->width, distance_field->height), distance_field->cell_size));

    uint32 xi = x / distance_field->cell_size;
    uint32 yi = y / distance_field->cell_size;

#if 0
    fprintf(stderr, "%f %f -> %f %f\n", aabb.min.x, aabb.min.y, aabb.max.x, aabb.max.y);
    fprintf(stderr, "%f %f -> %f %f -> %u %u\n", position.x, position.y, x, y, xi, yi);
#endif

#if 0
    if ((x < aabb.min.x) || (y < aabb.min.y) || (x > aabb.max.x) || (y > aabb.max.y))
    {
        fprintf(stderr, "out of bounds: %f %f -> %u %u\n", x, y, xi, yi);
        return NULL;
    }
#endif

    if ((xi >= distance_field->width) || (yi >= distance_field->height))
        return NULL;

    uint16 *value = &distance_field->values[yi * distance_field->width + xi];
    return value;
}

static void set_distance_field_value(struct DistanceField *distance_field, vec2 position, uint16 value)
{
    uint16 *original = get_distance_field_value(distance_field, position);
    if (original == NULL)
        return;

    *original = value;
}

static vec2 calc_distance_field_gradient(struct DistanceField *distance_field, vec2 position)
{
    // NOTE: THIS IS COMPLETELY BROKEN AT THE MOMENT!
    // TODO: FIX THIS MONSTROSITY

    uint16 current_value = *get_distance_field_value(distance_field, position);

    vec2 direction = vec2_zero();
    uint16 min_value = current_value + 1;

    for (int32 y = -1; y < 2; ++y)
    {
        for (int32 x = -1; x < 2; ++x)
        {
            if ((x == 0) && (y == 0))
                continue;

            vec2 neighbor_position = vec2_new(position.x + x, position.y + y);

            uint16 *value = get_distance_field_value(distance_field, neighbor_position);
            if (value == NULL)
                continue;

            if ((*value == distance_field->occupied_value) || (*value == distance_field->uninitialized_value))
                continue;

            if (*value < min_value)
            {
                min_value = *value;
                direction = vec2_normalize(vec2_sub(neighbor_position, position));
            }
        }
    }

    return direction;
}

static void calc_distance_field(struct GameState *game_state, struct DistanceField *distance_field, vec2 origin)
{
    for (uint32 i = 0; i < ARRAY_SIZE(distance_field->values); ++i)
        distance_field->values[i] = distance_field->uninitialized_value;

    for (uint32 i = 0; i < game_state->building_count; ++i)
    {
        struct Building *building = &game_state->buildings[i];
        struct AABB aabb = aabb_from_transform(building->position, building->size);

        set_distance_field_value(distance_field, vec2_new(aabb.min.x, aabb.min.y), distance_field->occupied_value);
        set_distance_field_value(distance_field, vec2_new(aabb.max.x, aabb.min.y), distance_field->occupied_value);
        set_distance_field_value(distance_field, vec2_new(aabb.max.x, aabb.max.y), distance_field->occupied_value);
        set_distance_field_value(distance_field, vec2_new(aabb.min.x, aabb.max.y), distance_field->occupied_value);
    }

    //
    // Dijkstra's algorithm.
    //

    // TODO: heap alloc?
    vec2 to_visit[64*64] = {0};
    uint32 to_visit_count = 0;

    // Set origin (target).
    to_visit[to_visit_count++] = origin;
    set_distance_field_value(distance_field, to_visit[0], 0);

    for (uint32 i = 0; i < to_visit_count; ++i)
    {
        vec2 parent = to_visit[i];
        uint16 *parent_value = get_distance_field_value(distance_field, parent);
        ASSERT_NOT_NULL(parent_value);

        vec2 neighbors[4];
        neighbors[0] = vec2_new(parent.x,                             parent.y + distance_field->cell_size);
        neighbors[1] = vec2_new(parent.x,                             parent.y - distance_field->cell_size);
        neighbors[2] = vec2_new(parent.x - distance_field->cell_size, parent.y                            );
        neighbors[3] = vec2_new(parent.x + distance_field->cell_size, parent.y                            );

        for (uint32 j = 0; j < 4; ++j)
        {
            vec2 neighbor = neighbors[j];

            uint16 *value = get_distance_field_value(distance_field, neighbor);
            if (value == NULL)
                continue;

            if (*value == distance_field->uninitialized_value)
            {
                *value = *parent_value + 1;

                ASSERT(to_visit_count < ARRAY_SIZE(to_visit));
                to_visit[to_visit_count++] = neighbor;
            }
        }
    }
}

void init_game(struct GameMemory *memory)
{
    ASSERT(memory->game_memory_size >= sizeof(struct GameState));
    struct GameState *game_state = (struct GameState *)memory->game_memory;

    struct Camera *camera = &game_state->camera;
    camera->zoom = 20.0f;

    game_state->ship_id_map = create_uint_hash_map();

    uint32 unit_count = 5;
    for (uint32 i = 0; i < unit_count; ++i)
    {
        struct Ship *ship = create_ship(game_state);

        ship->size = vec2_new(1, 1);
        ship->move_velocity = vec2_new(0, 0);

        float xp = 2.0f * ((float)i - unit_count/2.0f);
        float yp = -camera->zoom/4.0f;
        ship->position = vec2_new(xp, yp);

        ship->health = 5;
        ship->fire_cooldown = 2.0f;

        ship->team = TEAM_ALLY;
    }

#if 0
    for (uint32 i = 0; i < unit_count; ++i)
    {
        struct Ship *ship = create_ship(game_state);
        ship->size = vec2_new(1, 1);
        ship->move_velocity = vec2_new(0, 0);

        float xp = 2.0f * ((float)i - unit_count/2.0f);
        float yp = camera->zoom/4.0f;
        ship->position = vec2_new(xp, yp);

        ship->health = 5;
        ship->fire_cooldown = 2.0f;

        ship->team = TEAM_ENEMY;
    }
#endif

    for (uint32 i = 0; i < 32; ++i)
    {
        struct Building *building = create_building(game_state);
        building->position = vec2_new(random_int(-16, 16), random_int(-16, 16));
        building->size = vec2_new(2, 2);
    }

    uint32 distance_field_length = (uint32)sqrtf(ARRAY_SIZE(game_state->distance_field.values));
    game_state->distance_field.width = distance_field_length;
    game_state->distance_field.height = distance_field_length;
    game_state->distance_field.cell_size = 2.0f;

    game_state->distance_field.uninitialized_value = UINT16_MAX;
    game_state->distance_field.occupied_value      = UINT16_MAX - 1;

    calc_distance_field(game_state, &game_state->distance_field, vec2_zero());
}

static void tick_combat(struct GameState *game_state, float dt)
{
    for (uint32 i = 0; i < game_state->ship_count; ++i)
    {
        struct Ship *ship = &game_state->ships[i];

        if (ship->fire_cooldown_timer <= 0.0f)
        {
            struct Ship *target = find_nearest_enemy(game_state, ship);
            if (target == NULL)
                continue;

            fire_projectile(game_state, ship, target, 1);
        }
        else
        {
            ship->fire_cooldown_timer -= dt;
        }
    }
}

static void tick_physics(struct GameState *game_state, float dt)
{
    // Projectile kinematics.
    for (uint32 i = 0; i < game_state->projectile_count; ++i)
    {
        struct Projectile *projectile = &game_state->projectiles[i];

        // r = r0 + (v*t) + (a*t^2)/2
        projectile->position = vec2_add(projectile->position, vec2_mul(projectile->velocity, dt));
    }

    // Ship kinematics.
    for (uint32 i = 0; i < game_state->ship_count; ++i)
    {
        struct Ship *ship = &game_state->ships[i];

        vec2 move_acceleration = vec2_zero();

        // v = v0 + (a*t)
        ship->move_velocity = vec2_add(ship->move_velocity, vec2_mul(move_acceleration, dt));

        // r = r0 + (v*t) + (a*t^2)/2
        ship->position = vec2_add(vec2_add(ship->position, vec2_mul(ship->move_velocity, dt)), vec2_div(vec2_mul(move_acceleration, dt * dt), 2.0f));
    }

    //
    // TODO: spatial hashing
    //

    // Projectile collision.
    for (uint32 i = 0; i < game_state->projectile_count; ++i)
    {
        struct Projectile *projectile = &game_state->projectiles[i];
        struct AABB projectile_aabb = aabb_from_transform(projectile->position, projectile->size);

        // Projectile-building collision.
        for (uint32 j = 0; j < game_state->building_count; ++j)
        {
            struct Building *building = &game_state->buildings[j];
            struct AABB building_aabb = aabb_from_transform(building->position, building->size);

            if (aabb_intersection(projectile_aabb, building_aabb))
            {
                // TODO: damage building if not friendly
                destroy_projectile(game_state, projectile);
                break;
            }
        }

        // NOTE: owner may be dead and destroyed at this point, so checking for NULL might be required.
        struct Ship *owner = get_ship_by_id(game_state, projectile->owner);

        // Projectile-ship collision.
        for (uint32 j = 0; j < game_state->ship_count; ++j)
        {
            struct Ship *ship = &game_state->ships[j];
            if (ship->id == projectile->owner)
                continue;

#if 0
            // Allow projectiles to pass through teammates.
            if (ship->team == projectile->team)
                continue;
#endif

            struct AABB ship_aabb = aabb_from_transform(ship->position, ship->size);

            if (aabb_intersection(projectile_aabb, ship_aabb))
            {
                // Disable friendly fire.
                if (ship->team != projectile->team)
                    damage_ship(game_state, ship, projectile->damage);

                destroy_projectile(game_state, projectile);
                break;
            }
        }
    }

    // TODO: optimize
    // Ship collision.
    if (game_state->ship_count >= 2)
    {
        for (uint32 i = 0; i < game_state->ship_count - 1; ++i)
        {
            struct Ship *a = &game_state->ships[i];
            struct AABB a_aabb = aabb_from_transform(a->position, a->size);
            vec2 a_center = vec2_div(vec2_add(a_aabb.min, a_aabb.max), 2.0f);
            vec2 a_half_extents = vec2_div(vec2_sub(a_aabb.max, a_aabb.min), 2.0f);

            // Ship-building collision.
            for (uint32 j = 0; j < game_state->building_count; ++j)
            {
                struct Building *building = &game_state->buildings[j];
                struct AABB b_aabb = aabb_from_transform(building->position, building->size);

                if (aabb_intersection(a_aabb, b_aabb))
                {
                    vec2 b_center = vec2_div(vec2_add(b_aabb.min, b_aabb.max), 2.0f);
                    vec2 b_half_extents = vec2_div(vec2_sub(b_aabb.max, b_aabb.min), 2.0f);

                    vec2 intersection = vec2_sub(vec2_abs(vec2_sub(b_center, a_center)), vec2_add(a_half_extents, b_half_extents));
                    if (intersection.x > intersection.y)
                    {
                        a->move_velocity.x = 0.0f;

                        if (a->position.x < building->position.x)
                            a->position.x += intersection.x/2.0f;
                        else
                            a->position.x -= intersection.x/2.0f;
                    }
                    else
                    {
                        a->move_velocity.y = 0.0f;

                        if (a->position.y < building->position.y)
                            a->position.y += intersection.y/2.0f;
                        else
                            a->position.y -= intersection.y/2.0f;
                    }
                }
            }

            // Ship-ship collision.
            for (uint32 j = i + 1; j < game_state->ship_count; ++j)
            {
                struct Ship *b = &game_state->ships[j];
                struct AABB b_aabb = aabb_from_transform(b->position, b->size);

                if (aabb_intersection(a_aabb, b_aabb))
                {
                    vec2 b_center = vec2_div(vec2_add(b_aabb.min, b_aabb.max), 2.0f);
                    vec2 b_half_extents = vec2_div(vec2_sub(b_aabb.max, b_aabb.min), 2.0f);

                    vec2 intersection = vec2_sub(vec2_abs(vec2_sub(b_center, a_center)), vec2_add(a_half_extents, b_half_extents));
                    if (intersection.x > intersection.y)
                    {
                        if (abs_float(a->move_velocity.x) > abs_float(b->move_velocity.x))
                            a->move_velocity.x = 0.0f;
                        else
                            b->move_velocity.x = 0.0f;

                        if (a->position.x < b->position.x)
                        {
                            a->position.x += intersection.x/2.0f;
                            b->position.x -= intersection.x/2.0f;
                        }
                        else
                        {
                            a->position.x -= intersection.x/2.0f;
                            b->position.x += intersection.x/2.0f;
                        }
                    }
                    else
                    {
                        if (abs_float(a->move_velocity.y) > abs_float(b->move_velocity.y))
                            a->move_velocity.y = 0.0f;
                        else
                            b->move_velocity.y = 0.0f;

                        if (a->position.y < b->position.y)
                        {
                            a->position.y += intersection.y/2.0f;
                            b->position.y -= intersection.y/2.0f;
                        }
                        else
                        {
                            a->position.y -= intersection.y/2.0f;
                            b->position.y += intersection.y/2.0f;
                        }
                    }
                }
            }
        }
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

void tick_game(struct GameMemory *memory, struct Input *input, uint32 screen_width, uint32 screen_height, float dt)
{
    struct GameState *game_state = (struct GameState *)memory->game_memory;
    struct RenderBuffer *render_buffer = (struct RenderBuffer *)memory->render_memory;

    clear_render_buffer(render_buffer);

    char buffer[16];
    float max_distance = sqrtf(game_state->distance_field.width * game_state->distance_field.width + game_state->distance_field.height * game_state->distance_field.height);
    for (uint32 y = 0; y < game_state->distance_field.height; ++y)
    {
        for (uint32 x = 0; x < game_state->distance_field.width; ++x)
        {
            if (render_buffer->text.current_size >= ARRAY_SIZE(render_buffer->text.texts))
                continue;

            uint16 value = game_state->distance_field.values[y * game_state->distance_field.width + x];
            snprintf(buffer, sizeof(buffer), "%u", value);

            //vec2 position = vec2_mul(vec2_sub(vec2_new(x, y), vec2_div(vec2_new(game_state->distance_field.width, game_state->distance_field.height), 2.0f)), game_state->distance_field.cell_size);
            vec2 position = vec2_mul(vec2_new(x, y), game_state->distance_field.cell_size);
            position = vec2_sub(position, vec2_mul(vec2_new(game_state->distance_field.width, game_state->distance_field.height), game_state->distance_field.cell_size/2.0f));
            position = vec2_add(position, vec2_scalar(game_state->distance_field.cell_size));

            draw_world_text(buffer, position, vec3_new(1, 1, 1), &render_buffer->text, &game_state->camera, screen_width, screen_height);

            vec3 color = vec3_new(value / max_distance, 0.0f, 0.0f);
            draw_world_quad_buffered(render_buffer, position, vec2_scalar(game_state->distance_field.cell_size), vec4_zero(), color);
        }
    }

    draw_world_quad_buffered(render_buffer, vec2_zero(), vec2_scalar(0.2f), vec4_zero(), vec3_new(1, 1, 0));

    if (mouse_down(MOUSE_LEFT, input))
    {
        struct AABB box = calc_mouse_selection_box(input, MOUSE_LEFT);

        vec2 size = vec2_sub(box.max, box.min);
        vec2 bottom_left  = vec2_new(box.min.x, box.max.y);
        vec2 bottom_right = vec2_new(box.max.x, box.max.y);
        vec2 top_left     = vec2_new(box.min.x, box.min.y);
        vec2 top_right    = vec2_new(box.max.x, box.min.y);

        const uint32 outline_thickness = 1;

        draw_screen_quad_buffered(render_buffer, top_left, vec2_new(size.x, outline_thickness), vec4_zero(), vec3_new(1, 1, 0));
        draw_screen_quad_buffered(render_buffer, bottom_left, vec2_new(size.x, outline_thickness), vec4_zero(), vec3_new(1, 1, 0));
        draw_screen_quad_buffered(render_buffer, top_left, vec2_new(outline_thickness, size.y), vec4_zero(), vec3_new(1, 1, 0));
        draw_screen_quad_buffered(render_buffer, top_right, vec2_new(outline_thickness, size.y), vec4_zero(), vec3_new(1, 1, 0));
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
        vec2 target_position = screen_to_world_coords(input->mouse_position, &game_state->camera, screen_width, screen_height);
        // TODO: optimize this. should there be multiple distance fields,
        // one per move order?
        calc_distance_field(game_state, &game_state->distance_field, target_position);

        for (uint32 i = 0; i < game_state->selected_ship_count; ++i)
        {
            uint32 id = game_state->selected_ships[i];
            struct Ship *ship = get_ship_by_id(game_state, id);

            ship->target_position = target_position;
            ship->move_order = true;
        }
    }

    // Outline selected ships.
    for (uint32 i = 0; i < game_state->selected_ship_count; ++i)
    {
        uint32 id = game_state->selected_ships[i];
        struct Ship *ship = get_ship_by_id(game_state, id);

        draw_world_quad_buffered(render_buffer, ship->position, vec2_mul(ship->size, 1.1f), vec4_zero(), vec3_new(0, 1, 0));

        if (ship->move_order)
            draw_world_quad_buffered(render_buffer, ship->target_position, vec2_new(0.5f, 0.5f), vec4_zero(), vec3_new(0, 1, 0));
    }

    // Handle move orders.
    for (uint32 i = 0; i < game_state->ship_count; ++i)
    {
        struct Ship *ship = &game_state->ships[i];
        if (ship->move_order)
        {
            vec2 direction = calc_distance_field_gradient(&game_state->distance_field, ship->position);
            /*
            if (vec2_length2(direction) < FLOAT_EPSILON)
                direction = vec2_normalize(vec2_sub(ship->target_position, ship->position));
                */
            ship->move_velocity = vec2_mul(direction, 2.0f);

            /*
            vec2 direction = vec2_normalize(vec2_sub(ship->target_position, ship->position));
            ship->move_velocity = vec2_mul(direction, 2.0f);
            */

            if (vec2_distance2(ship->position, ship->target_position) < game_state->distance_field.cell_size/2.0f)
            {
                ship->move_velocity = vec2_zero();
                ship->move_order = false;
            }
        }
    }

    tick_camera(input, &game_state->camera, dt);

    tick_combat(game_state, dt);
    tick_physics(game_state, dt);
}

static void draw_buildings(struct GameState *game_state, struct Renderer *renderer)
{
    bind_program(renderer->quad_program);
    begin_sprite_batch(&renderer->sprite_batch);

    for (uint32 i = 0; i < game_state->building_count; ++i)
    {
        struct Building *building = &game_state->buildings[i];
        draw_quad(&renderer->sprite_batch, building->position, building->size, vec3_new(0.3f, 0.3f, 0.3f));
    }

    end_sprite_batch(&renderer->sprite_batch);
    bind_program(0);
}

static void draw_ships(struct GameState *game_state, struct Renderer *renderer)
{
    bind_program(renderer->quad_program);
    begin_sprite_batch(&renderer->sprite_batch);

    for (uint32 i = 0; i < game_state->ship_count; ++i)
    {
        struct Ship *ship = &game_state->ships[i];
        draw_quad(&renderer->sprite_batch, ship->position, ship->size, vec3_new(0.5f, 0.5f, 0.5f));
    }

    end_sprite_batch(&renderer->sprite_batch);
    bind_program(0);
}

static void draw_projectiles(struct GameState *game_state, struct Renderer *renderer)
{
    bind_program(renderer->quad_program);
    begin_sprite_batch(&renderer->sprite_batch);

    for (uint32 i = 0; i < game_state->projectile_count; ++i)
    {
        struct Projectile *projectile = &game_state->projectiles[i];
        draw_quad(&renderer->sprite_batch, projectile->position, projectile->size, vec3_new(0.0f, 1.0f, 0.0f));
    }

    end_sprite_batch(&renderer->sprite_batch);
    bind_program(0);
}

static void draw_frame_text_buffer(struct Renderer *renderer, struct RenderBuffer *render_buffer)
{
    bind_program(renderer->text_program);

    bind_texture_unit(0);
    set_uniform_int32("u_Texture", renderer->text_program, 0);
    bind_texture(renderer->debug_font.texture);

    draw_text_buffer(&renderer->sprite_batch, &renderer->debug_font, &render_buffer->text);

    bind_texture(0);
    bind_program(0);
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
    draw_world_quad_buffer(&renderer->sprite_batch, render_buffer);
    bind_program(0);

    draw_buildings(game_state, renderer);
    draw_ships(game_state, renderer);
    draw_projectiles(game_state, renderer);

    view_projection_matrix = mat4_orthographic(0, screen_width, screen_height, 0, 0, 1);
    update_ubo(renderer->camera_ubo, sizeof(mat4), &view_projection_matrix);

    bind_program(renderer->quad_program);
    draw_screen_quad_buffer(&renderer->sprite_batch, render_buffer);
    bind_program(0);

    draw_frame_text_buffer(renderer, render_buffer);
}
