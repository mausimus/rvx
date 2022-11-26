/*
    RVX Graphics Library 
    (c) 2022 mausimus.github.io
    MIT License
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "rvx.h"
#include "rvx_shaders.h"

#ifndef cglm_mat_h
#include "include/cglm/mat4.h"
#endif
#ifndef cglm_cam_h
#include "include/cglm/cam.h"
#endif
#ifndef cglm_affine_h
#include "include/cglm/affine.h"
#endif

#define RVX_VERTEX_SIZE ((2 /*position*/ + 1 /*color*/) * sizeof(float))
#define RVX_EDGE_VERTEX_SIZE ((2 /*position*/ + 1 /*color*/ + 1 /*edge*/) * sizeof(float))
#define RVX_EDGE_LENGTH 6
#define RVX_VOXEL_LENGTH 6
#define RVX_VOXEL_SIZE (RVX_VOXEL_LENGTH * RVX_VERTEX_SIZE)
#define RVX_EDGE_SIZE (RVX_EDGE_LENGTH * RVX_EDGE_VERTEX_SIZE)

#define PI 3.14159265358979323846f
#define DEG2RAD (PI / 180.0f)

const char* rvx_backend_gles3 = "gles3";
const char* rvx_backend_gl    = "gl";
const int   ALIGN_LEFT        = 1;
const int   ALIGN_RIGHT       = 2;
const int   ALIGN_BOTTOM      = 4;
const int   ALIGN_TOP         = 8;

void rvx_check_glerror(const char* function)
{
    int e = glGetError();
    if(e != 0)
    {
        if(function != NULL)
        {
            rvx_error("OpenGL error %04X in %s\n", e, function);
        }
        else
        {
            rvx_error("OpenGL error %04X\n", e);
        }
        exit(EXIT_FAILURE);
    }
}

inline void qlVertex2f(float x, float y, float z, float** vertexPtr)
{
    if(y == 0)
    {
        short** ptr = (short**)vertexPtr;
        *(*ptr)++   = (short)x;
        *(*ptr)++   = (short)(z * 16.0f);
        *vertexPtr  = (float*)*ptr;
    }
    else
    {
        short** ptr = (short**)vertexPtr;
        *(*ptr)++   = (short)-x - 1;
        *(*ptr)++   = (short)(z * 16.0f);
        *vertexPtr  = (float*)*ptr;
    }
}

inline void qlVertex4b(int is_back, float x, float y, float z, float** vertexPtr)
{
    unsigned char** ptr = (unsigned char**)vertexPtr;
    *(*ptr)++           = (unsigned char)x;
    *(*ptr)++           = (unsigned char)y;
    *(*ptr)++           = (unsigned char)z; // we need x16 in shader
    *(*ptr)++           = (unsigned char)is_back;
    *vertexPtr          = (float*)*ptr;
}

inline void qlVertex3f(float x, float y, float z, uint8_t ci, Color4* c, float** vertexPtr)
{
    short** ptr = (short**)vertexPtr;
    *(*ptr)++   = (short)x;
    *(*ptr)++   = (short)y;
    *(*ptr)++   = (short)(z * 16.0f);
    *(*ptr)++   = 1; // padding
    *vertexPtr  = (float*)*ptr;

    int** iptr = (int**)vertexPtr;
    *(*iptr)   = (ci << 24) + (c->b << 16) + (c->g << 8) + (c->r);
    *(*vertexPtr)++;
}

inline void qlVertex7f(float   x,
                       float   y,
                       float   z,
                       uint8_t ci,
                       Color4* c,
                       uint8_t edgeWidth,
                       uint8_t edgeSpacing,
                       uint8_t edgeHeight,
                       uint8_t alignFlags,
                       float** vertexPtr)
{
    short** sptr = (short**)vertexPtr;
    *(*sptr)++   = (short)x;
    *(*sptr)++   = (short)y;
    *(*sptr)++   = (short)(z * 16.0f);
    *(*sptr)++   = 1; // padding
    *vertexPtr   = (float*)*sptr;

    uint32_t** ptr = (uint32_t**)vertexPtr;
    *(*ptr)        = (ci << 24) + (c->b << 16) + (c->g << 8) + (c->r);
    *(*vertexPtr)++;

    ptr     = (uint32_t**)vertexPtr;
    *(*ptr) = (alignFlags << 24) + (edgeHeight << 16) + (edgeSpacing << 8) + edgeWidth;
    *(*vertexPtr)++;
}

