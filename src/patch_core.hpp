#pragma once

#include <cstddef>
#include <cstdint>

namespace sh3_noslurper
{
constexpr std::uint16_t kBrownSlurperType = 0x020A;
constexpr std::uint16_t kWhiteSlurperType = 0x020B;

constexpr std::size_t kEnemySlotCount = 32;
constexpr std::size_t kEnemySlotSize = 0x160;
constexpr std::size_t kEnemySlotFunctionOffset = 0x00;
constexpr std::size_t kEnemySlotCharacterOffset = 0x04;
constexpr std::size_t kEnemySlotDeadAnimFlagOffset = 0x90;
constexpr std::size_t kEnemySlotDeadParamOffset = 0x148;
constexpr std::size_t kEnemySlotStateOffset = 0x152;
constexpr std::size_t kEnemySlotStepOffset = 0x154;
constexpr std::size_t kEnemySlotActiveOffset = 0x158;

constexpr std::size_t kCharacterDeathLatchOffset = 0x7C;
constexpr std::size_t kCharacterKindOffset = 0x80;
constexpr std::size_t kCharacterCallbackOffset = 0x94;
constexpr std::size_t kCharacterCurrentHpOffset = 0x180;
constexpr std::size_t kCharacterMaxHpOffset = 0x184;
constexpr std::size_t kCharacterBattleStatusOffset = 0x1A4;

constexpr std::uint16_t kNativeDeadWorkState = 41;
constexpr std::uint32_t kBattleActiveBit = 0x2000;

struct PatchStats
{
    std::size_t brownTransitioned = 0;
    std::size_t whiteTransitioned = 0;

    [[nodiscard]] constexpr std::size_t totalTransitioned() const noexcept
    {
        return brownTransitioned + whiteTransitioned;
    }
};

// Mirrors the game's native Brown-Slurper descriptor-state 0x17 ("Dead")
// initialization on an already-instantiated Slurper, but deliberately does
// NOT change the character kind/type. This preserves 0x20A vs 0x20B model
// selection while switching the work item to the game's inert/dead logic.
PatchStats TransitionSlurperToNativeDead(
    std::byte* enemySlot,
    std::byte* character,
    std::uintptr_t nativeDeadAi,
    std::uintptr_t nativeDeadCallback) noexcept;
} // namespace sh3_noslurper
