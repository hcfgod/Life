#include "Core/Input/InputAction.h"

#include "Core/Input/InputSystem.h"
#include "Core/Log.h"
#include "Platform/PlatformDetection.h"
#include "Platform/SDL/SDLInputCodes.h"

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_keyboard.h>
#include <glm/geometric.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>

namespace Life
{
    namespace
    {
        using json = nlohmann::json;

        const char* PhaseToString(InputActionPhase phase)
        {
            switch (phase)
            {
            case InputActionPhase::Disabled: return "Disabled";
            case InputActionPhase::Waiting: return "Waiting";
            case InputActionPhase::Started: return "Started";
            case InputActionPhase::Performed: return "Performed";
            case InputActionPhase::Canceled: return "Canceled";
            default: return "Unknown";
            }
        }

        const char* ValueTypeToString(InputActionValueType type)
        {
            switch (type)
            {
            case InputActionValueType::Button: return "Button";
            case InputActionValueType::Axis1D: return "Axis1D";
            case InputActionValueType::Axis2D: return "Axis2D";
            default: return "Unknown";
            }
        }

        std::string ScancodeToString(KeyCode keyCode)
        {
            const SDL_Scancode scancode = ToSDLScancode(keyCode);
            const char* name = SDL_GetScancodeName(scancode);
            if (name != nullptr && name[0] != '\0')
                return std::string(name);
            return std::to_string(static_cast<int>(keyCode.GetValue()));
        }

        std::string GamepadButtonToString(GamepadButtonCode button)
        {
            const char* name = SDL_GetGamepadStringForButton(ToSDLGamepadButton(button));
            if (name != nullptr && name[0] != '\0')
                return std::string(name);
            return std::to_string(static_cast<int>(button.GetValue()));
        }

        std::string GamepadAxisToString(GamepadAxisCode axis)
        {
            const char* name = SDL_GetGamepadStringForAxis(ToSDLGamepadAxis(axis));
            if (name != nullptr && name[0] != '\0')
                return std::string(name);
            return std::to_string(static_cast<int>(axis.GetValue()));
        }

        float VectorLength(const glm::vec2& value)
        {
            return glm::length(value);
        }

        glm::vec2 Normalize(const glm::vec2& value)
        {
            const float length = VectorLength(value);
            if (length <= 0.000001f)
                return {};

            return glm::normalize(value);
        }

        float ApplyDeadzone1D(float value, float deadzone)
        {
            deadzone = std::max(deadzone, 0.0f);
            if (std::abs(value) <= deadzone)
                return 0.0f;

            const float sign = value < 0.0f ? -1.0f : 1.0f;
            const float magnitude = (std::abs(value) - deadzone) / (1.0f - deadzone);
            return sign * std::clamp(magnitude, 0.0f, 1.0f);
        }

        glm::vec2 ApplyDeadzone2D(const glm::vec2& value, float deadzone)
        {
            deadzone = std::max(deadzone, 0.0f);
            const float length = VectorLength(value);
            if (length <= deadzone)
                return {};

            const float scaledMagnitude = (length - deadzone) / (1.0f - deadzone);
            return Normalize(value) * std::clamp(scaledMagnitude, 0.0f, 1.0f);
        }

        InputActionValueType ParseValueType(const std::string& value)
        {
            if (value == "Button")
                return InputActionValueType::Button;
            if (value == "Axis1D")
                return InputActionValueType::Axis1D;
            if (value == "Axis2D")
                return InputActionValueType::Axis2D;
            return InputActionValueType::Button;
        }

        std::string ToString(InputActionValueType valueType)
        {
            switch (valueType)
            {
            case InputActionValueType::Button: return "Button";
            case InputActionValueType::Axis1D: return "Axis1D";
            case InputActionValueType::Axis2D: return "Axis2D";
            default: return "Button";
            }
        }

        KeyCode ParseScancode(const json& object, const char* scancodeKey, const char* nameKey)
        {
            if (object.contains(nameKey) && object[nameKey].is_string())
            {
                const std::string name = object[nameKey].get<std::string>();
                if (!name.empty())
                {
                    const SDL_Scancode parsed = SDL_GetScancodeFromName(name.c_str());
                    if (parsed != SDL_SCANCODE_UNKNOWN)
                        return ToKeyCode(parsed);
                }
            }

            if (object.contains(scancodeKey) && object[scancodeKey].is_number_integer())
            {
                const int rawScancode = object[scancodeKey].get<int>();
                if (rawScancode >= 0 && rawScancode < static_cast<int>(InputCodeLimits::KeyCount))
                    return KeyCode{ static_cast<uint16_t>(rawScancode) };
            }

            return KeyCodes::Unknown;
        }

        GamepadButtonCode ParseGamepadButton(const json& object, const char* buttonKey, const char* nameKey)
        {
            if (object.contains(nameKey) && object[nameKey].is_string())
            {
                const std::string name = object[nameKey].get<std::string>();
                if (!name.empty())
                {
                    const SDL_GamepadButton parsed = SDL_GetGamepadButtonFromString(name.c_str());
                    if (parsed != SDL_GAMEPAD_BUTTON_INVALID)
                        return ToGamepadButtonCode(parsed);
                }
            }

            if (object.contains(buttonKey) && object[buttonKey].is_number_integer())
            {
                const int rawButton = object[buttonKey].get<int>();
                if (rawButton >= 0 && rawButton < static_cast<int>(InputCodeLimits::GamepadButtonCount))
                    return GamepadButtonCode{ static_cast<int16_t>(rawButton) };
            }

            return GamepadButtons::Invalid;
        }

