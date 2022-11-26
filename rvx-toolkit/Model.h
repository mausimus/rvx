/*
    RVX Toolkit
    (c) 2022 mausimus.github.io
    MIT License
*/

#pragma once

#if defined(PLATFORM_DESKTOP)
#define GLSL_VERSION 330
#else // PLATFORM_RPI, PLATFORM_ANDROID, PLATFORM_WEB
#define GLSL_VERSION 100
#endif

#define RENDER_SHADER_PATH_VS "resources/shaders/glsl%i/render.vs"
#define RENDER_SHADER_PATH_FS "resources/shaders/glsl%i/render.fs"

#define WINDOW_TITLE "RVX Toolkit v1.0"
#define FULLSCREEN_KEY KEY_F11
#define TARGET_FPS 60
#define TEXTURE_FILTER TEXTURE_FILTER_BILINEAR
