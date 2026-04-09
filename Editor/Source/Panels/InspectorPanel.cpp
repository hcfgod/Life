#include "Editor/Panels/InspectorPanel.h"

#include "Editor/Camera/EditorCameraTool.h"
#include "Editor/EditorServices.h"

#if __has_include(<imgui.h>)
#include <imgui.h>
#endif

namespace EditorApp
{
    void InspectorPanel::Render(bool& isOpen, const EditorServices& services, const EditorCameraTool& cameraTool) const
    {
#if __has_include(<imgui.h>)
        if (!isOpen)
            return;

        if (ImGui::Begin("Inspector", &isOpen))
        {
            if (services.CameraManager)
            {
                if (auto editorCamera = cameraTool.TryGetCamera(services.CameraManager->get()))
                {
                    const Life::Camera& camera = editorCamera->get();
                    const glm::vec3& position = camera.GetPosition();
                    ImGui::Text("Camera: %s", camera.GetName().c_str());
                    ImGui::Text("Projection: %s", camera.GetProjectionType() == Life::ProjectionType::Perspective ? "Perspective" : "Orthographic");
                    ImGui::Text("Position: %.2f %.2f %.2f", position.x, position.y, position.z);
                    ImGui::Text("Aspect Ratio: %.3f", camera.GetAspectRatio());
                }
            }
        }
        ImGui::End();
#else
        (void)isOpen;
        (void)services;
        (void)cameraTool;
#endif
    }
}
