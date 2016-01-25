#include "gx_renderer.h"
#include "gx_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <GL/gl3w.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb/stb_truetype.h>

//static struct TextBuffer global_text_buffer;

static uint32 create_shader(const char *source, uint32 type)
{
    uint32 shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    return shader;
}

static uint32 create_program(uint32 vertex_shader, uint32 fragment_shader)
{
    uint32 program = glCreateProgram();

    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    glDetachShader(program, fragment_shader);
    glDetachShader(program, vertex_shader);
    glDeleteShader(fragment_shader);
    glDeleteShader(vertex_shader);

    return program;
}

struct Renderer init_renderer(void)
{
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.0, 0.0, 0.0, 1.0);

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    struct Renderer renderer = {0};

    renderer.sprite_batch = create_sprite_batch(1000);

    renderer.quad_program = load_program("quad");

    renderer.camera_ubo = generate_ubo(sizeof(mat4), UBO_CAMERA);
    glGenVertexArrays(1, &renderer.blank_vao);

    return renderer;
}

void clean_renderer(struct Renderer *renderer)
{
    glDeleteVertexArrays(1, &renderer->blank_vao);
}

static void bind_ubo(uint32 program, uint32 binding, const char *name)
{
    uint32 index = glGetUniformBlockIndex(program, name);

    //ASSERT(index != GL_INVALID_INDEX);
    if (index != GL_INVALID_INDEX)
        glUniformBlockBinding(program, index, binding);
}

uint32 generate_ubo(uint32 size, uint32 binding)
{
    uint32 id;
    glGenBuffers(1, &id);

    glBindBuffer(GL_UNIFORM_BUFFER, id);

    glBufferData(GL_UNIFORM_BUFFER, size, 0, GL_STREAM_DRAW);
    glBindBufferRange(GL_UNIFORM_BUFFER, binding, id, 0, size);

    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    return id;
}

void update_ubo(uint32 ubo, size_t size, void *data)
{
    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, size, data);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

uint32 load_program(const char *name)
{
    char path[64];
    snprintf(path, sizeof(path), "res/shader/%s.vert", name);
    struct File vertex_file = load_file(path);

    snprintf(path, sizeof(path), "res/shader/%s.frag", name);
    struct File fragment_file = load_file(path);

    uint32 vertex_shader = create_shader(vertex_file.source, GL_VERTEX_SHADER);
    uint32 fragment_shader = create_shader(fragment_file.source, GL_FRAGMENT_SHADER);

    free_file(&fragment_file);
    free_file(&vertex_file);

    uint32 program = create_program(vertex_shader, fragment_shader);

    bind_ubo(program, UBO_CAMERA, "u_Camera");

    return program;
}

void free_program(uint32 program)
{
    glDeleteProgram(program);
}

void bind_program(uint32 program)
{
    glUseProgram(program);
}

void set_uniform_int32(const char *name, uint32 program, int32 value)
{
    int32 location = glGetUniformLocation(program, name);
    glUniform1i(location, value);
}

void set_uniform_uint32(const char *name, uint32 program, uint32 value)
{
    int32 location = glGetUniformLocation(program, name);
    glUniform1ui(location, value);
}

void set_uniform_float(const char *name, uint32 program, float value)
{
    int32 location = glGetUniformLocation(program, name);
    glUniform1f(location, value);
}

void set_uniform_vec3(const char *name, uint32 program, vec3 value)
{
    int32 location = glGetUniformLocation(program, name);
    glUniform3f(location, value.x, value.y, value.z);
}

void set_uniform_mat4(const char *name, uint32 program, mat4 value)
{
    int32 location = glGetUniformLocation(program, name);
    glUniformMatrix4fv(location, 1, GL_FALSE, &value.m[0]);
}

static uint32 generate_font_texture(uint32 width, uint32 height, uint8 *data)
{
    uint32 id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);

    // TODO: async pbo loading?
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindTexture(GL_TEXTURE_2D, 0);

    return id;
}

struct Font load_font(const char *name)
{
    char path[64];
    snprintf(path, sizeof(path), "res/font/%s.ttf", name);
    struct File file = load_file(path);

    struct Font font = {0};
    font.size = 13;

    uint8 temp_bitmap[512 * 512] = {0};
    stbtt_BakeFontBitmap((uint8 *)file.source, 0, font.size, temp_bitmap, 512, 512, 32, 96, font.char_data);

    free_file(&file);

    font.texture = generate_font_texture(512, 512, temp_bitmap);

    return font;
}

void free_texture(uint32 *id)
{
    glDeleteTextures(1, id);
}

struct SpriteBatch create_sprite_batch(uint32 size)
{
    struct SpriteBatch batch = {0};
    batch.max_size = size;

    batch.data = malloc(batch.max_size * 4 * sizeof(struct SpriteVertex));
    ASSERT_NOT_NULL(batch.data);

