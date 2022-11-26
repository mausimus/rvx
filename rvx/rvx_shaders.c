/*
    RVX Graphics Library 
    (c) 2022 mausimus.github.io
    MIT License
*/

#include "rvx_shaders.h"

#include <string.h>

#ifndef SHADER_IMPORT

#define RVX_VERTEX_SHADER_BODY                                                                                                             \
    "layout(location = 0) in vec4 vertexPosition;\n"                                                                                       \
    "layout(location = 1) in vec4 vertexColor;\n"                                                                                          \
    "flat out vec4 fragColor;\n"                                                                                                           \
    "uniform mat4 view;\n"                                                                                                                 \
    "uniform float alpha;\n"                                                                                                               \
    "void main()\n"                                                                                                                        \
    "{\n"                                                                                                                                  \
    "	fragColor = vec4(vertexColor.xyz, alpha);\n"                                                                                         \
    "	gl_Position = view /** model*/ * vertexPosition;\n"                                                                                  \
    "}\n";

    const char* rvxVertexShaderSourceGLES = "#version 300 es\n" RVX_VERTEX_SHADER_BODY

    const char* rvxVertexShaderSourceGL = "#version 330 core\n" RVX_VERTEX_SHADER_BODY

#define EDGE_VERTEX_SHADER_BODY                                                                                                            \
    "layout(location = 0) in vec4 vertexPosition;\n"                                                                                       \
    "layout(location = 1) in vec3 vertexColor;\n"                                                                                          \
    "layout(location = 2) in vec4 edge;\n"                                                                                                 \
    "flat out vec4 fragColor;\n"                                                                                                           \
    "uniform mat4 view;\n"                                                                                                                 \
    "uniform float alpha;\n"                                                                                                               \
    "void main()\n"                                                                                                                        \
    "{\n"                                                                                                                                  \
    "	fragColor = vec4(vertexColor.xyz, alpha);\n"                                                                                         \
    "	gl_Position = view * /*model **/ vertexPosition;\n"                                                                                  \
    "float dvx;\n"                                                                                                                         \
    "float dvy;\n"                                                                                                                         \
    "int minmax;\n"                                                                                                                        \
    "int align=int(edge.w);\n"                                                                                                             \
    "if((align & 1) != 0)\n"                                                                                                               \
    "{\n"                                                                                                                                  \
    "   if(gl_Position.x < 0.0)\n"                                                                                                         \
    "   {\n"                                                                                                                               \
    "dvx = edge.x;\n"                                                                                                                      \
    "dvy = -edge.y;\n"                                                                                                                     \
    "minmax = -1;\n"                                                                                                                       \
    "   } else {\n"                                                                                                                        \
    "dvx = edge.x;\n"                                                                                                                      \
    "dvy = edge.y;\n"                                                                                                                      \
    "minmax = -1;\n"                                                                                                                       \
    "       \n"                                                                                                                            \
    "   }\n"                                                                                                                               \
    "} else if((align & 2) != 0)\n"                                                                                                        \
    "{\n"                                                                                                                                  \
    "   if(gl_Position.x > 0.0)\n"                                                                                                         \
    "   {\n"                                                                                                                               \
    "dvx = -edge.x;\n"                                                                                                                     \
    "dvy = -edge.y;\n"                                                                                                                     \
    "minmax = 1;\n"                                                                                                                        \
    "   } else {\n"                                                                                                                        \
    "dvx = -edge.x;\n"                                                                                                                     \
    "dvy = edge.y;\n"                                                                                                                      \
    "minmax = 1;\n"                                                                                                                        \
    "       \n"                                                                                                                            \
    "   }\n"                                                                                                                               \
    "}\n"                                                                                                                                  \
    "if((align & 3) != 0)\n"                                                                                                               \
    "{\n"                                                                                                                                  \
    "       vec4 refVertex = view * vec4(vertexPosition.x + dvx, vertexPosition.y + dvy, vertexPosition.z, 1.0);\n"            \
    "       float myX = gl_Position.x / gl_Position.w; \n"                                                                                 \
    "       float refX = refVertex.x / refVertex.w; \n"                                                                                    \
    "       float newX = refX * gl_Position.w;\n"                                                                                          \
    "       gl_Position.x = minmax == -1 ? min(newX, gl_Position.x) : max(newX, gl_Position.x);\n"                                         \
    "}\n"                                                                                                                                  \
    "if(((align & 4) != 0) || ((align & 8) != 0))\n"                                                                                       \
    "{\n"                                                                                                                                  \
    "       vec4 frontVertex = view * vec4(vertexPosition.x, vertexPosition.y - edge.y, vertexPosition.z + edge.z * 16.0, 1.0);\n"    \
    "       float myY = gl_Position.y / gl_Position.w; \n"                                                                                 \
    "       float frontY = frontVertex.y / frontVertex.w; \n"                                                                              \
    "       float newY = frontY * gl_Position.w;\n"                                                                                        \
    "       gl_Position.y = newY;\n"                                                                                                       \
    "}\n"                                                                                                                                  \
    "}\n";

    const char* edgeVertexShaderSourceGLES = "#version 300 es\n"
                                             "precision highp float;\n" EDGE_VERTEX_SHADER_BODY

    const char* edgeVertexShaderSourceGL = "#version 330 core\n" EDGE_VERTEX_SHADER_BODY

