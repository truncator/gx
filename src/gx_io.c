#include "gx_io.h"
#include <stdio.h>
#include <stdlib.h>

struct File load_file(const char *path)
{
    FILE *source = fopen(path, "r");
    ASSERT_NOT_NULL(source);

    struct File file = {0};
    fseek(source, 0, SEEK_END);
    file.size = ftell(source);
    fseek(source, 0, SEEK_SET);

    file.source = malloc(file.size + 1);
    fread(file.source, file.size, 1, source);
    file.source[file.size] = '\0';

    fclose(source);

    return file;
}

void free_file(struct File *file)
{
    free(file->source);
}

void scroll_wheel_callback(GLFWwindow *window, double dx, double dy)
{
    // TODO: larger user pointer struct?
    struct Input *input = (struct Input *)glfwGetWindowUserPointer(window);

    float previous_scroll_wheel = input->scroll_wheel;
    input->scroll_wheel += (float)dy;
    input->scroll_delta = input->scroll_wheel - previous_scroll_wheel;
}

bool key_down(uint32 keycode, struct Input *input)
{
    return input->keys[keycode] & 0x01;
}

bool key_down_new(uint32 keycode, struct Input *input)
{
    return key_down(keycode, input) && !(input->keys[keycode] & 0x02);
}

bool mouse_down(uint32 button, struct Input *input)
{
    ASSERT(button < 3);
    return input->mouse_buttons[button] & 0x01;
}

bool mouse_down_new(uint32 button, struct Input *input)
{
    return mouse_down(button, input) && !(input->mouse_buttons[button] & 0x02);
}

bool mouse_released(uint32 button, struct Input *input)
{
    return !mouse_down(button, input) && (input->mouse_buttons[button] & 0x02);
}

void wrap_cursor(GLFWwindow *window, struct Input *input, uint32 width, uint32 height)
{
    input->mouse_position.x = ((int32)input->mouse_position.x + width) % width;
    input->mouse_position.y = ((int32)input->mouse_position.y + height) % height;
    glfwSetCursorPos(window, input->mouse_position.x, input->mouse_position.y);
}

void process_input(GLFWwindow *window, struct Input *input)
{
    for (uint32 i = 0; i < ARRAY_SIZE(input->keys); ++i)
    {
        input->keys[i] <<= 1;

        if (glfwGetKey(window, i) == GLFW_PRESS)
            input->keys[i] |= 0x01;
    }

    double x, y;
    glfwGetCursorPos(window, &x, &y);
    input->mouse_delta = vec2_sub(input->mouse_position, vec2_new(x, y));
    input->mouse_position = vec2_new(x, y);

    for (uint32 i = 0; i < ARRAY_SIZE(input->mouse_buttons); ++i)
    {
        input->mouse_buttons[i] <<= 1;

        if (glfwGetMouseButton(window, i) == GLFW_PRESS)
            input->mouse_buttons[i] |= 0x01;

        if (mouse_down_new(i, input))
            input->mouse_down_positions[i] = input->mouse_position;
    }
}

void clear_input(struct Input *input)
{
    input->scroll_delta = 0;
}
