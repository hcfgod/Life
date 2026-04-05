#include "Core/Input/InputSystem.h"

#include "Core/Log.h"
#include "Platform/SDL/SDLInputCodes.h"

#include <SDL3/SDL.h>

#include <limits>

namespace Life
{
    namespace
    {
        bool TryConvertSdlGamepadId(SDL_JoystickID sdlGamepadId, GamepadId& outGamepadId)
        {
            constexpr SDL_JoystickID MaxGamepadId = static_cast<SDL_JoystickID>(std::numeric_limits<GamepadId>::max());
            if (sdlGamepadId > MaxGamepadId)
            {
                LOG_CORE_WARN("Ignoring SDL gamepad id {} because it exceeds the supported GamepadId range.", static_cast<unsigned int>(sdlGamepadId));
                return false;
            }

            outGamepadId = static_cast<GamepadId>(sdlGamepadId);
            return true;
        }
    }

    InputSystem::~InputSystem()
    {
        CloseAllGamepads();
    }

    void InputSystem::SetProjectActionAsset(Ref<InputActionAsset> asset)
    {
        m_ProjectActionAsset = std::move(asset);
        m_ProjectActionAssetPath.clear();
        m_WarnedMissingActions.clear();
    }

    Result<void> InputSystem::LoadProjectActionAssetFromFile(const std::string& path)
    {
        Result<Ref<InputActionAsset>> loadedAsset = InputActionAssetSerializer::LoadFromFile(path);
        if (loadedAsset.IsFailure())
            return Result<void>(loadedAsset.GetError());

        m_ProjectActionAsset = loadedAsset.GetValue();
        m_ProjectActionAssetPath = path;
        m_WarnedMissingActions.clear();
        return Result<void>();
    }

    Result<void> InputSystem::SaveProjectActionAssetToFile(const std::string& path) const
    {
        if (!m_ProjectActionAsset)
            return Result<void>(ErrorCode::InvalidState, "InputSystem does not have a project action asset to save.");

        return InputActionAssetSerializer::SaveToFile(*m_ProjectActionAsset, path);
    }

    Ref<InputActionAsset> InputSystem::GetActiveActionAsset() const
    {
        if (!m_ActionAssetOverrideStack.empty())
            return m_ActionAssetOverrideStack.back();
        return m_ProjectActionAsset;
    }

    void InputSystem::PushActionAssetOverride(Ref<InputActionAsset> asset)
    {
        if (!asset)
        {
            LOG_CORE_WARN("InputSystem::PushActionAssetOverride was called with a null asset.");
            return;
        }

        m_ActionAssetOverrideStack.push_back(std::move(asset));
        m_WarnedMissingActions.clear();
    }

    bool InputSystem::PopActionAssetOverride()
    {
        if (m_ActionAssetOverrideStack.empty())
            return false;

        m_ActionAssetOverrideStack.pop_back();
        m_WarnedMissingActions.clear();
        return true;
    }

    bool InputSystem::PopActionAssetOverride(const Ref<InputActionAsset>& expectedTop)
    {
        if (m_ActionAssetOverrideStack.empty())
            return false;

        if (expectedTop && m_ActionAssetOverrideStack.back() != expectedTop)
        {
            LOG_CORE_WARN("InputSystem::PopActionAssetOverride was called with a mismatched expected asset.");
            return false;
        }

        m_ActionAssetOverrideStack.pop_back();
        m_WarnedMissingActions.clear();
        return true;
    }

    void InputSystem::NotifyMouseWarped()
    {
        if (m_PendingSyntheticMouseMotionEvents < 8)
            ++m_PendingSyntheticMouseMotionEvents;
    }

    void InputSystem::SetKeyboardInputBlocked(bool blocked)
    {
        if (m_KeyboardInputBlocked == blocked)
            return;

        m_KeyboardInputBlocked = blocked;
        if (blocked)
        {
            m_KeyDown.reset();
            m_KeyPressedThisFrame.reset();
            m_KeyReleasedThisFrame.reset();
        }
    }

