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

void WriteU16(std::byte* p, std::uint16_t value)
{
    std::memcpy(p, &value, sizeof(value));
}

std::uint16_t ReadU16(const std::byte* p)
{
    std::uint16_t value = 0;
    std::memcpy(&value, p, sizeof(value));
    return value;
}

std::byte* Record(std::byte* base, std::size_t index)
{
    return base + (index * kEntityRecordSize);
}
} // namespace

int main()
{
    std::array<std::byte, kEntityRecordSize * 6> data{};
    std::byte* const base = data.data();

    // Brown Slurper, normal.
    WriteU16(Record(base, 0), kBrownSlurperType);
    WriteU16(Record(base, 0) + kEntityStateOffset, 0x16);

    // Brown Slurper, faking death.
    WriteU16(Record(base, 1), kBrownSlurperType);
    WriteU16(Record(base, 1) + kEntityStateOffset, 0x18);

    // White Slurper.
    WriteU16(Record(base, 2), kWhiteSlurperType);
    WriteU16(Record(base, 2) + kEntityStateOffset, 0x0000);

    // Unrelated enemy must remain untouched.
    WriteU16(Record(base, 3), 0x0204);
    WriteU16(Record(base, 3) + kEntityStateOffset, 0x0009);

    // Terminator.
    WriteU16(Record(base, 4), 0x0000);

    // Data after terminator must never be touched.
    WriteU16(Record(base, 5), kWhiteSlurperType);
    WriteU16(Record(base, 5) + kEntityStateOffset, 0x1234);

    const PatchStats stats = PatchEntityList(base, 6);

    assert(2 == stats.brownChanged);
    assert(1 == stats.whiteChanged);

    assert(kBrownSlurperType == ReadU16(Record(base, 0)));
    assert(kBrownSlurperDeadState == ReadU16(Record(base, 0) + kEntityStateOffset));

    assert(kBrownSlurperType == ReadU16(Record(base, 1)));
    assert(kBrownSlurperDeadState == ReadU16(Record(base, 1) + kEntityStateOffset));

    assert(kBrownSlurperType == ReadU16(Record(base, 2)));
    assert(kBrownSlurperDeadState == ReadU16(Record(base, 2) + kEntityStateOffset));

    assert(0x0204 == ReadU16(Record(base, 3)));
    assert(0x0009 == ReadU16(Record(base, 3) + kEntityStateOffset));

    assert(kWhiteSlurperType == ReadU16(Record(base, 5)));
    assert(0x1234 == ReadU16(Record(base, 5) + kEntityStateOffset));

    // A second pass should be idempotent.
    const PatchStats second = PatchEntityList(base, 6);
    assert(0 == second.totalChanged());

    std::cout << "NoSlurper patch_core tests passed.\n";
    return 0;
}
