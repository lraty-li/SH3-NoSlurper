# Reverse-engineering notes

These notes document the Windows executable build used during development.

Compatibility fingerprints:

- CRC32: `6368fb1f`
- SHA-256: `3B8B78D0D3C8E266FA17CE983E59F259413A968015249F3BE90C356B0F815122`

IDA Pro 9.2 was used for static analysis.

## Enemy manager

Function `0x004A2E20` allocates one of 32 enemy-work slots.

Relevant instructions:

```asm
004A2E20  mov ecx, offset unk_71294C0
004A2E30  mov dl, [ecx+158h]
004A2E3B  add ecx, 160h
004A2E41  cmp eax, 20h
004A2E4D  mov [ecx+4], edx
004A2E50  mov [ecx+158h], al
```

For this executable build:

- manager base encoded by the instruction: `0x071294C0`
- slot count: `32`
- slot size: `0x160`
- work-function pointer: slot `+0x00`
- SubCharacter pointer: slot `+0x04`
- dead animation/work flag: slot `+0x90`
- dead-state parameter: slot `+0x148`
- work state: slot `+0x152`
- work step: slot `+0x154`
- active/index byte: slot `+0x158`

The ASI scans for the allocator code pattern and extracts the manager pointer from the instruction instead of hard-coding the manager address.

## Slurper runtime layout

The enemy allocator dispatches from the character kind at SubCharacter `+0x80`.

- `0x20A` -> initializer `sub_4FE9D0`
- `0x20B` -> initializer `sub_4FE7E0`

Relevant SubCharacter fields:

- death callback latch: `+0x7C`
- character kind: `+0x80`
- character callback: `+0x94`
- current HP: `+0x180`
- max HP: `+0x184`
- battle status: `+0x1A4`

## Native dead Slurper state

The built-in `0x20A` descriptor state `0x17` initializes a Slurper as dead. Its runtime setup includes:

- current HP = `0.0f`;
- native dead AI = `sub_4F6110`;
- native death callback = `sub_4F5030`;
- enemy-work state = `41`;
- enemy-work step = `0`;
- dead animation/work flag = `1`;
- dead-state parameter = `0`.

The dead AI performs the inert/dead setup, while the death callback selects the Slurper death animation when the battle-active bit is present.

## Variant preservation

The mod applies the native dead runtime state to both `0x20A` and `0x20B` **without writing the character-kind field**.

This is important because the character kind selects the Slurper variant/model. Keeping the kind unchanged preserves the original scene presentation while disabling the enemy.

## Safety checks

Before applying the transition, the ASI:

- validates the enemy-manager signature;
- validates the native dead-AI code signature;
- validates the native death-callback code signature;
- checks relevant memory ranges before reading or writing them;
- waits until max HP contains a sane positive value, avoiding partially initialized character objects;
- ignores all non-Slurper character kinds.