        GamepadAxisCode ParseGamepadAxis(const json& object, const char* axisKey, const char* nameKey)
        {
            if (object.contains(nameKey) && object[nameKey].is_string())
            {
                const std::string name = object[nameKey].get<std::string>();
                if (!name.empty())
                {
                    const SDL_GamepadAxis parsed = SDL_GetGamepadAxisFromString(name.c_str());
                    if (parsed != SDL_GAMEPAD_AXIS_INVALID)
                        return ToGamepadAxisCode(parsed);
                }
            }

            if (object.contains(axisKey) && object[axisKey].is_number_integer())
            {
                const int rawAxis = object[axisKey].get<int>();
                if (rawAxis >= 0 && rawAxis < static_cast<int>(InputCodeLimits::GamepadAxisCount))
                    return GamepadAxisCode{ static_cast<int16_t>(rawAxis) };
            }

            return GamepadAxes::Invalid;
        }

        std::filesystem::path BuildInputOverridePath(const std::string& assetPath)
        {
            const std::string userDataPath = PlatformDetection::GetUserDataPath();
            if (userDataPath.empty())
                return {};

            std::filesystem::path overrideRoot = std::filesystem::path(userDataPath) / "InputActionsOverrides";
            std::filesystem::path relativePath(assetPath);
            if (relativePath.is_absolute())
                relativePath = relativePath.filename();

            return overrideRoot / relativePath;
        }

        Result<void> SaveInputAssetWithFallback(const InputActionAsset& asset, const std::string& assetPath)
        {
            if (assetPath.empty())
                return Result<void>(ErrorCode::InvalidArgument, "Input action asset path is empty.");

            const Result<void> saveResult = InputActionAssetSerializer::SaveToFile(asset, assetPath);
            if (saveResult.IsSuccess())
                return Result<void>();

            const std::filesystem::path overridePath = BuildInputOverridePath(assetPath);
            if (overridePath.empty())
                return saveResult;

            return InputActionAssetSerializer::SaveToFile(asset, overridePath.string());
        }

        Result<void> ApplyAndMaybeSaveBinding(const InputRebinding::Request& request, const InputBinding& binding)
        {
            Ref<InputActionAsset> asset = request.Asset;
            if (!asset)
            {
                if (request.AssetPath.empty())
                    return Result<void>(ErrorCode::InvalidArgument, "Input rebinding request does not specify an asset or asset path.");

                Result<Ref<InputActionAsset>> loadedAsset = InputActionAssetSerializer::LoadFromFile(request.AssetPath);
                if (loadedAsset.IsFailure())
                    return Result<void>(loadedAsset.GetError());

                asset = loadedAsset.GetValue();
            }

            InputActionMap* map = asset->FindMap(request.MapName);
            if (map == nullptr)
                return Result<void>(ErrorCode::InputConfigurationError, "Input action map not found: " + request.MapName);

            InputAction* action = map->FindAction(request.ActionName);
            if (action == nullptr)
                return Result<void>(ErrorCode::InputConfigurationError, "Input action not found: " + request.ActionName);

            if (!action->SetBinding(request.BindingIndex, binding))
                return Result<void>(ErrorCode::InputConfigurationError, "Input binding index is out of range.");

            if (!request.SaveToDisk)
                return Result<void>();

            return SaveInputAssetWithFallback(*asset, request.AssetPath);
        }
    }

    bool InputActionValue::AsButton() const
    {
        if (m_Type == InputActionValueType::Button)
            return std::get<bool>(m_Value);
        if (m_Type == InputActionValueType::Axis1D)
            return std::abs(std::get<float>(m_Value)) > 0.0001f;

        const glm::vec2 value = std::get<glm::vec2>(m_Value);
        return std::abs(value.x) > 0.0001f || std::abs(value.y) > 0.0001f;
    }

    float InputActionValue::AsAxis1D() const
    {
        if (m_Type == InputActionValueType::Axis1D)
            return std::get<float>(m_Value);
        if (m_Type == InputActionValueType::Button)
            return std::get<bool>(m_Value) ? 1.0f : 0.0f;

        return std::get<glm::vec2>(m_Value).x;
    }

    glm::vec2 InputActionValue::AsAxis2D() const
    {
        if (m_Type == InputActionValueType::Axis2D)
            return std::get<glm::vec2>(m_Value);
        if (m_Type == InputActionValueType::Axis1D)
            return { std::get<float>(m_Value), 0.0f };
        return std::get<bool>(m_Value) ? glm::vec2{ 1.0f, 0.0f } : glm::vec2{};
    }

    InputActionValue InputActionValue::Button(bool value)
    {
        InputActionValue actionValue;
        actionValue.m_Type = InputActionValueType::Button;
        actionValue.m_Value = value;
        return actionValue;
    }

