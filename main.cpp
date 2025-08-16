#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "linmath.h"
#include <vector>
#include <iostream>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

typedef struct Vertex
{
    vec2 pos;
} Vertex;

static const Vertex vertices[] =
{
    { {  1.0f, 1.0f } }, //top right
    { {  1.0f, -1.0f } }, //bottom right
    { {  -1.0f, -1.0f } }, //bottom left
    { { -1.0f, 1.0f } } //top left
};

unsigned int indices[] = {  // note that we start from 0!
    0, 1, 3,   // first triangle
    1, 2, 3    // second triangle
};


static float Random01Val()
{
    return (float)(rand() % 100)/100;
}

static const char* vertex_shader_text = R"(
    #version 330 core

    uniform mat4 MVP;
    in vec2 vPos;

    void main()
    {
        gl_Position = MVP * vec4(vPos, 0.0, 1.0);
    }
)";

static const char* fragment_shader_text = R"(
    #version 330 core
    uniform vec2 uResolution;     // window size in pixels
    layout(std140) uniform Seeds {
        vec4 uPoints[2048];
        vec4 uColors[2048];
    };
    uniform int uCount;

    out vec4 fragment;

    void main()
    {
        // Screen UV in [0..1], origin bottom-left (matches gl_FragCoord)
        vec2 uv = gl_FragCoord.xy / uResolution;

        int nearestIndex = 0;
        float nearestDist = distance(uv, uPoints[0].xy);

        for(int i = 1; i < uCount; ++i)
        {
            float d = distance(uv, uPoints[i].xy);
            if(d < nearestDist) {
                nearestDist = d;
                nearestIndex = i;
            }
        }

        fragment = vec4(uColors[nearestIndex].xyz, 1.0);
        //fragment = vec4(1.0, 1.0, 1.0, 1.0);
    }
)";

static void error_callback(int error, const char* description)
{
    fprintf(stderr, "Error: %s\n", description);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
}

int main(void)
{
    glfwSetErrorCallback(error_callback);

    if (!glfwInit())
        exit(EXIT_FAILURE);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Voronoi Diagram", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwSetKeyCallback(window, key_callback);

    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glfwSwapInterval(1);


    GLuint vertex_buffer;
    glGenBuffers(1, &vertex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    unsigned int EBO;
    glGenBuffers(1, &EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);


    const GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_text, NULL);
    glCompileShader(vertex_shader);

    const GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_text, NULL);
    glCompileShader(fragment_shader);

    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);


    #define MAX_SEEDS 2048

    struct SeedUBO {
        vec4 pos;   // usamos só .x/.y
        vec4 color; // usamos .x/.y/.z
    };

    int count = 2048;
    count = std::min(count, MAX_SEEDS);

    // cria e preenche vetor de seeds
    std::vector<SeedUBO> seeds(count);
    for (int i = 0; i < count; i++) {
        seeds[i].pos[0] = Random01Val();
        seeds[i].pos[1] = Random01Val();
        seeds[i].pos[2] = 0.0f;
        seeds[i].pos[3] = 0.0f;

        seeds[i].color[0] = Random01Val();
        seeds[i].color[1] = Random01Val();
        seeds[i].color[2] = Random01Val();
        seeds[i].color[3] = 1.0f;
    }

    GLuint ubo;
    glGenBuffers(1, &ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    GLsizeiptr totalSize = 2 * sizeof(vec4) * MAX_SEEDS; // points + colors
    glBufferData(GL_UNIFORM_BUFFER, totalSize, NULL, GL_STATIC_DRAW);

    // empacotar: [todos os points][todos os colors]
    std::vector<float> uboData(4 * 2 * MAX_SEEDS, 0.0f);

    // escrever points nos primeiros "MAX_SEEDS" vec4
    for (int i = 0; i < count; ++i) {
        uboData[4*i + 0] = seeds[i].pos[0];
        uboData[4*i + 1] = seeds[i].pos[1];
        uboData[4*i + 2] = seeds[i].pos[2];
        uboData[4*i + 3] = seeds[i].pos[3];
    }

    // escrever colors a partir do offset de "MAX_SEEDS" vec4
    size_t colorsOffset = 4 * MAX_SEEDS;
    for (int i = 0; i < count; ++i) {
        uboData[colorsOffset + 4*i + 0] = seeds[i].color[0];
        uboData[colorsOffset + 4*i + 1] = seeds[i].color[1];
        uboData[colorsOffset + 4*i + 2] = seeds[i].color[2];
        uboData[colorsOffset + 4*i + 3] = seeds[i].color[3];
    }

    glBufferSubData(GL_UNIFORM_BUFFER, 0, totalSize, uboData.data());
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // ligar o bloco "Seeds" no binding 0 e associar o buffer
    GLuint blockIndex = glGetUniformBlockIndex(program, "Seeds");
    glUniformBlockBinding(program, blockIndex, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo);


    const GLint mvp_location = glGetUniformLocation(program, "MVP");
    const GLint vpos_location = glGetAttribLocation(program, "vPos");
    const GLint locRes   = glGetUniformLocation(program, "uResolution");


    GLuint vertex_array;
    glGenVertexArrays(1, &vertex_array);
    glBindVertexArray(vertex_array);
    glEnableVertexAttribArray(vpos_location);
    glVertexAttribPointer(vpos_location, 2, GL_FLOAT, GL_FALSE,
                          sizeof(Vertex), (void*) offsetof(Vertex, pos));


    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "uCount"), count);

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    const float ratio = width / (float) height;

    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT);

    mat4x4 m, p, mvp;
    mat4x4_identity(m);
    mat4x4_ortho(p, -ratio, ratio, -1.f, 1.f, 1.f, -1.f);
    mat4x4_mul(mvp, p, m);

    glUseProgram(program);
    glUniformMatrix4fv(mvp_location, 1, GL_FALSE, (const float*)mvp);
    glUniform2f(locRes, (float)width, (float)height);

    glBindVertexArray(vertex_array);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);


    while (!glfwWindowShouldClose(window))
    {
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);

    glfwTerminate();
    exit(EXIT_SUCCESS);
}