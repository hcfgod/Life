#pragma once

#include "Core/Error.h"
#include "Core/Input/InputCodes.h"
#include "Core/Memory.h"

#include <glm/vec2.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

union SDL_Event;

namespace Life
{
    class InputSystem;

    using InputVector2 = glm::vec2;

    enum class InputActionValueType : uint8_t
    {
        Button = 0,
        Axis1D = 1,
        Axis2D = 2
    };

    enum class InputActionPhase : uint8_t
    {
        Disabled = 0,
        Waiting,
        Started,
        Performed,
        Canceled
    };

    class InputActionValue final
    {
    public:
        InputActionValue() = default;
        explicit InputActionValue(bool value) : m_Value(value) {}
        explicit InputActionValue(float value) : m_Value(value) {}
        explicit InputActionValue(const glm::vec2& value) : m_Value(value) {}

        InputActionValueType GetType() const { return m_Type; }

        bool AsButton() const;
        float AsAxis1D() const;
        glm::vec2 AsAxis2D() const;

        static InputActionValue Button(bool value);
        static InputActionValue Axis1D(float value);
        static InputActionValue Axis2D(const glm::vec2& value);

        bool IsActuated(float deadzone = 0.0001f) const;

    private:
        InputActionValueType m_Type = InputActionValueType::Button;
        std::variant<bool, float, InputVector2> m_Value{ false };
    };

    struct KeyboardButtonBinding
    {
        KeyCode Key = KeyCodes::Unknown;
    };

    struct MouseButtonBinding
    {
        MouseButtonCode Button = MouseButtons::None;
    };

    struct KeyboardAxis1DBinding
    {
        KeyCode Negative = KeyCodes::Unknown;
        KeyCode Positive = KeyCodes::Unknown;
        float NegativeScale = -1.0f;
        float PositiveScale = 1.0f;
    };

    struct KeyboardAxis2DBinding
    {
        KeyCode Up = KeyCodes::Unknown;
        KeyCode Down = KeyCodes::Unknown;
        KeyCode Left = KeyCodes::Unknown;
        KeyCode Right = KeyCodes::Unknown;
        float Scale = 1.0f;
    };

    struct MouseDeltaBinding
    {
        float Sensitivity = 1.0f;
        bool InvertY = false;
    };

    struct GamepadButtonBinding
    {
        GamepadButtonCode Button = GamepadButtons::Invalid;
        int PlayerIndex = 0;
    };

    struct GamepadAxis1DBinding
    {
        GamepadAxisCode Axis = GamepadAxes::Invalid;
        float Scale = 1.0f;
        float Deadzone = 0.15f;
        int PlayerIndex = 0;
    };

    struct GamepadAxis2DBinding
    {
        GamepadAxisCode XAxis = GamepadAxes::Invalid;
        GamepadAxisCode YAxis = GamepadAxes::Invalid;
        float Scale = 1.0f;
        float Deadzone = 0.15f;
        bool InvertY = false;
        int PlayerIndex = 0;
    };

    using InputBinding = std::variant<
        KeyboardButtonBinding,
        MouseButtonBinding,
        KeyboardAxis1DBinding,
        KeyboardAxis2DBinding,
        MouseDeltaBinding,
        GamepadButtonBinding,
        GamepadAxis1DBinding,
        GamepadAxis2DBinding>;

    class InputAction final
    {
    public:
        explicit InputAction(std::string name, InputActionValueType valueType);

        const std::string& GetName() const { return m_Name; }
        InputActionValueType GetValueType() const { return m_ValueType; }

        void AddBinding(InputBinding binding);
        const std::vector<InputBinding>& GetBindings() const { return m_Bindings; }
        std::size_t GetBindingCount() const { return m_Bindings.size(); }
        bool SetBinding(std::size_t index, InputBinding binding);
        void ClearBindings() { m_Bindings.clear(); }

        void SetEnabled(bool enabled);
        bool IsEnabled() const { return m_Enabled; }

        void Update(const InputSystem& input);

