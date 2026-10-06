# Reverse-engineering notes

These notes apply to the installed Windows executable:

- CRC32: `6368fb1f`
- SHA-256: `3B8B78D0D3C8E266FA17CE983E59F259413A968015249F3BE90C356B0F815122`

IDA database was produced with IDA Pro 9.2.

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

Therefore:

- manager base on this build: `0x071294C0`
- slot count: 32
- slot size: `0x160`
- SubCharacter pointer: slot `+0x04`
- active/index byte: slot `+0x158`

The ASI signature-scans this code and extracts the immediate manager address
instead of hard-coding `0x071294C0`.

## Runtime character layout used by Slurpers

The allocator dispatches by the value at SubCharacter `+0x80`.

- `0x20A` -> `sub_4FE9D0`
- `0x20B` -> `sub_4FE7E0`

Both initializers write:

- current HP: SubCharacter `+0x180`
- max HP: SubCharacter `+0x184`

For normal enemies these fields start equal.

The native `0x20A`, descriptor state `0x17` ("Dead") path in
`sub_4FE9D0` sets:

```c
*(float *)(scp + 0x180) = 0.0f;
*(float *)(scp + 0x184) = 800.0f; // encoded as 0x44480000
```

This independently confirms that `+0x180` is the current-HP field.

## Shared Slurper death callback

Both `0x20A` and `0x20B` normal initialization attach
`sub_4F4F70` as the SubCharacter callback.

The decompiled decision is effectively:

```c
if (battle_status & 0x2000) {
    if (current_hp > 0.0f) {
        // live path
    } else {
        // choose Slurper death animation/state
    }
}
```

This is why v0.2 only sets current HP to zero. The game itself performs the
death transition, while the original `0x20A` or `0x20B` kind remains
unchanged.

## v0.1 issue

Version 0.1 rewrote `0x20B` descriptors to `0x20A` in order to use
descriptor state `0x17`. Runtime logging showed 12 such conversions in one
play session, which explains visible variant/color changes.

Version 0.2 removes all descriptor rewriting.