    InputActionValue InputActionValue::Axis1D(float value)
    {
        InputActionValue actionValue;
        actionValue.m_Type = InputActionValueType::Axis1D;
        actionValue.m_Value = value;
        return actionValue;
    }

    InputActionValue InputActionValue::Axis2D(const glm::vec2& value)
    {
        InputActionValue actionValue;
        actionValue.m_Type = InputActionValueType::Axis2D;
        actionValue.m_Value = value;
        return actionValue;
    }

    bool InputActionValue::IsActuated(float deadzone) const
    {
        deadzone = std::max(deadzone, 0.0f);

        switch (m_Type)
        {
        case InputActionValueType::Button:
            return std::get<bool>(m_Value);
        case InputActionValueType::Axis1D:
            return std::abs(std::get<float>(m_Value)) > deadzone;
        case InputActionValueType::Axis2D:
        default:
        {
            const glm::vec2 value = std::get<glm::vec2>(m_Value);
            return std::abs(value.x) > deadzone || std::abs(value.y) > deadzone;
        }
        }
    }

    InputAction::InputAction(std::string name, InputActionValueType valueType)
        : m_Name(std::move(name)), m_ValueType(valueType)
    {
        switch (m_ValueType)
        {
        case InputActionValueType::Button:
            m_Value = InputActionValue::Button(false);
            break;
        case InputActionValueType::Axis1D:
            m_Value = InputActionValue::Axis1D(0.0f);
            break;
        case InputActionValueType::Axis2D:
        default:
            m_Value = InputActionValue::Axis2D({});
            break;
        }
    }

    void InputAction::AddBinding(InputBinding binding)
    {
        m_Bindings.push_back(std::move(binding));
    }

    bool InputAction::SetBinding(std::size_t index, InputBinding binding)
    {
        if (index >= m_Bindings.size())
            return false;

        m_Bindings[index] = std::move(binding);
        return true;
    }

    void InputAction::SetEnabled(bool enabled)
    {
        m_Enabled = enabled;
        if (!m_Enabled)
        {
            m_Phase = InputActionPhase::Disabled;
            m_WasActuatedLastFrame = false;
            switch (m_ValueType)
            {
            case InputActionValueType::Button:
                m_Value = InputActionValue::Button(false);
                break;
            case InputActionValueType::Axis1D:
                m_Value = InputActionValue::Axis1D(0.0f);
                break;
            case InputActionValueType::Axis2D:
            default:
                m_Value = InputActionValue::Axis2D({});
                break;
            }
            return;
        }

        m_Phase = InputActionPhase::Waiting;
    }

    void InputAction::Update(const InputSystem& input)
    {
        if (!m_Enabled)
        {
            m_Phase = InputActionPhase::Disabled;
            return;
        }

        const InputActionValue newValue = EvaluateValue(input);
        const bool actuated = newValue.IsActuated();

        if (!m_WasActuatedLastFrame && actuated)
            m_Phase = InputActionPhase::Started;
        else if (m_WasActuatedLastFrame && actuated)
            m_Phase = InputActionPhase::Performed;
        else if (m_WasActuatedLastFrame && !actuated)
            m_Phase = InputActionPhase::Canceled;
        else
            m_Phase = InputActionPhase::Waiting;

        m_Value = newValue;
        m_WasActuatedLastFrame = actuated;
    }

    InputActionValue InputAction::EvaluateValue(const InputSystem& input) const
    {
        if (m_Bindings.empty())
        {
            switch (m_ValueType)
            {
            case InputActionValueType::Button: return InputActionValue::Button(false);
            case InputActionValueType::Axis1D: return InputActionValue::Axis1D(0.0f);
            case InputActionValueType::Axis2D:
            default:
                return InputActionValue::Axis2D({});
            }
        }

        switch (m_ValueType)
        {
        case InputActionValueType::Button:
        {
            bool down = false;
            for (const InputBinding& binding : m_Bindings)
            {
                if (const auto* key = std::get_if<KeyboardButtonBinding>(&binding))
                {
                    down = down || input.IsKeyDown(key->Key) || input.WasKeyPressedThisFrame(key->Key);
                }
                else if (const auto* mouse = std::get_if<MouseButtonBinding>(&binding))
                {
                    down = down || input.IsMouseButtonDown(mouse->Button) || input.WasMouseButtonPressedThisFrame(mouse->Button);
                }
                else if (const auto* gamepad = std::get_if<GamepadButtonBinding>(&binding))
                {
                    down = down || input.IsGamepadButtonDown(gamepad->PlayerIndex, gamepad->Button)
                        || input.WasGamepadButtonPressedThisFrame(gamepad->PlayerIndex, gamepad->Button);
                }
            }
            return InputActionValue::Button(down);
        }
        case InputActionValueType::Axis1D:
        {
            float value = 0.0f;
            for (const InputBinding& binding : m_Bindings)
            {
                if (const auto* axis = std::get_if<KeyboardAxis1DBinding>(&binding))
                {
                    if (input.IsKeyDown(axis->Negative))
                        value += axis->NegativeScale;
                    if (input.IsKeyDown(axis->Positive))
                        value += axis->PositiveScale;
                }
                else if (const auto* gamepad = std::get_if<GamepadAxis1DBinding>(&binding))
                {
                    const float rawValue = input.GetGamepadAxis(gamepad->PlayerIndex, gamepad->Axis);
                    value += ApplyDeadzone1D(rawValue, gamepad->Deadzone) * gamepad->Scale;
                }
            }

            value = std::clamp(value, -1.0f, 1.0f);
            return InputActionValue::Axis1D(value);
        }
        case InputActionValueType::Axis2D:
        default:
        {
            glm::vec2 value{};
            for (const InputBinding& binding : m_Bindings)
            {
                if (const auto* axis = std::get_if<KeyboardAxis2DBinding>(&binding))
                {
                    if (input.IsKeyDown(axis->Left))
                        value.x -= axis->Scale;
                    if (input.IsKeyDown(axis->Right))
                        value.x += axis->Scale;
                    if (input.IsKeyDown(axis->Down))
                        value.y -= axis->Scale;
                    if (input.IsKeyDown(axis->Up))
                        value.y += axis->Scale;
                }
                else if (const auto* mouseDelta = std::get_if<MouseDeltaBinding>(&binding))
                {
                    glm::vec2 delta = input.GetMouseDelta() * mouseDelta->Sensitivity;
                    if (mouseDelta->InvertY)
                        delta.y = -delta.y;
                    value += delta;
                }
                else if (const auto* gamepad = std::get_if<GamepadAxis2DBinding>(&binding))
                {
                    glm::vec2 stick{ input.GetGamepadAxis(gamepad->PlayerIndex, gamepad->XAxis), input.GetGamepadAxis(gamepad->PlayerIndex, gamepad->YAxis) };
                    if (gamepad->InvertY)
                        stick.y = -stick.y;
                    value += ApplyDeadzone2D(stick, gamepad->Deadzone) * gamepad->Scale;
                }
            }

            return InputActionValue::Axis2D(value);
        }
        }
    }

