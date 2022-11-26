/*
    RVX Toolkit
    (c) 2022 mausimus.github.io
    MIT License
*/

#pragma once

#include "stdafx.h"
#include "rvx/rvx.h"

constexpr int c_maxSizeX = 1024;
constexpr int c_maxSizeY = 256;
constexpr int c_maxSizeZ = 256;

namespace rvx
{

class Area
{
public:
    int                m_no;
    std::string        m_name;
    int                m_sx; // space coords of 0,0
    int                m_sy;
    int                m_sz;
    std::vector<Voxel> m_voxels;
};

class Edge
{
public:
    int m_no;

    int area_no; // to check if we need to draw
    int sx;
    int ex;
    int sy;
    int ey;
    int sz;
    int ez;

    int     edge_dir; // -1 left edge; +1 right edge
    int     edge_width;
    int     edge_height;
    int     spacing; // spacing, to make blockier edges
    uint8_t top_left_col;
    uint8_t top_right_col;
    uint8_t bottom_left_col;
    uint8_t bottom_right_col;
};

class Scene
{
public:
    float cam_x;
    float cam_y;

    SceneParams        m_params;
    std::vector<Area>  m_areas;
    std::vector<Color> m_palette;
    std::vector<Edge>  m_edges;

    float m_deltaX = 0;

    RVX_MODEL* m_model;
};

class ViewerScene : public Scene
{
public:
    ViewerScene();
    void                  Load(const std::filesystem::path& scenePath);
    void                  GenerateVOX();
    void                  Save(const std::filesystem::path& scenePath);
    void                  Resize();
    std::filesystem::path AssetRelativePath(const std::string& assetName);
    std::filesystem::path AssetPath(const std::string& assetName);
    std::string           SceneRoot();
    void                  MarkUpdated();
    std::string           ExportOBJ(bool sRGB);
    std::string           ExportOBJ(const std::string& objName, bool sRGB);

    // construct
    bool                  m_isConstruct = true;
    int                   m_size[3]     = {288, 41, 132};
    bool                  m_roof        = true;
    std::string           m_name;
    std::filesystem::path m_scenePath;
    std::string           m_assetsFolder;
    std::string           m_voxFileName;
    std::filesystem::path _importPath; // if not imported yet
    volatile bool*        m_updated;
};

} // namespace rvx
