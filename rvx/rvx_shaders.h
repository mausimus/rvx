/*
    RVX Graphics Library 
    (c) 2022 mausimus.github.io
    MIT License
*/

#ifndef RVX_SHADERS_H
#define RVX_SHADERS_H

#ifdef __cplusplus
extern "C"
{
#endif

extern const char* s_shader_names[];
extern const char** rvx_get_shader_source(const char* backend, const char* shader);
extern void rvx_set_shader_source(const char* backend, const char* shader, const char* source);

#ifdef __cplusplus
}
#endif

#endif