    void InputSystem::SetMouseInputBlocked(bool blocked)
    {
        if (m_MouseInputBlocked == blocked)
            return;

        m_MouseInputBlocked = blocked;
        if (blocked)
        {
            m_MouseDown.fill(0);
            m_MousePressedThisFrame.fill(0);
            m_MouseReleasedThisFrame.fill(0);
            m_MouseDelta = {};
            m_MouseWheelDelta = {};
        }
    }

    void InputSystem::OnSdlEvent(const SDL_Event& event)
    {
        if (m_RebindingSession && m_RebindingSession->IsActive() && m_RebindingSession->TryConsumeEvent(event))
            return;

        switch (event.type)
        {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            if (m_KeyboardInputBlocked)
                break;
            OnKey(ToKeyCode(event.key.scancode), event.key.down, event.key.repeat);
            break;

        case SDL_EVENT_MOUSE_MOTION:
            if (m_MouseInputBlocked)
            {
                m_MousePosition = { event.motion.x, event.motion.y };
                break;
            }
            OnMouseMotion({ event.motion.x, event.motion.y }, { event.motion.xrel, event.motion.yrel });
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (m_MouseInputBlocked)
                break;
            OnMouseButton(ToMouseButtonCode(event.button.button), event.button.down);
            break;

        case SDL_EVENT_MOUSE_WHEEL:
            if (m_MouseInputBlocked)
                break;
            OnMouseWheel(event.wheel.x, event.wheel.y);
            break;

        case SDL_EVENT_GAMEPAD_ADDED:
        {
            GamepadId gamepadId = 0;
            if (TryConvertSdlGamepadId(event.gdevice.which, gamepadId))
                OnGamepadAdded(gamepadId);
            break;
        }

        case SDL_EVENT_GAMEPAD_REMOVED:
        {
            GamepadId gamepadId = 0;
            if (TryConvertSdlGamepadId(event.gdevice.which, gamepadId))
                OnGamepadRemoved(gamepadId);
            break;
        }

        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        {
            GamepadId gamepadId = 0;
            if (TryConvertSdlGamepadId(event.gaxis.which, gamepadId))
                OnGamepadAxis(gamepadId, ToGamepadAxisCode(static_cast<SDL_GamepadAxis>(event.gaxis.axis)), event.gaxis.value);
            break;
        }

        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
        {
            GamepadId gamepadId = 0;
            if (TryConvertSdlGamepadId(event.gbutton.which, gamepadId))
                OnGamepadButton(gamepadId, ToGamepadButtonCode(static_cast<SDL_GamepadButton>(event.gbutton.button)), event.gbutton.down);
            break;
        }

        default:
            break;
        }
    }

    void InputSystem::UpdateActions()
    {
        if (Ref<InputActionAsset> asset = GetActiveActionAsset())
            asset->Update(*this);
    }

    void InputSystem::SyncConnectedGamepads()
    {
        const bool gamepadSubsystemInitialized = (SDL_WasInit(SDL_INIT_GAMEPAD) & SDL_INIT_GAMEPAD) != 0;
        if (!gamepadSubsystemInitialized)
            return;

        SDL_ClearError();
        int discoveredGamepadCount = 0;
        SDL_JoystickID* discoveredGamepads = SDL_GetGamepads(&discoveredGamepadCount);
        if (discoveredGamepads == nullptr || discoveredGamepadCount <= 0)
        {
            if (discoveredGamepads != nullptr)
                SDL_free(discoveredGamepads);
            return;
        }

        for (int index = 0; index < discoveredGamepadCount; ++index)
        {
            GamepadId gamepadId = 0;
            if (TryConvertSdlGamepadId(discoveredGamepads[index], gamepadId))
                OnGamepadAdded(gamepadId);
        }

        SDL_free(discoveredGamepads);
    }

