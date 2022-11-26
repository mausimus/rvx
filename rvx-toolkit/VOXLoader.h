/*
    RVX Toolkit
    (c) 2022 mausimus.github.io
    MIT License
*/

#pragma once

#include "stdafx.h"
#include "Scene.h"
#include "include/raylib/raymath.h"

namespace rvx
{

class VOXLoader
{
public:
    static void                 ImportVOX(const ogt_vox_scene* vox, Scene& scene, bool optimize);
    static const ogt_vox_scene* LoadVOX(const char* fileName, bool retry);
    static void                 ExportVOX(const ogt_vox_scene* vox, const char* fileName);
    static const ogt_vox_scene* GenerateBox(int x, int y, int z, bool roof, int margin);
    static std::vector<Vector2> GenerateRoomSlices(int fw, int fh, int nw, int nh, int d);

private:
    static bool AllVoxelsSameColor(
        uint8_t* data, int sx, int ex, int y, int z, const ogt_vox_model* model, Matrix* inverse, uint8_t c, const int sizes[3]);
    static void RemoveVoxels(uint8_t* data, int sx, int ex, int y, int z, const ogt_vox_model* model, Matrix* inverse, const int sizes[3]);
    static int  GetOffset(int x, int y, int z, const ogt_vox_model* model, Matrix* inverse, const int sizes[3]);
    static void Transform(int& x, int& y, int& z, Matrix* transform);
};

} // namespace rvx