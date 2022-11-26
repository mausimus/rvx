/*
    RVX Graphics Library 
    (c) 2022 mausimus.github.io
    MIT License
*/

#ifndef RVX_COMMON_H
#define RVX_COMMON_H

#include <stdint.h>

#ifdef EMSCRIPTEN
#include <GLES3/gl3.h>
#else
#ifndef __glad_h_
#include "include/glad/glad.h"
#endif
#endif

struct scene_params_struct
{
    float CAM_FOV;
    float CAM_DIST;
    float CAM_HEIGHT;
    float TARGET_POS[3];
    float SHEARING_X;
    float SHEARING_Y;
    float OFFSET_X;
    float OFFSET_Y;
    float MOVING_SPEED;
};

typedef struct scene_params_struct SceneParams;

struct voxel_struct
{
#ifdef __cplusplus
    voxel_struct(uint8_t color, int16_t sx, int16_t ex, int16_t y, int16_t sz, int16_t ez, int8_t ext) :
        colorIndex(color), sx(sx), ex(ex), y(y), sz(sz), ez(ez), ext(ext)
    { }
#endif // __cplusplus

    uint8_t colorIndex;
    int16_t sx;
    int16_t ex;
    int16_t y;
    int16_t sz;
    int16_t ez;
    int8_t  ext;
};

typedef struct voxel_struct Voxel;

struct voxelf_struct
{
    uint8_t colorIndex;
    float   sx;
    float   ex;
    int16_t y;
    int16_t sz;
    int16_t ez;
};

typedef struct voxelf_struct Voxelf;

struct rvx_area_struct
{
    int no;
    int start;
    int len;
    int sx;
    int sy;
    int sz;
};

typedef struct rvx_area_struct RVX_AREA;

struct color_struct
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
};

typedef struct color_struct Color4;

struct rvx_edge_struct
{
    int no;
    int area_no; // to check if we need to draw
    // cube containing the edge
    int     sx;
    int     ex;
    int     sy;
    int     ey;
    int     sz;
    int     ez;
    int     edge_dir; // -1 left edge; +1 right edge
    int     edge_width;
    int     edge_height;
    int     spacing; // spacing, to make blockier edges
    uint8_t top_left_col;
    uint8_t top_right_col;
    uint8_t bottom_left_col;
    uint8_t bottom_right_col;
};

typedef struct rvx_edge_struct RVX_EDGE;

struct rvx_model_struct
{
    int         loaded;
    int         bound;
    SceneParams params;
    int         numVoxels;
    float*      buffer;
    int         bufferSize;
    GLuint      VAO;
    GLuint      VBO;
    int         numAreas;
    RVX_AREA*   areas;
    int         numEdges;
    RVX_EDGE*   edges;
    float*      edgeBuffer;
    int         edgeBufferSize;
    GLuint      edgeVAO;
    GLuint      edgeVBO;
    int         modelLength;
    int         edgesLength;
};

typedef struct rvx_model_struct RVX_MODEL;

struct rvx_renderer_struct
{
    // bindings
    GLuint rvxShaderProgram;
    GLuint viewLocation;
    GLuint alphaLocation;

    // setup
    int   aspectW;
    int   aspectH;
    float cullFar;
    float cullNear;

    // camera position
    float camX;
    float camY;

    // edges
    GLuint edgeShaderProgram;
    GLuint edgeViewLocation;
    GLuint edgeAlphaLocation;

    int renderWidth;
    int renderHeight;

    uint32_t palette[256];

    float       viewMatrix[16];
    const char* backend;
};

typedef struct rvx_renderer_struct RVX_RENDERER;

extern const char* rvx_backend_gles3;
extern const char* rvx_backend_gl;

typedef int (*control_func)(int x, int y);

#ifdef __cplusplus
extern "C"
{
#endif

    extern RVX_RENDERER* rvx_renderer_init(const char* backend, float paletteMix);
    extern void          rvx_renderer_free(RVX_RENDERER* renderer);
    extern void          rvx_renderer_begin(RVX_RENDERER* renderer);
    extern void          rvx_renderer_end(RVX_RENDERER* renderer);

    extern void rvx_renderer_init_edges(RVX_RENDERER* renderer);

    extern void rvx_renderer_view(RVX_RENDERER* renderer, SceneParams* params);
    extern void rvx_renderer_translate(RVX_RENDERER* renderer, float deltaX, float deltaY, float deltaZ);
    extern void rvx_renderer_affine(RVX_RENDERER* renderer, float deltaX, float deltaY, float deltaZ, float scaleX, float scaleY, float scaleZ, int shadow);

    extern RVX_MODEL* rvx_model_new();
    extern void       rvx_model_free(RVX_MODEL* model);
    extern void       rvx_model_populate_buffer(RVX_MODEL* model, Voxel* voxels, int modelVoxels, Color4 palette[256]);
    extern void       rvx_model_bind(RVX_RENDERER* renderer, RVX_MODEL* model);
    extern void       rvx_model_render(RVX_RENDERER* renderer, RVX_MODEL* model, int area);
    extern void       rvx_model_render_edges(RVX_RENDERER* renderer, RVX_MODEL* model);
    extern void       rvx_model_unbind(RVX_MODEL* model);

    extern inline void qlVertex2f(float x, float y, float z, float** vertexPtr);
    extern inline void qlVertex4b(int is_back, float x, float y, float z, float** vertexPtr);
    extern inline void qlVertex3f(float x, float y, float z, uint8_t ci, Color4* c, float** vertexPtr);
    extern inline void qlVertex7f(float   x,
                                  float   y,
                                  float   z,
                                  uint8_t ci,
                                  Color4* c,
                                  uint8_t edgeWidth,
                                  uint8_t edgeSpacing,
                                  uint8_t edgeHeight,
                                  uint8_t voxelSide,
                                  float** vertexPtr);
    extern void        rvx_emit_voxel(Voxel* voxel, Color4* color, float** bufferPtr);
    extern void        rvx_emit_voxelf(Voxelf* voxelf,
                                       Color4* color,
                                       uint8_t edgeWidth,
                                       uint8_t edgeSpacing,
                                       uint8_t edgeHeight,
                                       uint8_t edgeDir,
                                       uint8_t alignSide,
                                       float** bufferPtr);

    extern int  rvx_get_edge_length(RVX_EDGE* edge);
    extern void rvx_update_edge_buffer(RVX_EDGE* edge, float** bufferPtr, Color4 palette[256]);

    extern int rvx_compile_shader(const char** vertexShaderSource, const char** fragmentShaderSource);

    extern void rvx_check_glerror(const char* function);
    extern void rvx_error(const char* format_string, ...);

#ifdef __cplusplus
}
#endif

#endif
