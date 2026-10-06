# SH3-NoSlurper

A tiny ASI mod for the Windows PC version of **Silent Hill 3**.

It does one thing: every Slurper enemy descriptor is converted to the game's known
**Brown Slurper / Dead** state before it can remain active.

## Behavior

The game uses these enemy IDs in its entity descriptors:

- Brown Slurper: `0x20A`
- White Slurper: `0x20B`
- Brown Slurper "Dead" state: `0x17`

This mod applies the following transformation:

- `0x20A` Brown Slurper -> keep type `0x20A`, force state `0x17`
- `0x20B` White Slurper -> change type to `0x20A`, force state `0x17`
- Every other entity type -> untouched

It does **not** set an entity type to zero. In SH3, a zero type ID terminates an
entity-descriptor list, so doing that could prevent later entities in the same list
from loading.

The patcher runs periodically because the game can change or initialize entity-list
pointers after the ASI has already loaded. The pass is intentionally small and only
writes when it sees one of the two Slurper type IDs.

## Compatibility / safety

The mod does not hard-code the enemy table address. At runtime it scans the game's
`.text` section for the pair of instructions that reference the enemy-table pointer,
then validates all memory before reading or writing it.

The implementation was researched against a compatible 32-bit Windows executable build:

`<SH3_DIR>\SILENT HILL 3.exe`

Observed hashes during development:

- CRC32: `6368fb1f`
- SHA-256: `3B8B78D0D3C8E266FA17CE983E59F259413A968015249F3BE90C356B0F815122`

If the code signature is not found on another executable, the ASI does nothing rather
than falling back to a guessed address.

## Install

1. Build **Release | Win32**.
2. Copy `NoSlurper.asi` to the game's ASI-loader folder.
   With Ultimate ASI Loader / ThirteenAG fixes this is typically:
   `<Silent Hill 3>\scripts\NoSlurper.asi`
3. Launch the game normally.
4. `NoSlurper.log` is written next to the ASI and records whether the enemy table
   was found and how many Slurpers were patched.

To uninstall, delete `NoSlurper.asi`. No save files or game archives are modified.

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

The ASI is produced under:

`build\Release\NoSlurper.asi`

## Research references

The entity-table layout and Slurper IDs/states were cross-checked against the
open-source RandomHill implementations:

- JokieW/RandomHill
- mercury501/silent_hill_randomizer

Those projects show a 40-entry table rooted at the SH3 enemy-table pointer, entity
records of `0x18` bytes, type ID at `+0x00`, and variant/state at `+0x16`.

## Scope

This project deliberately does **not**:

- randomize or replace other enemies;
- alter bosses;
- edit SH3 data archives;
- modify saves;
- change controller/input mods;
- change game difficulty.
