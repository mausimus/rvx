/*
    RVX Toolkit
    (c) 2022 mausimus.github.io
    MIT License
*/

#pragma once

#include "stdafx.h"

#include "Model.h"
#include "Renderer.h"

namespace rvx
{

class Viewer
{
protected:
    void LoadScene(const std::filesystem::path& scenePath);
    void ImportVOX(const char* fileName, bool optimize);
    void CheckSceneReload();
    void DrawUI();
    void RecalculateTarget(Rectangle viewportRect);
    void Reset();

    Renderer    m_renderer;
    std::string m_screenshot;
    ViewerScene m_scene;
    int         m_frameCounter = 0;
    int         m_mouseX       = 0;
    int         m_mouseY       = 0;
    double      m_screenTime   = 0;
    bool        m_guiVisible   = true;
    bool        m_autoReload   = true;
    bool        m_sRGB         = false;
    long        m_lastModTime  = 0;
    long        m_lastSecond   = 0;
    std::string m_exportPath;
    Rectangle   m_targetRect;
    Texture2D   m_overlay;

public:
    bool m_renderResized;
    bool m_windowResized;
    bool m_dialogPaused;

    Viewer();
    void Load();
    void Tick(float deltaTime, float totalTime);
    void Draw(Rectangle viewportRect, bool viewportResized);
    void Unload();
};

} // namespace rvx