    std::string InputAction::DebugDump() const
    {
        std::ostringstream stream;
        stream << "InputAction '" << m_Name << "'"
            << " type=" << ValueTypeToString(m_ValueType)
            << " phase=" << PhaseToString(m_Phase)
            << " enabled=" << (m_Enabled ? "true" : "false");

        if (m_ValueType == InputActionValueType::Button)
        {
            stream << " value=" << (m_Value.AsButton() ? "true" : "false");
        }
        else if (m_ValueType == InputActionValueType::Axis1D)
        {
            stream << " value=" << m_Value.AsAxis1D();
        }
        else
        {
            const glm::vec2 value = m_Value.AsAxis2D();
            stream << " value=(" << value.x << "," << value.y << ")";
        }

        stream << " bindings=[";
        for (std::size_t index = 0; index < m_Bindings.size(); ++index)
        {
            if (index > 0)
                stream << ", ";

            const InputBinding& binding = m_Bindings[index];
            if (const auto* keyboardButton = std::get_if<KeyboardButtonBinding>(&binding))
            {
                stream << "KeyboardButton(" << ScancodeToString(keyboardButton->Key) << ")";
            }
            else if (const auto* mouseButton = std::get_if<MouseButtonBinding>(&binding))
            {
                stream << "MouseButton(" << static_cast<int>(mouseButton->Button.GetValue()) << ")";
            }
            else if (const auto* keyboardAxis1D = std::get_if<KeyboardAxis1DBinding>(&binding))
            {
                stream << "KeyboardAxis1D(neg=" << ScancodeToString(keyboardAxis1D->Negative)
                    << ",pos=" << ScancodeToString(keyboardAxis1D->Positive) << ")";
            }
            else if (const auto* keyboardAxis2D = std::get_if<KeyboardAxis2DBinding>(&binding))
            {
                stream << "KeyboardAxis2D(U=" << ScancodeToString(keyboardAxis2D->Up)
                    << ",D=" << ScancodeToString(keyboardAxis2D->Down)
                    << ",L=" << ScancodeToString(keyboardAxis2D->Left)
                    << ",R=" << ScancodeToString(keyboardAxis2D->Right) << ")";
            }
            else if (const auto* mouseDelta = std::get_if<MouseDeltaBinding>(&binding))
            {
                stream << "MouseDelta(sens=" << mouseDelta->Sensitivity
                    << ",invertY=" << (mouseDelta->InvertY ? "true" : "false") << ")";
            }
            else if (const auto* gamepadButton = std::get_if<GamepadButtonBinding>(&binding))
            {
                stream << "GamepadButton(" << GamepadButtonToString(gamepadButton->Button);
                if (gamepadButton->PlayerIndex != 0)
                    stream << ",player=" << gamepadButton->PlayerIndex;
                stream << ")";
            }
            else if (const auto* gamepadAxis1D = std::get_if<GamepadAxis1DBinding>(&binding))
            {
                stream << "GamepadAxis1D(axis=" << GamepadAxisToString(gamepadAxis1D->Axis)
                    << ",deadzone=" << gamepadAxis1D->Deadzone
                    << ",scale=" << gamepadAxis1D->Scale;
                if (gamepadAxis1D->PlayerIndex != 0)
                    stream << ",player=" << gamepadAxis1D->PlayerIndex;
                stream << ")";
            }
            else if (const auto* gamepadAxis2D = std::get_if<GamepadAxis2DBinding>(&binding))
            {
                stream << "GamepadAxis2D(x=" << GamepadAxisToString(gamepadAxis2D->XAxis)
                    << ",y=" << GamepadAxisToString(gamepadAxis2D->YAxis)
                    << ",deadzone=" << gamepadAxis2D->Deadzone
                    << ",scale=" << gamepadAxis2D->Scale
                    << ",invertY=" << (gamepadAxis2D->InvertY ? "true" : "false");
                if (gamepadAxis2D->PlayerIndex != 0)
                    stream << ",player=" << gamepadAxis2D->PlayerIndex;
                stream << ")";
            }
        }
        stream << "]";

        return stream.str();
    }

