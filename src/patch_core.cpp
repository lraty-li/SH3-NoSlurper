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

PatchStats TransitionSlurperToNativeDead(
    std::byte* enemySlot,
    std::byte* character,
    std::uintptr_t nativeDeadAi,
    std::uintptr_t nativeDeadCallback) noexcept
{
    PatchStats stats{};

    if ((nullptr == enemySlot) || (nullptr == character) ||
        (0 == nativeDeadAi) || (0 == nativeDeadCallback))
    {
        return stats;
    }

    const std::uint16_t kind =
        ReadValue<std::uint16_t>(character + kCharacterKindOffset);

    if ((kBrownSlurperType != kind) && (kWhiteSlurperType != kind))
        return stats;

    const float maxHp =
        ReadValue<float>(character + kCharacterMaxHpOffset);

    // Wait until normal initialization has populated a sane max-HP value.
    if (!std::isfinite(maxHp) || (maxHp <= 0.0f) || (maxHp > 100000.0f))
        return stats;

    const auto currentAi =
        ReadValue<std::uintptr_t>(enemySlot + kEnemySlotFunctionOffset);
    const auto currentCallback =
        ReadValue<std::uintptr_t>(character + kCharacterCallbackOffset);

    // Already transitioned.
    if ((currentAi == nativeDeadAi) &&
        (currentCallback == nativeDeadCallback))
    {
        return stats;
    }

    // This field is the latch checked by both Slurper death callbacks.
    // Reset it so the native death callback performs its first-frame setup.
    WriteValue<std::uint32_t>(
        character + kCharacterDeathLatchOffset, 0u);

    // Keep the original kind/model. Only current HP is zeroed.
    WriteValue<float>(
        character + kCharacterCurrentHpOffset, 0.0f);

    // Ensure the shared Slurper death callback is allowed to run.
    auto battleStatus =
        ReadValue<std::uint32_t>(
            character + kCharacterBattleStatusOffset);
    battleStatus |= kBattleActiveBit;
    WriteValue<std::uint32_t>(
        character + kCharacterBattleStatusOffset,
        battleStatus);

    // sub_474180(character, sub_4F5030) in the game's native dead
    // initializer is simply: character[+0x94] = callback.
    WriteValue<std::uintptr_t>(
        character + kCharacterCallbackOffset,
        nativeDeadCallback);

    // Mirror the work fields written by the game's descriptor-state 0x17
    // initialization before switching the function pointer.
    WriteValue<std::uint16_t>(
        enemySlot + kEnemySlotDeadAnimFlagOffset, 1u);
    WriteValue<std::uint32_t>(
        enemySlot + kEnemySlotDeadParamOffset, 0u);
    WriteValue<std::uint16_t>(
        enemySlot + kEnemySlotStateOffset, kNativeDeadWorkState);
    WriteValue<std::uint16_t>(
        enemySlot + kEnemySlotStepOffset, 0u);

    // sub_4A38B0(slot, sub_4F6110) reduces to slot[+0] = function
    // plus a generic callback reset. The Slurper dead routine itself is
    // generic and does not rewrite character kind/model.
    WriteValue<std::uintptr_t>(
        enemySlot + kEnemySlotFunctionOffset,
        nativeDeadAi);

    if (kBrownSlurperType == kind)
        ++stats.brownTransitioned;
    else
        ++stats.whiteTransitioned;

    return stats;
}
} // namespace sh3_noslurper
