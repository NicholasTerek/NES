#include "nes/mapper.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

int failures = 0;

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void nrom_mirrors_single_program_bank() {
    nes::Mapper0 mapper(1, 1);
    expect(mapper.cpu_read(0x8000) == 0x0000, "NROM maps the first CPU byte");
    expect(mapper.cpu_read(0xBFFF) == 0x3FFF, "NROM maps the end of a 16 KiB bank");
    expect(mapper.cpu_read(0xC000) == 0x0000, "NROM-128 mirrors its program bank");
    expect(!mapper.cpu_read(0x7FFF), "NROM ignores expansion space");
}

void nrom_maps_character_memory() {
    nes::Mapper0 rom_mapper(1, 1);
    expect(rom_mapper.ppu_read(0x1FFF) == 0x1FFF, "NROM exposes pattern memory");
    expect(!rom_mapper.ppu_write(0x0100), "CHR ROM rejects writes");

    nes::Mapper0 ram_mapper(1, 0);
    expect(ram_mapper.ppu_write(0x0100) == 0x0100, "CHR RAM accepts writes");
}

}  // namespace

int run_cartridge_tests() {
    nrom_mirrors_single_program_bank();
    nrom_maps_character_memory();
    return failures;
}
