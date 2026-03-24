#pragma once

#include <cstddef>
#include <cstdint>

namespace Life
{
    // -----------------------------------------------------------------------------
    // XxHash64
    //
    // Fast non-cryptographic 64-bit hash used for incremental build caching.
    // -----------------------------------------------------------------------------
    class XxHash64 final
    {
    public:
        struct Seed final
        {
            constexpr Seed() = default;
            explicit constexpr Seed(uint64_t value)
                : Value(value)
            {
            }

            uint64_t Value = 0;
        };

        class State final
        {
        public:
            explicit State(uint64_t seed = 0);

            void Reset(uint64_t seed = 0);
            void Update(const void* data, size_t size);
            uint64_t Digest() const;

        private:
            void UpdateImpl(const uint8_t* input, size_t length);

        private:
            uint64_t m_Seed = 0;
            uint64_t m_TotalLength = 0;

            uint64_t m_V1 = 0;
            uint64_t m_V2 = 0;
            uint64_t m_V3 = 0;
            uint64_t m_V4 = 0;

            uint8_t m_Buffer[32] = {};
            uint32_t m_BufferSize = 0;
        };

        static uint64_t Compute(const void* data, size_t size, Seed seed = {});
    };
}