    InputActionMap::InputActionMap(std::string name)
        : m_Name(std::move(name))
    {
    }

    void InputActionMap::SetEnabled(bool enabled)
    {
        m_Enabled = enabled;
        for (const std::unique_ptr<InputAction>& action : m_Actions)
        {
            if (action)
                action->SetEnabled(enabled);
        }
    }

    InputAction& InputActionMap::AddAction(const std::string& name, InputActionValueType valueType)
    {
        const auto existing = m_ActionByName.find(name);
        if (existing != m_ActionByName.end())
        {
            LOG_CORE_WARN("InputActionMap '{}': action '{}' already exists; returning existing action.", m_Name, name);
            return *existing->second;
        }

        auto action = std::make_unique<InputAction>(name, valueType);
        InputAction* actionPtr = action.get();
        m_Actions.push_back(std::move(action));
        m_ActionByName.emplace(name, actionPtr);
        return *actionPtr;
    }

    InputAction* InputActionMap::FindAction(std::string_view name)
    {
        const auto iterator = m_ActionByName.find(std::string(name));
        return iterator != m_ActionByName.end() ? iterator->second : nullptr;
    }

    const InputAction* InputActionMap::FindAction(std::string_view name) const
    {
        const auto iterator = m_ActionByName.find(std::string(name));
        return iterator != m_ActionByName.end() ? iterator->second : nullptr;
    }

    void InputActionMap::Update(const InputSystem& input)
    {
        if (!m_Enabled)
            return;

        for (const std::unique_ptr<InputAction>& action : m_Actions)
        {
            if (action)
                action->Update(input);
        }
    }

    InputActionMap& InputActionAsset::AddMap(const std::string& name)
    {
        const auto existing = m_MapByName.find(name);
        if (existing != m_MapByName.end())
        {
            LOG_CORE_WARN("InputActionAsset: map '{}' already exists; returning existing map.", name);
            return *existing->second;
        }

        auto map = std::make_unique<InputActionMap>(name);
        InputActionMap* mapPtr = map.get();
        m_Maps.push_back(std::move(map));
        m_MapByName.emplace(name, mapPtr);
        return *mapPtr;
    }

    InputActionMap* InputActionAsset::FindMap(std::string_view name)
    {
        const auto iterator = m_MapByName.find(std::string(name));
        return iterator != m_MapByName.end() ? iterator->second : nullptr;
    }

    const InputActionMap* InputActionAsset::FindMap(std::string_view name) const
    {
        const auto iterator = m_MapByName.find(std::string(name));
        return iterator != m_MapByName.end() ? iterator->second : nullptr;
    }

    void InputActionAsset::Update(const InputSystem& input)
    {
        for (const std::unique_ptr<InputActionMap>& map : m_Maps)
        {
            if (map)
                map->Update(input);
        }
    }

    Result<Ref<InputActionAsset>> InputActionAssetSerializer::LoadFromFile(const std::string& path)
    {
        Ref<InputActionAsset> asset = CreateRef<InputActionAsset>();
        Result<void> loadResult = LoadInto(*asset, path);
        if (loadResult.IsFailure())
            return Result<Ref<InputActionAsset>>(loadResult.GetError());

        return Result<Ref<InputActionAsset>>(std::move(asset));
    }

    Result<void> InputActionAssetSerializer::LoadInto(InputActionAsset& outAsset, const std::string& path)
    {
        std::ifstream file(path, std::ios::in | std::ios::binary);
        if (!file.is_open())
            return Result<void>(ErrorCode::FileNotFound, "Input action asset file not found: " + path);

        std::stringstream buffer;
        buffer << file.rdbuf();
        return LoadIntoFromString(outAsset, buffer.str(), path);
    }