    glGenBuffers(1, &batch.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, batch.vbo);
    glBufferData(GL_ARRAY_BUFFER, batch.max_size * 4 * sizeof(struct SpriteVertex), NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    uint32 index_count = batch.max_size * 6;
    uint32 *index_data = malloc(index_count * sizeof(uint32));
    ASSERT_NOT_NULL(index_data);
    for (uint32 i = 0, face_offset = 0; i < index_count; i += 6, face_offset += 4)
    {
        index_data[i + 0] = 0 + face_offset;
        index_data[i + 1] = 1 + face_offset;
        index_data[i + 2] = 2 + face_offset;
        index_data[i + 3] = 0 + face_offset;
        index_data[i + 4] = 2 + face_offset;
        index_data[i + 5] = 3 + face_offset;
    }

    glGenBuffers(1, &batch.ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, batch.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_count * sizeof(uint32), index_data, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glGenVertexArrays(1, &batch.vao);
    glBindVertexArray(batch.vao);

    glBindBuffer(GL_ARRAY_BUFFER, batch.vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, false, sizeof(struct SpriteVertex), &((struct SpriteVertex *)0)->position);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, false, sizeof(struct SpriteVertex), &((struct SpriteVertex *)0)->uv);

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, false, sizeof(struct SpriteVertex), &((struct SpriteVertex *)0)->color);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, batch.ibo);

    glBindVertexArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    free(index_data);

    return batch;
}

void begin_sprite_batch(struct SpriteBatch *batch)
{
    ASSERT(!batch->drawing);
    batch->drawing = true;
}

void end_sprite_batch(struct SpriteBatch *batch)
{
    flush_sprite_batch(batch);

    ASSERT(batch->drawing);
    batch->drawing = false;
}

