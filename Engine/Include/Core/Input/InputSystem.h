#pragma once

#include "Core/Error.h"
#include "Core/Input/InputAction.h"
#include "Core/Memory.h"

#include <array>
#include <bitset>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

union SDL_Event;
struct SDL_Gamepad;

namespace Life
{
    class InputSystem final
    {
    public:
        static constexpr int MaxGamepads = 4;

        InputSystem() = default;
        ~InputSystem();

        InputSystem(const InputSystem&) = delete;
        InputSystem& operator=(const InputSystem&) = delete;
        InputSystem(InputSystem&&) = delete;
        InputSystem& operator=(InputSystem&&) = delete;

        void OnSdlEvent(const SDL_Event& event);
        void UpdateActions();
        void SyncConnectedGamepads();
        void EndFrame();
        void SetKeyboardInputBlocked(bool blocked);
        void SetMouseInputBlocked(bool blocked);
        bool IsKeyboardInputBlocked() const noexcept { return m_KeyboardInputBlocked; }
        bool IsMouseInputBlocked() const noexcept { return m_MouseInputBlocked; }

        void SetProjectActionAsset(Ref<InputActionAsset> asset);
        const Ref<InputActionAsset>& GetProjectActionAsset() const { return m_ProjectActionAsset; }
        Ref<InputActionAsset> GetActiveActionAsset() const;
        Result<void> LoadProjectActionAssetFromFile(const std::string& path);
        Result<void> SaveProjectActionAssetToFile(const std::string& path) const;
        const std::string& GetProjectActionAssetPath() const { return m_ProjectActionAssetPath; }

        void PushActionAssetOverride(Ref<InputActionAsset> asset);
        bool PopActionAssetOverride();
        bool PopActionAssetOverride(const Ref<InputActionAsset>& expectedTop);

        bool IsKeyDown(KeyCode keyCode) const;
        bool WasKeyPressedThisFrame(KeyCode keyCode) const;
        bool WasKeyReleasedThisFrame(KeyCode keyCode) const;

        bool IsMouseButtonDown(MouseButtonCode button) const;
        bool WasMouseButtonPressedThisFrame(MouseButtonCode button) const;
        bool WasMouseButtonReleasedThisFrame(MouseButtonCode button) const;

        const InputVector2& GetMousePosition() const { return m_MousePosition; }
        const InputVector2& GetMouseDelta() const { return m_MouseInputBlocked ? m_ZeroVector : m_MouseDelta; }
        const InputVector2& GetMouseWheelDelta() const { return m_MouseInputBlocked ? m_ZeroVector : m_MouseWheelDelta; }
        void NotifyMouseWarped();

        int GetGamepadCount() const;
        bool HasGamepad(int playerIndex = 0) const;
        bool IsGamepadButtonDown(GamepadButtonCode button) const { return IsGamepadButtonDown(0, button); }
        bool IsGamepadButtonDown(int playerIndex, GamepadButtonCode button) const;
        bool WasGamepadButtonPressedThisFrame(GamepadButtonCode button) const { return WasGamepadButtonPressedThisFrame(0, button); }
        bool WasGamepadButtonPressedThisFrame(int playerIndex, GamepadButtonCode button) const;
        bool WasGamepadButtonReleasedThisFrame(GamepadButtonCode button) const { return WasGamepadButtonReleasedThisFrame(0, button); }
        bool WasGamepadButtonReleasedThisFrame(int playerIndex, GamepadButtonCode button) const;
        float GetGamepadAxis(GamepadAxisCode axis) const { return GetGamepadAxis(0, axis); }
        float GetGamepadAxis(int playerIndex, GamepadAxisCode axis) const;

        void SetRebindingSession(Ref<InputRebinding> session) { m_RebindingSession = std::move(session); }
        Ref<InputRebinding> GetRebindingSession() const { return m_RebindingSession; }

        bool IsActionPressed(std::string_view mapName, std::string_view actionName, float deadzone = 0.0001f) const;
        bool WasActionStartedThisFrame(std::string_view mapName, std::string_view actionName) const;
        bool WasActionPerformedThisFrame(std::string_view mapName, std::string_view actionName) const;
        bool WasActionCanceledThisFrame(std::string_view mapName, std::string_view actionName) const;
        bool ReadActionButton(std::string_view mapName, std::string_view actionName) const;
        float ReadActionAxis1D(std::string_view mapName, std::string_view actionName) const;
        InputVector2 ReadActionAxis2D(std::string_view mapName, std::string_view actionName) const;
        bool HasAction(std::string_view mapName, std::string_view actionName) const;

    private:
        static constexpr std::size_t MaxMouseButtons = InputCodeLimits::MouseButtonCount;

        struct PerGamepadState
        {
            GamepadId Id = 0;
            SDL_Gamepad* Gamepad = nullptr;
            std::array<int16_t, InputCodeLimits::GamepadAxisCount> Axis{};
            std::array<uint8_t, InputCodeLimits::GamepadButtonCount> ButtonDown{};
            std::array<uint8_t, InputCodeLimits::GamepadButtonCount> ButtonPressedThisFrame{};
            std::array<uint8_t, InputCodeLimits::GamepadButtonCount> ButtonReleasedThisFrame{};
        };

        void CloseAllGamepads() noexcept;
        void OnKey(KeyCode keyCode, bool down, bool repeat);
        void OnMouseMotion(const InputVector2& position, const InputVector2& delta);
        void OnMouseButton(MouseButtonCode button, bool down);
        void OnMouseWheel(float offsetX, float offsetY);
        void OnGamepadAdded(GamepadId which);
        void OnGamepadRemoved(GamepadId which);
        void OnGamepadAxis(GamepadId which, GamepadAxisCode axis, int16_t value);
        void OnGamepadButton(GamepadId which, GamepadButtonCode button, bool down);
        const InputAction* FindAction(std::string_view mapName, std::string_view actionName, bool warnIfMissing) const;
        void WarnMissingActionOnce(std::string_view mapName, std::string_view actionName) const;

        static std::size_t FindSlotByGamepadId(std::array<PerGamepadState, MaxGamepads>& gamepads, GamepadId id);
        static std::size_t FindFreeGamepadSlot(std::array<PerGamepadState, MaxGamepads>& gamepads);

        std::bitset<InputCodeLimits::KeyCount> m_KeyDown;
        std::bitset<InputCodeLimits::KeyCount> m_KeyPressedThisFrame;
        std::bitset<InputCodeLimits::KeyCount> m_KeyReleasedThisFrame;

        std::array<uint8_t, MaxMouseButtons> m_MouseDown{};
        std::array<uint8_t, MaxMouseButtons> m_MousePressedThisFrame{};
        std::array<uint8_t, MaxMouseButtons> m_MouseReleasedThisFrame{};

        InputVector2 m_ZeroVector{};
        InputVector2 m_MousePosition{};
        InputVector2 m_MouseDelta{};
        InputVector2 m_MouseWheelDelta{};
        uint8_t m_PendingSyntheticMouseMotionEvents = 0;
        bool m_KeyboardInputBlocked = false;
        bool m_MouseInputBlocked = false;

        std::array<PerGamepadState, MaxGamepads> m_Gamepads{};

        Ref<InputActionAsset> m_ProjectActionAsset;
        std::vector<Ref<InputActionAsset>> m_ActionAssetOverrideStack;
        std::string m_ProjectActionAssetPath;
        mutable std::unordered_set<std::string> m_WarnedMissingActions;
        Ref<InputRebinding> m_RebindingSession;
    };
}
