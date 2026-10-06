#pragma once

#include <cstddef>
#include <cstdint>

namespace sh3_noslurper
{
constexpr std::uint16_t kBrownSlurperType = 0x020A;
constexpr std::uint16_t kWhiteSlurperType = 0x020B;

constexpr std::size_t kEnemySlotCount = 32;
constexpr std::size_t kEnemySlotSize = 0x160;
constexpr std::size_t kEnemySlotCharacterOffset = 0x04;
constexpr std::size_t kEnemySlotActiveOffset = 0x158;

constexpr std::size_t kCharacterKindOffset = 0x80;
constexpr std::size_t kCharacterCurrentHpOffset = 0x180;
constexpr std::size_t kCharacterMaxHpOffset = 0x184;

struct PatchStats
{
    std::size_t brownKilled = 0;
    std::size_t whiteKilled = 0;

    [[nodiscard]] constexpr std::size_t totalKilled() const noexcept
    {
        return brownKilled + whiteKilled;
    }
};

// Sets only the current HP of an instantiated Slurper to zero.
// Type ID, max HP, model, descriptor variant, and every other field are preserved.
// The caller is responsible for validating that the supplied memory is readable/writable.
PatchStats KillInstantiatedSlurper(std::byte* character) noexcept;
} // namespace sh3_noslurper