void rvx_model_populate_buffer(RVX_MODEL* model, Voxel* voxels, int modelVoxels, Color4 palette[256])
{
    int oldBufferSize = 0;
    if(model->buffer)
        oldBufferSize = model->bufferSize;

    int oldEdgeBufferSize = 0;
    if(model->edgeBuffer)
        oldEdgeBufferSize = model->edgeBufferSize;

    model->numVoxels      = modelVoxels;
    model->modelLength    = modelVoxels * RVX_VOXEL_LENGTH;
    model->bufferSize     = (modelVoxels)*RVX_VOXEL_SIZE;
    model->edgeBufferSize = 0;

    // if we have edges, reserve buffer for them
    if(model->numEdges > 0)
    {
        model->edgesLength = 0;
        for(int e = 0; e < model->numEdges; e++)
        {
            RVX_EDGE* edge     = model->edges + e;
            int       edge_len = rvx_get_edge_length(edge);
            model->edgesLength += edge_len;
            model->edgeBufferSize += edge_len * RVX_EDGE_SIZE;
        }
    }

    if(oldEdgeBufferSize != model->edgeBufferSize)
    {
        if(model->edgeBuffer)
            free(model->edgeBuffer);

        model->edgeBuffer = (float*)malloc(model->edgeBufferSize);
    }

    if(model->numEdges > 0)
    {
        float* vertexPtr = model->edgeBuffer;
        for(int e = 0; e < model->numEdges; e++)
        {
            RVX_EDGE* edge = model->edges + e;
            rvx_update_edge_buffer(edge, &vertexPtr, palette);
        }
    }

    // need new buffer?
    if(oldBufferSize != model->bufferSize)
    {
        if(model->buffer)
            free(model->buffer);

        model->buffer = (float*)malloc(model->bufferSize);
    }
    float* vertexPtr = model->buffer;
    Voxel* vx        = voxels;

    int min_x = 5000;
    int max_x = 0;
    int min_y = 5000;
    int max_y = 0;

    for(int v = 0; v < modelVoxels; v++)
    {
        if(vx->sx < min_x)
            min_x = vx->sx;
        if(vx->ex > max_x)
            max_x = vx->ex;
        if(vx->y < min_y)
            min_y = vx->y;
        if(vx->y > max_y)
            max_y = vx->y;
        rvx_emit_voxel(vx, palette + vx->colorIndex, &vertexPtr);
        vx++;
    }
}

RVX_MODEL* rvx_model_new()
{
    RVX_MODEL* model = (RVX_MODEL*)malloc(sizeof(RVX_MODEL));
    if(model == NULL)
        abort();

    model->loaded         = 1;
    model->bound          = 0;
    model->numVoxels      = 0;
    model->buffer         = NULL;
    model->bufferSize     = 0;
    model->areas          = NULL;
    model->numAreas       = 0;
    model->modelLength    = 0;
    model->numEdges       = 0;
    model->edges          = NULL;
    model->edgesLength    = 0;
    model->edgeBufferSize = 0;
    model->edgeBuffer     = NULL;
    return model;
}

void rvx_model_free(RVX_MODEL* model)
{
    if(model->buffer != NULL)
        free(model->buffer);
    if(model->areas != NULL)
        free(model->areas);
    if(model->edges != NULL)
        free(model->edges);

    free(model);
}

