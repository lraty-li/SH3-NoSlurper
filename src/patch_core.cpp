#include "patch_core.hpp"

#include <cmath>
#include <cstring>

namespace sh3_noslurper
{
namespace
{
template <typename T>
T ReadValue(const std::byte* address) noexcept
{
    T value{};
    std::memcpy(&value, address, sizeof(value));
    return value;
}

template <typename T>
void WriteValue(std::byte* address, const T& value) noexcept
{
    std::memcpy(address, &value, sizeof(value));
}
} // namespace

PatchStats KillInstantiatedSlurper(std::byte* character) noexcept
{
    PatchStats stats{};
    if (nullptr == character) return stats;

    const std::uint16_t kind =
        ReadValue<std::uint16_t>(character + kCharacterKindOffset);

    if ((kBrownSlurperType != kind) && (kWhiteSlurperType != kind))
        return stats;

    const float maxHp =
        ReadValue<float>(character + kCharacterMaxHpOffset);
    const float currentHp =
        ReadValue<float>(character + kCharacterCurrentHpOffset);

    // Initialization writes max HP before the enemy begins normal behavior.
    // Waiting for a sane positive max HP avoids racing an incompletely
    // initialized SubCharacter.
    if (!std::isfinite(maxHp) || (maxHp <= 0.0f) || (maxHp > 100000.0f))
        return stats;

    // Already dead/dying Slurpers are left completely untouched.
    if (!std::isfinite(currentHp) || (currentHp <= 0.0f))
        return stats;

    constexpr float zeroHp = 0.0f;
    WriteValue(character + kCharacterCurrentHpOffset, zeroHp);

    if (kBrownSlurperType == kind)
        ++stats.brownKilled;
    else
        ++stats.whiteKilled;

    return stats;
}
} // namespace sh3_noslurper
