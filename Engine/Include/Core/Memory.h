#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace Life
{
    template<typename T, typename Deleter = std::default_delete<T>>
    using Scope = std::unique_ptr<T, Deleter>;

    template<typename T, typename... Args>
    Scope<T> CreateScope(Args&&... args)
    {
        return std::make_unique<T>(std::forward<Args>(args)...);
    }

    template<typename T>
    using Ref = std::shared_ptr<T>;

    template<typename T>
    using WeakRef = std::weak_ptr<T>;

    template<typename T>
    using Optional = std::optional<T>;

    template<typename T>
    using OptionalRef = std::optional<std::reference_wrapper<T>>;

    template<typename T>
    using OptionalConstRef = std::optional<std::reference_wrapper<const T>>;

    inline constexpr std::nullopt_t NullOpt = std::nullopt;

    template<typename T, typename... Args>
    Ref<T> CreateRef(Args&&... args)
    {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }

    template<typename T>
    WeakRef<T> CreateWeakRef(const Ref<T>& reference) noexcept
    {
        return WeakRef<T>(reference);
    }

    template<typename T>
    std::reference_wrapper<T> RefOf(T& value) noexcept
    {
        return std::ref(value);
    }

    template<typename T>
    std::reference_wrapper<const T> RefOf(const T& value) noexcept
    {
        return std::cref(value);
    }

    template<typename T>
    OptionalRef<T> MakeOptionalRef(T& value) noexcept
    {
        return RefOf(value);
    }

    template<typename T>
    OptionalRef<T> MakeOptionalRef(T* value) noexcept
    {
        if (value == nullptr)
            return NullOpt;

        return RefOf(*value);
    }

    template<typename T>
    OptionalConstRef<T> MakeOptionalRef(const T& value) noexcept
    {
        return RefOf(value);
    }

    template<typename T>
    OptionalConstRef<T> MakeOptionalRef(const T* value) noexcept
    {
        if (value == nullptr)
            return NullOpt;

        return RefOf(*value);
    }
}
