#include "patch_core.hpp"

#include <Windows.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace
{
using namespace sh3_noslurper;

constexpr DWORD kPatchIntervalMs = 16;

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
        if (0 == VirtualQuery(
                     reinterpret_cast<const void*>(current),
                     &mbi,
                     sizeof(mbi)))
        {
            return false;
        }

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

        const auto regionBegin =
            reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
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
        if (0 == VirtualQuery(
                     reinterpret_cast<const void*>(current),
                     &mbi,
                     sizeof(mbi)))
        {
            return false;
        }

        const DWORD protection = mbi.Protect & 0xFF;
        const bool writable =
            (PAGE_READWRITE == protection) ||
            (PAGE_WRITECOPY == protection) ||
            (PAGE_EXECUTE_READWRITE == protection) ||
            (PAGE_EXECUTE_WRITECOPY == protection);

        if (!writable) return false;

        const auto regionBegin =
            reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
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
    auto* const base =
        reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    if (nullptr == base) return {};

    const auto* const dos =
        reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if ((IMAGE_DOS_SIGNATURE != dos->e_magic) ||
        (dos->e_lfanew <= 0))
    {
        return {};
    }

    const auto* const nt =
        reinterpret_cast<const IMAGE_NT_HEADERS32*>(
            base + dos->e_lfanew);
    if (IMAGE_NT_SIGNATURE != nt->Signature) return {};
    if (IMAGE_NT_OPTIONAL_HDR32_MAGIC != nt->OptionalHeader.Magic)
        return {};

    const IMAGE_SECTION_HEADER* const sections =
        IMAGE_FIRST_SECTION(nt);

    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i)
    {
        char name[9]{};
        std::memcpy(name, sections[i].Name, 8);

        if (0 == std::strcmp(name, ".text"))
        {
            return {
                base + sections[i].VirtualAddress,
                static_cast<std::size_t>(
                    sections[i].Misc.VirtualSize)
            };
        }
    }

    return {};
}

std::uintptr_t FindEnemyManager() noexcept
{
    const ImageRange text = GetTextSection();
    if ((nullptr == text.base) || (text.size < 48)) return 0;

    // Signature from the beginning of the game's enemy-manager allocator.
    //
    //   mov ecx, enemyManager
    //   xor eax, eax
    //   jmp short ...
    //   ...
    //   mov dl, [ecx+158h]   ; slot active/index byte
    //   ...
    //   add ecx, 160h        ; EnemyData slot size
    //   cmp eax, 20h         ; 32 slots
    //
    // The manager address itself is extracted from the mov-immediate operand,
    // so the absolute address is not hard-coded.
    for (std::size_t i = 0; (i + 40) < text.size; ++i)
    {
        const auto* const p =
            reinterpret_cast<const std::uint8_t*>(text.base + i);

        if ((0xB9 != p[0]) ||
            (0x33 != p[5]) || (0xC0 != p[6]) ||
            (0xEB != p[7]) || (0x07 != p[8]) ||
            (0x8A != p[16]) || (0x91 != p[17]) ||
            (0x58 != p[18]) || (0x01 != p[19]) ||
            (0x84 != p[22]) || (0xD2 != p[23]) ||
            (0x74 != p[24]) || (0x0D != p[25]) ||
            (0x40 != p[26]) ||
            (0x81 != p[27]) || (0xC1 != p[28]) ||
            (0x60 != p[29]) || (0x01 != p[30]) ||
            (0x83 != p[33]) || (0xF8 != p[34]) ||
            (0x20 != p[35]) ||
            (0x7C != p[36]) || (0xEA != p[37]))
        {
            continue;
        }

        std::uint32_t managerAddress32 = 0;
        std::memcpy(
            &managerAddress32,
            p + 1,
            sizeof(managerAddress32));

        const auto managerAddress =
            static_cast<std::uintptr_t>(managerAddress32);

        if (IsReadableRange(
                reinterpret_cast<const void*>(managerAddress),
                kEnemySlotCount * kEnemySlotSize))
        {
            return managerAddress;
        }
    }

    return 0;
}

