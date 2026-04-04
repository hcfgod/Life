#pragma once

#include <cstddef>
#include <cstdint>

namespace Life
{
    using GamepadId = int32_t;

    class KeyCode final
    {
    public:
        constexpr KeyCode() = default;
        constexpr explicit KeyCode(uint16_t value) : m_Value(value) {}

        constexpr uint16_t GetValue() const { return m_Value; }
        constexpr explicit operator bool() const { return m_Value != 0; }

        friend constexpr bool operator==(KeyCode left, KeyCode right) { return left.m_Value == right.m_Value; }
        friend constexpr bool operator!=(KeyCode left, KeyCode right) { return !(left == right); }

    private:
        uint16_t m_Value = 0;
    };

    class MouseButtonCode final
    {
    public:
        constexpr MouseButtonCode() = default;
        constexpr explicit MouseButtonCode(uint8_t value) : m_Value(value) {}

        constexpr uint8_t GetValue() const { return m_Value; }
        constexpr explicit operator bool() const { return m_Value != 0; }

        friend constexpr bool operator==(MouseButtonCode left, MouseButtonCode right) { return left.m_Value == right.m_Value; }
        friend constexpr bool operator!=(MouseButtonCode left, MouseButtonCode right) { return !(left == right); }

    private:
        uint8_t m_Value = 0;
    };

    class GamepadButtonCode final
    {
    public:
        constexpr GamepadButtonCode() = default;
        constexpr explicit GamepadButtonCode(int16_t value) : m_Value(value) {}

        constexpr int16_t GetValue() const { return m_Value; }
        constexpr explicit operator bool() const { return m_Value >= 0; }

        friend constexpr bool operator==(GamepadButtonCode left, GamepadButtonCode right) { return left.m_Value == right.m_Value; }
        friend constexpr bool operator!=(GamepadButtonCode left, GamepadButtonCode right) { return !(left == right); }

    private:
        int16_t m_Value = -1;
    };

    class GamepadAxisCode final
    {
    public:
        constexpr GamepadAxisCode() = default;
        constexpr explicit GamepadAxisCode(int16_t value) : m_Value(value) {}

        constexpr int16_t GetValue() const { return m_Value; }
        constexpr explicit operator bool() const { return m_Value >= 0; }

        friend constexpr bool operator==(GamepadAxisCode left, GamepadAxisCode right) { return left.m_Value == right.m_Value; }
        friend constexpr bool operator!=(GamepadAxisCode left, GamepadAxisCode right) { return !(left == right); }

    private:
        int16_t m_Value = -1;
    };

    namespace InputCodeLimits
    {
        inline constexpr std::size_t KeyCount = 512;
        inline constexpr std::size_t MouseButtonCount = 8;
        inline constexpr std::size_t GamepadButtonCount = 21;
        inline constexpr std::size_t GamepadAxisCount = 6;
    }

