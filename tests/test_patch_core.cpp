#include "patch_core.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>

namespace
{
using namespace sh3_noslurper;

template <typename T>
void WriteValue(std::byte* p, const T& value)
{
    std::memcpy(p, &value, sizeof(value));
}

template <typename T>
T ReadValue(const std::byte* p)
{
    T value{};
    std::memcpy(&value, p, sizeof(value));
    return value;
}

void PrepareCharacter(
    std::byte* character,
    std::uint16_t kind,
    float currentHp,
    float maxHp)
{
    WriteValue(character + kCharacterKindOffset, kind);
    WriteValue(character + kCharacterCurrentHpOffset, currentHp);
    WriteValue(character + kCharacterMaxHpOffset, maxHp);
}
} // namespace

int main()
{
    constexpr std::size_t kCharacterSize = 0x1A8;

    std::array<std::byte, kCharacterSize> brown{};
    PrepareCharacter(brown.data(), kBrownSlurperType, 712.5f, 712.5f);

    const PatchStats brownStats = KillInstantiatedSlurper(brown.data());
    assert(1 == brownStats.brownKilled);
    assert(0 == brownStats.whiteKilled);
    assert(kBrownSlurperType ==
           ReadValue<std::uint16_t>(brown.data() + kCharacterKindOffset));
    assert(0.0f ==
           ReadValue<float>(brown.data() + kCharacterCurrentHpOffset));
    assert(712.5f ==
           ReadValue<float>(brown.data() + kCharacterMaxHpOffset));

    std::array<std::byte, kCharacterSize> white{};
    PrepareCharacter(white.data(), kWhiteSlurperType, 900.0f, 900.0f);

    const PatchStats whiteStats = KillInstantiatedSlurper(white.data());
    assert(0 == whiteStats.brownKilled);
    assert(1 == whiteStats.whiteKilled);
    // Critical regression check: the special Slurper remains 0x20B.
    assert(kWhiteSlurperType ==
           ReadValue<std::uint16_t>(white.data() + kCharacterKindOffset));
    assert(0.0f ==
           ReadValue<float>(white.data() + kCharacterCurrentHpOffset));
    assert(900.0f ==
           ReadValue<float>(white.data() + kCharacterMaxHpOffset));

    // Already dead/dying Slurpers are not rewritten.
    const PatchStats second = KillInstantiatedSlurper(white.data());
    assert(0 == second.totalKilled());

    // Non-Slurper enemy remains byte-for-byte unchanged at the fields we know.
    std::array<std::byte, kCharacterSize> other{};
    PrepareCharacter(other.data(), 0x0204, 1234.0f, 1500.0f);
    const PatchStats otherStats = KillInstantiatedSlurper(other.data());
    assert(0 == otherStats.totalKilled());
    assert(0x0204 ==
           ReadValue<std::uint16_t>(other.data() + kCharacterKindOffset));
    assert(1234.0f ==
           ReadValue<float>(other.data() + kCharacterCurrentHpOffset));
    assert(1500.0f ==
           ReadValue<float>(other.data() + kCharacterMaxHpOffset));

    // Do not race a partially initialized character.
    std::array<std::byte, kCharacterSize> uninitialized{};
    PrepareCharacter(uninitialized.data(), kWhiteSlurperType, 1.0f, 0.0f);
    const PatchStats initStats =
        KillInstantiatedSlurper(uninitialized.data());
    assert(0 == initStats.totalKilled());
    assert(1.0f ==
           ReadValue<float>(
               uninitialized.data() + kCharacterCurrentHpOffset));

    std::cout << "NoSlurper runtime-HP tests passed.\n";
    return 0;
}
