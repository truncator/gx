#include <GL/gl3w.h>
#include <GLFW/glfw3.h>
#include <stdlib.h>

#include "gx_define.h"
#include "gx_io.h"
#include "gx_math.h"
#include "gx_renderer.h"
#include "gx.h"

struct GameWindow
{
    GLFWwindow *glfw;
    uint32 width;
    uint32 height;
};

static void APIENTRY debug_message_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* user_param)
{
    // TODO: output severity?
    fprintf(stderr, "[GL_ERROR] %s\n", message);
}

static struct GameWindow create_window(const char *title, uint32 width, uint32 height)
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    const GLFWvidmode* video_mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    glfwWindowHint(GLFW_RED_BITS, video_mode->redBits);
    glfwWindowHint(GLFW_GREEN_BITS, video_mode->greenBits);
    glfwWindowHint(GLFW_BLUE_BITS, video_mode->blueBits);
    glfwWindowHint(GLFW_REFRESH_RATE, video_mode->refreshRate);

    glfwWindowHint(GLFW_VISIBLE, GL_TRUE);
    glfwWindowHint(GLFW_SRGB_CAPABLE, GL_TRUE);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GL_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 0);

    GLFWwindow *glfw_window = glfwCreateWindow(width, height, title, NULL, NULL);
    ASSERT_NOT_NULL(glfw_window);
    glfwMakeContextCurrent(glfw_window);

    glfwSetInputMode(glfw_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    struct GameWindow window = {0};
    window.glfw = glfw_window;
    window.width = width;
    window.height = height;
    return window;
}

int main(int argc, char *argv[])
{
    //
    // glfw
    //

    if (!glfwInit())
    {
        fprintf(stderr, "[ERROR] Failed to initialize GLFW.\n");
        return 1;
    }

    struct GameWindow window = create_window("floating", 1280, 720);


    //
    // gl3w
    //

    if (gl3wInit())
    {
        fprintf(stderr, "[ERROR] Failed to initialize GL3W.\n");
        return 1;
    }

    if (!gl3wIsSupported(3, 3))
    {
        fprintf(stderr, "[ERROR] System does not support OpenGL 3.3.\n");
        return 1;
    }

    if (GL_ARB_debug_output)
    {
        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageCallbackARB((GLDEBUGPROC)&debug_message_callback, NULL);
    }


    //
    // init
    //

    init_random(234);

    struct Input input = {0};
    struct Renderer renderer = init_renderer();

    struct GameMemory game_memory = {0};
    game_memory.game_memory_size = MEGABYTES(2);
    game_memory.render_memory_size = MEGABYTES(1);
    game_memory.game_memory = calloc(1, game_memory.game_memory_size);
    game_memory.render_memory = calloc(1, game_memory.render_memory_size);

    init_game(&game_memory);


    //
    // main loop
    //

    const double tick_dt = 1.0 / 60.0;
    double time = glfwGetTime();
    double tick_accumulator = 0.0;

    while (!glfwWindowShouldClose(window.glfw))
    {
        //
        // tick
        //

        glfwPollEvents();

        double new_time = glfwGetTime();
        double frame_time = new_time - time;
        time = new_time;

        frame_time = max_double(0.0, frame_time);
        frame_time = min_double(0.2, frame_time);

        tick_accumulator += frame_time;

        while (tick_accumulator >= tick_dt)
        {
            process_input(window.glfw, &input);

            tick_game(&game_memory, &input, window.width, window.height, tick_dt);
            clear_input(&input);

            tick_accumulator -= tick_dt;
        }


        //
        // render
        //

        glViewport(0, 0, window.width, window.height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        render_game(&game_memory, &renderer, window.width, window.height);

        glfwSwapBuffers(window.glfw);
    }


    //
    // cleanup
    //

    free(game_memory.render_memory);
    free(game_memory.game_memory);
    clean_renderer(&renderer);

    glfwDestroyWindow(window.glfw);
    glfwTerminate();

    return 0;
}