    void InputSystem::EndFrame()
    {
        m_KeyPressedThisFrame.reset();
        m_KeyReleasedThisFrame.reset();
        m_MousePressedThisFrame.fill(0);
        m_MouseReleasedThisFrame.fill(0);
        m_MouseDelta = {};
        m_MouseWheelDelta = {};

        for (PerGamepadState& gamepad : m_Gamepads)
        {
            if (gamepad.Gamepad != nullptr)
            {
                gamepad.ButtonPressedThisFrame.fill(0);
                gamepad.ButtonReleasedThisFrame.fill(0);
            }
        }
    }

    bool InputSystem::IsKeyDown(KeyCode keyCode) const
    {
        if (m_KeyboardInputBlocked)
            return false;

        const uint16_t keyValue = keyCode.GetValue();
        if (keyValue == 0 || keyValue >= InputCodeLimits::KeyCount)
            return false;
        return m_KeyDown.test(static_cast<std::size_t>(keyValue));
    }

    bool InputSystem::WasKeyPressedThisFrame(KeyCode keyCode) const
    {
        if (m_KeyboardInputBlocked)
            return false;

        const uint16_t keyValue = keyCode.GetValue();
        if (keyValue == 0 || keyValue >= InputCodeLimits::KeyCount)
            return false;
        return m_KeyPressedThisFrame.test(static_cast<std::size_t>(keyValue));
    }

    bool InputSystem::WasKeyReleasedThisFrame(KeyCode keyCode) const
    {
        if (m_KeyboardInputBlocked)
            return false;

        const uint16_t keyValue = keyCode.GetValue();
        if (keyValue == 0 || keyValue >= InputCodeLimits::KeyCount)
            return false;
        return m_KeyReleasedThisFrame.test(static_cast<std::size_t>(keyValue));
    }

    bool InputSystem::IsMouseButtonDown(MouseButtonCode button) const
    {
        if (m_MouseInputBlocked)
            return false;

        const uint8_t buttonValue = button.GetValue();
        if (buttonValue == 0 || buttonValue >= MaxMouseButtons)
            return false;
        return m_MouseDown[buttonValue] != 0;
    }

    bool InputSystem::WasMouseButtonPressedThisFrame(MouseButtonCode button) const
    {
        if (m_MouseInputBlocked)
            return false;

        const uint8_t buttonValue = button.GetValue();
        if (buttonValue == 0 || buttonValue >= MaxMouseButtons)
            return false;
        return m_MousePressedThisFrame[buttonValue] != 0;
    }

    bool InputSystem::WasMouseButtonReleasedThisFrame(MouseButtonCode button) const
    {
        if (m_MouseInputBlocked)
            return false;

        const uint8_t buttonValue = button.GetValue();
        if (buttonValue == 0 || buttonValue >= MaxMouseButtons)
            return false;
        return m_MouseReleasedThisFrame[buttonValue] != 0;
    }

    int InputSystem::GetGamepadCount() const
    {
        int count = 0;
        for (const PerGamepadState& gamepad : m_Gamepads)
        {
            if (gamepad.Gamepad != nullptr)
                ++count;
        }
        return count;
    }

    bool InputSystem::HasGamepad(int playerIndex) const
    {
        if (playerIndex < 0 || playerIndex >= MaxGamepads)
            return false;
        return m_Gamepads[static_cast<std::size_t>(playerIndex)].Gamepad != nullptr;
    }

    bool InputSystem::IsGamepadButtonDown(int playerIndex, GamepadButtonCode button) const
    {
        const int16_t buttonValue = button.GetValue();
        if (playerIndex < 0 || playerIndex >= MaxGamepads || buttonValue < 0 || buttonValue >= static_cast<int16_t>(InputCodeLimits::GamepadButtonCount))
            return false;

        const PerGamepadState& gamepad = m_Gamepads[static_cast<std::size_t>(playerIndex)];
        return gamepad.Gamepad != nullptr && gamepad.ButtonDown[static_cast<std::size_t>(buttonValue)] != 0;
    }