void flush_sprite_batch(struct SpriteBatch *batch)
{
    ASSERT(batch->drawing);
    if (batch->current_size == 0)
        return;

    glBindBuffer(GL_ARRAY_BUFFER, batch->vbo);
    glBufferData(GL_ARRAY_BUFFER, batch->current_size * 4 * sizeof(struct SpriteVertex), NULL, GL_DYNAMIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, batch->current_size * 4 * sizeof(struct SpriteVertex), batch->data);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindVertexArray(batch->vao);
    glDrawElements(GL_TRIANGLES, batch->current_size * 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    batch->current_size = 0;
}

static void add_quad_to_sprite_batch(struct SpriteBatch *batch, vec3 bottom_left, vec3 bottom_right, vec3 top_right, vec3 top_left, vec3 color)
{
    uint32 index = 4 * batch->current_size;
    if (index + 3 >= batch->max_size)
        flush_sprite_batch(batch);

    struct SpriteVertex *v0 = &batch->data[index++];
    v0->position = vec2_new(bottom_left.x, bottom_left.y);
    v0->uv = vec2_zero();
    v0->color = color;

    struct SpriteVertex *v1 = &batch->data[index++];
    v1->position = vec2_new(bottom_right.x, bottom_right.y);
    v1->uv = vec2_zero();
    v1->color = color;

    struct SpriteVertex *v2 = &batch->data[index++];
    v2->position = vec2_new(top_right.x, top_right.y);
    v2->uv = vec2_zero();
    v2->color = color;

    struct SpriteVertex *v3 = &batch->data[index++];
    v3->position = vec2_new(top_left.x, top_left.y);
    v3->uv = vec2_zero();
    v3->color = color;

    ++batch->current_size;
}

void draw_quad(struct SpriteBatch *batch, vec2 position, vec2 size, vec3 color)
{
    ASSERT(batch->drawing);

    mat4 matrix = mat4_identity();
    matrix = mat4_translate(matrix, vec3_new(position.x, position.y, 0.0f));
    matrix = mat4_scale(matrix, vec3_new(size.x, size.y, 1.0f));

    vec3 bottom_left  = mat4_mul_vec3(matrix, vec3_new(-0.5f, -0.5f, 0.0f));
    vec3 bottom_right = mat4_mul_vec3(matrix, vec3_new( 0.5f, -0.5f, 0.0f));
    vec3 top_right    = mat4_mul_vec3(matrix, vec3_new( 0.5f,  0.5f, 0.0f));
    vec3 top_left     = mat4_mul_vec3(matrix, vec3_new(-0.5f,  0.5f, 0.0f));

    add_quad_to_sprite_batch(batch, bottom_left, bottom_right, top_right, top_left, color);
}

static void draw_screen_quad(struct SpriteBatch *batch, vec2 position, vec2 size, vec3 color)
{
    ASSERT(batch->drawing);

    mat4 matrix = mat4_identity();
    matrix = mat4_translate(matrix, vec3_new(position.x, position.y, 0.0f));
    matrix = mat4_scale(matrix, vec3_new(size.x, size.y, 1.0f));

    vec3 bottom_left  = mat4_mul_vec3(matrix, vec3_new(0.0f, 1.0f, 0.0f));
    vec3 bottom_right = mat4_mul_vec3(matrix, vec3_new(1.0f, 1.0f, 0.0f));
    vec3 top_right    = mat4_mul_vec3(matrix, vec3_new(1.0f, 0.0f, 0.0f));
    vec3 top_left     = mat4_mul_vec3(matrix, vec3_new(0.0f, 0.0f, 0.0f));

    add_quad_to_sprite_batch(batch, bottom_left, bottom_right, top_right, top_left, color);
}

#if 0
void draw_text(const char *string, vec3 color)
{
    ASSERT(global_text_buffer.current_size < ARRAY_SIZE(global_text_buffer.texts));
    struct Text *text = &global_text_buffer.texts[global_text_buffer.current_size++];

    text->position = global_text_buffer.cursor;
    strcpy(text->string, string);
    text->color = color;

    // TODO: non-hardcoded font size
    global_text_buffer.cursor.y += 13;
}

void draw_global_text_buffer(struct SpriteBatch *sprite_batch, struct Font *font)
{
    begin_sprite_batch(sprite_batch);

    for (uint32 i = 0; i < global_text_buffer.current_size; ++i)
    {
        struct Text *text = &global_text_buffer.texts[i];

        float x = text->position.x;
        float y = text->position.y;
        for (char *c = text->string; *c != '\0'; ++c)
        {
            if (*c < 32)
                continue;

            stbtt_aligned_quad q;
            stbtt_GetBakedQuad(font->char_data, 512, 512, *c - 32, &x, &y, &q, 1);

            vec2 position = vec2_new(q.x0, q.y1 + font->size * 0.75f);
            vec2 size     = vec2_new(q.x1 - q.x0, q.y1 - q.y0);
            vec4 uv       = vec4_new(q.s0, q.t0, q.s1 - q.s0, q.t1 - q.t0);

            position.y -= size.y;

            struct Quad quad = {0};
            quad.position = vec3_new(position.x, position.y, 0.0f);
            quad.size     = vec3_new(size.x, size.y, 1.0f);
            quad.uv       = uv;
            quad.color    = text->color;
            add_quad_to_sprite_batch(sprite_batch, &quad);
        }
    }

    end_sprite_batch(sprite_batch);
}

void clear_global_text_buffer()
{
    global_text_buffer.current_size = 0;
    global_text_buffer.cursor = vec2_new(2, 2);
}
#endif

#if 0
void draw_line(struct LineBuffer *buffer, vec3 start, vec3 end, vec3 color)
{
    ASSERT(buffer->current_size < ARRAY_SIZE(buffer->lines));
    struct Line *line = &buffer->lines[buffer->current_size++];

    line->start = start;
    line->end = end;
    line->color = color;
}

void draw_line_buffer(struct LineBuffer *buffer, uint32 program)
{
    glBindVertexArray(global_blank_vao);
    for (uint32 i = 0; i < buffer->current_size; ++i)
    {
        struct Line *line = &buffer->lines[i];

        set_uniform_vec3("u_Start", program, line->start);
        set_uniform_vec3("u_End", program, line->end);
        set_uniform_vec3("u_Color", program, line->color);

        glDrawArrays(GL_LINES, 0, 2);
    }
    glBindVertexArray(0);
}

void clear_line_buffer(struct LineBuffer *buffer)
{
    buffer->current_size = 0;
}
#endif

void draw_quad_buffered(struct RenderBuffer *render_buffer, vec2 position, vec2 size, vec4 uv, vec3 color)
{
    struct QuadBuffer *buffer = &render_buffer->quads;

    ASSERT(buffer->current_size < ARRAY_SIZE(buffer->quads));
    struct Quad *quad = &buffer->quads[buffer->current_size++];

    quad->position = position;
    quad->size = size;
    quad->uv = uv;
    quad->color = color;
}

void draw_screen_quad_buffered(struct RenderBuffer *render_buffer, vec2 position, vec2 size, vec4 uv, vec3 color)
{
    struct QuadBuffer *buffer = &render_buffer->screen_quads;

    ASSERT(buffer->current_size < ARRAY_SIZE(buffer->quads));
    struct Quad *quad = &buffer->quads[buffer->current_size++];

    quad->position = vec2_new(position.x, position.y);
    quad->size = vec2_new(size.x, size.y);
    quad->uv = uv;
    quad->color = color;
}

void draw_quad_buffer(struct SpriteBatch *sprite_batch, struct RenderBuffer *render_buffer)
{
    begin_sprite_batch(sprite_batch);

    struct QuadBuffer *buffer = &render_buffer->quads;
    for (uint32 i = 0; i < buffer->current_size; ++i)
    {
        struct Quad *quad = &buffer->quads[i];
        draw_quad(sprite_batch, quad->position, quad->size, quad->color);
    }

    end_sprite_batch(sprite_batch);
}

void draw_screen_quad_buffer(struct SpriteBatch *sprite_batch, struct RenderBuffer *render_buffer)
{
    begin_sprite_batch(sprite_batch);

    struct QuadBuffer *buffer = &render_buffer->screen_quads;
    for (uint32 i = 0; i < buffer->current_size; ++i)
    {
        struct Quad *quad = &buffer->quads[i];
        draw_screen_quad(sprite_batch, quad->position, quad->size, quad->color);
    }

    end_sprite_batch(sprite_batch);
}

void clear_quad_buffer(struct QuadBuffer *buffer)
{
    buffer->current_size = 0;
}
