#include "patch_core.hpp"

#include <cstring>

namespace sh3_noslurper
{
namespace
{
std::uint16_t ReadU16(const std::byte* address) noexcept
{
    std::uint16_t value = 0;
    std::memcpy(&value, address, sizeof(value));
    return value;
}

void WriteU16(std::byte* address, std::uint16_t value) noexcept
{
    std::memcpy(address, &value, sizeof(value));
}
} // namespace

PatchStats PatchEntityList(std::byte* list, std::size_t maxRecords) noexcept
{
    PatchStats stats{};

    if (nullptr == list) return stats;

    for (std::size_t i = 0; i < maxRecords; ++i)
    {
        std::byte* const record = list + (i * kEntityRecordSize);
        const std::uint16_t type = ReadU16(record);

        if (0 == type) break;

        std::byte* const state = record + kEntityStateOffset;

        if (kBrownSlurperType == type)
        {
            if (ReadU16(state) != kBrownSlurperDeadState)
            {
                WriteU16(state, kBrownSlurperDeadState);
                ++stats.brownChanged;
            }
        }
        else if (kWhiteSlurperType == type)
        {
            // White Slurper has no known "dead" variant in the discovered table.
            // Convert it to Brown Slurper only after setting the Brown dead state.
            // This ordering minimizes the window in which the game could observe
            // an alive Brown Slurper during a concurrent read.
            WriteU16(state, kBrownSlurperDeadState);
            WriteU16(record, kBrownSlurperType);
            ++stats.whiteChanged;
        }
    }

    return stats;
}
} // namespace sh3_noslurper
