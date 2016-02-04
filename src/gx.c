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

static bool aabb_aabb_intersection(struct AABB a, struct AABB b)
{
    if ((a.max.x > b.min.x) && (a.min.x < b.max.x))
    {
        if ((a.max.y > b.min.y) && (a.min.y < b.max.y))
            return true;
    }

    return false;
}

static bool line_line_intersection(vec2 a0, vec2 a1, vec2 b0, vec2 b1)
{
#if 1
    // Implemented based on http://stackoverflow.com/a/17198094/4354008
    vec2 v = vec2_sub(a0, a1);
    vec2 normal = vec2_new(v.y, -v.x);

    vec2 v0 = vec2_sub(b0, a0);
    vec2 v1 = vec2_sub(b1, a0);

    float proj0 = vec2_dot(v0, normal);
    float proj1 = vec2_dot(v1, normal);

    if ((proj0 == 0) || (proj1 == 0))
        return true;

    // TODO: float_sign()
    if ((proj0 > 0) && (proj1 < 0))
        return true;
    if ((proj0 < 0) && (proj1 > 0))
        return true;

    return false;
#else
    vec2 intersection = vec2_zero();

    vec2 b = vec2_sub(a1, a0);
    vec2 d = vec2_sub(b1, b0);
    float b_dot_p_perp = (b.x * d.y) - (b.y * d.x);

    if (b_dot_p_perp == 0)
        return false;

    vec2 c = vec2_sub(b0, a0);
    float t = ((c.x * d.y) - (c.y * d.x)) / b_dot_p_perp;
    if ((t < 0) || (t > 1))
        return false;

    float u = ((c.x * b.y) - (c.y * b.x)) / b_dot_p_perp;
    if ((u < 0) || (u > 1))
        return false;

    // TODO: make this an output parameter
    intersection = vec2_add(a0, vec2_mul(b, t));

    return true;
#endif
}

static bool line_aabb_intersection(struct AABB aabb, vec2 line_start, vec2 line_end)
{
    // TODO: optimize?
    return (line_line_intersection(line_start, line_end, vec2_new(aabb.min.x, aabb.min.y), vec2_new(aabb.max.x, aabb.min.y)) ||
            line_line_intersection(line_start, line_end, vec2_new(aabb.max.x, aabb.min.y), vec2_new(aabb.max.x, aabb.max.y)) ||
            line_line_intersection(line_start, line_end, vec2_new(aabb.max.x, aabb.max.y), vec2_new(aabb.min.x, aabb.max.y)) ||
            line_line_intersection(line_start, line_end, vec2_new(aabb.min.x, aabb.max.y), vec2_new(aabb.min.x, aabb.min.y)));
}