    namespace KeyCodes
    {
        inline constexpr KeyCode Unknown{ 0 };
        inline constexpr KeyCode A{ 4 };
        inline constexpr KeyCode B{ 5 };
        inline constexpr KeyCode C{ 6 };
        inline constexpr KeyCode D{ 7 };
        inline constexpr KeyCode E{ 8 };
        inline constexpr KeyCode F{ 9 };
        inline constexpr KeyCode G{ 10 };
        inline constexpr KeyCode H{ 11 };
        inline constexpr KeyCode I{ 12 };
        inline constexpr KeyCode J{ 13 };
        inline constexpr KeyCode K{ 14 };
        inline constexpr KeyCode L{ 15 };
        inline constexpr KeyCode M{ 16 };
        inline constexpr KeyCode N{ 17 };
        inline constexpr KeyCode O{ 18 };
        inline constexpr KeyCode P{ 19 };
        inline constexpr KeyCode Q{ 20 };
        inline constexpr KeyCode R{ 21 };
        inline constexpr KeyCode S{ 22 };
        inline constexpr KeyCode T{ 23 };
        inline constexpr KeyCode U{ 24 };
        inline constexpr KeyCode V{ 25 };
        inline constexpr KeyCode W{ 26 };
        inline constexpr KeyCode X{ 27 };
        inline constexpr KeyCode Y{ 28 };
        inline constexpr KeyCode Z{ 29 };
        inline constexpr KeyCode D1{ 30 };
        inline constexpr KeyCode D2{ 31 };
        inline constexpr KeyCode D3{ 32 };
        inline constexpr KeyCode D4{ 33 };
        inline constexpr KeyCode D5{ 34 };
        inline constexpr KeyCode D6{ 35 };
        inline constexpr KeyCode D7{ 36 };
        inline constexpr KeyCode D8{ 37 };
        inline constexpr KeyCode D9{ 38 };
        inline constexpr KeyCode D0{ 39 };
        inline constexpr KeyCode Enter{ 40 };
        inline constexpr KeyCode Escape{ 41 };
        inline constexpr KeyCode Backspace{ 42 };
        inline constexpr KeyCode Tab{ 43 };
        inline constexpr KeyCode Space{ 44 };
        inline constexpr KeyCode Left{ 80 };
        inline constexpr KeyCode Right{ 79 };
        inline constexpr KeyCode Down{ 81 };
        inline constexpr KeyCode Up{ 82 };
        inline constexpr KeyCode LeftControl{ 224 };
        inline constexpr KeyCode LeftShift{ 225 };
        inline constexpr KeyCode LeftAlt{ 226 };
        inline constexpr KeyCode RightControl{ 228 };
        inline constexpr KeyCode RightShift{ 229 };
        inline constexpr KeyCode RightAlt{ 230 };
    }

    namespace MouseButtons
    {
        inline constexpr MouseButtonCode None{ 0 };
        inline constexpr MouseButtonCode Left{ 1 };
        inline constexpr MouseButtonCode Middle{ 2 };
        inline constexpr MouseButtonCode Right{ 3 };
        inline constexpr MouseButtonCode X1{ 4 };
        inline constexpr MouseButtonCode X2{ 5 };
    }

    namespace GamepadButtons
    {
        inline constexpr GamepadButtonCode Invalid{ -1 };
        inline constexpr GamepadButtonCode South{ 0 };
        inline constexpr GamepadButtonCode East{ 1 };
        inline constexpr GamepadButtonCode West{ 2 };
        inline constexpr GamepadButtonCode North{ 3 };
        inline constexpr GamepadButtonCode Back{ 4 };
        inline constexpr GamepadButtonCode Guide{ 5 };
        inline constexpr GamepadButtonCode Start{ 6 };
        inline constexpr GamepadButtonCode LeftStick{ 7 };
        inline constexpr GamepadButtonCode RightStick{ 8 };
        inline constexpr GamepadButtonCode LeftShoulder{ 9 };
        inline constexpr GamepadButtonCode RightShoulder{ 10 };
        inline constexpr GamepadButtonCode DPadUp{ 11 };
        inline constexpr GamepadButtonCode DPadDown{ 12 };
        inline constexpr GamepadButtonCode DPadLeft{ 13 };
        inline constexpr GamepadButtonCode DPadRight{ 14 };
        inline constexpr GamepadButtonCode Misc1{ 15 };
        inline constexpr GamepadButtonCode RightPaddle1{ 16 };
        inline constexpr GamepadButtonCode LeftPaddle1{ 17 };
        inline constexpr GamepadButtonCode RightPaddle2{ 18 };
        inline constexpr GamepadButtonCode LeftPaddle2{ 19 };
        inline constexpr GamepadButtonCode Touchpad{ 20 };
    }

    namespace GamepadAxes
    {
        inline constexpr GamepadAxisCode Invalid{ -1 };
        inline constexpr GamepadAxisCode LeftX{ 0 };
        inline constexpr GamepadAxisCode LeftY{ 1 };
        inline constexpr GamepadAxisCode RightX{ 2 };
        inline constexpr GamepadAxisCode RightY{ 3 };
        inline constexpr GamepadAxisCode LeftTrigger{ 4 };
        inline constexpr GamepadAxisCode RightTrigger{ 5 };
    }
}
