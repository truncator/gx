#include "gx_math.h"

#include <math.h>
#include <x86intrin.h> // rdtsc()

#define STB_PERLIN_IMPLEMENTATION
#include <stb/stb_perlin.h>

struct Random
{
    uint32 x, y, z, w;
};
static struct Random global_random;

float degrees_to_radians(float degrees)
{
    return degrees * (PI / 180.0f);
}

int32 min_int32(int32 a, int32 b)
{
    return (a < b) ? a : b;
}

int32 max_int32(int32 a, int32 b)
{
    return (a > b) ? a : b;
}

uint32 min_uint32(uint32 a, uint32 b)
{
    return (a < b) ? a : b;
}

uint32 max_uint32(uint32 a, uint32 b)
{
    return (a > b) ? a : b;
}

float min_float(float a, float b)
{
    return (a < b) ? a : b;
}

float max_float(float a, float b)
{
    return (a > b) ? a : b;
}

double min_double(double a, double b)
{
    return (a < b) ? a : b;
}

double max_double(double a, double b)
{
    return (a > b) ? a : b;
}

vec2 min_vec2(vec2 a, vec2 b)
{
    vec2 result;
    result.x = min_float(a.x, b.x);
    result.y = min_float(a.y, b.y);
    return result;
}

vec2 max_vec2(vec2 a, vec2 b)
{
    vec2 result;
    result.x = max_float(a.x, b.x);
    result.y = max_float(a.y, b.y);
    return result;
}

vec3 min_vec3(vec3 a, vec3 b)
{
    vec3 result;
    result.x = min_float(a.x, b.x);
    result.y = min_float(a.y, b.y);
    result.z = min_float(a.z, b.z);
    return result;
}

vec3 max_vec3(vec3 a, vec3 b)
{
    vec3 result;
    result.x = max_float(a.x, b.x);
    result.y = max_float(a.y, b.y);
    result.z = max_float(a.z, b.z);
    return result;
}

float clamp_float(float value, float min, float max)
{
    ASSERT(min < max);
    return min_float(max_float(value, min), max);
}

float abs_float(float value)
{
    return (value < 0) ? -value : value;
}

vec3 calc_normal(vec3 v0, vec3 v1, vec3 v2)
{
    const vec3 d0 = vec3_sub(v1, v0);
    const vec3 d1 = vec3_sub(v2, v0);
    const vec3 normal = vec3_cross(d0, d1);
    return vec3_normalize(normal);
}

static struct Random create_random(void)
{
    struct Random random = {0};

    random.x = 123456789;
    random.y = 362436069;
    random.z = 521288629;
    random.w = 88675123;

    return random;
}

static void random_seed(struct Random *random, uint32 seed)
{
    random->w = seed;
}

void init_random(uint32 seed)
{
    global_random = create_random();
    random_seed(&global_random, seed);
}

static uint32 random_xorshift(struct Random *random)
{
    uint32 t = random->x ^ (random->x << 15);
    random->x = random->y;
    random->y = random->z;
    random->z = random->w;
    random->w = random->w ^ (random->w >> 21) ^ t ^ (t >> 4);
    return random->w;
}

uint64 rdtsc(void)
{
    return __rdtsc();
}

float random(void)
{
    uint32 ri = random_xorshift(&global_random);
    return (float)ri / (float)UINT32_MAX;
}

int32 random_int(int32 min, int32 max)
{
    ASSERT(max > min);
    return min + (int32)(random() * (max - min));
}

float random_float(float min, float max)
{
    ASSERT(max > min);
    return min + (float)(random() * (max - min));
}

float noise(float x, float y, float z)
{
    return stb_perlin_noise3(x, y, z, 0, 0, 0);
}

float fractal_noise(float x, float y, float z, uint32 octaves, float frequency, float persistence)
{
    float n = 0.0f;

    float amplitude = 1.0f;
    float max_amplitude = 0.0f;

    for (uint32 i = 0; i < octaves; ++i)
    {
        n += noise(x * frequency, y * frequency, z * frequency) * amplitude;
        frequency *= 2;

        max_amplitude += amplitude;
        amplitude *= persistence;
    }

    n /= max_amplitude;

    return n;
}