static void tick_camera(struct Input *input, struct Camera *camera, float dt)
{
    //
    // move
    //

    vec2 camera_up = vec2_direction(camera->rotation);
    vec2 camera_right = vec2_direction(camera->rotation - PI_OVER_TWO);

    vec2 move_acceleration = vec2_zero();
    if (key_down(KEY_W, input))
        move_acceleration = vec2_add(move_acceleration, camera_up);
    if (key_down(KEY_A, input))
        move_acceleration = vec2_add(move_acceleration, vec2_negate(camera_right));
    if (key_down(KEY_S, input))
        move_acceleration = vec2_add(move_acceleration, vec2_negate(camera_up));
    if (key_down(KEY_D, input))
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
    if (key_down(KEY_SPACE, input))
        zoom_acceleration += 1.0f;
    if (key_down(KEY_LEFT_CONTROL, input))
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

static bool is_visible(struct GameState *game_state, vec2 start, vec2 end, float edge_padding)
{
    for (uint32 i = 0; i < game_state->building_count; ++i)
    {
        struct Building *building = &game_state->buildings[i];
        struct AABB aabb = aabb_from_transform(building->position, vec2_add(building->size, vec2_scalar(edge_padding)));

        if (line_aabb_intersection(aabb, start, end))
            return false;
    }

    return true;
}

static void calc_visibility_graph(struct GameState *game_state, struct VisibilityGraph *graph)
{
    graph->vertex_count = 0;
    graph->node_count = 0;

    const uint32 resolution = 4;
    const float world_size = 64.0f;

    // Add border vertices.
    for (uint32 x = 0; x < resolution; ++x)
    {
        float xp = world_size * ((float)x / (float)(resolution - 1) - 0.5f);

        ASSERT(graph->vertex_count + 1 < ARRAY_SIZE(graph->vertices));
        graph->vertices[graph->vertex_count++] = vec2_new(xp, -world_size/2.0f);
        graph->vertices[graph->vertex_count++] = vec2_new(xp,  world_size/2.0f);
    }
    for (uint32 y = 0; y < resolution; ++y)
    {
        float yp = world_size * ((float)y / (float)(resolution - 1) - 0.5f);

        ASSERT(graph->vertex_count + 1 < ARRAY_SIZE(graph->vertices));
        graph->vertices[graph->vertex_count++] = vec2_new(-world_size/2.0f, yp);
        graph->vertices[graph->vertex_count++] = vec2_new( world_size/2.0f, yp);
    }

    // Add building vertices.
    const float edge_padding = 0.5f;
    for (uint32 i = 0; i < game_state->building_count; ++i)
    {
        struct Building *building = &game_state->buildings[i];
        struct AABB aabb = aabb_from_transform(building->position, building->size);

        ASSERT(graph->vertex_count + 3 < ARRAY_SIZE(graph->vertices));
        graph->vertices[graph->vertex_count++] = vec2_new(aabb.min.x - edge_padding/2.0f, aabb.min.y - edge_padding/2.0f);
        graph->vertices[graph->vertex_count++] = vec2_new(aabb.max.x + edge_padding/2.0f, aabb.min.y - edge_padding/2.0f);
        graph->vertices[graph->vertex_count++] = vec2_new(aabb.max.x + edge_padding/2.0f, aabb.max.y + edge_padding/2.0f);
        graph->vertices[graph->vertex_count++] = vec2_new(aabb.min.x - edge_padding/2.0f, aabb.max.y + edge_padding/2.0f);
    }

    ASSERT(graph->vertex_count > 1);

    // Generate adjacency lists for each vertex.
    for (uint32 i = 0; i < graph->vertex_count; ++i)
    {
        ASSERT(graph->node_count < ARRAY_SIZE(graph->nodes));
        struct VisibilityNode *n0 = &graph->nodes[graph->node_count++];

        vec2 v0 = graph->vertices[i];
        n0->vertex_index = i;

        for (uint32 j = 0; j < graph->vertex_count; ++j)
        {
            if (i == j)
                continue;

            vec2 v1 = graph->vertices[j];
            if (is_visible(game_state, v0, v1, 0.0f))
            {
                ASSERT(n0->neighbor_index_count < ARRAY_SIZE(n0->neighbor_indices));
                n0->neighbor_indices[n0->neighbor_index_count++] = j;
            }
        }
    }

    fprintf(stderr, "Generated visibility graph containing %u verts, %u nodes.\n", graph->vertex_count, graph->node_count);
}

static bool is_node_in_list(struct WorkingPathNode *list, uint32 list_size, struct VisibilityNode *node, struct WorkingPathNode **match)
{
    for (uint32 i = 0; i < list_size; ++i)
    {
        if (list[i].node == node)
        {
            *match = &list[i];
            return true;
        }
    }

    return false;
}

static float calc_g_cost(vec2 start, vec2 end)
{
    return vec2_distance(start, end);
}

static float calc_h_cost(vec2 start, vec2 end)
{
    // Simple euclidean distance.
    // TODO: test better alternatives?
    return vec2_distance(start, end);
}

static struct Path find_path(struct VisibilityGraph *graph, vec2 start, vec2 end)
{
    float min_start_distance = FLOAT_MAX;
    float min_end_distance = FLOAT_MAX;
    struct VisibilityNode *starting_visibility_node = NULL;
    struct VisibilityNode *ending_visibility_node = NULL;

    // TODO: optimize this O(n) search if needed.
    // Find the node closest to the starting/ending positions
    // ('start', 'end' parameters).
    for (uint32 i = 0; i < graph->node_count; ++i)
    {
        struct VisibilityNode *candidate_node = &graph->nodes[i];
        vec2 candidate_position = graph->vertices[candidate_node->vertex_index];

        // Test for a closer 'start' node.
        float start_distance = vec2_distance2(start, candidate_position);
        if (start_distance < min_start_distance)
        {
            min_start_distance = start_distance;
            starting_visibility_node = candidate_node;
        }

        // Test for a closer 'end' node.
        float end_distance = vec2_distance2(end, candidate_position);
        if (end_distance < min_end_distance)
        {
            min_end_distance = end_distance;
            ending_visibility_node = candidate_node;
        }
    }

    ASSERT_NOT_NULL(starting_visibility_node);
    ASSERT_NOT_NULL(ending_visibility_node);

    if (starting_visibility_node == ending_visibility_node)
    {
        //fprintf(stderr, "empty path, exiting early\n");
        struct Path empty_path = {0};
        empty_path.start = start;
        empty_path.end = end;
        return empty_path;
    }

    //fprintf(stderr, "finding path from (%f, %f) to (%f, %f)\n", start.x, start.y, end.x, end.y);
    //fprintf(stderr, "searching %u nodes...\n", graph->node_count);
    //fprintf(stderr, "found starting node: %u neighbors, distance %f, index %u\n", starting_visibility_node->neighbor_index_count, min_start_distance, si);
    //fprintf(stderr, "found ending node: %u neighbors, distance %f, index %u\n", ending_visibility_node->neighbor_index_count, min_end_distance, ei);

    // TODO: optimize this size
    // Set up the temporary working node lists.
    struct WorkingPathNode open_nodes[64];
    struct WorkingPathNode closed_nodes[64];
    uint32 open_node_count = 0;
    uint32 closed_node_count = 0;

    // Add the starting node to the open node list.
    struct WorkingPathNode starting_node = {0};
    starting_node.node = starting_visibility_node;
    starting_node.parent = NULL;
    open_nodes[open_node_count++] = starting_node;
    
    // Variable that will eventually point to the actual ending node.
    struct WorkingPathNode *ending_node = NULL;

    // Process the list of open nodes.
    //   This continues until either the goal node is found or
    //   the entire graph has been searched without finding it.
    while (open_node_count > 0)
    {
        // TODO: optimize this list; binary heap or sorted open node list if needed
        // Search the open node list for the node with the lowest F cost.
        float min_cost = FLOAT_MAX;
        struct WorkingPathNode *cheapest_node = NULL;
        uint32 cheapest_node_index = UINT32_MAX;

        //fprintf(stderr, "searching for cheapest node from %u nodes in open list\n", open_node_count);
        for (uint32 i = 0; i < open_node_count; ++i)
        {
            struct WorkingPathNode *node = &open_nodes[i];
            if (node->f_cost < min_cost)
            {
                min_cost = node->f_cost;
                cheapest_node = node;
                cheapest_node_index = i;
            }
        }

        ASSERT_NOT_NULL(cheapest_node);
        //fprintf(stderr, "cheapest node has %u neighbors\n", cheapest_node->node->neighbor_index_count);

        // Add the resulting node to the closed list.
        struct WorkingPathNode *current_node = &closed_nodes[closed_node_count++];
        *current_node = *cheapest_node;

        // Found the ending node! End the search.
        if (current_node->node == ending_visibility_node)
        {
            ending_node = current_node;
            break;
        }

        // Remove the resulting node from the open list.
        if (cheapest_node_index == open_node_count - 1)
        {
            // Node is already the last element in the array.
            --open_node_count;
        }
        else
        {
            // Swap the nearest node with the last node in the list.
            struct WorkingPathNode *last_node = &open_nodes[open_node_count - 1];
            open_nodes[cheapest_node_index] = *last_node;

            --open_node_count;
        }

        // Consider the node's neighbors.
        struct VisibilityNode *current_visibility_node = current_node->node;
        //fprintf(stderr, "considering %u neighbors: %u, %u\n", current_visibility_node->neighbor_index_count, open_node_count, closed_node_count);
        for (uint32 i = 0; i < current_visibility_node->neighbor_index_count; ++i)
        {
            uint32 neighbor_index = current_visibility_node->neighbor_indices[i];
            struct VisibilityNode *neighbor = &graph->nodes[neighbor_index];
            ASSERT_NOT_NULL(neighbor);

            // Out parameter of is_node_in_list().
            struct WorkingPathNode *match = NULL;

            // Ignore the node if it's already in the closed list.
            if (is_node_in_list(closed_nodes, closed_node_count, neighbor, &match))
            {
                //fprintf(stderr, "in closed list, ignoring: %u, %u\n", open_node_count, closed_node_count);
                continue;
            }

            // Check if the node is already in the open list.
            if (!is_node_in_list(open_nodes, open_node_count, neighbor, &match))
            {
                //fprintf(stderr, "not in open list, adding: %u, %u\n", open_node_count, closed_node_count);

                // Not in the open list, so add it and calculate its F=G+H cost.
                struct WorkingPathNode *new_node = &open_nodes[open_node_count++];
                new_node->node = neighbor;
                new_node->parent = current_node;

                vec2 new_node_position = graph->vertices[new_node->node->vertex_index];

                // F = G + H
                new_node->g_cost = calc_g_cost(start, new_node_position);
                new_node->h_cost = calc_h_cost(start, new_node_position);
                new_node->f_cost = new_node->g_cost + new_node->h_cost;
            }
            else
            {
                // Already in the open list, so check if this path's G cost is
                // better than the one calculated when it was originally added.
                ASSERT_NOT_NULL(match);

                //fprintf(stderr, "already in open list, reconsidering: %u, %u\n", open_node_count, closed_node_count);

                vec2 node_position = graph->vertices[match->node->vertex_index];
                float candidate_g_cost = calc_g_cost(start, node_position);

                if (candidate_g_cost < match->g_cost)
                {
                    // Better path found, so adjust the parent and F/G costs.
                    match->parent = current_node;
                    match->g_cost = candidate_g_cost;
                    match->f_cost = match->g_cost + match->h_cost;
                }
            }
        }
    }

    ASSERT_NOT_NULL(ending_node);

    // Construct the final path.
    struct Path path = {0};
    path.start = start;
    path.end = end;

    // Move backward through the path nodes, adding each one to the final path list.
    while (ending_node->parent != NULL)
    {
        path.nodes[path.node_count++] = ending_node->node;
        ending_node = ending_node->parent;
    }

    // Reverse the path list.
    if (path.node_count > 1)
    {
        uint32 i = 0;
        uint32 j = path.node_count - 1;
        while (i < j)
        {
            struct VisibilityNode *temp = path.nodes[i];
            path.nodes[i] = path.nodes[j];
            path.nodes[j] = temp;

            ++i;
            --j;
        }
    }

    return path;
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

    for (uint32 i = 0; i < 4; ++i)
    {
        struct Building *building = create_building(game_state);
        building->position = vec2_new(random_int(-32, 32), random_int(-32, 32));
        building->size = vec2_new(2, 2);
    }

    calc_visibility_graph(game_state, &game_state->visibility_graph);
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

            if (aabb_aabb_intersection(projectile_aabb, building_aabb))
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

            if (aabb_aabb_intersection(projectile_aabb, ship_aabb))
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

                if (aabb_aabb_intersection(a_aabb, b_aabb))
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

                if (aabb_aabb_intersection(a_aabb, b_aabb))
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

    // Draw mouse selection box.
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

    // Select entities.
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

            if (aabb_aabb_intersection(world_selection_box, ship_aabb))
                game_state->selected_ships[game_state->selected_ship_count++] = ship->id;
        }

        if (game_state->selected_ship_count > 0)
            fprintf(stderr, "selected %u ships\n", game_state->selected_ship_count);
    }

    // Issue move orders.
    if (mouse_down(MOUSE_RIGHT, input))
    {
        vec2 end = screen_to_world_coords(input->mouse_position, &game_state->camera, screen_width, screen_height);

        for (uint32 i = 0; i < game_state->selected_ship_count; ++i)
        {
            uint32 id = game_state->selected_ships[i];
            struct Ship *ship = get_ship_by_id(game_state, id);

            vec2 start = ship->position;

            ship->path = find_path(&game_state->visibility_graph, start, end);
            ship->flags |= UNIT_MOVE_ORDER;
        }
    }

    // Handle move orders.
    for (uint32 i = 0; i < game_state->ship_count; ++i)
    {
        struct Ship *ship = &game_state->ships[i];
        if (ship->flags & UNIT_MOVE_ORDER)
        {
            // Ship has reached the final node and is pathing to the exact target coordinates.
            if ((ship->path.node_count == 0) || (ship->path.current_node_index == ship->path.node_count - 1))
            {
                if (vec2_distance2(ship->position, ship->path.end) < 0.1f)
                {
                    // Final destination reached.
                    ship->flags &= ~UNIT_MOVE_ORDER;
                    ship->move_velocity = vec2_zero();
                }
                else
                {
                    vec2 direction = vec2_normalize(vec2_sub(ship->path.end, ship->position));
                    ship->move_velocity = vec2_mul(direction, 2.0f);
                }

                continue;
            }

            // Move toward the current node in the stored path.
            struct VisibilityNode *target_node = ship->path.nodes[ship->path.current_node_index];
            ASSERT_NOT_NULL(target_node);
            vec2 target_node_position = game_state->visibility_graph.vertices[target_node->vertex_index];

            // Current node has been reached, so move to the next node.
            if (vec2_distance2(ship->position, target_node_position) < 0.1f)
            {
                ++ship->path.current_node_index;

                // Ship has reached the final node.
                if (ship->path.current_node_index == ship->path.node_count - 1)
                    continue;

                // Update the target.
                target_node = ship->path.nodes[ship->path.current_node_index];
                target_node_position = game_state->visibility_graph.vertices[target_node->vertex_index];
            }

            vec2 direction = vec2_normalize(vec2_sub(target_node_position, ship->position));
            ship->move_velocity = vec2_mul(direction, 2.0f);
        }
    }

    tick_camera(input, &game_state->camera, dt);

    tick_combat(game_state, dt);
    tick_physics(game_state, dt);