    bool InputSystem::WasGamepadButtonPressedThisFrame(int playerIndex, GamepadButtonCode button) const
    {
        const int16_t buttonValue = button.GetValue();
        if (playerIndex < 0 || playerIndex >= MaxGamepads || buttonValue < 0 || buttonValue >= static_cast<int16_t>(InputCodeLimits::GamepadButtonCount))
            return false;

        const PerGamepadState& gamepad = m_Gamepads[static_cast<std::size_t>(playerIndex)];
        return gamepad.Gamepad != nullptr && gamepad.ButtonPressedThisFrame[static_cast<std::size_t>(buttonValue)] != 0;
    }

    bool InputSystem::WasGamepadButtonReleasedThisFrame(int playerIndex, GamepadButtonCode button) const
    {
        const int16_t buttonValue = button.GetValue();
        if (playerIndex < 0 || playerIndex >= MaxGamepads || buttonValue < 0 || buttonValue >= static_cast<int16_t>(InputCodeLimits::GamepadButtonCount))
            return false;

        const PerGamepadState& gamepad = m_Gamepads[static_cast<std::size_t>(playerIndex)];
        return gamepad.Gamepad != nullptr && gamepad.ButtonReleasedThisFrame[static_cast<std::size_t>(buttonValue)] != 0;
    }

    float InputSystem::GetGamepadAxis(int playerIndex, GamepadAxisCode axis) const
    {
        const int16_t axisValue = axis.GetValue();
        if (playerIndex < 0 || playerIndex >= MaxGamepads || axisValue < 0 || axisValue >= static_cast<int16_t>(InputCodeLimits::GamepadAxisCount))
            return 0.0f;

        const PerGamepadState& gamepad = m_Gamepads[static_cast<std::size_t>(playerIndex)];
        if (gamepad.Gamepad == nullptr)
            return 0.0f;

        const int16_t rawValue = gamepad.Axis[static_cast<std::size_t>(axisValue)];
        if (axis == GamepadAxes::LeftTrigger || axis == GamepadAxes::RightTrigger)
            return std::clamp(static_cast<float>(rawValue) / 32767.0f, 0.0f, 1.0f);
        return std::clamp(static_cast<float>(rawValue) / 32767.0f, -1.0f, 1.0f);
    }

    bool InputSystem::IsActionPressed(std::string_view mapName, std::string_view actionName, float deadzone) const
    {
        const InputAction* action = FindAction(mapName, actionName, true);
        return action != nullptr ? action->IsPressed(deadzone) : false;
    }

    bool InputSystem::WasActionStartedThisFrame(std::string_view mapName, std::string_view actionName) const
    {
        const InputAction* action = FindAction(mapName, actionName, true);
        return action != nullptr ? action->WasStartedThisFrame() : false;
    }

    bool InputSystem::WasActionPerformedThisFrame(std::string_view mapName, std::string_view actionName) const
    {
        const InputAction* action = FindAction(mapName, actionName, true);
        return action != nullptr ? action->WasPerformedThisFrame() : false;
    }

    bool InputSystem::WasActionCanceledThisFrame(std::string_view mapName, std::string_view actionName) const
    {
        const InputAction* action = FindAction(mapName, actionName, true);
        return action != nullptr ? action->WasCanceledThisFrame() : false;
    }

    bool InputSystem::ReadActionButton(std::string_view mapName, std::string_view actionName) const
    {
        const InputAction* action = FindAction(mapName, actionName, true);
        return action != nullptr ? action->ReadButton() : false;
    }

    float InputSystem::ReadActionAxis1D(std::string_view mapName, std::string_view actionName) const
    {
        const InputAction* action = FindAction(mapName, actionName, true);
        return action != nullptr ? action->ReadAxis1D() : 0.0f;
    }

    InputVector2 InputSystem::ReadActionAxis2D(std::string_view mapName, std::string_view actionName) const
    {
        const InputAction* action = FindAction(mapName, actionName, true);
        return action != nullptr ? action->ReadAxis2D() : InputVector2{};
    }