void rvx_model_bind(RVX_RENDERER* renderer, RVX_MODEL* model)
{
    if(model->bound)
        return;

    glGenVertexArrays(1, &model->VAO);
    glGenBuffers(1, &model->VBO);

    glBindVertexArray(model->VAO);

    glBindBuffer(GL_ARRAY_BUFFER, model->VBO);
    glBufferData(GL_ARRAY_BUFFER, model->bufferSize, model->buffer, GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 4, GL_SHORT, GL_FALSE, 6 * sizeof(short), (void*)0);
    glEnableVertexAttribArray(0);
    // color attribute
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, 6 * sizeof(short), (void*)(4 * sizeof(short)));
    glEnableVertexAttribArray(1);

    // edges
    if(model->numEdges > 0)
    {
        // buffers
        glGenVertexArrays(1, &model->edgeVAO);
        glGenBuffers(1, &model->edgeVBO);

        glBindVertexArray(model->edgeVAO);

        glBindBuffer(GL_ARRAY_BUFFER, model->edgeVBO);

        glBufferData(GL_ARRAY_BUFFER, model->edgeBufferSize, model->edgeBuffer, GL_DYNAMIC_DRAW);

        // position attribute
        glVertexAttribPointer(0, 4, GL_SHORT, GL_FALSE, 8 * sizeof(short), (void*)0);
        glEnableVertexAttribArray(0);
        // color attribute
        glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, 8 * sizeof(short), (void*)(4 * sizeof(short)));
        glEnableVertexAttribArray(1);
        // edge attributes
        glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_FALSE, 8 * sizeof(short), (void*)(6 * sizeof(short)));
        glEnableVertexAttribArray(2);
    }
    model->bound = 1;
}

void rvx_model_unbind(RVX_MODEL* model)
{
    if(model->bound)
    {
        glDeleteVertexArrays(1, &model->VAO);
        glDeleteBuffers(1, &model->VBO);
        if(model->numEdges > 0)
        {
            glDeleteVertexArrays(1, &model->edgeVAO);
            glDeleteBuffers(1, &model->edgeVBO);
        }
        model->bound = 0;
    }
}

void rvx_model_render(RVX_RENDERER* renderer, RVX_MODEL* model, int area)
{
    int buffer_update_required = 0;
    if(!model->bound)
    {
        rvx_model_bind(renderer, model);
    }

    glBindVertexArray(model->VAO);

    if(area == 0 || model->numAreas == 0)
    {
        glDrawArrays(GL_TRIANGLES, 0, model->numVoxels * 6);
    }
    else
    {
        for(int a = 0; a < model->numAreas; a++)
        {
            if(model->areas[a].no == area)
            {
                glDrawArrays(GL_TRIANGLES, model->areas[a].start * 6, model->areas[a].len * 6);
                break;
            }
        }
    }
}

RVX_RENDERER* rvx_renderer_init(const char* backend, float paletteMix)
{
    RVX_RENDERER* renderer = (RVX_RENDERER*)malloc(sizeof(RVX_RENDERER));
    if(renderer == NULL)
        return NULL;

    renderer->backend = backend;
    renderer->rvxShaderProgram =
        rvx_compile_shader(rvx_get_shader_source(renderer->backend, "rvxVertex"), rvx_get_shader_source(renderer->backend, "rvxFragment"));

    renderer->viewLocation  = glGetUniformLocation(renderer->rvxShaderProgram, "view");
    renderer->alphaLocation = glGetUniformLocation(renderer->rvxShaderProgram, "alpha");

    renderer->aspectW      = 320;
    renderer->aspectH      = 168;
    renderer->cullFar      = 300.0f;
    renderer->cullNear     = 10.0f;
    renderer->camX         = 0;
    renderer->camY         = 0;
    renderer->renderWidth  = 1280;
    renderer->renderHeight = 672;

    rvx_renderer_init_edges(renderer);

    return renderer;
}

void rvx_renderer_init_edges(RVX_RENDERER* renderer)
{
    renderer->edgeShaderProgram =
        rvx_compile_shader(rvx_get_shader_source(renderer->backend, "edgeVertex"), rvx_get_shader_source(renderer->backend, "rvxFragment"));

    renderer->edgeViewLocation  = glGetUniformLocation(renderer->edgeShaderProgram, "view");
    renderer->edgeAlphaLocation = glGetUniformLocation(renderer->edgeShaderProgram, "alpha");
}

