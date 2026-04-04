#include "Core/LayerStack.h"

#include "Core/Application.h"
#include "Core/Events/Event.h"
#include "Core/Log.h"

#include <algorithm>
#include <exception>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>

namespace Life
{
    LayerStack::LayerStack(Application& application)
        : m_Application(&application)
    {
    }

    Application& LayerStack::GetApplication()
    {
        return RequireApplication();
    }

    const Application& LayerStack::GetApplication() const
    {
        return RequireApplication();
    }

    void LayerStack::PushLayer(LayerRef layer)
    {
        InsertLayer(std::move(layer), false);
    }

    void LayerStack::PushOverlay(LayerRef overlay)
    {
        InsertLayer(std::move(overlay), true);
    }

    bool LayerStack::PopLayer(const LayerRef& layer)
    {
        if (layer == nullptr)
            return false;

        auto iterator = FindIterator(layer);
        if (iterator == m_Layers.end() || IsOverlayIterator(iterator))
            return false;

        return RemoveAt(iterator);
    }

    bool LayerStack::PopOverlay(const LayerRef& overlay)
    {
        if (overlay == nullptr)
            return false;

        auto iterator = FindIterator(overlay);
        if (iterator == m_Layers.end() || !IsOverlayIterator(iterator))
            return false;

        return RemoveAt(iterator);
    }

    bool LayerStack::Remove(const LayerRef& layer)
    {
        if (layer == nullptr)
            return false;

        const auto iterator = FindIterator(layer);
        if (iterator == m_Layers.end())
            return false;

        return RemoveAt(iterator);
    }

    bool LayerStack::Remove(std::string_view debugName)
    {
        const auto iterator = FindIterator(debugName);
        if (iterator == m_Layers.end())
            return false;

        return RemoveAt(iterator);
    }

    void LayerStack::Clear()
    {
        std::exception_ptr failure;
        for (auto iterator = m_Layers.rbegin(); iterator != m_Layers.rend(); ++iterator)
        {
            if (*iterator == nullptr)
                continue;

            try
            {
                DetachLayer(*(*iterator));
            }
            catch (...)
            {
                if (failure == nullptr)
                    failure = std::current_exception();
            }
        }

        m_Layers.clear();
        m_LayerInsertIndex = 0;

        if (failure != nullptr)
            std::rethrow_exception(failure);
    }

    LayerRef LayerStack::Find(std::string_view debugName) const
    {
        const auto iterator = FindIterator(debugName);
        return iterator != m_Layers.end() ? *iterator : nullptr;
    }

    bool LayerStack::Contains(const LayerRef& layer) const
    {
        return layer != nullptr && FindIterator(layer) != m_Layers.end();
    }

    bool LayerStack::Contains(std::string_view debugName) const
    {
        return FindIterator(debugName) != m_Layers.end();
    }

    void LayerStack::OnUpdate(float timestep)
    {
        for (const LayerRef& layer : m_Layers)
        {
            if (layer == nullptr || !layer->IsEnabled())
                continue;

            layer->OnUpdate(timestep);
        }
    }

    void LayerStack::OnRender()
    {
        for (const LayerRef& layer : m_Layers)
        {
            if (layer == nullptr || !layer->IsEnabled())
                continue;

            layer->OnRender();
        }
    }

    void LayerStack::OnEvent(Event& event)
    {
        for (auto iterator = m_Layers.rbegin(); iterator != m_Layers.rend(); ++iterator)
        {
            const LayerRef& layer = *iterator;
            if (layer == nullptr || !layer->IsEnabled())
                continue;

            layer->OnEvent(event);
            if (event.IsPropagationStopped())
                break;
        }
    }

    void LayerStack::InsertLayer(LayerRef layer, bool overlay)
    {
        if (layer == nullptr)
            throw std::invalid_argument("LayerStack cannot insert a null layer.");

        RequireApplication();

        if (Contains(layer))
            throw std::logic_error("LayerStack cannot insert the same layer instance more than once.");

        auto iterator = overlay
            ? m_Layers.insert(m_Layers.end(), std::move(layer))
            : m_Layers.insert(m_Layers.begin() + static_cast<std::ptrdiff_t>(m_LayerInsertIndex), std::move(layer));

        if (!overlay)
            ++m_LayerInsertIndex;

        try
        {
            AttachLayer(*(*iterator), overlay);
        }
        catch (...)
        {
            if (!overlay)
                --m_LayerInsertIndex;

            m_Layers.erase(iterator);
            throw;
        }
    }

    bool LayerStack::RemoveAt(LayerContainer::iterator iterator)
    {
        if (iterator == m_Layers.end() || *iterator == nullptr)
            return false;

        const bool overlay = IsOverlayIterator(iterator);
        const std::string debugName = (*iterator)->GetDebugName();
        DetachLayer(*(*iterator));
        m_Layers.erase(iterator);
        if (!overlay)
            --m_LayerInsertIndex;

        LOG_CORE_INFO("Removed {} '{}'", overlay ? "overlay" : "layer", debugName);
        return true;
    }

    bool LayerStack::IsOverlayIterator(LayerContainer::const_iterator iterator) const
    {
        return static_cast<std::size_t>(std::distance(m_Layers.cbegin(), iterator)) >= m_LayerInsertIndex;
    }

    void LayerStack::AttachLayer(Layer& layer, bool overlay)
    {
        layer.Bind(RequireApplication(), overlay);
        try
        {
            layer.OnAttach();
        }
        catch (...)
        {
            layer.Unbind();
            throw;
        }

        LOG_CORE_INFO("Attached {} '{}'", overlay ? "overlay" : "layer", layer.GetDebugName());
    }

    void LayerStack::DetachLayer(Layer& layer)
    {
        const std::string debugName = layer.GetDebugName();
        const bool overlay = layer.IsOverlay();
        try
        {
            layer.OnDetach();
        }
        catch (...)
        {
            layer.Unbind();
            throw;
        }

        layer.Unbind();
        LOG_CORE_INFO("Detached {} '{}'", overlay ? "overlay" : "layer", debugName);
    }

    Application& LayerStack::RequireApplication()
    {
        if (m_Application == nullptr)
            throw std::logic_error("LayerStack is not bound to an Application.");

        return *m_Application;
    }

    const Application& LayerStack::RequireApplication() const
    {
        if (m_Application == nullptr)
            throw std::logic_error("LayerStack is not bound to an Application.");

        return *m_Application;
    }

    LayerStack::LayerContainer::iterator LayerStack::FindIterator(const LayerRef& layer)
    {
        return std::find(m_Layers.begin(), m_Layers.end(), layer);
    }

    LayerStack::LayerContainer::const_iterator LayerStack::FindIterator(const LayerRef& layer) const
    {
        return std::find(m_Layers.cbegin(), m_Layers.cend(), layer);
    }

    LayerStack::LayerContainer::iterator LayerStack::FindIterator(std::string_view debugName)
    {
        return std::find_if(m_Layers.begin(), m_Layers.end(),
            [debugName](const LayerRef& layer)
            {
                return layer != nullptr && layer->GetDebugName() == debugName;
            });
    }

    LayerStack::LayerContainer::const_iterator LayerStack::FindIterator(std::string_view debugName) const
    {
        return std::find_if(m_Layers.cbegin(), m_Layers.cend(),
            [debugName](const LayerRef& layer)
            {
                return layer != nullptr && layer->GetDebugName() == debugName;
            });
    }
}