        InputActionPhase GetPhase() const { return m_Phase; }
        bool WasStartedThisFrame() const { return m_Phase == InputActionPhase::Started; }
        bool WasPerformedThisFrame() const { return m_Phase == InputActionPhase::Performed; }
        bool WasCanceledThisFrame() const { return m_Phase == InputActionPhase::Canceled; }

        bool IsPressed(float deadzone = 0.0001f) const { return m_Value.IsActuated(deadzone); }
        bool ReadButton() const { return m_Value.AsButton(); }
        float ReadAxis1D() const { return m_Value.AsAxis1D(); }
        InputVector2 ReadAxis2D() const { return m_Value.AsAxis2D(); }

        std::string DebugDump() const;

    private:
        InputActionValue EvaluateValue(const InputSystem& input) const;

        std::string m_Name;
        InputActionValueType m_ValueType = InputActionValueType::Button;
        std::vector<InputBinding> m_Bindings;
        bool m_Enabled = true;
        InputActionPhase m_Phase = InputActionPhase::Waiting;
        InputActionValue m_Value{};
        bool m_WasActuatedLastFrame = false;
    };

    class InputActionMap final
    {
    public:
        explicit InputActionMap(std::string name);

        const std::string& GetName() const { return m_Name; }

        void SetEnabled(bool enabled);
        bool IsEnabled() const { return m_Enabled; }

        InputAction& AddAction(const std::string& name, InputActionValueType valueType);
        InputAction* FindAction(std::string_view name);
        const InputAction* FindAction(std::string_view name) const;

        const std::vector<std::unique_ptr<InputAction>>& GetActions() const { return m_Actions; }

        void Update(const InputSystem& input);

        void ClearActions()
        {
            m_Actions.clear();
            m_ActionByName.clear();
        }

    private:
        std::string m_Name;
        bool m_Enabled = true;
        std::vector<std::unique_ptr<InputAction>> m_Actions;
        std::unordered_map<std::string, InputAction*> m_ActionByName;
    };

    class InputActionAsset final
    {
    public:
        InputActionAsset() = default;

        InputActionMap& AddMap(const std::string& name);
        InputActionMap* FindMap(std::string_view name);
        const InputActionMap* FindMap(std::string_view name) const;

        const std::vector<std::unique_ptr<InputActionMap>>& GetMaps() const { return m_Maps; }

        void Update(const InputSystem& input);

        void ClearMaps()
        {
            m_Maps.clear();
            m_MapByName.clear();
        }

    private:
        std::vector<std::unique_ptr<InputActionMap>> m_Maps;
        std::unordered_map<std::string, InputActionMap*> m_MapByName;
    };

    struct InputActionAssetLoadOptions
    {
        std::string DebugName;
    };

    class InputActionAssetSerializer final
    {
    public:
        static Result<Ref<InputActionAsset>> LoadFromFile(const std::string& path);
        static Result<void> LoadInto(InputActionAsset& outAsset, const std::string& path);
        static Result<void> LoadIntoFromString(InputActionAsset& outAsset, const std::string& jsonText, const InputActionAssetLoadOptions& options = {});
        static Result<void> SaveToFile(const InputActionAsset& asset, const std::string& path);
    };

    class InputRebinding final
    {
    public:
        enum class DeviceFilter : uint8_t
        {
            KeyboardMouse = 0,
            Gamepad = 1,
            Any = 2
        };

        struct Request
        {
            Ref<InputActionAsset> Asset;
            std::string AssetPath;
            std::string MapName;
            std::string ActionName;
            std::size_t BindingIndex = 0;
            DeviceFilter Filter = DeviceFilter::Any;
            bool SaveToDisk = true;
        };

        using CompletionCallback = std::function<void(Result<InputBinding>)>;

        InputRebinding() = default;

        const Request& GetRequest() const { return m_Request; }
        bool IsActive() const { return m_Active; }

        void Start(Request request, CompletionCallback onComplete);
        void Cancel();
        bool TryConsumeEvent(const SDL_Event& event);

    private:
        bool AcceptsKeyboardMouse() const;
        bool AcceptsGamepad() const;
        void Complete(Result<InputBinding> result);

        Request m_Request{};
        CompletionCallback m_OnComplete;
        bool m_Active = false;
    };
}
