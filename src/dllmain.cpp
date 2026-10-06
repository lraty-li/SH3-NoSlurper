#include "patch_core.hpp"

#include <Windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace
{
using namespace sh3_noslurper;

constexpr std::size_t kEnemyTableCount = 40;
constexpr std::size_t kEntityListPointerOffset = 0x10;
constexpr std::size_t kMaxEntityRecordsPerList = 2048;
constexpr DWORD kPatchIntervalMs = 250;

HMODULE g_module = nullptr;
volatile LONG g_stopRequested = 0;

bool IsReadableRange(const void* address, std::size_t size) noexcept
{
    if ((nullptr == address) || (0 == size)) return false;

    auto current = reinterpret_cast<std::uintptr_t>(address);
    const auto end = current + size;
    if (end < current) return false;

    while (current < end)
    {
        MEMORY_BASIC_INFORMATION mbi{};
        if (0 == VirtualQuery(reinterpret_cast<const void*>(current), &mbi, sizeof(mbi))) return false;
        if (MEM_COMMIT != mbi.State) return false;
        if (0 != (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;

        const DWORD protection = mbi.Protect & 0xFF;
        const bool readable =
            (PAGE_READONLY == protection) ||
            (PAGE_READWRITE == protection) ||
            (PAGE_WRITECOPY == protection) ||
            (PAGE_EXECUTE == protection) ||
            (PAGE_EXECUTE_READ == protection) ||
            (PAGE_EXECUTE_READWRITE == protection) ||
            (PAGE_EXECUTE_WRITECOPY == protection);
        if (!readable) return false;

        const auto regionBegin = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
        const auto regionEnd = regionBegin + mbi.RegionSize;
        if (regionEnd <= current) return false;
        current = regionEnd;
    }

    return true;
}

bool IsWritableRange(const void* address, std::size_t size) noexcept
{
    if (!IsReadableRange(address, size)) return false;

    auto current = reinterpret_cast<std::uintptr_t>(address);
    const auto end = current + size;

    while (current < end)
    {
        MEMORY_BASIC_INFORMATION mbi{};
        if (0 == VirtualQuery(reinterpret_cast<const void*>(current), &mbi, sizeof(mbi))) return false;

        const DWORD protection = mbi.Protect & 0xFF;
        const bool writable =
            (PAGE_READWRITE == protection) ||
            (PAGE_WRITECOPY == protection) ||
            (PAGE_EXECUTE_READWRITE == protection) ||
            (PAGE_EXECUTE_WRITECOPY == protection);
        if (!writable) return false;

        const auto regionBegin = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
        const auto regionEnd = regionBegin + mbi.RegionSize;
        if (regionEnd <= current) return false;
        current = regionEnd;
    }

    return true;
}

struct ImageRange
{
    std::byte* base = nullptr;
    std::size_t size = 0;
};

ImageRange GetTextSection() noexcept
{
    auto* const base = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    if (nullptr == base) return {};

    const auto* const dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if ((IMAGE_DOS_SIGNATURE != dos->e_magic) || (dos->e_lfanew <= 0)) return {};

    const auto* const nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    if (IMAGE_NT_SIGNATURE != nt->Signature) return {};
    if (IMAGE_NT_OPTIONAL_HDR32_MAGIC != nt->OptionalHeader.Magic) return {};

    const IMAGE_SECTION_HEADER* const sections = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i)
    {
        char name[9]{};
        std::memcpy(name, sections[i].Name, 8);

        if (0 == std::strcmp(name, ".text"))
        {
            return {
                base + sections[i].VirtualAddress,
                static_cast<std::size_t>(sections[i].Misc.VirtualSize)
            };
        }
    }

    return {};
}

std::uintptr_t FindEnemyTable() noexcept
{
    const ImageRange text = GetTextSection();
    if ((nullptr == text.base) || (text.size < 32)) return 0;

    // Known SH3 code shape:
    //   cmp ecx, dword ptr [eax*4 + enemyTable]
    //   je  +0x19
    //   ...
    //   mov eax, dword ptr [edx*4 + enemyTable]
    //
    // We intentionally discover the absolute table address from the code instead
    // of hard-coding 0x006CF7D0, which makes this safer across compatible builds.
    for (std::size_t i = 0; (i + 32) < text.size; ++i)
    {
        const auto* const p = reinterpret_cast<const std::uint8_t*>(text.base + i);

        if ((0x3B != p[0]) || (0x0C != p[1]) || (0x85 != p[2]) ||
            (0x74 != p[7]) || (0x19 != p[8]))
        {
            continue;
        }

        std::uint32_t firstAddress = 0;
        std::memcpy(&firstAddress, p + 3, sizeof(firstAddress));
        if (0 == firstAddress) continue;

        for (std::size_t j = 9; j < 28; ++j)
        {
            if ((0x8B == p[j]) && (0x04 == p[j + 1]) && (0x95 == p[j + 2]))
            {
                std::uint32_t secondAddress = 0;
                std::memcpy(&secondAddress, p + j + 3, sizeof(secondAddress));

                if (firstAddress == secondAddress)
                {
                    const auto table = static_cast<std::uintptr_t>(firstAddress);
                    if (IsReadableRange(
                            reinterpret_cast<const void*>(table),
                            kEnemyTableCount * sizeof(std::uint32_t)))
                    {
                        return table;
                    }
                }
            }
        }
    }

    return 0;
}

PatchStats PatchAllSlurpers(std::uintptr_t tableAddress) noexcept
{
    PatchStats total{};

    for (std::size_t tableIndex = 0; tableIndex < kEnemyTableCount; ++tableIndex)
    {
        const auto slotAddress = tableAddress + (tableIndex * sizeof(std::uint32_t));
        if (!IsReadableRange(reinterpret_cast<const void*>(slotAddress), sizeof(std::uint32_t))) continue;

        std::uint32_t ownerAddress32 = 0;
        std::memcpy(&ownerAddress32, reinterpret_cast<const void*>(slotAddress), sizeof(ownerAddress32));
        if (0 == ownerAddress32) continue;

        const auto ownerAddress = static_cast<std::uintptr_t>(ownerAddress32);
        const auto entityListSlot = ownerAddress + kEntityListPointerOffset;
        if (!IsReadableRange(reinterpret_cast<const void*>(entityListSlot), sizeof(std::uint32_t))) continue;

        std::uint32_t entityListAddress32 = 0;
        std::memcpy(
            &entityListAddress32,
            reinterpret_cast<const void*>(entityListSlot),
            sizeof(entityListAddress32));
        if (0 == entityListAddress32) continue;

        auto* const list = reinterpret_cast<std::byte*>(
            static_cast<std::uintptr_t>(entityListAddress32));

        // Validate record-by-record because lists can cross memory-region boundaries.
        for (std::size_t i = 0; i < kMaxEntityRecordsPerList; ++i)
        {
            std::byte* const record = list + (i * kEntityRecordSize);
            if (!IsWritableRange(record, kEntityRecordSize)) break;

            std::uint16_t type = 0;
            std::memcpy(&type, record, sizeof(type));
            if (0 == type) break;

            if ((kBrownSlurperType != type) && (kWhiteSlurperType != type)) continue;

            PatchStats one = PatchEntityList(record, 1);
            total.brownChanged += one.brownChanged;
            total.whiteChanged += one.whiteChanged;
        }
    }

    return total;
}

void AppendLogLine(const char* text) noexcept
{
    if ((nullptr == g_module) || (nullptr == text)) return;

    wchar_t modulePath[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(g_module, modulePath, MAX_PATH);
    if ((0 == length) || (length >= MAX_PATH)) return;

    wchar_t* const slash = wcsrchr(modulePath, L'\\');
    if (nullptr == slash) return;
    *(slash + 1) = L'\0';
    wcscat_s(modulePath, L"NoSlurper.log");

    HANDLE file = CreateFileW(
        modulePath,
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (INVALID_HANDLE_VALUE == file) return;

    DWORD written = 0;
    WriteFile(file, text, static_cast<DWORD>(std::strlen(text)), &written, nullptr);
    WriteFile(file, "\r\n", 2, &written, nullptr);
    CloseHandle(file);
}

DWORD WINAPI WorkerThread(void*) noexcept
{
    AppendLogLine("NoSlurper: worker started.");

    std::uintptr_t tableAddress = 0;
    while ((0 == InterlockedCompareExchange(&g_stopRequested, 0, 0)) && (0 == tableAddress))
    {
        tableAddress = FindEnemyTable();
        if (0 == tableAddress) Sleep(250);
    }

    if (0 == tableAddress)
    {
        AppendLogLine("NoSlurper: stopped before enemy table was found.");
        return 0;
    }

    {
        char line[128]{};
        std::snprintf(
            line,
            sizeof(line),
            "NoSlurper: enemy table found at 0x%08X.",
            static_cast<unsigned int>(tableAddress));
        AppendLogLine(line);
    }

    std::size_t cumulativeBrown = 0;
    std::size_t cumulativeWhite = 0;

    while (0 == InterlockedCompareExchange(&g_stopRequested, 0, 0))
    {
        const PatchStats stats = PatchAllSlurpers(tableAddress);

        if (stats.totalChanged() > 0)
        {
            cumulativeBrown += stats.brownChanged;
            cumulativeWhite += stats.whiteChanged;

            char line[160]{};
            std::snprintf(
                line,
                sizeof(line),
                "NoSlurper: patched %zu Slurper(s) this pass (brown=%zu, white=%zu; cumulative=%zu).",
                stats.totalChanged(),
                stats.brownChanged,
                stats.whiteChanged,
                cumulativeBrown + cumulativeWhite);
            AppendLogLine(line);
        }

        Sleep(kPatchIntervalMs);
    }

    AppendLogLine("NoSlurper: worker stopped.");
    return 0;
}
} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (DLL_PROCESS_ATTACH == reason)
    {
        g_module = module;
        DisableThreadLibraryCalls(module);

        HANDLE thread = CreateThread(nullptr, 0, WorkerThread, nullptr, 0, nullptr);
        if (nullptr != thread) CloseHandle(thread);
    }
    else if (DLL_PROCESS_DETACH == reason)
    {
        InterlockedExchange(&g_stopRequested, 1);
    }

    return TRUE;
}
