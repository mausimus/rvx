/*
    RVX Toolkit
    (c) 2022 mausimus.github.io
    MIT License
*/

#include "Model.h"
#include "Scene.h"

namespace rvx
{

class Renderer
{
public:
    Renderer(const Scene& scene);
    void Load();
    void Render();

    void Update(); // update vertices (params change)
    void Rebuild(); // rebuild vertices (scene/length change)
    void Resize(); // resize viewport
    void Unload();

    Rectangle       m_renderRect;
    RenderTexture2D m_renderTexture;
    int             m_resolution[2]   = {320 * 8, 168 * 8};
    volatile bool   m_rebuildRequired = false;

private:
    RVX_RENDERER* m_rvx;
    const Scene& m_scene;

    void PopulateBuffers();
    void DeleteBuffers();
};

} // namespace rvx