#if 0
// This only applies to perspective projections. This code is correct, though!
static vec3 unproject(vec3 position, mat4 view_projection, uint32 screen_width, uint32 screen_height)
{
    mat4 inverse = mat4_inverse(view_projection);

    vec4 tmp = vec4_new(position.x, screen_height - position.y - 1.0f, position.z, 1.0f);
    tmp.x = tmp.x / screen_width;
    tmp.y = tmp.y / screen_height;

    tmp = vec4_mul(tmp, 2.0f);
    tmp.x -= 1.0f;
    tmp.y -= 1.0f;
    tmp.z -= 1.0f;
    tmp.w -= 1.0f;

    vec4 obj = mat4_mul_vec4(inverse, tmp);
    return vec4_normalize(obj);
}

vec3 calc_screen_ray(vec2 position, mat4 view_projection, uint32 screen_width, uint32 screen_height)
{
    vec3 mouse_near = unproject(vec3_new(position.x, position.y, 0.0f),
                                view_projection, screen_width, screen_height);
    vec3 mouse_far  = unproject(vec3_new(position.x, position.y, 1.0f),
                                view_projection, screen_width, screen_height);

    vec3 dv = vec3_sub(mouse_far, mouse_near);
    return vec3_normalize(dv);
}
#endif

vec2 vec2_zero(void)
{
    return vec2_new(0, 0);
}

vec2 vec2_new(float x, float y)
{
    vec2 result;
    result.x = x;
    result.y = y;
    return result;
}

vec2 vec2_scalar(float s)
{
    return vec2_new(s, s);
}

vec2 vec2_from_vec3(vec3 v)
{
    return vec2_new(v.x, v.y);
}

vec2 vec2_add(vec2 a, vec2 b)
{
    vec2 result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    return result;
}

vec2 vec2_sub(vec2 a, vec2 b)
{
    vec2 result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    return result;
}

vec2 vec2_mul(vec2 v, float s)
{
    vec2 result;
    result.x = v.x * s;
    result.y = v.y * s;
    return result;
}

vec2 vec2_div(vec2 v, float s)
{
    return vec2_mul(v, 1.0f / s);
}

vec2 vec2_negate(vec2 v)
{
    vec2 result;
    result.x = -v.x;
    result.y = -v.y;
    return result;
}

vec2 vec2_abs(vec2 v)
{
    vec2 result;
    result.x = abs_float(v.x);
    result.y = abs_float(v.y);
    return result;
}

float vec2_length(vec2 v)
{
    return sqrtf(vec2_length2(v));
}

float vec2_length2(vec2 v)
{
    return (v.x * v.x) + (v.y * v.y);
}

vec2 vec2_normalize(vec2 v)
{
    const float inv_length = 1.0f / vec2_length(v);

    vec2 result = vec2_mul(v, inv_length);
    return result;
}

float vec2_distance(vec2 a, vec2 b)
{
    return sqrtf(vec2_distance2(a, b));
}

float vec2_distance2(vec2 a, vec2 b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;

    return (dx * dx) + (dy * dy);
}

vec2 vec2_direction(float rotation)
{
    float x = cosf(rotation + PI_OVER_TWO);
    float y = sinf(rotation + PI_OVER_TWO);
    return vec2_normalize(vec2_new(x, y));
}

bool vec2_equal(vec2 a, vec2 b)
{
    return (a.x == b.x) && (a.y == b.y);
}

vec3 vec3_zero(void)
{
    return vec3_new(0, 0, 0);
}

vec3 vec3_new(float x, float y, float z)
{
    vec3 result;
    result.x = x;
    result.y = y;
    result.z = z;
    return result;
}

vec3 vec3_scalar(float s)
{
    return vec3_new(s, s, s);
}

vec3 vec3_add(vec3 a, vec3 b)
{
    vec3 result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    result.z = a.z + b.z;
    return result;
}

vec3 vec3_sub(vec3 a, vec3 b)
{
    vec3 result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    result.z = a.z - b.z;
    return result;
}

