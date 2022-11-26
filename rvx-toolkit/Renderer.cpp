/*
    RVX Toolkit
    (c) 2022 mausimus.github.io
    MIT License
*/

#include "Renderer.h"
#include "VOXLoader.h"

#include "include/raylib/rlgl.h"
#include "include/raylib/raymath.h"

namespace rvx
{

Renderer::Renderer(const Scene& scene) : m_scene(scene) { }

void Renderer::Load()
{
    m_rvx = rvx_renderer_init(rvx_backend_gl, 0);
    Resize();
}

void Renderer::Resize()
{
    if(m_resolution[0] != m_renderTexture.texture.width || m_resolution[1] != m_renderTexture.texture.height)
    {
        if(m_renderTexture.id)
            UnloadRenderTexture(m_renderTexture);

        m_renderTexture     = LoadRenderTexture(m_resolution[0], m_resolution[1]);
        m_renderRect.x      = 0;
        m_renderRect.y      = 0;
        m_renderRect.width  = (float)m_resolution[0];
        m_renderRect.height = (float)-m_resolution[1];
        SetTextureFilter(m_renderTexture.texture, TEXTURE_FILTER);
    }
}

void Renderer::Unload()
{
    rvx_renderer_free(m_rvx);
    UnloadRenderTexture(m_renderTexture);
}

void Renderer::Update()
{
    if(m_rebuildRequired)
    {
        Rebuild();
        return;
    }

    glBindVertexArray(m_scene.m_model->VAO);
    PopulateBuffers();
    glBindBuffer(GL_ARRAY_BUFFER, m_scene.m_model->VBO);
    glBufferData(GL_ARRAY_BUFFER, m_scene.m_model->bufferSize, m_scene.m_model->buffer, GL_DYNAMIC_DRAW);
}

void Renderer::PopulateBuffers()
{
    int modelVoxels = 0;
    for(const auto& ar : m_scene.m_areas)
    {
        modelVoxels += ar.m_voxels.size();
    }

    std::vector<Voxel> voxels;
    for(const auto& ar : m_scene.m_areas)
    {
        std::copy(ar.m_voxels.begin(), ar.m_voxels.end(), std::back_inserter(voxels));
    }
    memcpy(&m_scene.m_model->params, &m_scene.m_params, sizeof(SceneParams));

    // rebuild edges
    if(m_scene.m_model->numEdges != m_scene.m_edges.size())
    {
        if(m_scene.m_model->edges)
            free(m_scene.m_model->edges);

        if(m_scene.m_edges.size())
            m_scene.m_model->edges = (RVX_EDGE*)malloc(m_scene.m_edges.size() * sizeof(RVX_EDGE));
        else
            m_scene.m_model->edges = nullptr;

        m_scene.m_model->numEdges = m_scene.m_edges.size();
    }

    for(int en = 0; en < m_scene.m_edges.size(); en++)
    {
        RVX_EDGE*   re       = m_scene.m_model->edges + en;
        const Edge& se       = m_scene.m_edges[en];
        re->area_no          = se.area_no;
        re->edge_dir         = se.edge_dir;
        re->sx               = se.sx;
        re->ex               = se.ex;
        re->sy               = se.sy;
        re->ey               = se.ey;
        re->sz               = se.sz;
        re->ez               = se.ez;
        re->edge_width       = se.edge_width;
        re->edge_height      = se.edge_height;
        re->spacing          = se.spacing;
        re->top_left_col     = se.top_left_col;
        re->top_right_col    = se.top_right_col;
        re->bottom_left_col  = se.bottom_left_col;
        re->bottom_right_col = se.bottom_right_col;
    }

    rvx_model_populate_buffer(m_scene.m_model,
                              voxels.data(),
                              modelVoxels,
                              reinterpret_cast<Color4*>(const_cast<Color*>(m_scene.m_palette.data())));
}

void Renderer::DeleteBuffers()
{
    rvx_model_unbind(m_scene.m_model);
}

void Renderer::Rebuild()
{
    m_rebuildRequired = false;
    DeleteBuffers();

    int numVoxels = 0;
    for(const auto& ar : m_scene.m_areas)
    {
        numVoxels += (int)ar.m_voxels.size();
    }

    if(numVoxels == 0)
        return;

    numVoxels++;
    int numVertices = numVoxels * 6;

    m_scene.m_model->buffer     = new float[numVertices * 6];
    m_scene.m_model->bufferSize = numVertices * 6 * sizeof(float);
    m_scene.m_model->numVoxels  = numVoxels;

    PopulateBuffers();

    rvx_model_bind(m_rvx, m_scene.m_model);
}

void Renderer::Render()
{
    if(m_rebuildRequired)
        Rebuild();

    BeginTextureMode(m_renderTexture);

    m_rvx->camX = m_scene.cam_x;
    m_rvx->camY = m_scene.cam_y;

    glClearColor(0, 0, 0, 1);
    rvx_renderer_begin(m_rvx);
    rvx_renderer_view(m_rvx, const_cast<SceneParams*>(&m_scene.m_params));
    rvx_model_render(m_rvx, m_scene.m_model, 0);
    rvx_model_render_edges(m_rvx, m_scene.m_model);
    rvx_renderer_end(m_rvx);

    EndTextureMode();
}

} // namespace rvx