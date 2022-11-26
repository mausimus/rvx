/*
    RVX Toolkit
    (c) 2022 mausimus.github.io
    MIT License
*/

#include "stdafx.h"
#include "Viewer.h"

#include "include/glfw/glfw3.h"

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

constexpr int screenWidth  = 320 * 4;
constexpr int screenHeight = 168 * 4;

bool      firstFrame = true;
Rectangle viewportRect {0, 0, 0, 0};
rvx::Viewer    viewer;
double    totalTime;

void UpdateRenderSize()
{
    viewportRect.width  = (float)GetScreenWidth();
    viewportRect.height = (float)GetScreenHeight();
}

void UpdateDrawFrame()
{
#ifdef FULLSCREEN_KEY
    if(IsKeyPressed(FULLSCREEN_KEY))
    {
        ToggleFullscreen();
        UpdateRenderSize();
    }
#endif

    auto currentTime = GetTime();
    viewer.Tick((float)(currentTime - totalTime), (float)currentTime);
    totalTime = currentTime;

    BeginDrawing();
    {
        auto resizeRequired = IsWindowResized() || firstFrame || viewer.m_renderResized || viewer.m_windowResized;
        if(resizeRequired)
        {
            UpdateRenderSize();
        }

        viewer.Draw(viewportRect, resizeRequired);
        if(viewer.m_dialogPaused)
        {
            // skip this frame's time
            totalTime = GetTime();
        }

        firstFrame = false;
    }
    EndDrawing();
}

int main()
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(screenWidth, screenHeight, WINDOW_TITLE);

    GLFWwindow* glfwWindow = glfwGetCurrentContext();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = NULL;
    ImGui_ImplGlfw_InitForOpenGL(glfwWindow, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    SetWindowMinSize(640, 400);
    UpdateRenderSize();

    viewer.Load();
    totalTime = GetTime();

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else
    SetTargetFPS(TARGET_FPS);
    while(!WindowShouldClose())
    {
        UpdateDrawFrame();
    }
#endif

    viewer.Unload();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    CloseWindow();

    return 0;
}