    Result<void> InputActionAssetSerializer::LoadIntoFromString(InputActionAsset& outAsset, const std::string& jsonText, const std::string& debugName)
    {
        json root;
        try
        {
            root = json::parse(jsonText);
        }
        catch (const std::exception& exception)
        {
            return Result<void>(ErrorCode::ConfigParseError, std::string("Failed to parse input action asset JSON: ") + exception.what());
        }

        if (!root.contains("maps") || !root["maps"].is_array())
            return Result<void>(ErrorCode::InputConfigurationError, "Input action asset JSON is missing a 'maps' array.");

        outAsset.ClearMaps();
        for (const auto& mapJson : root["maps"])
        {
            const std::string mapName = mapJson.value("name", "Default");
            InputActionMap& map = outAsset.AddMap(mapName);
            map.SetEnabled(mapJson.value("enabled", true));

            const auto actionsIt = mapJson.find("actions");
            if (actionsIt == mapJson.end() || !actionsIt->is_array())
                continue;

            for (const auto& actionJson : *actionsIt)
            {
                const std::string actionName = actionJson.value("name", "Action");
                const std::string typeName = actionJson.value("type", "Button");
                InputAction& action = map.AddAction(actionName, ParseValueType(typeName));
                action.ClearBindings();

                const auto bindingsIt = actionJson.find("bindings");
                if (bindingsIt == actionJson.end() || !bindingsIt->is_array())
                    continue;

                for (const auto& bindingJson : *bindingsIt)
                {
                    const std::string bindingType = bindingJson.value("binding", "");
                    if (bindingType == "KeyboardButton")
                    {
                        KeyboardButtonBinding binding{};
                        binding.Key = ParseScancode(bindingJson, "scancode", "key");
                        action.AddBinding(binding);
                    }
                    else if (bindingType == "MouseButton")
                    {
                        MouseButtonBinding binding{};
                        binding.Button = MouseButtonCode{ static_cast<uint8_t>(bindingJson.value("button", 0)) };
                        action.AddBinding(binding);
                    }
                    else if (bindingType == "KeyboardAxis1D")
                    {
                        KeyboardAxis1DBinding binding{};
                        binding.Negative = ParseScancode(bindingJson, "negative_scancode", "negative");
                        binding.Positive = ParseScancode(bindingJson, "positive_scancode", "positive");
                        binding.NegativeScale = bindingJson.value("negative_scale", -1.0f);
                        binding.PositiveScale = bindingJson.value("positive_scale", 1.0f);
                        action.AddBinding(binding);
                    }
                    else if (bindingType == "KeyboardAxis2D")
                    {
                        KeyboardAxis2DBinding binding{};
                        binding.Up = ParseScancode(bindingJson, "up_scancode", "up");
                        binding.Down = ParseScancode(bindingJson, "down_scancode", "down");
                        binding.Left = ParseScancode(bindingJson, "left_scancode", "left");
                        binding.Right = ParseScancode(bindingJson, "right_scancode", "right");
                        binding.Scale = bindingJson.value("scale", 1.0f);
                        action.AddBinding(binding);
                    }
                    else if (bindingType == "MouseDelta")
                    {
                        MouseDeltaBinding binding{};
                        binding.Sensitivity = bindingJson.value("sensitivity", 1.0f);
                        binding.InvertY = bindingJson.value("invert_y", false);
                        action.AddBinding(binding);
                    }
                    else if (bindingType == "GamepadButton")
                    {
                        GamepadButtonBinding binding{};
                        binding.Button = ParseGamepadButton(bindingJson, "button_id", "button");
                        binding.PlayerIndex = bindingJson.value("player_index", 0);
                        action.AddBinding(binding);
                    }
                    else if (bindingType == "GamepadAxis1D")
                    {
                        GamepadAxis1DBinding binding{};
                        binding.Axis = ParseGamepadAxis(bindingJson, "axis_id", "axis");
                        binding.Scale = bindingJson.value("scale", 1.0f);
                        binding.Deadzone = bindingJson.value("deadzone", 0.15f);
                        binding.PlayerIndex = bindingJson.value("player_index", 0);
                        action.AddBinding(binding);
                    }
                    else if (bindingType == "GamepadAxis2D")
                    {
                        GamepadAxis2DBinding binding{};
                        binding.XAxis = ParseGamepadAxis(bindingJson, "x_axis_id", "x_axis");
                        binding.YAxis = ParseGamepadAxis(bindingJson, "y_axis_id", "y_axis");
                        binding.Scale = bindingJson.value("scale", 1.0f);
                        binding.Deadzone = bindingJson.value("deadzone", 0.15f);
                        binding.InvertY = bindingJson.value("invert_y", false);
                        binding.PlayerIndex = bindingJson.value("player_index", 0);
                        action.AddBinding(binding);
                    }
                    else
                    {
                        LOG_CORE_WARN("InputActionAssetSerializer: unknown binding type '{}' in '{}::{}' ({})", bindingType, mapName, actionName, debugName);
                    }
                }
            }
        }

        return Result<void>();
    }