vec3 vec3_mul(vec3 v, float s)
{
    vec3 result;
    result.x = v.x * s;
    result.y = v.y * s;
    result.z = v.z * s;
    return result;
}

vec3 vec3_div(vec3 v, float s)
{
    return vec3_mul(v, 1.0f / s);
}

vec3 vec3_negate(vec3 v)
{
    vec3 result;
    result.x = -v.x;
    result.y = -v.y;
    result.z = -v.z;
    return result;
}

float vec3_dot(vec3 a, vec3 b)
{
    return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
}

vec3 vec3_cross(vec3 a, vec3 b)
{
    vec3 result;
    result.x = (a.y * b.z) - (a.z * b.y);
    result.y = (a.z * b.x) - (a.x * b.z);
    result.z = (a.x * b.y) - (a.y * b.x);
    return result;
}

float vec3_length(vec3 v)
{
    return sqrtf(vec3_length2(v));
}

float vec3_length2(vec3 v)
{
    return (v.x * v.x) + (v.y * v.y) + (v.z * v.z);
}

vec3 vec3_normalize(vec3 v)
{
    const float inv_length = 1.0f / vec3_length(v);

    vec3 result = vec3_mul(v, inv_length);
    return result;
}

float vec3_distance(vec3 a, vec3 b)
{
    return sqrtf(vec3_distance2(a, b));
}

float vec3_distance2(vec3 a, vec3 b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;

    return (dx * dx) + (dy * dy) + (dz * dz);
}

vec3 vec3_direction(vec3 euler)
{
    const float sin_x = sinf(euler.x);
    const float sin_y = sinf(euler.y);
    const float cos_x = cosf(euler.x);
    const float cos_y = cosf(euler.y);

    vec3 result;
    result.x = cos_x * sin_y;
    result.y = sin_x;
    result.z = cos_x * cos_y;
    result = vec3_normalize(result);
    return result;
}

bool vec3_equal(vec3 a, vec3 b)
{
    return (a.x == b.x) && (a.y == b.y) && (a.z == b.z);
}

vec4 vec4_zero(void)
{
    return vec4_new(0, 0, 0, 0);
}

vec4 vec4_new(float x, float y, float z, float w)
{
    vec4 result;
    result.x = x;
    result.y = y;
    result.z = z;
    result.w = w;
    return result;
}

vec4 vec4_mul(vec4 v, float s)
{
    vec4 result;
    result.x = v.x * s;
    result.y = v.y * s;
    result.z = v.z * s;
    result.w = v.w * s;
    return result;
}

vec4 vec4_div(vec4 v, float s)
{
    return vec4_mul(v, 1.0f / s);
}

vec3 vec4_normalize(vec4 v)
{
    vec3 result;
    result.x = v.x / v.w;
    result.y = v.y / v.w;
    result.z = v.z / v.w;
    return result;
}

mat4 mat4_identity(void)
{
    mat4 identity = {0};
    identity.m[ 0] = 1;
    identity.m[ 5] = 1;
    identity.m[10] = 1;
    identity.m[15] = 1;
    return identity;
}

mat4 mat4_model(vec3 position, vec3 rotation, vec3 scale)
{
    mat4 model = mat4_identity();
    model = mat4_translate(model, position);

    // TODO: check this
    model = mat4_rotate(model, rotation.z, vec3_new(0, 0, 1));
    model = mat4_rotate(model, rotation.y, vec3_new(0, 1, 0));
    model = mat4_rotate(model, rotation.x, vec3_new(1, 0, 0));

    model = mat4_scale(model, scale);
    return model;
}

mat4 mat4_translate(mat4 m, vec3 translation)
{
    mat4 matrix = mat4_identity();
    matrix.m[12] = translation.x;
    matrix.m[13] = translation.y;
    matrix.m[14] = translation.z;

    mat4 result = mat4_mul(matrix, m);
    return result;
}