    bool InputSystem::HasAction(std::string_view mapName, std::string_view actionName) const
    {
        return FindAction(mapName, actionName, false) != nullptr;
    }

    void InputSystem::CloseAllGamepads() noexcept
    {
        for (PerGamepadState& gamepad : m_Gamepads)
        {
            if (gamepad.Gamepad != nullptr)
            {
                SDL_CloseGamepad(gamepad.Gamepad);
                gamepad.Gamepad = nullptr;
            }

            gamepad.Id = 0;
            gamepad.Axis.fill(0);
            gamepad.ButtonDown.fill(0);
            gamepad.ButtonPressedThisFrame.fill(0);
            gamepad.ButtonReleasedThisFrame.fill(0);
        }
    }

    void InputSystem::OnKey(KeyCode keyCode, bool down, bool repeat)
    {
        const uint16_t keyValue = keyCode.GetValue();
        if (keyValue == 0 || keyValue >= InputCodeLimits::KeyCount)
            return;

        const std::size_t index = static_cast<std::size_t>(keyValue);
        const bool wasDown = m_KeyDown.test(index);
        m_KeyDown.set(index, down);

        if (!repeat)
        {
            if (!wasDown && down)
                m_KeyPressedThisFrame.set(index);
            if (wasDown && !down)
                m_KeyReleasedThisFrame.set(index);
        }
    }

    void InputSystem::OnMouseMotion(const InputVector2& position, const InputVector2& delta)
    {
        m_MousePosition = position;
        if (m_PendingSyntheticMouseMotionEvents > 0)
        {
            --m_PendingSyntheticMouseMotionEvents;
            return;
        }

        m_MouseDelta += delta;
    }

    void InputSystem::OnMouseButton(MouseButtonCode button, bool down)
    {
        const uint8_t buttonValue = button.GetValue();
        if (buttonValue == 0 || buttonValue >= MaxMouseButtons)
            return;

        const bool wasDown = m_MouseDown[buttonValue] != 0;
        m_MouseDown[buttonValue] = down ? 1 : 0;
        if (!wasDown && down)
            m_MousePressedThisFrame[buttonValue] = 1;
        if (wasDown && !down)
            m_MouseReleasedThisFrame[buttonValue] = 1;
    }

    void InputSystem::OnMouseWheel(float offsetX, float offsetY)
    {
        m_MouseWheelDelta += InputVector2{ offsetX, offsetY };
    }

    void InputSystem::OnGamepadAdded(GamepadId which)
    {
        if (FindSlotByGamepadId(m_Gamepads, which) != static_cast<std::size_t>(-1))
            return;

        if (!SDL_IsGamepad(which))
            return;

        const std::size_t slot = FindFreeGamepadSlot(m_Gamepads);
        if (slot == static_cast<std::size_t>(-1))
        {
            LOG_CORE_WARN("InputSystem could not track gamepad {} because all {} slots are already in use.", static_cast<int>(which), MaxGamepads);
            return;
        }

        SDL_Gamepad* openedGamepad = SDL_OpenGamepad(which);
        if (openedGamepad == nullptr)
        {
            LOG_CORE_WARN("InputSystem failed to open SDL gamepad {}: {}", static_cast<int>(which), SDL_GetError());
            return;
        }

        PerGamepadState& gamepad = m_Gamepads[slot];
        gamepad.Id = which;
        gamepad.Gamepad = openedGamepad;
        gamepad.Axis.fill(0);
        gamepad.ButtonDown.fill(0);
        gamepad.ButtonPressedThisFrame.fill(0);
        gamepad.ButtonReleasedThisFrame.fill(0);
    }

