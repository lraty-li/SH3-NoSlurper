#pragma once

#include <cstddef>
#include <cstdint>

namespace sh3_noslurper
{
constexpr std::uint16_t kBrownSlurperType = 0x020A;
constexpr std::uint16_t kWhiteSlurperType = 0x020B;
constexpr std::uint16_t kBrownSlurperDeadState = 0x0017;
constexpr std::size_t kEntityRecordSize = 0x18;
constexpr std::size_t kEntityStateOffset = 0x16;

struct PatchStats
{
    std::size_t brownChanged = 0;
    std::size_t whiteChanged = 0;

    [[nodiscard]] constexpr std::size_t totalChanged() const noexcept
    {
        return brownChanged + whiteChanged;
    }
};

// Patches a valid, writable SH3 entity-descriptor list in place.
// The list is terminated by a record whose type ID is 0.
// maxRecords is a safety cap against malformed or unexpected data.
PatchStats PatchEntityList(std::byte* list, std::size_t maxRecords) noexcept;
} // namespace sh3_noslurper
