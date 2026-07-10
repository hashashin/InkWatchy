#!/usr/bin/env python3

import argparse
from pathlib import Path

WORD_ROM_SIZE = 12288


def pack_rom(source: bytes) -> bytes:
    if len(source) != WORD_ROM_SIZE:
        raise ValueError(f"expected {WORD_ROM_SIZE} bytes, got {len(source)}")

    instructions = []
    for offset in range(0, len(source), 2):
        instruction = int.from_bytes(source[offset : offset + 2], "big")
        if instruction > 0xFFF:
            raise ValueError(
                f"instruction at 0x{offset:04x} is not 12-bit: 0x{instruction:04x}"
            )
        instructions.append(instruction)

    packed = bytearray()
    for offset in range(0, len(instructions), 2):
        first = instructions[offset]
        second = instructions[offset + 1]
        packed.extend(
            (
                first >> 4,
                ((first & 0xF) << 4) | (second >> 8),
                second & 0xFF,
            )
        )
    return bytes(packed)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Pack a 16-bit big-endian Tamagotchi ROM into 12-bit pairs"
    )
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()

    packed = pack_rom(args.source.read_bytes())
    args.destination.parent.mkdir(parents=True, exist_ok=True)
    args.destination.write_bytes(packed)


if __name__ == "__main__":
    main()