void rvx_renderer_free(RVX_RENDERER* renderer)
{
    glDeleteProgram(renderer->rvxShaderProgram);
    glDeleteProgram(renderer->edgeShaderProgram);
    free(renderer);
}

void rvx_renderer_translate(RVX_RENDERER* renderer, float deltaX, float deltaY, float deltaZ)
{
    mat4 model;
    vec3 model_pos;
    model_pos[0] = deltaX;
    model_pos[1] = deltaY;
    model_pos[2] = deltaZ * 16.0f;
    glm_translate_make(model, model_pos);

    mat4 view;
    memcpy(view, renderer->viewMatrix, 16 * sizeof(float));
    glm_mat4_mul(view, model, view);

    glUseProgram(renderer->edgeShaderProgram);
    glUniformMatrix4fv(renderer->edgeViewLocation, 1, 0, (GLfloat*)&view);
    glUseProgram(renderer->rvxShaderProgram);
    glUniformMatrix4fv(renderer->viewLocation, 1, 0, (GLfloat*)&view);
}

void rvx_renderer_affine(
    RVX_RENDERER* renderer, float deltaX, float deltaY, float deltaZ, float scaleX, float scaleY, float scaleZ, int shadow)
{
    mat4 modelMat;
    vec3 translateVec = {deltaX, deltaY, deltaZ * 16.0f};
    glm_translate_make(modelMat, translateVec);
    vec3 scaleVec = {scaleX, scaleY, scaleZ};
    glm_scale(modelMat, scaleVec);

    mat4 view;
    memcpy(view, renderer->viewMatrix, 16 * sizeof(float));
    glm_mat4_mul(view, modelMat, view);

    glUseProgram(renderer->rvxShaderProgram);
    glUniformMatrix4fv(renderer->viewLocation, 1, GL_FALSE, (GLfloat*)&view);
}

void rvx_renderer_view(RVX_RENDERER* renderer, SceneParams* params)
{
    mat4 matrix;
    glm_mat4_identity(matrix);

    const float aspect = renderer->aspectW / (float)renderer->aspectH;
    float       top    = renderer->cullNear * tanf(params->CAM_FOV * 0.5f * DEG2RAD);
    float       right  = top * aspect;

    glm_frustum(-right, right, -top, top, renderer->cullNear, renderer->cullFar, (vec4*)&matrix);

    vec3 eye    = {renderer->camX, renderer->camY - params->CAM_DIST, params->CAM_HEIGHT};
    vec3 target = {renderer->camX, renderer->camY, params->CAM_HEIGHT};
    vec3 up     = {0, 0, 1};

    mat4 camMatrix;
    glm_lookat(eye, target, up, camMatrix);

    if(params->SHEARING_X != 0 || params->SHEARING_Y != 0)
    {
        float sx   = params->SHEARING_X;
        float sy   = params->SHEARING_Y;
        float dist = params->CAM_DIST;
        mat4  shearMatrix;

        shearMatrix[0][0] = 1.0f;
        shearMatrix[1][0] = 0.0f;
        shearMatrix[2][0] = sx;
        shearMatrix[3][0] = sx * dist;

        shearMatrix[0][1] = 0.0f;
        shearMatrix[1][1] = 1.0f;
        shearMatrix[2][1] = sy;
        shearMatrix[3][1] = sy * dist;

        shearMatrix[0][2] = 0.0f;
        shearMatrix[1][2] = 0.0f;
        shearMatrix[2][2] = 1.0f;
        shearMatrix[3][2] = 0.0f;

        shearMatrix[0][3] = 0.0f;
        shearMatrix[1][3] = 0.0f;
        shearMatrix[2][3] = 0.0f;
        shearMatrix[3][3] = 1.0f;

        glm_mat4_mul(matrix, shearMatrix, matrix);
    }

    if(params->OFFSET_X != 0 || params->OFFSET_Y != 0)
    {
        float sx   = params->OFFSET_X;
        float sy   = params->OFFSET_Y;
        float dist = renderer->camY;
        mat4  shearMatrix;

        shearMatrix[0][0] = 1.0f;
        shearMatrix[1][0] = 0.0f;
        shearMatrix[2][0] = sx;
        shearMatrix[3][0] = sx * dist;

        shearMatrix[0][1] = 0.0f;
        shearMatrix[1][1] = 1.0f;
        shearMatrix[2][1] = sy;
        shearMatrix[3][1] = 0;

        shearMatrix[0][2] = 0.0f;
        shearMatrix[1][2] = 0.0f;
        shearMatrix[2][2] = 1.0f;
        shearMatrix[3][2] = 0.0f;

        shearMatrix[0][3] = 0.0f;
        shearMatrix[1][3] = 0.0f;
        shearMatrix[2][3] = 0.0f;
        shearMatrix[3][3] = 1.0f;

        glm_mat4_mul(matrix, shearMatrix, matrix);
    }

    glm_mat4_mul(matrix, camMatrix, matrix);

    vec3 accuracy_scale = {1, 1, 0.0625f};
    glm_scale(matrix, accuracy_scale);

    glUseProgram(renderer->edgeShaderProgram);
    glUniformMatrix4fv(renderer->edgeViewLocation, 1, GL_FALSE, (GLfloat*)&matrix);
    glUniform1f(renderer->edgeAlphaLocation, 1.0f);

    glUseProgram(renderer->rvxShaderProgram);
    glUniformMatrix4fv(renderer->viewLocation, 1, GL_FALSE, (GLfloat*)&matrix);
    glUniform1f(renderer->alphaLocation, 1.0f);

    memcpy(renderer->viewMatrix, matrix, 16 * sizeof(float));
}