mat4 mat4_rotate(mat4 m, float angle, vec3 axis)
{
    const float cos_a = cosf(angle);
    const float sin_a = sinf(angle);

    axis = vec3_normalize(axis);
    vec3 temp = vec3_mul(axis, 1.0f - cos_a);

    mat4 rotation = mat4_identity();
    rotation.m[ 0] = cos_a + (temp.x * axis.x);
    rotation.m[ 1] = 0     + (temp.x * axis.y) + (sin_a * axis.z);
    rotation.m[ 2] = 0     + (temp.x * axis.z) - (sin_a * axis.y);

    rotation.m[ 4] = 0     + (temp.y * axis.x) - (sin_a * axis.z);
    rotation.m[ 5] = cos_a + (temp.y * axis.y);
    rotation.m[ 6] = 0     + (temp.y * axis.z) + (sin_a * axis.x);

    rotation.m[ 8] = 0     + (temp.z * axis.x) + (sin_a * axis.y);
    rotation.m[ 9] = 0     + (temp.z * axis.y) - (sin_a * axis.x);
    rotation.m[10] = cos_a + (temp.z * axis.z);

    mat4 result = mat4_mul(rotation, m);
    return result;
}

mat4 mat4_scale(mat4 m, vec3 scale)
{
    mat4 r = m;
    r.m[ 0] *= scale.x;
    r.m[ 5] *= scale.y;
    r.m[10] *= scale.z;
    return r;
}

mat4 mat4_mul(mat4 a, mat4 b)
{
    mat4 r;

    r.m[ 0] = (b.m[ 0] * a.m[ 0]) + (b.m[ 1] * a.m[ 4]) + (b.m[ 2] * a.m[ 8]) + (b.m[ 3] * a.m[12]);
    r.m[ 1] = (b.m[ 0] * a.m[ 1]) + (b.m[ 1] * a.m[ 5]) + (b.m[ 2] * a.m[ 9]) + (b.m[ 3] * a.m[13]);
    r.m[ 2] = (b.m[ 0] * a.m[ 2]) + (b.m[ 1] * a.m[ 6]) + (b.m[ 2] * a.m[10]) + (b.m[ 3] * a.m[14]);
    r.m[ 3] = (b.m[ 0] * a.m[ 3]) + (b.m[ 1] * a.m[ 7]) + (b.m[ 2] * a.m[11]) + (b.m[ 3] * a.m[15]);

    r.m[ 4] = (b.m[ 4] * a.m[ 0]) + (b.m[ 5] * a.m[ 4]) + (b.m[ 6] * a.m[ 8]) + (b.m[ 7] * a.m[12]);
    r.m[ 5] = (b.m[ 4] * a.m[ 1]) + (b.m[ 5] * a.m[ 5]) + (b.m[ 6] * a.m[ 9]) + (b.m[ 7] * a.m[13]);
    r.m[ 6] = (b.m[ 4] * a.m[ 2]) + (b.m[ 5] * a.m[ 6]) + (b.m[ 6] * a.m[10]) + (b.m[ 7] * a.m[14]);
    r.m[ 7] = (b.m[ 4] * a.m[ 3]) + (b.m[ 5] * a.m[ 7]) + (b.m[ 6] * a.m[11]) + (b.m[ 7] * a.m[15]);

    r.m[ 8] = (b.m[ 8] * a.m[ 0]) + (b.m[ 9] * a.m[ 4]) + (b.m[10] * a.m[ 8]) + (b.m[11] * a.m[12]);
    r.m[ 9] = (b.m[ 8] * a.m[ 1]) + (b.m[ 9] * a.m[ 5]) + (b.m[10] * a.m[ 9]) + (b.m[11] * a.m[13]);
    r.m[10] = (b.m[ 8] * a.m[ 2]) + (b.m[ 9] * a.m[ 6]) + (b.m[10] * a.m[10]) + (b.m[11] * a.m[14]);
    r.m[11] = (b.m[ 8] * a.m[ 3]) + (b.m[ 9] * a.m[ 7]) + (b.m[10] * a.m[11]) + (b.m[11] * a.m[15]);

    r.m[12] = (b.m[12] * a.m[ 0]) + (b.m[13] * a.m[ 4]) + (b.m[14] * a.m[ 8]) + (b.m[15] * a.m[12]);
    r.m[13] = (b.m[12] * a.m[ 1]) + (b.m[13] * a.m[ 5]) + (b.m[14] * a.m[ 9]) + (b.m[15] * a.m[13]);
    r.m[14] = (b.m[12] * a.m[ 2]) + (b.m[13] * a.m[ 6]) + (b.m[14] * a.m[10]) + (b.m[15] * a.m[14]);
    r.m[15] = (b.m[12] * a.m[ 3]) + (b.m[13] * a.m[ 7]) + (b.m[14] * a.m[11]) + (b.m[15] * a.m[15]);

    return r;
}

