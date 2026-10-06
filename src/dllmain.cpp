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

constexpr std::uintptr_t kNativeDeadAiRva = 0x000F6110;
constexpr std::uintptr_t kNativeDeadCallbackRva = 0x000F5030;

bool ResolveNativeDeadFunctions(
    std::uintptr_t& deadAi,
    std::uintptr_t& deadCallback) noexcept
{
    const auto base = reinterpret_cast<std::uintptr_t>(
        GetModuleHandleW(nullptr));
    if (0 == base) return false;

    deadAi = base + kNativeDeadAiRva;
    deadCallback = base + kNativeDeadCallbackRva;

    static constexpr std::uint8_t kDeadAiSig[] = {
        0x56, 0x8B, 0x74, 0x24, 0x08, 0x66, 0x83, 0xBE,
        0x54, 0x01, 0x00, 0x00, 0x00, 0x57, 0x8B, 0x7E
    };
    static constexpr std::uint8_t kDeadCallbackSig[] = {
        0x56, 0x8B, 0x74, 0x24, 0x08, 0x8B, 0x46, 0x7C,
        0x85, 0xC0, 0x75, 0x33, 0x8B, 0x86, 0xA4, 0x01
    };

    if (!IsBadReadPtr(
            reinterpret_cast<const void*>(deadAi),
            sizeof(kDeadAiSig)) &&
        !IsBadReadPtr(
            reinterpret_cast<const void*>(deadCallback),
            sizeof(kDeadCallbackSig)))
    {
        return
            (0 == std::memcmp(
                reinterpret_cast<const void*>(deadAi),
                kDeadAiSig,
                sizeof(kDeadAiSig))) &&
            (0 == std::memcmp(
                reinterpret_cast<const void*>(deadCallback),
                kDeadCallbackSig,
                sizeof(kDeadCallbackSig)));
    }

    return false;
}

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

PatchStats TransitionAllSlurpersToNativeDead(
    std::uintptr_t managerAddress,
    std::uintptr_t nativeDeadAi,
    std::uintptr_t nativeDeadCallback) noexcept
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

        const bool slotWritable =
            IsWritableRange(
                slot + kEnemySlotFunctionOffset,
                sizeof(std::uintptr_t)) &&
            IsWritableRange(
                slot + kEnemySlotDeadAnimFlagOffset,
                sizeof(std::uint16_t)) &&
            IsWritableRange(
                slot + kEnemySlotDeadParamOffset,
                0x10);

        const bool characterWritable =
            IsWritableRange(
                character + kCharacterDeathLatchOffset,
                sizeof(std::uint32_t)) &&
            IsWritableRange(
                character + kCharacterCallbackOffset,
                sizeof(std::uintptr_t)) &&
            IsWritableRange(
                character + kCharacterCurrentHpOffset,
                sizeof(float)) &&
            IsReadableRange(
                character + kCharacterMaxHpOffset,
                sizeof(float)) &&
            IsWritableRange(
                character + kCharacterBattleStatusOffset,
                sizeof(std::uint32_t));

        if (!slotWritable || !characterWritable) continue;

        const PatchStats one =
            TransitionSlurperToNativeDead(
                slot,
                character,
                nativeDeadAi,
                nativeDeadCallback);

        total.brownTransitioned += one.brownTransitioned;
        total.whiteTransitioned += one.whiteTransitioned;
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
        "NoSlurper v0.3.0: native-dead-state worker started.");

    std::uintptr_t nativeDeadAi = 0;
    std::uintptr_t nativeDeadCallback = 0;

    if (!ResolveNativeDeadFunctions(
            nativeDeadAi,
            nativeDeadCallback))
    {
        AppendLogLine(
            "NoSlurper v0.3.0: native dead functions did not match "
            "the installed executable; no patch applied.");
        return 0;
    }

    {
        char line[192]{};
        std::snprintf(
            line,
            sizeof(line),
            "NoSlurper v0.3.0: native dead AI=0x%08X, "
            "callback=0x%08X.",
            static_cast<unsigned int>(nativeDeadAi),
            static_cast<unsigned int>(nativeDeadCallback));
        AppendLogLine(line);
    }

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
            "NoSlurper v0.3.0: stopped before enemy manager was found.");
        return 0;
    }

    {
        char line[160]{};
        std::snprintf(
            line,
            sizeof(line),
            "NoSlurper v0.3.0: enemy manager found at 0x%08X.",
            static_cast<unsigned int>(managerAddress));
        AppendLogLine(line);
    }

    std::size_t cumulativeBrown = 0;
    std::size_t cumulativeWhite = 0;

    while (0 == InterlockedCompareExchange(
                    &g_stopRequested, 0, 0))
    {
        const PatchStats stats =
            TransitionAllSlurpersToNativeDead(
                managerAddress,
                nativeDeadAi,
                nativeDeadCallback);

        if (stats.totalTransitioned() > 0)
        {
            cumulativeBrown += stats.brownTransitioned;
            cumulativeWhite += stats.whiteTransitioned;

            char line[224]{};
            std::snprintf(
                line,
                sizeof(line),
                "NoSlurper v0.3.0: transitioned %zu Slurper(s) "
                "to native dead state (0x20A=%zu, 0x20B=%zu; "
                "cumulative=%zu).",
                stats.totalTransitioned(),
                stats.brownTransitioned,
                stats.whiteTransitioned,
                cumulativeBrown + cumulativeWhite);
            AppendLogLine(line);
        }

        Sleep(kPatchIntervalMs);
    }

    AppendLogLine("NoSlurper v0.3.0: worker stopped.");
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