void rvx_renderer_begin(RVX_RENDERER* renderer)
{
    glUseProgram(renderer->rvxShaderProgram);
    glClearStencil(0);
    glStencilMask(0xFF);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

void rvx_renderer_end(RVX_RENDERER* renderer)
{
    glUseProgram(0);
}

void rvx_emit_voxel(Voxel* voxel, Color4* color, float** bufferPtr)
{
    const float adjustedY = voxel->y * 1.0f;
    const float sizeH  = 1;
    const float sizeW  = 1;
    const float dx     = 0;
    const float dz     = 0;
    const float startX = dx + (voxel->sx * 1); //    -sizeW / 2.0f;
    const float startZ = dz + voxel->sz; //    -sizeH / 2.0f;
    const float endX   = dx + (voxel->ex * 1) + sizeW; //    / 2.0f;
    const float endZ   = dz + voxel->ez + sizeH; //    / 2.0f;

    qlVertex3f(startX, adjustedY, startZ, voxel->colorIndex, color, bufferPtr);
    qlVertex3f(endX, adjustedY, endZ, voxel->colorIndex, color, bufferPtr);
    qlVertex3f(startX, adjustedY, endZ, voxel->colorIndex, color, bufferPtr);
    qlVertex3f(endX, adjustedY, startZ, voxel->colorIndex, color, bufferPtr);
    qlVertex3f(endX, adjustedY, endZ, voxel->colorIndex, color, bufferPtr);
    qlVertex3f(startX, adjustedY, startZ, voxel->colorIndex, color, bufferPtr);
}

void rvx_emit_voxelf(Voxelf* voxelf,
                     Color4* color,
                     uint8_t edgeWidth,
                     uint8_t edgeSpacing,
                     uint8_t edgeHeight,
                     uint8_t edgeDir,
                     uint8_t alignSide,
                     float** bufferPtr)
{
    const float adjustedY = voxelf->y;
    const float startX    = voxelf->sx;
    const float startZ    = voxelf->sz;
    const float endX      = voxelf->ex;
    const float endZ      = voxelf->ez;

    qlVertex7f(startX,
               adjustedY,
               startZ,
               voxelf->colorIndex,
               color,
               edgeWidth,
               edgeSpacing,
               edgeHeight,
               (alignSide & ALIGN_BOTTOM ? ALIGN_BOTTOM : 0) | (alignSide & ALIGN_LEFT ? edgeDir : 0),
               bufferPtr);
    qlVertex7f(endX,
               adjustedY,
               endZ,
               voxelf->colorIndex,
               color,
               edgeWidth,
               edgeSpacing,
               edgeHeight,
               (alignSide & ALIGN_TOP ? ALIGN_TOP : 0) | (alignSide & ALIGN_RIGHT ? edgeDir : 0),
               bufferPtr);
    qlVertex7f(startX,
               adjustedY,
               endZ,
               voxelf->colorIndex,
               color,
               edgeWidth,
               edgeSpacing,
               edgeHeight,
               (alignSide & ALIGN_TOP ? ALIGN_TOP : 0) | (alignSide & ALIGN_LEFT ? edgeDir : 0),
               bufferPtr);
    qlVertex7f(endX,
               adjustedY,
               startZ,
               voxelf->colorIndex,
               color,
               edgeWidth,
               edgeSpacing,
               edgeHeight,
               (alignSide & ALIGN_BOTTOM ? ALIGN_BOTTOM : 0) | (alignSide & ALIGN_RIGHT ? edgeDir : 0),
               bufferPtr);
    qlVertex7f(endX,
               adjustedY,
               endZ,
               voxelf->colorIndex,
               color,
               edgeWidth,
               edgeSpacing,
               edgeHeight,
               (alignSide & ALIGN_TOP ? ALIGN_TOP : 0) | (alignSide & ALIGN_RIGHT ? edgeDir : 0),
               bufferPtr);
    qlVertex7f(startX,
               adjustedY,
               startZ,
               voxelf->colorIndex,
               color,
               edgeWidth,
               edgeSpacing,
               edgeHeight,
               (alignSide & ALIGN_BOTTOM ? ALIGN_BOTTOM : 0) | (alignSide & ALIGN_LEFT ? edgeDir : 0),
               bufferPtr);
}

int rvx_get_edge_length(RVX_EDGE* edge)
{
    // how many voxels? two per Y (edge and inner)
    int numVoxels = 0;
    for(int y = edge->sy; y <= edge->ey; y += edge->spacing)
    {
        numVoxels += 4;
    }
    return numVoxels * RVX_EDGE_LENGTH;
}

void rvx_update_edge_buffer(RVX_EDGE* edge, float** bufferPtr, Color4 palette[256])
{
    // emit voxels
    float x = (edge->sx + edge->ex) / 2.0f;
    for(int y = edge->sy; y <= edge->ey; y += edge->spacing)
    {
        // for each line
        uint8_t ew = edge->edge_width;
        uint8_t cw = edge->ex - edge->sx + 1 - ew; // counter width

        Voxelf topLeftVoxel;
        topLeftVoxel.sx         = (float)edge->sx;
        topLeftVoxel.y          = y;
        topLeftVoxel.sz         = edge->ez + 1.0f * (1 - edge->edge_height); //edge->sz;
        topLeftVoxel.ez         = edge->ez + 1.0f; // def height
        topLeftVoxel.colorIndex = edge->top_left_col;

        Voxelf topRightVoxel;
        topRightVoxel.ex         = (float)(edge->ex + 1); // def width
        topRightVoxel.y          = y;
        topRightVoxel.sz         = edge->ez + 1.0f * (1 - edge->edge_height); //edge->sz;
        topRightVoxel.ez         = edge->ez + 1.0f; // def height
        topRightVoxel.colorIndex = edge->top_right_col;

        Voxelf bottomLeftVoxel;
        bottomLeftVoxel.sx         = (float)edge->sx;
        bottomLeftVoxel.y          = y;
        bottomLeftVoxel.sz         = edge->sz;
        bottomLeftVoxel.ez         = edge->ez + 1.0f * (1 - edge->edge_height); //edge->ez + 1; // def height
        bottomLeftVoxel.colorIndex = edge->bottom_left_col;

        Voxelf bottomRightVoxel;
        bottomRightVoxel.ex         = (float)(edge->ex + 1); // def width
        bottomRightVoxel.y          = y;
        bottomRightVoxel.sz         = edge->sz;
        bottomRightVoxel.ez         = edge->ez + 1.0f * (1 - edge->edge_height); //edge->ez + 1; // def height
        bottomRightVoxel.colorIndex = edge->bottom_right_col;

        if(edge->edge_dir == -1)
        {
            // left voxel is the edge
            topLeftVoxel.ex     = (float)(edge->sx + ew);
            topRightVoxel.sx    = (float)(edge->sx + ew);
            bottomLeftVoxel.ex  = (float)(edge->sx + ew);
            bottomRightVoxel.sx = (float)(edge->sx + ew);
        }
        else if(edge->edge_dir == 1)
        {
            // right voxel is the edge
            topLeftVoxel.ex     = (float)(edge->sx + cw);
            topRightVoxel.sx    = (float)(edge->sx + cw);
            bottomLeftVoxel.ex  = (float)(edge->sx + cw);
            bottomRightVoxel.sx = (float)(edge->sx + cw);
        }

        if(palette[edge->top_left_col].a != 0)
            rvx_emit_voxelf(&topLeftVoxel,
                            palette + edge->top_left_col,
                            ew,
                            edge->spacing,
                            edge->edge_height,
                            edge->edge_dir == -1 ? ALIGN_RIGHT : ALIGN_LEFT,
                            ALIGN_BOTTOM | ALIGN_RIGHT,
                            bufferPtr);
        if(palette[edge->top_right_col].a != 0)
            rvx_emit_voxelf(&topRightVoxel,
                            palette + edge->top_right_col,
                            ew,
                            edge->spacing,
                            edge->edge_height,
                            edge->edge_dir == -1 ? ALIGN_RIGHT : ALIGN_LEFT,
                            ALIGN_BOTTOM | ALIGN_LEFT,
                            bufferPtr);
        if(palette[edge->bottom_left_col].a != 0)
            rvx_emit_voxelf(&bottomLeftVoxel,
                            palette + edge->bottom_left_col,
                            ew,
                            edge->spacing,
                            edge->edge_height,
                            edge->edge_dir == -1 ? ALIGN_RIGHT : ALIGN_LEFT,
                            ALIGN_TOP | ALIGN_RIGHT,
                            bufferPtr);
        if(palette[edge->bottom_right_col].a != 0)
            rvx_emit_voxelf(&bottomRightVoxel,
                            palette + edge->bottom_right_col,
                            ew,
                            edge->spacing,
                            edge->edge_height,
                            edge->edge_dir == -1 ? ALIGN_RIGHT : ALIGN_LEFT,
                            ALIGN_TOP | ALIGN_LEFT,
                            bufferPtr);
    }
}

void rvx_model_render_edges(RVX_RENDERER* renderer, RVX_MODEL* model)
{
    if(!model->bound)
    {
        rvx_model_bind(renderer, model);
    }

    // recalculate and render
    if(model->numEdges == 0)
        return;

    // draw edges
    glUseProgram(renderer->edgeShaderProgram);
    glBindVertexArray(model->edgeVAO);
    glDrawArrays(GL_TRIANGLES, 0, model->edgesLength);
    glUseProgram(renderer->rvxShaderProgram);
}

int rvx_compile_shader(const char** vertexShaderSource, const char** fragmentShaderSource)
{
    // vertex shader
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    // check for shader compile errors
    int  success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if(!success)
    {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        rvx_error("Shader compilation failed!\nMessage: %s", infoLog);
        exit(EXIT_FAILURE);
    }
    // fragment shader
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    // check for shader compile errors
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if(!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        rvx_error("Shader compilation failed!\nMessage: %s", infoLog);
        exit(EXIT_FAILURE);
    }
    // link shaders
    int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    // check for linking errors
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if(!success)
    {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        rvx_error("Shader compilation failed!\nMessage: %s", infoLog);
        exit(EXIT_FAILURE);
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

void rvx_error(const char* format_string, ...)
{
    char message[2048];

    if(format_string)
    {
        va_list args;
        va_start(args, format_string);
        vsnprintf(message, 2048, format_string, args);
        va_end(args);
    }
    else
    {
        snprintf(message, 2048, "ERROR");
    }

    printf("%s\n", message);
    FILE* logfile = fopen("error.log", "a");
    if(logfile != NULL)
    {
        fprintf(logfile, "%s\n", message);
        fclose(logfile);
    }
}
