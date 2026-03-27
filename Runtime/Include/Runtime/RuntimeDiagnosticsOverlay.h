#pragma once

#include "Engine.h"

namespace RuntimeApp
{
    class RuntimeDiagnosticsOverlay final : public Life::Layer
    {
    public:
        RuntimeDiagnosticsOverlay();

    protected:
        void OnAttach() override;
        void OnDetach() override;
        void OnEvent(Life::Event& event) override;
    };
}
