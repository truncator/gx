#pragma once

#include "gx_define.h"
#include "gx_math.h"
#include <stb/stb_truetype.h>

struct SpriteVertex
{
    vec2 position;
    vec2 uv;
    vec3 color;
};

struct SpriteBatch
{
    uint32 vbo;
    uint32 ibo;
    uint32 vao;

    bool drawing;

    // Number of quads.
    uint32 current_size;
    uint32 max_size;

    struct SpriteVertex *data;
};

struct Font
{
    uint32 texture;
    stbtt_bakedchar char_data[96];
    uint32 size;
};

struct Text
{
    char string[64];
    vec2 position;
    vec3 color;
};

struct TextBuffer
{
    struct Text texts[512];
    uint32 current_size;

    // Auto-formatting.
    vec2 cursor;
};

struct Line
{
    vec3 start;
    vec3 end;
    vec3 color;
};

struct LineBuffer
{
    struct Line lines[4096];
    uint32 current_size;
};

struct Quad
{
    vec2 position;
    vec2 size;
    vec4 uv;
    vec3 color;
};

struct QuadBuffer
{
    struct Quad quads[4096];
    uint32 current_size;
};

enum UniformBuffer
{
    UBO_CAMERA,
};

struct RenderBuffer
{
    struct QuadBuffer quads;
    struct QuadBuffer screen_quads;
};

struct Renderer
{
    struct SpriteBatch sprite_batch;

    uint32 quad_program;

    uint32 camera_ubo;
    uint32 blank_vao;
};

struct Renderer init_renderer(void);
void clean_renderer(struct Renderer *renderer);

uint32 generate_ubo(uint32 size, uint32 binding);
void update_ubo(uint32 ubo, size_t size, void *data);

uint32 load_program(const char *name);
void free_program(uint32 program);
void bind_program(uint32 program);

void set_uniform_int32(const char *name, uint32 program, int32 value);
void set_uniform_uint32(const char *name, uint32 program, uint32 value);
void set_uniform_float(const char *name, uint32 program, float value);
void set_uniform_vec3(const char *name, uint32 program, vec3 value);
void set_uniform_mat4(const char *name, uint32 program, mat4 value);

struct Font load_font(const char *name);
void free_texture(uint32 *id);

struct SpriteBatch create_sprite_batch(uint32 size);

void begin_sprite_batch(struct SpriteBatch *batch);
void end_sprite_batch(struct SpriteBatch *batch);
void flush_sprite_batch(struct SpriteBatch *batch);

void draw_quad(struct SpriteBatch *batch, vec2 position, vec2 size, vec3 color);

#if 0
void draw_text(const char *string, vec3 color);
void draw_global_text_buffer(struct SpriteBatch *sprite_batch, struct Font *font);
void clear_global_text_buffer(void);
#endif

#if 0
void draw_line(struct LineBuffer *buffer, vec3 start, vec3 end, vec3 color);
void draw_line_buffer(struct LineBuffer *buffer, uint32 program);
void clear_line_buffer(struct LineBuffer *buffer);
#endif

void draw_quad_buffered(struct RenderBuffer *render_buffer, vec2 position, vec2 size, vec4 uv, vec3 color);
void draw_screen_quad_buffered(struct RenderBuffer *render_buffer, vec2 position, vec2 size, vec4 uv, vec3 color);
void draw_quad_buffer(struct SpriteBatch *sprite_batch, struct RenderBuffer *render_buffer);
void draw_screen_quad_buffer(struct SpriteBatch *sprite_batch, struct RenderBuffer *render_buffer);
void clear_quad_buffer(struct QuadBuffer *buffer);