    Result<void> InputActionAssetSerializer::SaveToFile(const InputActionAsset& asset, const std::string& path)
    {
        json root;
        root["maps"] = json::array();

        for (const std::unique_ptr<InputActionMap>& map : asset.GetMaps())
        {
            if (!map)
                continue;

            json mapJson;
            mapJson["name"] = map->GetName();
            mapJson["enabled"] = map->IsEnabled();
            mapJson["actions"] = json::array();

            for (const std::unique_ptr<InputAction>& action : map->GetActions())
            {
                if (!action)
                    continue;

                json actionJson;
                actionJson["name"] = action->GetName();
                actionJson["type"] = ToString(action->GetValueType());
                actionJson["bindings"] = json::array();

                for (const InputBinding& binding : action->GetBindings())
                {
                    json bindingJson;
                    if (const auto* keyboardButton = std::get_if<KeyboardButtonBinding>(&binding))
                    {
                        bindingJson["binding"] = "KeyboardButton";
                        bindingJson["key"] = ScancodeToString(keyboardButton->Key);
                        bindingJson["scancode"] = static_cast<int>(keyboardButton->Key.GetValue());
                    }
                    else if (const auto* mouseButton = std::get_if<MouseButtonBinding>(&binding))
                    {
                        bindingJson["binding"] = "MouseButton";
                        bindingJson["button"] = static_cast<int>(mouseButton->Button.GetValue());
                    }
                    else if (const auto* keyboardAxis1D = std::get_if<KeyboardAxis1DBinding>(&binding))
                    {
                        bindingJson["binding"] = "KeyboardAxis1D";
                        bindingJson["negative"] = ScancodeToString(keyboardAxis1D->Negative);
                        bindingJson["positive"] = ScancodeToString(keyboardAxis1D->Positive);
                        bindingJson["negative_scancode"] = static_cast<int>(keyboardAxis1D->Negative.GetValue());
                        bindingJson["positive_scancode"] = static_cast<int>(keyboardAxis1D->Positive.GetValue());
                        bindingJson["negative_scale"] = keyboardAxis1D->NegativeScale;
                        bindingJson["positive_scale"] = keyboardAxis1D->PositiveScale;
                    }
                    else if (const auto* keyboardAxis2D = std::get_if<KeyboardAxis2DBinding>(&binding))
                    {
                        bindingJson["binding"] = "KeyboardAxis2D";
                        bindingJson["up"] = ScancodeToString(keyboardAxis2D->Up);
                        bindingJson["down"] = ScancodeToString(keyboardAxis2D->Down);
                        bindingJson["left"] = ScancodeToString(keyboardAxis2D->Left);
                        bindingJson["right"] = ScancodeToString(keyboardAxis2D->Right);
                        bindingJson["up_scancode"] = static_cast<int>(keyboardAxis2D->Up.GetValue());
                        bindingJson["down_scancode"] = static_cast<int>(keyboardAxis2D->Down.GetValue());
                        bindingJson["left_scancode"] = static_cast<int>(keyboardAxis2D->Left.GetValue());
                        bindingJson["right_scancode"] = static_cast<int>(keyboardAxis2D->Right.GetValue());
                        bindingJson["scale"] = keyboardAxis2D->Scale;
                    }
                    else if (const auto* mouseDelta = std::get_if<MouseDeltaBinding>(&binding))
                    {
                        bindingJson["binding"] = "MouseDelta";
                        bindingJson["sensitivity"] = mouseDelta->Sensitivity;
                        bindingJson["invert_y"] = mouseDelta->InvertY;
                    }
                    else if (const auto* gamepadButton = std::get_if<GamepadButtonBinding>(&binding))
                    {
                        bindingJson["binding"] = "GamepadButton";
                        const char* buttonName = SDL_GetGamepadStringForButton(ToSDLGamepadButton(gamepadButton->Button));
                        bindingJson["button"] = buttonName != nullptr ? std::string(buttonName) : std::string();
                        bindingJson["button_id"] = static_cast<int>(gamepadButton->Button.GetValue());
                        if (gamepadButton->PlayerIndex != 0)
                            bindingJson["player_index"] = gamepadButton->PlayerIndex;
                    }
                    else if (const auto* gamepadAxis1D = std::get_if<GamepadAxis1DBinding>(&binding))
                    {
                        bindingJson["binding"] = "GamepadAxis1D";
                        const char* axisName = SDL_GetGamepadStringForAxis(ToSDLGamepadAxis(gamepadAxis1D->Axis));
                        bindingJson["axis"] = axisName != nullptr ? std::string(axisName) : std::string();
                        bindingJson["axis_id"] = static_cast<int>(gamepadAxis1D->Axis.GetValue());
                        bindingJson["scale"] = gamepadAxis1D->Scale;
                        bindingJson["deadzone"] = gamepadAxis1D->Deadzone;
                        if (gamepadAxis1D->PlayerIndex != 0)
                            bindingJson["player_index"] = gamepadAxis1D->PlayerIndex;
                    }
                    else if (const auto* gamepadAxis2D = std::get_if<GamepadAxis2DBinding>(&binding))
                    {
                        bindingJson["binding"] = "GamepadAxis2D";
                        const char* xAxisName = SDL_GetGamepadStringForAxis(ToSDLGamepadAxis(gamepadAxis2D->XAxis));
                        const char* yAxisName = SDL_GetGamepadStringForAxis(ToSDLGamepadAxis(gamepadAxis2D->YAxis));
                        bindingJson["x_axis"] = xAxisName != nullptr ? std::string(xAxisName) : std::string();
                        bindingJson["y_axis"] = yAxisName != nullptr ? std::string(yAxisName) : std::string();
                        bindingJson["x_axis_id"] = static_cast<int>(gamepadAxis2D->XAxis.GetValue());
                        bindingJson["y_axis_id"] = static_cast<int>(gamepadAxis2D->YAxis.GetValue());
                        bindingJson["scale"] = gamepadAxis2D->Scale;
                        bindingJson["deadzone"] = gamepadAxis2D->Deadzone;
                        bindingJson["invert_y"] = gamepadAxis2D->InvertY;
                        if (gamepadAxis2D->PlayerIndex != 0)
                            bindingJson["player_index"] = gamepadAxis2D->PlayerIndex;
                    }

                    if (!bindingJson.empty())
                        actionJson["bindings"].push_back(std::move(bindingJson));
                }

                mapJson["actions"].push_back(std::move(actionJson));
            }

            root["maps"].push_back(std::move(mapJson));
        }

        std::filesystem::path outputPath(path);
        if (outputPath.has_parent_path())
        {
            std::error_code createDirectoriesError;
            std::filesystem::create_directories(outputPath.parent_path(), createDirectoriesError);
            if (createDirectoriesError)
                return Result<void>(ErrorCode::FileAccessDenied, "Failed to create input action asset directories: " + outputPath.parent_path().string());
        }

        std::ofstream file(path, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!file.is_open())
            return Result<void>(ErrorCode::FileAccessDenied, "Failed to open input action asset for writing: " + path);

        file << root.dump(2);
        file.close();
        return Result<void>();
    }

