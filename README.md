# SH3-NoSlurper

A small ASI mod for the Windows PC version of **Silent Hill 3**.

It disables Slurper enemies while preserving their original type, model, color/variant, and scene placement. The mod does not replace one Slurper variant with another.

## Behavior

Silent Hill 3 uses these runtime character kinds:

- Slurper E: `0x20A`
- Slurper X: `0x20B`

For an instantiated Slurper, the mod mirrors the game's native dead-state setup:

- keeps the original `0x20A` or `0x20B` kind unchanged;
- keeps model/material/scene selection unchanged;
- keeps max HP unchanged;
- sets current HP to zero;
- switches the enemy work function to the game's native Slurper dead AI;
- switches the character callback to the game's native Slurper death callback;
- initializes the same dead-state work fields used by the game's built-in dead Slurper descriptor.

Other enemy types are ignored.

## Install

Requirements:

- Windows PC version of Silent Hill 3
- an ASI loader compatible with the game

Build **Release | Win32**, then copy:

`build\Release\NoSlurper.asi`

to:

`<SH3_DIR>\scripts\NoSlurper.asi`

The mod writes `NoSlurper.log` next to the ASI.

To uninstall, remove `NoSlurper.asi`. The mod does not edit save files, game archives, or the executable on disk.

## Build

Requirements:

- Visual Studio 2022 with C++ desktop tools
- CMake 3.24+
- Win32/x86 target

Example:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A Win32
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The output is:

`build\Release\NoSlurper.asi`

## Compatibility

The current implementation was reverse-engineered and tested against one 32-bit Windows executable build with these fingerprints:

- CRC32: `6368fb1f`
- SHA-256: `3B8B78D0D3C8E266FA17CE983E59F259413A968015249F3BE90C356B0F815122`

The enemy-manager address is discovered through a code signature rather than a hard-coded absolute address. The native dead-AI and death-callback code are also signature-validated before the mod applies changes. If validation fails, the mod does nothing.

## Reverse-engineering summary

IDA analysis identified:

- enemy-manager allocator: `0x004A2E20`
- enemy-manager slot count: `32`
- enemy-manager slot size: `0x160`
- SubCharacter pointer: slot `+0x04`
- active/index byte: slot `+0x158`
- character kind: SubCharacter `+0x80`
- character callback: SubCharacter `+0x94`
- current HP: SubCharacter `+0x180`
- max HP: SubCharacter `+0x184`
- battle status: SubCharacter `+0x1A4`
- native Slurper dead AI: `0x004F6110`
- native Slurper death callback: `0x004F5030`

The mod preserves the character kind and applies the native dead-state runtime fields to both Slurper variants.

See `reverse/NOTES.md` for additional details.

## Research references

The initial enemy-type and descriptor research was cross-checked against:

- JokieW/RandomHill
- mercury501/silent_hill_randomizer
- dreamingmoths/memory-of-alessa

The final runtime behavior was verified against the Windows executable in IDA.

## Scope

This mod deliberately does **not**:

- replace one Slurper type with another;
- randomize other enemies;
- alter bosses;
- edit game archives;
- edit save files;
- modify controller/input behavior;
- modify game difficulty.