#if 0
    // Draw visibility graph vertices.
    for (uint32 i = 0; i < game_state->visibility_graph.vertex_count; ++i)
    {
        vec2 vertex = game_state->visibility_graph.vertices[i];
        draw_world_quad_buffered(render_buffer, vertex, vec2_scalar(0.1f), vec4_zero(), vec3_new(0, 1, 1));
    }
#endif

#if 0
    static uint32 index = 0;
    if (key_down_new(KEY_F, input))
        ++index;
#endif
#if 0
    // Draw visibility graph edges.
    for (uint32 i = 0; i < game_state->visibility_graph.node_count; ++i)
    {
        struct VisibilityNode *node = &game_state->visibility_graph.nodes[i];
        for (uint32 j = 0; j < node->neighbor_index_count; ++j)
        {
            vec2 p0 = game_state->visibility_graph.vertices[node->vertex_index];
            vec2 p1 = game_state->visibility_graph.vertices[node->neighbor_indices[j]];

            draw_world_line_buffered(render_buffer, p0, p1, vec3_new(1, 1, 0));
        }
    }
#endif

    // Outline selected ships.
    for (uint32 i = 0; i < game_state->selected_ship_count; ++i)
    {
        uint32 id = game_state->selected_ships[i];
        struct Ship *ship = get_ship_by_id(game_state, id);

        draw_world_quad_buffered(render_buffer, ship->position, vec2_mul(ship->size, 1.1f), vec4_zero(), vec3_new(0, 1, 0));

        // Draw path.
        if (ship->flags & UNIT_MOVE_ORDER)
        {
            draw_world_quad_buffered(render_buffer, ship->path.start, vec2_scalar(0.5f), vec4_zero(), vec3_new(0, 0, 1));
            draw_world_quad_buffered(render_buffer, ship->path.end, vec2_scalar(0.5f), vec4_zero(), vec3_new(0, 1, 1));

            for (uint32 j = 0; j < ship->path.node_count; ++j)
            {
                vec2 node_position = game_state->visibility_graph.vertices[ship->path.nodes[j]->vertex_index];
                vec3 color = (j == ship->path.current_node_index) ? vec3_new(0, 1, 0) : vec3_new(1, 0, 0);
                draw_world_quad_buffered(render_buffer, node_position, vec2_scalar(0.5f), vec4_zero(), color);
            }

            if (ship->path.node_count == 0)
            {
                draw_world_line_buffered(render_buffer, ship->path.start, ship->path.end, vec3_new(1, 1, 0));
            }
            else
            {
                vec2 first_node_position = game_state->visibility_graph.vertices[ship->path.nodes[0]->vertex_index];
                draw_world_line_buffered(render_buffer, ship->path.start, first_node_position, vec3_new(1, 1, 0));

                if (ship->path.node_count == 1)
                {
                    vec2 last_node_position = game_state->visibility_graph.vertices[ship->path.nodes[0]->vertex_index];
                    draw_world_line_buffered(render_buffer, last_node_position, ship->path.end, vec3_new(1, 1, 0));
                }
                else
                {
                    for (uint32 j = 0; j < ship->path.node_count - 1; ++j)
                    {
                        vec2 p0 = game_state->visibility_graph.vertices[ship->path.nodes[j]->vertex_index];
                        vec2 p1 = game_state->visibility_graph.vertices[ship->path.nodes[j + 1]->vertex_index];
                        draw_world_line_buffered(render_buffer, p0, p1, vec3_new(1, 1, 0));
                    }
                }
            }
        }
    }
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

    bind_program(renderer->line_program);
    draw_world_line_buffer(renderer, render_buffer, renderer->line_program);
    bind_program(0);

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
