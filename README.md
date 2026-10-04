# NES Emulator

[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=c%2B%2B)](https://en.cppreference.com/w/cpp/20)
[![CMake](https://img.shields.io/badge/build-CMake-064F8C?logo=cmake)](CMakeLists.txt)
[![License: MIT](https://img.shields.io/badge/license-MIT-yellow.svg)](LICENSE)

A cycle-aware Nintendo Entertainment System emulator written in C++20. It models the
Ricoh 2A03 CPU, picture and audio processing units, cartridge hardware, DMA, and
controller input, with both an SDL2 frontend and headless development tools.

## Highlights

- 6502-compatible CPU with all official instructions, commonly used unofficial
  instructions, cycle penalties, dummy bus accesses, interrupts, and reset behavior
- Dot-clocked NTSC PPU with scrolling, background rendering, 8x8 and 8x16 sprites,
  sprite-zero hits, sprite overflow behavior, vblank/NMI timing, and open-bus behavior
- Five-channel APU with two pulse channels, triangle, noise, DMC, frame IRQs, nonlinear
  mixing, and 44.1 kHz audio output
- CPU/PPU buses with RAM mirroring, OAM DMA, DMC stalls, and two controller ports
- iNES cartridge loading with PRG ROM/RAM, CHR ROM/RAM, trainers, battery metadata,
  and horizontal, vertical, one-screen, and four-screen mirroring
- Resizable SDL2 frontend with video, audio, and keyboard input
- Headless runner that can export frames as PPM images and audio as WAV files
- Unit and integration coverage, plus dedicated CPU functional and public ROM
  conformance runners

## Supported mappers

| Mapper | Board | Status |
| ---: | --- | --- |
| 0 | NROM | Supported |
| 1 | MMC1 / SxROM | Supported |
| 2 | UxROM | Supported |
| 3 | CNROM | Supported |
| 66 | GxROM | Supported |

Only iNES 1.0 images are currently accepted. See [Roadmap](#roadmap) for planned
cartridge support.

## Build

### Ubuntu or WSL2

Install the required tools and SDL2 development package:

```bash
sudo apt update
sudo apt install -y git build-essential cmake ninja-build libsdl2-dev
```

Clone and build:

```bash
git clone https://github.com/NicholasTerek/NES.git
cd NES
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DNES_BUILD_TESTS=ON -DNES_BUILD_SDL=ON
cmake --build build --parallel
```

The SDL frontend requires a working desktop environment. On Windows, use WSL2 with
WSLg enabled or build the project natively with SDL2 available to CMake.

## Run

Launch a legally obtained iNES ROM:

```bash
./build/nes_sdl "/path/to/game.nes"
```

Windows files are available to WSL under `/mnt`. For example:

```bash
./build/nes_sdl "/mnt/c/Users/username/Downloads/game.nes"
```

### Controls

| NES control | Keyboard |
| --- | --- |
| D-pad | Arrow keys |
| A | `X` |
| B | `Z` |
| Start | `Enter` |
| Select | Right `Shift` |
| Quit | `Escape` |

### Headless runner

Run a fixed number of frames without opening a window:

```bash
./build/nes_runner "/path/to/game.nes" --frames 300
```

The runner can also capture the final frame and generated audio:

```bash
./build/nes_runner "/path/to/game.nes" --frames 300 --frame frame.ppm --audio audio.wav
```

Controller states can be supplied as NES button bitmasks with `--controller1` and
`--controller2`.

## Test

Run the internal test suite:

```bash
ctest --test-dir build --output-on-failure
```

The emulator has also passed a 54-ROM public conformance matrix covering:

- Official and unofficial CPU instructions, timing, dummy accesses, interrupts, and reset
- PPU vblank/NMI timing, register behavior, open bus, OAM, sprite-zero hits, and overflow
- APU channel timing, frame sequencer behavior, DMC, IRQs, and reset behavior

Public test ROMs are not distributed with this repository. The `nes_conformance` tool
supports Blargg-style status output and the legacy `$00F8` result protocol:

```bash
./build/nes_conformance "/path/to/test.nes" --frames 1800
./build/nes_conformance "/path/to/legacy-test.nes" --frames 1800 --legacy-f8
```

## Architecture

| Component | Responsibility |
| --- | --- |
| `Cpu` | Instruction execution, interrupts, flags, stack, and cycle accounting |
| `Ppu` | Video memory, registers, rendering pipeline, sprites, and NMI timing |
| `Apu` | Audio channels, frame sequencer, IRQs, mixing, and sample generation |
| `Bus` | CPU memory map, device routing, DMA, controllers, and system clocking |
| `Cartridge` | iNES parsing, PRG/CHR storage, PRG RAM, mirroring, and mapper selection |
| `Mapper` | Cartridge-specific PRG/CHR bank switching and mirroring control |
| `Emulator` | Frame execution, reset coordination, controller input, and audio collection |

## Current limitations

- NTSC timing only; PAL timing is not implemented
- Battery-backed PRG RAM is kept in memory but is not yet persisted to `.sav` files
- No Zapper/light-gun input, save states, rewind, or debugger UI
- SDL input currently maps one keyboard controller; the core supports two controller ports
- NES 2.0 headers and unsupported cartridge mappers are rejected

## Roadmap

- [ ] Add MMC3 / mapper 4 bank switching and scanline IRQs for games such as
      *Super Mario Bros. 3*, *Kirby's Adventure*, and *Mega Man 3-6*
- [ ] Persist battery-backed PRG RAM to `.sav` files
- [ ] Add PAL clock rates and frame timing
- [ ] Add SDL gamepad and second-controller bindings
- [ ] Add Zapper input and light detection
- [ ] Add save states and debugging tools

## ROMs

ROM images and copyrighted game assets are not included. Use homebrew software or dump
cartridges you legally own, subject to the laws that apply where you live.

## License

This project is available under the [MIT License](LICENSE).