#define RVX_FRAGMENT_SHADER_BODY                                                                                                           \
    "flat in vec4 fragColor;\n"                                                                                                            \
    "out vec4 finalColor;\n"                                                                                                               \
    "void main()\n"                                                                                                                        \
    "{\n"                                                                                                                                  \
    " finalColor = fragColor;\n"                                                                                                           \
    "}\n";

    const char* rvxFragmentShaderSourceGLES = "#version 300 es\n"
                                              "precision mediump float;\n" RVX_FRAGMENT_SHADER_BODY

    const char* rvxFragmentShaderSourceGL = "#version 330 core\n" RVX_FRAGMENT_SHADER_BODY

#else
const char* rvxFragmentShaderSourceGLES      = 0;
const char* rvxVertexShaderSourceGLES        = 0;
const char* edgeVertexShaderSourceGLES       = 0;
const char* rvxFragmentShaderSourceGL        = 0;
const char* rvxVertexShaderSourceGL          = 0;
const char* edgeVertexShaderSourceGL         = 0;
#endif

const char *s_shader_names[] = {"rvxFragment", "rvxVertex", "edgeVertex"};

const char** rvx_get_shader_source(const char* backend, const char* shader)
{
    if(strcmp("gles3", backend) == 0)
    {
        if(strcmp("rvxFragment", shader) == 0)
            return &rvxFragmentShaderSourceGLES;
        if(strcmp("rvxVertex", shader) == 0)
            return &rvxVertexShaderSourceGLES;
        if(strcmp("edgeVertex", shader) == 0)
            return &edgeVertexShaderSourceGLES;
    }
    if(strcmp("gl", backend) == 0)
    {
        if(strcmp("rvxFragment", shader) == 0)
            return &rvxFragmentShaderSourceGL;
        if(strcmp("rvxVertex", shader) == 0)
            return &rvxVertexShaderSourceGL;
        if(strcmp("edgeVertex", shader) == 0)
            return &edgeVertexShaderSourceGL;
    }
    return NULL;
}

void rvx_set_shader_source(const char* backend, const char* shader, const char* source)
{
    if(strcmp("gles3", backend) == 0)
    {
        if(strcmp("rvxFragment", shader) == 0)
            rvxFragmentShaderSourceGLES = source;
        else if(strcmp("rvxVertex", shader) == 0)
            rvxVertexShaderSourceGLES = source;
        else if(strcmp("edgeVertex", shader) == 0)
            edgeVertexShaderSourceGLES = source;
    }
    if(strcmp("gl", backend) == 0)
    {
        if(strcmp("rvxFragment", shader) == 0)
            rvxFragmentShaderSourceGL = source;
        else if(strcmp("rvxVertex", shader) == 0)
            rvxVertexShaderSourceGL = source;
        else if(strcmp("edgeVertex", shader) == 0)
            edgeVertexShaderSourceGL = source;
    }
}