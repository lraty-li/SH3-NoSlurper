# SH3-NoSlurper

A tiny ASI mod for the Windows PC version of **Silent Hill 3**.

Version 0.2.1 preserves every Slurper's original type, model, color/variant, and
scene placement. It only sets the instantiated enemy's **current HP** to zero and
lets Silent Hill 3's own Slurper death logic handle the rest.

## What it changes

Runtime enemy types:

- Slurper E: `0x20A`
- Slurper X: `0x20B`

For either type, once the enemy has been fully initialized:

- type ID is left untouched;
- descriptor variant/state is left untouched;
- model/material/scene data is left untouched;
- max HP is left untouched;
- **current HP is set to 0.0f**.

Every other enemy is ignored.

This fixes the v0.1 behavior where `0x20B` had to be converted to `0x20A`
in order to reuse the known Brown Slurper dead descriptor. That conversion could
visibly change special/red/pale Slurper appearances and is no longer used.

## Why setting HP to zero works

IDA analysis of the installed PC executable found:

- enemy-manager allocator: `0x004A2E20`
- current-build enemy manager: `0x071294C0`
- enemy-manager slot size: `0x160`
- active/index byte: slot `+0x158`
- SubCharacter pointer: slot `+0x04`
- enemy kind: SubCharacter `+0x80`
- current HP: SubCharacter `+0x180`
- max HP: SubCharacter `+0x184`

The mod does **not** hard-code the manager address. It signature-scans the
allocator and extracts the manager pointer from the instruction.

The Slurper initialization functions are:

- `0x004FE9D0` — type `0x20A`
- `0x004FE7E0` — type `0x20B`

Both initialize current/max HP at `+0x180/+0x184`.

Most importantly, both types attach the same game callback:

- `0x004F4F70`

That callback explicitly branches on current HP. When HP is greater than zero it
continues the live path; when HP is zero or below it selects the game's normal
Slurper death animation/state. Version 0.2 therefore uses the game's own death
path rather than changing the enemy into another variant.

## Install

Build **Release | Win32**, then copy:

`build\Release\NoSlurper.asi`

to:

`<Silent Hill 3>\scripts\NoSlurper.asi`

Install destination:

`<SH3_DIR>\scripts\NoSlurper.asi`

Ultimate ASI Loader is already present in that installation.

A `NoSlurper.log` file is written next to the ASI. Version 0.2.1 log entries say
`runtime-HP` and report how many `0x20A` / `0x20B` enemies had their HP
zeroed.

## Uninstall / rollback

Delete:

`scripts\NoSlurper.asi`

No save files, stage archives, enemy descriptors, or executable bytes are
modified on disk.

If a previous ASI was backed up during installation, restoring that file reverts
to the older implementation.

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

## Compatibility notes

The implementation was researched against a compatible 32-bit Windows executable build:

`<SH3_DIR>\SILENT HILL 3.exe`

Observed hashes:

- CRC32: `6368fb1f`
- SHA-256: `3B8B78D0D3C8E266FA17CE983E59F259413A968015249F3BE90C356B0F815122`

The runtime lookup is signature-based. If the enemy-manager allocator signature
is not found, the mod does nothing rather than using a guessed address.

## Research basis

The initial entity-type research was cross-checked against:

- JokieW/RandomHill
- mercury501/silent_hill_randomizer
- dreamingmoths/memory-of-alessa (Silent Hill 3 decompilation)

The final v0.2 runtime offsets and death-path behavior were verified directly in
IDA 9.2 against the installed Windows executable.

See `reverse/NOTES.md` for the specific findings.

## Scope

The mod deliberately does **not**:

- replace one Slurper type with another;
- modify Slurper descriptor variants;
- randomize other enemies;
- alter bosses;
- edit game archives;
- modify saves;
- modify controller/input behavior;
- modify game difficulty.