    void InputRebinding::Start(Request request, CompletionCallback onComplete)
    {
        m_Request = std::move(request);
        m_OnComplete = std::move(onComplete);
        m_Active = true;
    }

    void InputRebinding::Cancel()
    {
        if (!m_Active)
            return;

        Complete(Result<InputBinding>(ErrorCode::Cancelled, "Input rebinding was cancelled."));
    }

    bool InputRebinding::AcceptsKeyboardMouse() const
    {
        return m_Request.Filter == DeviceFilter::Any || m_Request.Filter == DeviceFilter::KeyboardMouse;
    }

    bool InputRebinding::AcceptsGamepad() const
    {
        return m_Request.Filter == DeviceFilter::Any || m_Request.Filter == DeviceFilter::Gamepad;
    }

    void InputRebinding::Complete(Result<InputBinding> result)
    {
        m_Active = false;
        if (m_OnComplete)
            m_OnComplete(std::move(result));
        m_OnComplete = nullptr;
        m_Request = {};
    }

    bool InputRebinding::TryConsumeEvent(const SDL_Event& event)
    {
        if (!m_Active)
            return false;

        if (AcceptsKeyboardMouse() && event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat)
        {
            KeyboardButtonBinding binding{};
            binding.Key = ToKeyCode(event.key.scancode);

            const Result<void> result = ApplyAndMaybeSaveBinding(m_Request, binding);
            if (result.IsFailure())
                Complete(Result<InputBinding>(result.GetError()));
            else
                Complete(Result<InputBinding>(InputBinding(binding)));
            return true;
        }

        if (AcceptsKeyboardMouse() && event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
        {
            MouseButtonBinding binding{};
            binding.Button = ToMouseButtonCode(event.button.button);

            const Result<void> result = ApplyAndMaybeSaveBinding(m_Request, binding);
            if (result.IsFailure())
                Complete(Result<InputBinding>(result.GetError()));
            else
                Complete(Result<InputBinding>(InputBinding(binding)));
            return true;
        }

        if (AcceptsGamepad() && event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN)
        {
            GamepadButtonBinding binding{};
            binding.Button = ToGamepadButtonCode(static_cast<SDL_GamepadButton>(event.gbutton.button));

            const Result<void> result = ApplyAndMaybeSaveBinding(m_Request, binding);
            if (result.IsFailure())
                Complete(Result<InputBinding>(result.GetError()));
            else
                Complete(Result<InputBinding>(InputBinding(binding)));
            return true;
        }

        if (AcceptsGamepad() && event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION)
        {
            const GamepadAxisCode axis = ToGamepadAxisCode(static_cast<SDL_GamepadAxis>(event.gaxis.axis));
            const int16_t value = event.gaxis.value;
            if (std::abs(static_cast<int>(value)) < 16000)
                return false;

            if (axis == GamepadAxes::LeftX || axis == GamepadAxes::LeftY)
            {
                GamepadAxis2DBinding binding{};
                binding.XAxis = GamepadAxes::LeftX;
                binding.YAxis = GamepadAxes::LeftY;
                const Result<void> result = ApplyAndMaybeSaveBinding(m_Request, binding);
                if (result.IsFailure())
                    Complete(Result<InputBinding>(result.GetError()));
                else
                    Complete(Result<InputBinding>(InputBinding(binding)));
                return true;
            }

            if (axis == GamepadAxes::RightX || axis == GamepadAxes::RightY)
            {
                GamepadAxis2DBinding binding{};
                binding.XAxis = GamepadAxes::RightX;
                binding.YAxis = GamepadAxes::RightY;
                binding.InvertY = true;
                const Result<void> result = ApplyAndMaybeSaveBinding(m_Request, binding);
                if (result.IsFailure())
                    Complete(Result<InputBinding>(result.GetError()));
                else
                    Complete(Result<InputBinding>(InputBinding(binding)));
                return true;
            }

            if (axis == GamepadAxes::LeftTrigger || axis == GamepadAxes::RightTrigger)
            {
                GamepadAxis1DBinding binding{};
                binding.Axis = axis;
                binding.Deadzone = 0.05f;
                const Result<void> result = ApplyAndMaybeSaveBinding(m_Request, binding);
                if (result.IsFailure())
                    Complete(Result<InputBinding>(result.GetError()));
                else
                    Complete(Result<InputBinding>(InputBinding(binding)));
                return true;
            }
        }

        return false;
    }
}