vec3 mat4_mul_vec3(mat4 m, vec3 v)
{
    vec3 r;

    r.x = (m.m[ 0] * v.x) + (m.m[ 4] * v.y) + (m.m[ 8] * v.z) + (m.m[12] * 1.0f);
    r.y = (m.m[ 1] * v.x) + (m.m[ 5] * v.y) + (m.m[ 9] * v.z) + (m.m[13] * 1.0f);
    r.z = (m.m[ 2] * v.x) + (m.m[ 6] * v.y) + (m.m[10] * v.z) + (m.m[14] * 1.0f);

#if 0
    // TODO: separate functions . mat4_mul_vec2, mat4_mul_vec3?
    float w = (m.m[12] * v.x) + (m.m[13] * v.y) + (m.m[14] * v.z) + (m.m[15] * 1.0f);
    if (w != 1)
    {
        r.x /= w;
        r.y /= w;
        r.z /= w;
    }
#endif

    return r;
}

vec4 mat4_mul_vec4(mat4 m, vec4 v)
{
    vec4 r;

    r.x = (m.m[ 0] * v.x) + (m.m[ 4] * v.y) + (m.m[ 8] * v.z) + (m.m[12] * 1.0f);
    r.y = (m.m[ 1] * v.x) + (m.m[ 5] * v.y) + (m.m[ 9] * v.z) + (m.m[13] * 1.0f);
    r.z = (m.m[ 2] * v.x) + (m.m[ 6] * v.y) + (m.m[10] * v.z) + (m.m[14] * 1.0f);
    r.w = (m.m[ 3] * v.x) + (m.m[ 7] * v.y) + (m.m[11] * v.z) + (m.m[15] * 1.0f);

    return r;
}

mat4 mat4_transpose(mat4 m)
{
    mat4 r;

    r.m[ 0] = m.m[ 0];
    r.m[ 1] = m.m[ 4];
    r.m[ 2] = m.m[ 8];
    r.m[ 3] = m.m[12];

    r.m[ 4] = m.m[ 1];
    r.m[ 5] = m.m[ 5];
    r.m[ 6] = m.m[ 9];
    r.m[ 7] = m.m[13];

    r.m[ 8] = m.m[ 2];
    r.m[ 9] = m.m[ 6];
    r.m[10] = m.m[10];
    r.m[11] = m.m[14];

    r.m[12] = m.m[ 3];
    r.m[13] = m.m[ 7];
    r.m[14] = m.m[11];
    r.m[15] = m.m[15];

    return r;
}