    void InputSystem::OnGamepadRemoved(GamepadId which)
    {
        const std::size_t slot = FindSlotByGamepadId(m_Gamepads, which);
        if (slot == static_cast<std::size_t>(-1))
            return;

        PerGamepadState& gamepad = m_Gamepads[slot];
        if (gamepad.Gamepad != nullptr)
            SDL_CloseGamepad(gamepad.Gamepad);

        gamepad.Id = 0;
        gamepad.Gamepad = nullptr;
        gamepad.Axis.fill(0);
        gamepad.ButtonDown.fill(0);
        gamepad.ButtonPressedThisFrame.fill(0);
        gamepad.ButtonReleasedThisFrame.fill(0);
    }

    void InputSystem::OnGamepadAxis(GamepadId which, GamepadAxisCode axis, int16_t value)
    {
        const int16_t axisValue = axis.GetValue();
        const std::size_t slot = FindSlotByGamepadId(m_Gamepads, which);
        if (slot == static_cast<std::size_t>(-1) || axisValue < 0 || axisValue >= static_cast<int16_t>(InputCodeLimits::GamepadAxisCount))
            return;

        m_Gamepads[slot].Axis[static_cast<std::size_t>(axisValue)] = value;
    }

    void InputSystem::OnGamepadButton(GamepadId which, GamepadButtonCode button, bool down)
    {
        const int16_t buttonValue = button.GetValue();
        const std::size_t slot = FindSlotByGamepadId(m_Gamepads, which);
        if (slot == static_cast<std::size_t>(-1) || buttonValue < 0 || buttonValue >= static_cast<int16_t>(InputCodeLimits::GamepadButtonCount))
            return;

        PerGamepadState& gamepad = m_Gamepads[slot];
        const std::size_t index = static_cast<std::size_t>(buttonValue);
        const bool wasDown = gamepad.ButtonDown[index] != 0;
        gamepad.ButtonDown[index] = down ? 1 : 0;
        if (!wasDown && down)
            gamepad.ButtonPressedThisFrame[index] = 1;
        else if (wasDown && !down)
            gamepad.ButtonReleasedThisFrame[index] = 1;
    }

    const InputAction* InputSystem::FindAction(std::string_view mapName, std::string_view actionName, bool warnIfMissing) const
    {
        const Ref<InputActionAsset> asset = GetActiveActionAsset();
        if (!asset)
        {
            if (warnIfMissing)
                WarnMissingActionOnce(mapName, actionName);
            return nullptr;
        }

        const InputActionMap* map = asset->FindMap(mapName);
        if (map == nullptr)
        {
            if (warnIfMissing)
                WarnMissingActionOnce(mapName, actionName);
            return nullptr;
        }

        const InputAction* action = map->FindAction(actionName);
        if (action == nullptr && warnIfMissing)
            WarnMissingActionOnce(mapName, actionName);
        return action;
    }

    void InputSystem::WarnMissingActionOnce(std::string_view mapName, std::string_view actionName) const
    {
        std::string missingKey;
        missingKey.reserve(mapName.size() + actionName.size() + 2);
        missingKey.append(mapName);
        missingKey.append("::");
        missingKey.append(actionName);

        if (!m_WarnedMissingActions.insert(missingKey).second)
            return;

        LOG_CORE_WARN("InputSystem action '{}::{}' was requested but is missing from the active input action asset.", mapName, actionName);
    }

    std::size_t InputSystem::FindSlotByGamepadId(std::array<PerGamepadState, MaxGamepads>& gamepads, GamepadId id)
    {
        for (std::size_t index = 0; index < gamepads.size(); ++index)
        {
            if (gamepads[index].Gamepad != nullptr && gamepads[index].Id == id)
                return index;
        }
        return static_cast<std::size_t>(-1);
    }

    std::size_t InputSystem::FindFreeGamepadSlot(std::array<PerGamepadState, MaxGamepads>& gamepads)
    {
        for (std::size_t index = 0; index < gamepads.size(); ++index)
        {
            if (gamepads[index].Gamepad == nullptr)
                return index;
        }
        return static_cast<std::size_t>(-1);
    }
}