PatchStats KillAllInstantiatedSlurpers(
    std::uintptr_t managerAddress) noexcept
{
    PatchStats total{};

    for (std::size_t i = 0; i < kEnemySlotCount; ++i)
    {
        auto* const slot = reinterpret_cast<std::byte*>(
            managerAddress + (i * kEnemySlotSize));

        if (!IsReadableRange(
                slot + kEnemySlotActiveOffset,
                sizeof(std::uint8_t)))
        {
            continue;
        }

        const auto active =
            *reinterpret_cast<const std::uint8_t*>(
                slot + kEnemySlotActiveOffset);
        if (0 == active) continue;

        if (!IsReadableRange(
                slot + kEnemySlotCharacterOffset,
                sizeof(std::uint32_t)))
        {
            continue;
        }

        std::uint32_t characterAddress32 = 0;
        std::memcpy(
            &characterAddress32,
            slot + kEnemySlotCharacterOffset,
            sizeof(characterAddress32));
        if (0 == characterAddress32) continue;

        auto* const character = reinterpret_cast<std::byte*>(
            static_cast<std::uintptr_t>(characterAddress32));

        if (!IsReadableRange(
                character + kCharacterKindOffset,
                sizeof(std::uint16_t)))
        {
            continue;
        }

        std::uint16_t kind = 0;
        std::memcpy(
            &kind,
            character + kCharacterKindOffset,
            sizeof(kind));

        if ((kBrownSlurperType != kind) &&
            (kWhiteSlurperType != kind))
        {
            continue;
        }

        if (!IsReadableRange(
                character + kCharacterMaxHpOffset,
                sizeof(float)) ||
            !IsWritableRange(
                character + kCharacterCurrentHpOffset,
                sizeof(float)))
        {
            continue;
        }

        const PatchStats one =
            KillInstantiatedSlurper(character);
        total.brownKilled += one.brownKilled;
        total.whiteKilled += one.whiteKilled;
    }

    return total;
}

void AppendLogLine(const char* text) noexcept
{
    if ((nullptr == g_module) || (nullptr == text)) return;

    wchar_t modulePath[MAX_PATH]{};
    const DWORD length =
        GetModuleFileNameW(g_module, modulePath, MAX_PATH);

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
    WriteFile(
        file,
        text,
        static_cast<DWORD>(std::strlen(text)),
        &written,
        nullptr);
    WriteFile(file, "\r\n", 2, &written, nullptr);
    CloseHandle(file);
}

DWORD WINAPI WorkerThread(void*) noexcept
{
    AppendLogLine(
        "NoSlurper v0.2.1: runtime-HP worker started.");

    std::uintptr_t managerAddress = 0;

    while ((0 == InterlockedCompareExchange(
                     &g_stopRequested, 0, 0)) &&
           (0 == managerAddress))
    {
        managerAddress = FindEnemyManager();
        if (0 == managerAddress) Sleep(250);
    }

    if (0 == managerAddress)
    {
        AppendLogLine(
            "NoSlurper v0.2.1: stopped before enemy manager was found.");
        return 0;
    }

    {
        char line[160]{};
        std::snprintf(
            line,
            sizeof(line),
            "NoSlurper v0.2.1: enemy manager found at 0x%08X.",
            static_cast<unsigned int>(managerAddress));
        AppendLogLine(line);
    }

    std::size_t cumulativeBrown = 0;
    std::size_t cumulativeWhite = 0;

    while (0 == InterlockedCompareExchange(
                    &g_stopRequested, 0, 0))
    {
        const PatchStats stats =
            KillAllInstantiatedSlurpers(managerAddress);

        if (stats.totalKilled() > 0)
        {
            cumulativeBrown += stats.brownKilled;
            cumulativeWhite += stats.whiteKilled;

            char line[192]{};
            std::snprintf(
                line,
                sizeof(line),
                "NoSlurper v0.2.1: zeroed HP for %zu Slurper(s) "
                "(0x20A=%zu, 0x20B=%zu; cumulative=%zu).",
                stats.totalKilled(),
                stats.brownKilled,
                stats.whiteKilled,
                cumulativeBrown + cumulativeWhite);
            AppendLogLine(line);
        }

        Sleep(kPatchIntervalMs);
    }

    AppendLogLine("NoSlurper v0.2.1: worker stopped.");
    return 0;
}
} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (DLL_PROCESS_ATTACH == reason)
    {
        g_module = module;
        DisableThreadLibraryCalls(module);

        HANDLE thread = CreateThread(
            nullptr,
            0,
            WorkerThread,
            nullptr,
            0,
            nullptr);

        if (nullptr != thread) CloseHandle(thread);
    }
    else if (DLL_PROCESS_DETACH == reason)
    {
        InterlockedExchange(&g_stopRequested, 1);
    }

    return TRUE;
}
