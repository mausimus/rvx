/*
    RVX Toolkit
    (c) 2022 mausimus.github.io
    MIT License
*/

#include "Scene.h"
#include "include/raylib/raymath.h"

#include "Scene.h"
#include "VOXLoader.h"

#include "include/iniparser.hpp"

#define OBJ_SCALE 0.01f

namespace rvx
{

ViewerScene::ViewerScene()
{
    m_palette.resize(256);

    m_params.CAM_FOV       = 108.0f;
    m_params.CAM_DIST      = 106.0f;
    m_params.CAM_HEIGHT    = 46.0f;
    m_params.TARGET_POS[0] = 127.0f;
    m_params.TARGET_POS[1] = 35.0f;
    m_params.TARGET_POS[2] = 0.0f;
    m_params.SHEARING_X    = 0.0f;
    m_params.SHEARING_Y    = -0.6f;
    m_params.OFFSET_X      = 0.0f;
    m_params.OFFSET_Y      = 0.1f;
    m_params.MOVING_SPEED  = 35.0f;

    m_model = rvx_model_new();
}

void ViewerScene::Load(const std::filesystem::path& scenePath)
{
    INI::File sceneFile;
    sceneFile.Load(scenePath.string());

    auto format = sceneFile.GetValue("format").AsString();
    if(format != "rvx100")
        return;

    m_scenePath = scenePath;

    auto sceneSection = sceneFile.GetSection("scene");
    m_name            = sceneSection->GetValue("name").AsString();
    m_assetsFolder    = sceneSection->GetValue("assets_folder").AsString();
    m_voxFileName     = sceneSection->GetValue("vox_file").AsString();
    m_size[0]         = sceneSection->GetValue("size_x").AsInt();
    m_size[1]         = sceneSection->GetValue("size_y").AsInt();
    m_size[2]         = sceneSection->GetValue("size_z").AsInt();

    auto viewSection       = sceneFile.GetSection("view");
    m_params.CAM_FOV       = viewSection->GetValue("fov").AsT<float>();
    m_params.SHEARING_X    = viewSection->GetValue("shearing_x").AsT<float>();
    m_params.SHEARING_Y    = viewSection->GetValue("shearing_y").AsT<float>();
    m_params.OFFSET_X      = viewSection->GetValue("offset_x", "0").AsT<float>();
    m_params.OFFSET_Y      = viewSection->GetValue("offset_y", "0").AsT<float>();
    m_params.CAM_DIST      = viewSection->GetValue("cam_dist").AsT<float>();
    m_params.CAM_HEIGHT    = viewSection->GetValue("cam_height").AsT<float>();
    m_params.TARGET_POS[0] = viewSection->GetValue("target_pos_x").AsT<float>();
    m_params.TARGET_POS[1] = viewSection->GetValue("target_pos_y").AsT<float>();
    m_params.TARGET_POS[2] = viewSection->GetValue("target_pos_z").AsT<float>();

    MarkUpdated();
}

void ViewerScene::Save(const std::filesystem::path& scenePath)
{
    m_scenePath = scenePath;
    if(m_scenePath.extension().empty())
    {
        m_scenePath = m_scenePath.replace_extension(".rvx");
    }

    if(!m_assetsFolder.size())
    {
        // generate default assets folder name
        m_assetsFolder = SceneRoot() + "-assets";
        std::filesystem::create_directory(AssetPath(""));
    }

    INI::File sceneFile;

    sceneFile.SetValue("format", "rvx100");

    auto sceneSection = sceneFile.GetSection("scene");
    sceneSection->SetValue("name", m_name);
    sceneSection->SetValue("assets_folder", m_assetsFolder);
    sceneSection->SetValue("vox_file", m_voxFileName);
    sceneSection->SetValue("size_x", m_size[0]);
    sceneSection->SetValue("size_y", m_size[1]);
    sceneSection->SetValue("size_z", m_size[2]);

    auto viewSection = sceneFile.GetSection("view");
    viewSection->SetValue("fov", m_params.CAM_FOV);
    viewSection->SetValue("shearing_x", m_params.SHEARING_X);
    viewSection->SetValue("shearing_y", m_params.SHEARING_Y);
    viewSection->SetValue("offset_x", m_params.OFFSET_X);
    viewSection->SetValue("offset_y", m_params.OFFSET_Y);
    viewSection->SetValue("cam_dist", m_params.CAM_DIST);
    viewSection->SetValue("cam_height", m_params.CAM_HEIGHT);
    viewSection->SetValue("target_pos_x", m_params.TARGET_POS[0]);
    viewSection->SetValue("target_pos_y", m_params.TARGET_POS[1]);
    viewSection->SetValue("target_pos_z", m_params.TARGET_POS[2]);

    if(!_importPath.empty())
    {
        auto assetsPath = AssetPath(m_voxFileName);
        if(_importPath != assetsPath)
            std::filesystem::copy_file(_importPath, assetsPath);
        _importPath.clear();
    }

    sceneFile.Save(m_scenePath.string());
}

void ViewerScene::Resize()
{
    if(!m_isConstruct)
        return;

    m_size[0] = std::clamp(m_size[0], 1, c_maxSizeX);
    m_size[1] = std::clamp(m_size[1], 1, c_maxSizeY);
    m_size[2] = std::clamp(m_size[2], 1, c_maxSizeZ);

    auto vox = VOXLoader::GenerateBox(m_size[0], m_size[1], m_size[2], m_roof, 4);
    VOXLoader::ImportVOX(vox, *this, true);
    ogt_vox_destroy_scene(vox);

    MarkUpdated();
}

void ViewerScene::GenerateVOX()
{
    if(m_scenePath.empty() || !m_isConstruct)
        return;

    if(m_voxFileName.empty())
    {
        m_voxFileName = SceneRoot() + ".vox";
    }

    auto vox = VOXLoader::GenerateBox(m_size[0], m_size[1], m_size[2], m_roof, 4);
    VOXLoader::ExportVOX(vox, AssetPath(m_voxFileName).string().c_str());
    VOXLoader::ImportVOX(vox, *this, true);
    ogt_vox_destroy_scene(vox);

    Save(m_scenePath);
    MarkUpdated();
}

std::filesystem::path ViewerScene::AssetRelativePath(const std::string& assetName)
{
    return std::filesystem::path(m_assetsFolder).append(assetName);
}

std::filesystem::path ViewerScene::AssetPath(const std::string& assetName)
{
    // same directory as .rvx with assets subfolder
    return m_scenePath.parent_path().append(m_assetsFolder).append(assetName);
}

std::string ViewerScene::SceneRoot()
{
    return m_scenePath.filename().replace_extension().string();
}

void ViewerScene::MarkUpdated()
{
    *m_updated = true;
}

static void VoxelOut(FILE* out, const Voxel& v)
{
    fprintf(out, "v %f %f %f\n", -OBJ_SCALE * v.sx, OBJ_SCALE * v.sz, OBJ_SCALE * v.y);
    fprintf(out, "v %f %f %f\n", -OBJ_SCALE * (v.ex + 1), OBJ_SCALE * v.sz, OBJ_SCALE * v.y);
    fprintf(out, "v %f %f %f\n", -OBJ_SCALE * v.sx, OBJ_SCALE * (v.ez + 1), OBJ_SCALE * v.y);
    fprintf(out, "v %f %f %f\n", -OBJ_SCALE * (v.ex + 1), OBJ_SCALE * (v.ez + 1), OBJ_SCALE * v.y);
}

static void FaceOut(FILE* out, int col, int f)
{
    fprintf(out, "usemtl c%d\n", col);
    fprintf(out, "f %d %d %d\n", 1 + (f * 4), 1 + (f * 4 + 1), 1 + (f * 4 + 2));
    fprintf(out, "f %d %d %d\n", 1 + (f * 4 + 3), 1 + (f * 4 + 2), 1 + (f * 4 + 1));
}

std::string ViewerScene::ExportOBJ(const std::string& objName, bool sRGB)
{
    auto objPath = (objName + ".obj");

    FILE* out = fopen(objPath.c_str(), "wt");
    fprintf(out, "o %s\n", SceneRoot().c_str());
    fprintf(out, "mtllib %s.mtl\n", SceneRoot().c_str());
    for(const auto& a : m_areas)
    {
        for(const auto& v : a.m_voxels)
        {
            VoxelOut(out, v);
        }
    }

    int f = 0;
    for(const auto& a : m_areas)
    {
        for(const auto& v : a.m_voxels)
        {
            FaceOut(out, v.colorIndex, f++);
        }
    }
    fclose(out);

    FILE* mat = fopen((objName + ".mtl").c_str(), "wt");
    for(int c = 1; c < 256; c++)
    {
        fprintf(out, "newmtl c%d\n", c);
        if(sRGB)
            fprintf(out,
                    "Kd %f %f %f\n",
                    pow(m_palette[c].r / 255.0f, 2.2f),
                    pow(m_palette[c].g / 255.0f, 2.2f),
                    pow(m_palette[c].b / 255.0f, 2.2f));
        else
            fprintf(out, "Kd %f %f %f\n", m_palette[c].r / 255.0f, m_palette[c].g / 255.0f, m_palette[c].b / 255.0f);
    }
    fclose(mat);

    return objPath;
}

std::string ViewerScene::ExportOBJ(bool sRGB)
{
    if(m_scenePath.empty() || !m_isConstruct)
        return std::string();

    return ExportOBJ(AssetPath(SceneRoot()).string().c_str(), sRGB);
}

uint8_t FindColor(Color color, Color* palette, int colorCount)
{
    for(int i = 0; i < colorCount; i++)
    {
        if(palette[i].r == color.r && palette[i].g == color.g && palette[i].b == color.b && palette[i].a == color.a)
            return i;
    }
    return 0;
}

bool sameColor(Color* c1, Color* c2)
{
    return c1->r == c2->r && c1->g == c2->g && c1->b == c2->b && c1->a == c2->a;
}

} // namespace rvx