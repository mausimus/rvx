/*
    RVX Toolkit
    (c) 2022 mausimus.github.io
    MIT License
*/

#include "Viewer.h"
#include "VOXLoader.h"
#include "include/raylib/rlgl.h"
#include "include/nfd/nfd.h"

namespace rvx
{

float _totalTime = 0;

Viewer::Viewer() : m_scene(), m_renderer(m_scene), m_frameCounter {}, m_mouseX {}, m_mouseY {}, m_autoReload(true) { }

void Viewer::Load()
{
    m_scene.m_updated = &m_renderer.m_rebuildRequired;

    m_renderer.Load();
    m_scene.Resize();
    Reset();
}

void Viewer::Unload()
{
    m_renderer.Unload();
}

void Viewer::Tick(float deltaTime, float totalTime)
{
    _totalTime += deltaTime;

    m_frameCounter++;
    m_mouseX = GetMouseX();
    m_mouseY = GetMouseY();

    bool secondPassed = false;
    auto second       = (long)totalTime;
    if(second != m_lastSecond)
    {
        m_lastSecond = second;
        secondPassed = true;
    }

    if(IsKeyPressed(KEY_TAB))
    {
        m_guiVisible = !m_guiVisible;
    }

    if(m_autoReload && secondPassed)
    {
        CheckSceneReload();
    }

    int vx = 0;
    int vy = 0;
    int vz = 0;

    if(IsKeyDown(KEY_UP))
        vy++;
    if(IsKeyDown(KEY_DOWN))
        vy--;
    if(IsKeyDown(KEY_LEFT))
        vx--;
    if(IsKeyDown(KEY_RIGHT))
        vx++;
    if(IsKeyDown(KEY_PAGE_UP))
        vz++;
    if(IsKeyDown(KEY_PAGE_DOWN))
        vz--;

    float dx = vx * m_scene.m_params.MOVING_SPEED * deltaTime;
    float dy = vy * m_scene.m_params.MOVING_SPEED * deltaTime;
    float dz = vz * m_scene.m_params.MOVING_SPEED * deltaTime;

    m_scene.m_params.CAM_HEIGHT += dz;
    m_scene.m_params.TARGET_POS[0] += dx;
    m_scene.m_params.TARGET_POS[1] += dy;
    m_scene.cam_x += dx;
    m_scene.cam_y += dy;
}

void Viewer::RecalculateTarget(Rectangle viewportRect)
{
    auto gcd     = std::gcd(m_renderer.m_resolution[0], m_renderer.m_resolution[1]);
    auto aspectV = m_renderer.m_resolution[0] / gcd;
    auto aspectH = m_renderer.m_resolution[1] / gcd;

    m_targetRect.width  = viewportRect.width;
    m_targetRect.height = viewportRect.height;
    if(m_targetRect.height * aspectV < m_targetRect.width * aspectH)
    {
        m_targetRect.width = (m_targetRect.height * aspectV) / aspectH;
    }
    else
    {
        m_targetRect.height = (m_targetRect.width * aspectH) / aspectV;
    }
    m_targetRect.x = (viewportRect.width - m_targetRect.width); // align right
    m_targetRect.y = (viewportRect.height - m_targetRect.height) / 2;
}

void Viewer::Draw(Rectangle viewportRect, bool viewportResized)
{
    m_dialogPaused  = false;
    m_windowResized = false;
    m_renderResized = false;

    m_renderer.Render();

    // resized window
    if(viewportResized)
        RecalculateTarget(viewportRect);

    ClearBackground(DARKGRAY);
    DrawTexturePro(m_renderer.m_renderTexture.texture, m_renderer.m_renderRect, m_targetRect, Vector2(), 0, WHITE);

    if(m_overlay.id)
        DrawTexturePro(m_overlay,
                       Rectangle {0, 0, (float)m_overlay.width, (float)m_overlay.height},
                       Rectangle {m_targetRect.x,
                                  m_targetRect.y,
                                  m_targetRect.width,
                                  m_targetRect.width * (float)m_overlay.height / (float)m_overlay.width}, // retain ratio
                       Vector2(),
                       0,
                       Color {255, 127, 127, 127});
    rlDrawRenderBatchActive();

    if(m_screenshot.size())
    {
        auto image = LoadImageFromTexture(m_renderer.m_renderTexture.texture);
        ImageFlipVertical(&image);
        ExportImage(image, m_screenshot.c_str());
        m_screenshot.clear();
    }

    DrawUI();
};

static void HelpMarker(const char* desc)
{
    ImGui::TextDisabled("(?)");
    if(ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void Viewer::LoadScene(const std::filesystem::path& scenePath)
{
    m_scene           = ViewerScene();
    m_scene.m_updated = &m_renderer.m_rebuildRequired;
    m_scene.Load(scenePath);
    Reset();
    if(m_scene.m_voxFileName.size())
        ImportVOX(m_scene.AssetPath(m_scene.m_voxFileName).string().c_str(), true);
}

void Viewer::DrawUI()
{
    if(!m_guiVisible)
        return;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Appearing, ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(440.0f, 600.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowBgAlpha(0.75f);

    if(ImGui::Begin("RVX Toolkit"))
    {
        if(ImGui::TreeNode("Scene"))
        {
            if(ImGui::Button("New"))
            {
                m_scene           = ViewerScene();
                m_scene.m_updated = &m_renderer.m_rebuildRequired;
                m_scene.Resize();
                Reset();
            }

            ImGui::SameLine();

            if(ImGui::Button("Load..."))
            {
                nfdchar_t* outPath = NULL;
                m_dialogPaused     = true;
                auto samplesPath   = std::filesystem::current_path() / "samples";
                if(NFD_OpenDialog("rvx", samplesPath.string().c_str(), &outPath) == NFD_OKAY)
                {
                    LoadScene(outPath);
                }
            }

            ImGui::SameLine();

            if(!m_scene.m_scenePath.empty())
            {
                if(ImGui::Button("Save"))
                {
                    m_scene.Save(m_scene.m_scenePath);
                }
            }
            else
            {
                if(ImGui::Button("Save as..."))
                {
                    nfdchar_t* outPath = NULL;
                    if(NFD_SaveDialog("rvx", NULL, &outPath) == NFD_OKAY)
                    {
                        m_scene.Save(std::filesystem::path(outPath));
                    }
                }
            }

            ImGui::InputTextWithHint("Name", "Scene name", &m_scene.m_name);

            std::string scenePath = m_scene.m_scenePath.empty() ? "<new>" : m_scene.m_scenePath.string();
            ImGui::InputText("Path", &scenePath, ImGuiInputTextFlags_ReadOnly);

            if(m_scene.m_voxFileName.empty())
            {
                // no VOX - construct mode
                ImGui::Text("Blank Construct");
                if(ImGui::InputInt3("Size", m_scene.m_size, ImGuiInputTextFlags_EnterReturnsTrue))
                {
                    m_scene.Resize();
                }
                ImGui::SameLine();
                HelpMarker("Enter to apply");

                if(ImGui::Button("Generate VOX"))
                {
                    if(m_scene.m_scenePath.empty())
                    {
                        ImGui::OpenPopup("Save notification");
                    }
                    else
                    {
                        m_scene.GenerateVOX();
                        m_scene._importPath.clear();
                    }
                }
                ImGui::SameLine();
                HelpMarker("Generate .vox file using current\r\nconstruct for editing in MagicaVoxel");

                ImVec2 center = ImGui::GetMainViewport()->GetCenter();
                ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
                if(ImGui::BeginPopupModal("Save notification", NULL, ImGuiWindowFlags_AlwaysAutoResize))
                {
                    ImGui::Text("Please save the scene first.\n\n");
                    ImGui::Separator();
                    ImGui::SetItemDefaultFocus();
                    if(ImGui::Button("OK", ImVec2(120, 0)))
                    {
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }

                if(ImGui::Button("Import VOX..."))
                {
                    nfdchar_t* outPath = NULL;
                    m_dialogPaused     = true;
                    if(NFD_OpenDialog("vox", m_scene.m_scenePath.string().c_str(), &outPath) == NFD_OKAY)
                    {
                        ImportVOX(outPath, true);
                        m_scene._importPath   = outPath;
                        m_scene.m_voxFileName = m_scene._importPath.filename().string();
                    }
                }
                ImGui::SameLine();
                HelpMarker("Load an existing .vox file into the scene");
            }
            else
            {
                //ImGui::Text("VOX File Path");
                //ImGui::TextUnformatted(m_scene.AssetRelativePath(m_scene.m_voxFileName).string().c_str());
                std::string voxPath = m_scene.AssetRelativePath(m_scene.m_voxFileName).string();
                ImGui::InputText("VOX", &voxPath, ImGuiInputTextFlags_ReadOnly);
                ImGui::Checkbox("Auto-reload", &m_autoReload);
                ImGui::SameLine();
                HelpMarker("Monitor .vox file for changes");

                if(ImGui::Button("Regenerate VOX"))
                    ImGui::OpenPopup("Regenerate VOX?");

                ImVec2 center = ImGui::GetMainViewport()->GetCenter();
                ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
                if(ImGui::BeginPopupModal("Regenerate VOX?", NULL, ImGuiWindowFlags_AlwaysAutoResize))
                {
                    ImGui::Text("This will overwrite existing .vox file with a new blank construct.\nAre you sure?\n\n");
                    ImGui::Separator();

                    if(ImGui::Button("Yes", ImVec2(120, 0)))
                    {
                        m_scene.m_voxFileName.clear();
                        m_scene.Resize();
                        m_scene.GenerateVOX();
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SetItemDefaultFocus();
                    ImGui::SameLine();
                    if(ImGui::Button("No", ImVec2(120, 0)))
                    {
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }
            }

            if(ImGui::Button("Rebuild"))
            {
                m_scene.MarkUpdated();
            }

            if(ImGui::Button("Overlay image..."))
            {
                nfdchar_t* outPath = NULL;
                m_dialogPaused     = true;
                if(NFD_OpenDialog("png;jpg;gif", m_scene.m_scenePath.string().c_str(), &outPath) == NFD_OKAY)
                {
                    if(m_overlay.id)
                        UnloadTexture(m_overlay);
                    m_overlay = LoadTexture(outPath);
                }
            }
            ImGui::SameLine();
            HelpMarker("Overlay a reference image for aligment");
            if(m_overlay.id && ImGui::Button("Clear"))
            {
                UnloadTexture(m_overlay);
                m_overlay.id = 0;
            }
            ImGui::TreePop();
        }

        if(ImGui::TreeNode("View"))
        {
            ImGui::SliderFloat("FOV", &m_scene.m_params.CAM_FOV, 1, 180, "%.0f deg", ImGuiSliderFlags_AlwaysClamp);
            ImGui::SameLine();
            HelpMarker("Field-of-view");

            ImGui::SliderFloat("X Offset", &m_scene.m_params.OFFSET_X, -2.0f, 2.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            ImGui::SliderFloat("Y Offset", &m_scene.m_params.OFFSET_Y, -2.0f, 2.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            ImGui::SliderFloat("X Shearing", &m_scene.m_params.SHEARING_X, -2.0f, 2.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            ImGui::SliderFloat("Y Shearing", &m_scene.m_params.SHEARING_Y, -2.0f, 2.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            ImGui::SliderFloat("Camera Distance", &m_scene.m_params.CAM_DIST, 0, 600.0f, "%.1f");
            ImGui::SliderFloat("Camera Height", &m_scene.m_params.CAM_HEIGHT, 0, 600.0f, "%.1f");
            ImGui::SameLine();
            HelpMarker("PgUp/Down");
            ImGui::SliderFloat("Moving Speed", &m_scene.m_params.MOVING_SPEED, 1.0f, 100.0f, "%.0f");
            ImGui::InputFloat2("Target Pos", m_scene.m_params.TARGET_POS, "%.2f", ImGuiInputTextFlags_ReadOnly);
            ImGui::SameLine();
            HelpMarker("Arrow keys");

            ImGui::TreePop();
        }

        if(ImGui::TreeNode("Export"))
        {
            if(ImGui::Button("Screenshot"))
            {
                nfdchar_t* outPath = NULL;
                if(NFD_SaveDialog("png", NULL, &outPath) == NFD_OKAY)
                {
                    std::filesystem::path screenshotPath(outPath);
                    if(screenshotPath.extension().empty())
                    {
                        screenshotPath = screenshotPath.replace_extension(".png");
                    }
                    m_screenshot = screenshotPath.string();
                }
            }

            //HelpMarker("Convert colors to sRGB when exporting");

            if(ImGui::Button("Wavefront .obj"))
            {
                if(m_scene.m_scenePath.empty())
                {
                    ImGui::OpenPopup("Save notification");
                }
                else
                {
                    m_exportPath = m_scene.ExportOBJ(m_sRGB);
                    ImGui::OpenPopup("Exported notification");
                }
            }

            ImGui::SameLine();
            ImGui::Checkbox("sRGB", &m_sRGB);

            ImVec2 center = ImGui::GetMainViewport()->GetCenter();
            ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            if(ImGui::BeginPopupModal("Save notification", NULL, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::Text("Please save the scene first.\n\n");
                ImGui::Separator();
                ImGui::SetItemDefaultFocus();
                if(ImGui::Button("OK", ImVec2(120, 0)))
                {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            if(ImGui::BeginPopupModal("Exported notification", NULL, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::Text("Exported as\n\n%s\n\n", m_exportPath.c_str());
                ImGui::Separator();
                ImGui::SetItemDefaultFocus();
                if(ImGui::Button("OK", ImVec2(120, 0)))
                {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            ImGui::SameLine();
            HelpMarker("Export .obj file for importing into 3D engines");
            ImGui::TreePop();
        }

        if(ImGui::TreeNode("Render"))
        {
            if(ImGui::InputInt2("Resolution", m_renderer.m_resolution, ImGuiInputTextFlags_EnterReturnsTrue))
            {
                m_renderer.m_resolution[0] = std::clamp(m_renderer.m_resolution[0], 64, 3840);
                m_renderer.m_resolution[1] = std::clamp(m_renderer.m_resolution[1], 64, 2160);
                m_renderer.Resize();
                m_renderResized = true;
            }
            auto        gcd = std::gcd(m_renderer.m_resolution[0], m_renderer.m_resolution[1]);
            std::string aspectRatio =
                std::to_string(m_renderer.m_resolution[0] / gcd) + ":" + std::to_string(m_renderer.m_resolution[1] / gcd);
            ImGui::InputText("Aspect Ratio", &aspectRatio, ImGuiInputTextFlags_ReadOnly);

            ImGui::TextUnformatted("Scale Window");
            if(ImGui::Button("200%"))
            {
                SetWindowSize(m_renderer.m_resolution[0] * 2, m_renderer.m_resolution[1] * 2);
                m_windowResized = true;
            }
            ImGui::SameLine();
            if(ImGui::Button("100%"))
            {
                SetWindowSize(m_renderer.m_resolution[0] * 1, m_renderer.m_resolution[1] * 1);
                m_windowResized = true;
            }
            ImGui::SameLine();
            if(ImGui::Button("50%"))
            {
                SetWindowSize(m_renderer.m_resolution[0] / 2, m_renderer.m_resolution[1] / 2);
                m_windowResized = true;
            }
            ImGui::TreePop();
        }
    }

    if(ImGui::TreeNode("Help"))
    {
        ImGui::TextUnformatted("Tab to toggle UI");
        ImGui::TextUnformatted("Arrow keys to move camera X/Y");
        ImGui::TextUnformatted("PgUp/Down to move camera up/down");
        ImGui::TreePop();
    }

    if(ImGui::TreeNode("About"))
    {
        ImGui::TextUnformatted(WINDOW_TITLE);
        ImGui::TextUnformatted("(c) 2022 mausimus.github.io");
        ImGui::TextUnformatted("Free software under MIT License");
        ImGui::TreePop();
    }

    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Viewer::ImportVOX(const char* fileName, bool optimize)
{
    auto vox = VOXLoader::LoadVOX(fileName, false);
    VOXLoader::ImportVOX(vox, m_scene, optimize);
    ogt_vox_destroy_scene(vox);
    m_scene.MarkUpdated();
}

void Viewer::CheckSceneReload()
{
    if(!m_scene.m_voxFileName.empty())
    {
        auto voxPath = m_scene._importPath.empty() ? m_scene.AssetPath(m_scene.m_voxFileName).string() : m_scene._importPath.string();
        auto modTime = GetFileModTime(voxPath.c_str());
        if(modTime > m_lastModTime)
        {
            auto vox = VOXLoader::LoadVOX(voxPath.c_str(), true);
            VOXLoader::ImportVOX(vox, m_scene, true);
            ogt_vox_destroy_scene(vox);
            m_scene.MarkUpdated();
            m_lastModTime = modTime;
        }
    }
}

void Viewer::Reset()
{
    m_scene.cam_x = m_scene.m_params.TARGET_POS[0];
    m_scene.cam_y = m_scene.m_params.TARGET_POS[1];
}

} // namespace rvx