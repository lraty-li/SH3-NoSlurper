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

void Prepare(
    std::byte* slot,
    std::byte* character,
    std::uint16_t kind,
    float currentHp,
    float maxHp,
    std::uintptr_t ai,
    std::uintptr_t callback)
{
    WriteValue(slot + kEnemySlotFunctionOffset, ai);
    WriteValue(character + kCharacterKindOffset, kind);
    WriteValue(character + kCharacterCallbackOffset, callback);
    WriteValue(character + kCharacterCurrentHpOffset, currentHp);
    WriteValue(character + kCharacterMaxHpOffset, maxHp);
    WriteValue<std::uint32_t>(
        character + kCharacterDeathLatchOffset, 7u);
    WriteValue<std::uint32_t>(
        character + kCharacterBattleStatusOffset, 0x40u);
}
} // namespace

int main()
{
    constexpr std::size_t kCharacterSize = 0x1B0;
    constexpr std::uintptr_t kDeadAi = 0x004F6110;
    constexpr std::uintptr_t kDeadCallback = 0x004F5030;
    constexpr std::uintptr_t kLiveAi = 0x004FE480;
    constexpr std::uintptr_t kLiveCallback = 0x004F4F70;

    std::array<std::byte, kEnemySlotSize> whiteSlot{};
    std::array<std::byte, kCharacterSize> white{};
    Prepare(
        whiteSlot.data(),
        white.data(),
        kWhiteSlurperType,
        900.0f,
        900.0f,
        kLiveAi,
        kLiveCallback);

    const PatchStats whiteStats =
        TransitionSlurperToNativeDead(
            whiteSlot.data(),
            white.data(),
            kDeadAi,
            kDeadCallback);

    assert(1 == whiteStats.whiteTransitioned);
    assert(0 == whiteStats.brownTransitioned);

    // Critical narrative-preservation checks.
    assert(kWhiteSlurperType ==
           ReadValue<std::uint16_t>(
               white.data() + kCharacterKindOffset));
    assert(900.0f ==
           ReadValue<float>(
               white.data() + kCharacterMaxHpOffset));

    // Native dead-state transition.
    assert(0.0f ==
           ReadValue<float>(
               white.data() + kCharacterCurrentHpOffset));
    assert(0u ==
           ReadValue<std::uint32_t>(
               white.data() + kCharacterDeathLatchOffset));
    assert(0 !=
           (ReadValue<std::uint32_t>(
                white.data() + kCharacterBattleStatusOffset) &
            kBattleActiveBit));
    assert(kDeadCallback ==
           ReadValue<std::uintptr_t>(
               white.data() + kCharacterCallbackOffset));
    assert(kDeadAi ==
           ReadValue<std::uintptr_t>(
               whiteSlot.data() + kEnemySlotFunctionOffset));
    assert(1u ==
           ReadValue<std::uint16_t>(
               whiteSlot.data() + kEnemySlotDeadAnimFlagOffset));
    assert(0u ==
           ReadValue<std::uint32_t>(
               whiteSlot.data() + kEnemySlotDeadParamOffset));
    assert(kNativeDeadWorkState ==
           ReadValue<std::uint16_t>(
               whiteSlot.data() + kEnemySlotStateOffset));
    assert(0u ==
           ReadValue<std::uint16_t>(
               whiteSlot.data() + kEnemySlotStepOffset));

    // Idempotent after transition.
    const PatchStats second =
        TransitionSlurperToNativeDead(
            whiteSlot.data(),
            white.data(),
            kDeadAi,
            kDeadCallback);
    assert(0 == second.totalTransitioned());

    // Brown variant is also preserved.
    std::array<std::byte, kEnemySlotSize> brownSlot{};
    std::array<std::byte, kCharacterSize> brown{};
    Prepare(
        brownSlot.data(),
        brown.data(),
        kBrownSlurperType,
        650.0f,
        650.0f,
        kLiveAi,
        kLiveCallback);
    const PatchStats brownStats =
        TransitionSlurperToNativeDead(
            brownSlot.data(),
            brown.data(),
            kDeadAi,
            kDeadCallback);
    assert(1 == brownStats.brownTransitioned);
    assert(kBrownSlurperType ==
           ReadValue<std::uint16_t>(
               brown.data() + kCharacterKindOffset));

    // Other enemies remain untouched.
    std::array<std::byte, kEnemySlotSize> otherSlot{};
    std::array<std::byte, kCharacterSize> other{};
    Prepare(
        otherSlot.data(),
        other.data(),
        0x0204,
        1234.0f,
        1500.0f,
        0x11111111,
        0x22222222);
    const auto otherBefore = other;
    const auto otherSlotBefore = otherSlot;
    const PatchStats otherStats =
        TransitionSlurperToNativeDead(
            otherSlot.data(),
            other.data(),
            kDeadAi,
            kDeadCallback);
    assert(0 == otherStats.totalTransitioned());
    assert(0 == std::memcmp(
        other.data(), otherBefore.data(), other.size()));
    assert(0 == std::memcmp(
        otherSlot.data(), otherSlotBefore.data(), otherSlot.size()));

    std::cout << "NoSlurper native-dead-state tests passed.\n";
    return 0;
}