mat4 mat4_inverse(mat4 m)
{
    mat4 inverse;

    inverse.m[ 0] =  m.m[ 5] * m.m[10] * m.m[15] - m.m[ 5] * m.m[11] * m.m[14] - m.m[ 9] * m.m[ 6] * m.m[15] + m.m[ 9] * m.m[ 7] * m.m[14] + m.m[13] * m.m[ 6] * m.m[11] - m.m[13] * m.m[ 7] * m.m[10];
    inverse.m[ 4] = -m.m[ 4] * m.m[10] * m.m[15] + m.m[ 4] * m.m[11] * m.m[14] + m.m[ 8] * m.m[ 6] * m.m[15] - m.m[ 8] * m.m[ 7] * m.m[14] - m.m[12] * m.m[ 6] * m.m[11] + m.m[12] * m.m[ 7] * m.m[10];
    inverse.m[ 8] =  m.m[ 4] * m.m[ 9] * m.m[15] - m.m[ 4] * m.m[11] * m.m[13] - m.m[ 8] * m.m[ 5] * m.m[15] + m.m[ 8] * m.m[ 7] * m.m[13] + m.m[12] * m.m[ 5] * m.m[11] - m.m[12] * m.m[ 7] * m.m[ 9];
    inverse.m[12] = -m.m[ 4] * m.m[ 9] * m.m[14] + m.m[ 4] * m.m[10] * m.m[13] + m.m[ 8] * m.m[ 5] * m.m[14] - m.m[ 8] * m.m[ 6] * m.m[13] - m.m[12] * m.m[ 5] * m.m[10] + m.m[12] * m.m[ 6] * m.m[ 9];

    inverse.m[ 1] = -m.m[ 1] * m.m[10] * m.m[15] + m.m[ 1] * m.m[11] * m.m[14] + m.m[ 9] * m.m[ 2] * m.m[15] - m.m[ 9] * m.m[ 3] * m.m[14] - m.m[13] * m.m[ 2] * m.m[11] + m.m[13] * m.m[ 3] * m.m[10];
    inverse.m[ 5] =  m.m[ 0] * m.m[10] * m.m[15] - m.m[ 0] * m.m[11] * m.m[14] - m.m[ 8] * m.m[ 2] * m.m[15] + m.m[ 8] * m.m[ 3] * m.m[14] + m.m[12] * m.m[ 2] * m.m[11] - m.m[12] * m.m[ 3] * m.m[10];
    inverse.m[ 9] = -m.m[ 0] * m.m[ 9] * m.m[15] + m.m[ 0] * m.m[11] * m.m[13] + m.m[ 8] * m.m[ 1] * m.m[15] - m.m[ 8] * m.m[ 3] * m.m[13] - m.m[12] * m.m[ 1] * m.m[11] + m.m[12] * m.m[ 3] * m.m[ 9];
    inverse.m[13] =  m.m[ 0] * m.m[ 9] * m.m[14] - m.m[ 0] * m.m[10] * m.m[13] - m.m[ 8] * m.m[ 1] * m.m[14] + m.m[ 8] * m.m[ 2] * m.m[13] + m.m[12] * m.m[ 1] * m.m[10] - m.m[12] * m.m[ 2] * m.m[ 9];

    inverse.m[ 2] =  m.m[ 1] * m.m[ 6] * m.m[15] - m.m[ 1] * m.m[ 7] * m.m[14] - m.m[ 5] * m.m[ 2] * m.m[15] + m.m[ 5] * m.m[ 3] * m.m[14] + m.m[13] * m.m[ 2] * m.m[ 7] - m.m[13] * m.m[ 3] * m.m[ 6];
    inverse.m[ 6] = -m.m[ 0] * m.m[ 6] * m.m[15] + m.m[ 0] * m.m[ 7] * m.m[14] + m.m[ 4] * m.m[ 2] * m.m[15] - m.m[ 4] * m.m[ 3] * m.m[14] - m.m[12] * m.m[ 2] * m.m[ 7] + m.m[12] * m.m[ 3] * m.m[ 6];
    inverse.m[10] =  m.m[ 0] * m.m[ 5] * m.m[15] - m.m[ 0] * m.m[ 7] * m.m[13] - m.m[ 4] * m.m[ 1] * m.m[15] + m.m[ 4] * m.m[ 3] * m.m[13] + m.m[12] * m.m[ 1] * m.m[ 7] - m.m[12] * m.m[ 3] * m.m[ 5];
    inverse.m[14] = -m.m[ 0] * m.m[ 5] * m.m[14] + m.m[ 0] * m.m[ 6] * m.m[13] + m.m[ 4] * m.m[ 1] * m.m[14] - m.m[ 4] * m.m[ 2] * m.m[13] - m.m[12] * m.m[ 1] * m.m[ 6] + m.m[12] * m.m[ 2] * m.m[ 5];

    inverse.m[ 3] = -m.m[ 1] * m.m[ 6] * m.m[11] + m.m[ 1] * m.m[ 7] * m.m[10] + m.m[ 5] * m.m[ 2] * m.m[11] - m.m[ 5] * m.m[ 3] * m.m[10] - m.m[ 9] * m.m[ 2] * m.m[ 7] + m.m[ 9] * m.m[ 3] * m.m[ 6];
    inverse.m[ 7] =  m.m[ 0] * m.m[ 6] * m.m[11] - m.m[ 0] * m.m[ 7] * m.m[10] - m.m[ 4] * m.m[ 2] * m.m[11] + m.m[ 4] * m.m[ 3] * m.m[10] + m.m[ 8] * m.m[ 2] * m.m[ 7] - m.m[ 8] * m.m[ 3] * m.m[ 6];
    inverse.m[11] = -m.m[ 0] * m.m[ 5] * m.m[11] + m.m[ 0] * m.m[ 7] * m.m[ 9] + m.m[ 4] * m.m[ 1] * m.m[11] - m.m[ 4] * m.m[ 3] * m.m[ 9] - m.m[ 8] * m.m[ 1] * m.m[ 7] + m.m[ 8] * m.m[ 3] * m.m[ 5];
    inverse.m[15] =  m.m[ 0] * m.m[ 5] * m.m[10] - m.m[ 0] * m.m[ 6] * m.m[ 9] - m.m[ 4] * m.m[ 1] * m.m[10] + m.m[ 4] * m.m[ 2] * m.m[ 9] + m.m[ 8] * m.m[ 1] * m.m[ 6] - m.m[ 8] * m.m[ 2] * m.m[ 5];

    float determinant = ((m.m[ 0] * inverse.m[ 0]) +
                         (m.m[ 1] * inverse.m[ 4]) +
                         (m.m[ 2] * inverse.m[ 8]) +
                         (m.m[ 3] * inverse.m[12]));

    ASSERT(determinant != 0);

    float inv_determinant = 1.0f / determinant;
    for (uint32 i = 0; i < 16; ++i)
        inverse.m[i] *= inv_determinant;

    return inverse;
}

