#pragma once

#include "gx_define.h"


//
// types
//

typedef struct
{
    union
    {
        struct { float x, y; };
        struct { float w, h; };
        float v[2];
    };
} vec2;

typedef struct
{
    union
    {
        struct { float x, y, z; };
        struct { float r, g, b; };
        float v[3];
    };
} vec3;

typedef struct
{
    union
    {
        struct { float x, y, z, w; };
        struct { float r, g, b, a; };
        float v[4];
    };
} vec4;

typedef struct
{
    float m[16];
} mat4;


//
// trig
//

// <math.h> forward declarations.
float sqrtf(float x);
float sinf(float x);
float cosf(float x);

//
// utility
//

float degrees_to_radians(float degrees);

// TODO: change back to type_operation
int32  min_int32(int32 a, int32 b);
int32  max_int32(int32 a, int32 b);
uint32 min_uint32(uint32 a, uint32 b);
uint32 max_uint32(uint32 a, uint32 b);
float  min_float(float a, float b);
float  max_float(float a, float b);
double min_double(double a, double b);
double max_double(double a, double b);
vec2   min_vec2(vec2 a, vec2 b);
vec2   max_vec2(vec2 a, vec2 b);
vec3   min_vec3(vec3 a, vec3 b);
vec3   max_vec3(vec3 a, vec3 b);

float clamp_float(float value, float min, float max);

float abs_float(float value);

vec3  calc_normal(vec3 v0, vec3 v1, vec3 v2);


//
// random
//

void init_random(uint32 seed);
uint64 rdtsc(void);

float random(void);
int32 random_int(int32 min, int32 max);
float random_float(float min, float max);

float noise(float x, float y, float z);
float fractal_noise(float x, float y, float z, uint32 octaves, float frequency, float persistence);


//
// vec2
//

vec2  vec2_zero(void);
vec2  vec2_new(float x, float y);
vec2  vec2_scalar(float s);
vec2  vec2_from_vec3(vec3 v);

vec2  vec2_add(vec2 a, vec2 b);
vec2  vec2_sub(vec2 a, vec2 b);
vec2  vec2_mul(vec2 v, float s);
vec2  vec2_div(vec2 v, float s);
vec2  vec2_negate(vec2 v);
vec2  vec2_abs(vec2 v);

float vec2_length(vec2 v);
float vec2_length2(vec2 v);
vec2  vec2_normalize(vec2 v);
float vec2_distance(vec2 a, vec2 b);
float vec2_distance2(vec2 a, vec2 b);

vec2  vec2_direction(float rotation);

bool  vec2_equal(vec2 a, vec2 b);


//
// vec3
//

vec3  vec3_zero(void);
vec3  vec3_new(float x, float y, float z);
vec3  vec3_scalar(float s);

vec3  vec3_add(vec3 a, vec3 b);
vec3  vec3_sub(vec3 a, vec3 b);
vec3  vec3_mul(vec3 v, float s);
vec3  vec3_div(vec3 v, float s);
vec3  vec3_negate(vec3 v);

float vec3_dot(vec3 a, vec3 b);
vec3  vec3_cross(vec3 a, vec3 b);

float vec3_length(vec3 v);
float vec3_length2(vec3 v);
vec3  vec3_normalize(vec3 v);
float vec3_distance(vec3 a, vec3 b);
float vec3_distance2(vec3 a, vec3 b);

vec3  vec3_direction(vec3 euler);

bool  vec3_equal(vec3 a, vec3 b);


//
// vec4
//

vec4 vec4_zero(void);
vec4 vec4_new(float x, float y, float z, float w);

vec4 vec4_mul(vec4 v, float s);
vec4 vec4_div(vec4 v, float s);

vec3 vec4_normalize(vec4 v);

//
// mat4
//

mat4 mat4_identity(void);

mat4 mat4_model(vec3 position, vec3 rotation, vec3 scale);

mat4 mat4_translate(mat4 m, vec3 translation);
mat4 mat4_rotate(mat4 m, float angle, vec3 axis);
mat4 mat4_scale(mat4 m, vec3 scale);

mat4 mat4_mul(mat4 a, mat4 b);
vec3 mat4_mul_vec3(mat4 m, vec3 v);
vec4 mat4_mul_vec4(mat4 m, vec4 v);

mat4 mat4_transpose(mat4 m);
mat4 mat4_inverse(mat4 m);

mat4 mat4_perspective(float fov_degrees, float aspect_ratio, float near, float far);
mat4 mat4_orthographic(float left, float right, float bottom, float top, float near, float far);

mat4 mat4_look_at(vec3 eye_position, vec3 target_position, vec3 world_up);
