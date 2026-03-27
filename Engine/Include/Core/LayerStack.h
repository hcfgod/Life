#pragma once

#include "Core/Layer.h"

#include <cstddef>
#include <string_view>
#include <vector>

namespace Life
{
    class Application;
    class Event;

    class LayerStack final
    {
    public:
        LayerStack() = default;
        explicit LayerStack(Application& application);
        ~LayerStack() = default;

        LayerStack(const LayerStack&) = delete;
        LayerStack& operator=(const LayerStack&) = delete;
        LayerStack(LayerStack&&) = delete;
        LayerStack& operator=(LayerStack&&) = delete;

        bool IsBound() const noexcept { return m_Application != nullptr; }
        void BindApplication(Application& application) noexcept { m_Application = &application; }
        Application& GetApplication();
        const Application& GetApplication() const;

        void PushLayer(LayerRef layer);
        void PushOverlay(LayerRef overlay);
        bool PopLayer(const LayerRef& layer);
        bool PopOverlay(const LayerRef& overlay);
        bool Remove(const LayerRef& layer);
        bool Remove(std::string_view debugName);
        void Clear();

        LayerRef Find(std::string_view debugName) const;
        bool Contains(const LayerRef& layer) const;
        bool Contains(std::string_view debugName) const;

        std::size_t GetCount() const noexcept { return m_Layers.size(); }
        std::size_t GetRegularLayerCount() const noexcept { return m_LayerInsertIndex; }
        std::size_t GetOverlayCount() const noexcept { return m_Layers.size() - m_LayerInsertIndex; }

        void OnUpdate(float timestep);
        void OnEvent(Event& event);

        auto begin() noexcept { return m_Layers.begin(); }
        auto end() noexcept { return m_Layers.end(); }
        auto begin() const noexcept { return m_Layers.begin(); }
        auto end() const noexcept { return m_Layers.end(); }
        auto cbegin() const noexcept { return m_Layers.cbegin(); }
        auto cend() const noexcept { return m_Layers.cend(); }
        auto rbegin() noexcept { return m_Layers.rbegin(); }
        auto rend() noexcept { return m_Layers.rend(); }
        auto rbegin() const noexcept { return m_Layers.rbegin(); }
        auto rend() const noexcept { return m_Layers.rend(); }
        auto crbegin() const noexcept { return m_Layers.crbegin(); }
        auto crend() const noexcept { return m_Layers.crend(); }

    private:
        using LayerContainer = std::vector<LayerRef>;

        void InsertLayer(LayerRef layer, bool overlay);
        bool RemoveAt(LayerContainer::iterator iterator);
        bool IsOverlayIterator(LayerContainer::const_iterator iterator) const;
        void AttachLayer(Layer& layer, bool overlay);
        void DetachLayer(Layer& layer);
        Application& RequireApplication();
        const Application& RequireApplication() const;
        LayerContainer::iterator FindIterator(const LayerRef& layer);
        LayerContainer::const_iterator FindIterator(const LayerRef& layer) const;
        LayerContainer::iterator FindIterator(std::string_view debugName);
        LayerContainer::const_iterator FindIterator(std::string_view debugName) const;

        Application* m_Application = nullptr;
        LayerContainer m_Layers;
        std::size_t m_LayerInsertIndex = 0;
    };
}