mat4 mat4_perspective(float fov_degrees, float aspect_ratio, float near, float far)
{
    const float fov_radians = degrees_to_radians(fov_degrees);
    float scale = 1.0f / tanf(fov_radians / 2.0f);

    mat4 r = {0};

    r.m[ 0] = scale / aspect_ratio;
    r.m[ 5] = scale;
    r.m[10] = -(far + near) / (far - near);
    r.m[11] = -1;
    r.m[14] = -(2.0f * far * near) / (far - near);

    return r;
}

mat4 mat4_orthographic(float left, float right, float bottom, float top, float near, float far)
{
    const float dx = right - left;
    const float dy = top - bottom;
    const float dz = far - near;

    mat4 r = mat4_identity();

    r.m[ 0] = 2.0f / dx;
    r.m[ 5] = 2.0f / dy;
    r.m[10] = -2.0f / dz;
    r.m[12] = -(right + left) / dx;
    r.m[13] = -(top + bottom) / dy;
    r.m[14] = -(far + near) / dz;

    return r;
}

mat4 mat4_look_at(vec3 eye_position, vec3 target_position, vec3 world_up)
{
    vec3 eye_direction = vec3_normalize(vec3_sub(target_position, eye_position));
    vec3 eye_right = vec3_normalize(vec3_cross(eye_direction, world_up));
    vec3 eye_up    = vec3_normalize(vec3_cross(eye_right, eye_direction));

    mat4 r;

    r.m[ 0] = eye_right.x;
    r.m[ 4] = eye_right.y;
    r.m[ 8] = eye_right.z;
    r.m[12] = -vec3_dot(eye_right, eye_position);

    r.m[ 1] = eye_up.x;
    r.m[ 5] = eye_up.y;
    r.m[ 9] = eye_up.z;
    r.m[13] = -vec3_dot(eye_up, eye_position);

    r.m[ 2] = -eye_direction.x;
    r.m[ 6] = -eye_direction.y;
    r.m[10] = -eye_direction.z;
    r.m[14] = vec3_dot(eye_direction, eye_position);

    r.m[ 3] = 0;
    r.m[ 7] = 0;
    r.m[11] = 0;
    r.m[15] = 1;

    return r;
}
