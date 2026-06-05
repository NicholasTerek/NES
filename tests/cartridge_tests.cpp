#include "nes/cartridge.hpp"
#include "nes/mapper.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

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
    expect(!rom_mapper.ppu_write(0x0100).address, "CHR ROM rejects writes");

    nes::Mapper0 ram_mapper(1, 0);
    expect(ram_mapper.ppu_write(0x0100).address == 0x0100, "CHR RAM accepts writes");
}

std::vector<std::uint8_t> ines_image(std::uint8_t program_banks,
                                     std::uint8_t character_banks,
                                     std::uint8_t flags6 = 0,
                                     std::uint8_t program_ram_banks = 0) {
    const auto trainer_bytes = (flags6 & 0x04U) != 0U ? 512U : 0U;
    std::vector<std::uint8_t> image(
        16U + trainer_bytes + static_cast<std::size_t>(program_banks) * 16U * 1024U +
            static_cast<std::size_t>(character_banks) * 8U * 1024U,
        0);
    image[0] = 'N';
    image[1] = 'E';
    image[2] = 'S';
    image[3] = 0x1A;
    image[4] = program_banks;
    image[5] = character_banks;
    image[6] = flags6;
    image[8] = program_ram_banks;
    return image;
}

void cartridge_parses_ines_and_routes_accesses() {
    auto image = ines_image(1, 1, 0x01);
    image[16] = 0x42;
    image[16 + 16 * 1024] = 0x24;
    const auto cartridge = nes::Cartridge::from_ines(image);

    expect(cartridge->mapper_id() == 0, "iNES parser extracts mapper zero");
    expect(cartridge->mirror() == nes::Mirror::vertical, "iNES parser extracts mirroring");
    expect(cartridge->program_size() == 16 * 1024, "iNES parser loads program ROM");
    expect(cartridge->character_size() == 8 * 1024, "iNES parser loads character ROM");

    std::uint8_t value = 0;
    expect(cartridge->cpu_read(0x8000, value) && value == 0x42,
           "cartridge routes CPU reads through its mapper");
    expect(cartridge->cpu_read(0xC000, value) && value == 0x42,
           "single-bank cartridge mirrors program ROM");
    expect(cartridge->ppu_read(0x0000, value) && value == 0x24,
           "cartridge routes PPU reads through its mapper");
}

void cartridge_allocates_character_ram() {
    const auto cartridge = nes::Cartridge::from_ines(ines_image(1, 0));
    expect(cartridge->character_size() == 8 * 1024, "zero CHR banks allocate CHR RAM");
    expect(cartridge->ppu_write(0x1234, 0xA5), "CHR RAM accepts cartridge writes");
    std::uint8_t value = 0;
    expect(cartridge->ppu_read(0x1234, value) && value == 0xA5,
           "CHR RAM preserves written bytes");
}

void cartridge_maps_program_ram() {
    const auto cartridge = nes::Cartridge::from_ines(ines_image(1, 1, 0x02, 2));
    expect(cartridge->program_ram_size() == 16 * 1024,
           "iNES byte eight sizes cartridge program RAM");
    expect(cartridge->has_battery(), "battery-backed RAM metadata is preserved");

    std::uint8_t value = 0;
    expect(cartridge->cpu_write(0x6000, 0x4A), "cartridge accepts program RAM writes");
    expect(cartridge->cpu_write(0x7FFF, 0xA4), "program RAM includes its upper boundary");
    expect(cartridge->cpu_read(0x6000, value) && value == 0x4A,
           "cartridge reads the first program RAM byte");
    expect(cartridge->cpu_read(0x7FFF, value) && value == 0xA4,
           "cartridge reads the last mapped program RAM byte");
}

void trainer_initializes_program_ram() {
    auto image = ines_image(1, 1, 0x04);
    image[16] = 0x35;
    image[16 + 511] = 0x53;
    image[16 + 512] = 0xA9;
    const auto cartridge = nes::Cartridge::from_ines(image);

    std::uint8_t value = 0;
    expect(cartridge->cpu_read(0x7000, value) && value == 0x35,
           "trainer begins at CPU address $7000");
    expect(cartridge->cpu_read(0x71FF, value) && value == 0x53,
           "trainer fills its 512-byte program RAM window");
    expect(cartridge->cpu_read(0x8000, value) && value == 0xA9,
           "program ROM begins after the trainer");
}

void expect_invalid_image(const std::vector<std::uint8_t>& image, std::string_view message) {
    bool rejected = false;
    try {
        static_cast<void>(nes::Cartridge::from_ines(image));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    expect(rejected, message);
}

void cartridge_rejects_invalid_images() {
    expect_invalid_image(std::vector<std::uint8_t>(16, 0), "invalid iNES magic is rejected");

    expect_invalid_image(ines_image(0, 1), "images without program ROM are rejected");

    auto nes2 = ines_image(1, 1);
    nes2[7] = 0x08;
    expect_invalid_image(nes2, "NES 2.0 images are rejected explicitly");

    auto unknown_format = ines_image(1, 1);
    unknown_format[7] = 0x04;
    expect_invalid_image(unknown_format, "reserved header formats are rejected");

    auto unsupported = ines_image(1, 1, 0x10);
    expect_invalid_image(unsupported, "unsupported mappers fail explicitly");

    expect_invalid_image(ines_image(3, 1), "NROM rejects oversized program ROM");
    expect_invalid_image(ines_image(1, 2), "NROM rejects oversized character ROM");
    expect_invalid_image(ines_image(1, 0, 0x20), "UxROM requires switchable program banks");
    expect_invalid_image(ines_image(2, 1, 0x20), "UxROM requires character RAM");

    auto truncated = ines_image(1, 1);
    truncated.pop_back();
    expect_invalid_image(truncated, "truncated ROM payloads are rejected");
}

void uxrom_switches_lower_program_bank() {
    auto image = ines_image(3, 0, 0x20);
    image[16] = 0x10;
    image[16 + 16 * 1024] = 0x20;
    image[16 + 2 * 16 * 1024] = 0x30;
    const auto cartridge = nes::Cartridge::from_ines(image);
    std::uint8_t value = 0;

    expect(cartridge->cpu_read(0x8000, value) && value == 0x10,
           "UxROM starts with bank zero selected");
    expect(cartridge->cpu_read(0xC000, value) && value == 0x30,
           "UxROM fixes the last bank at the top of memory");
    expect(cartridge->cpu_write(0x8000, 1), "UxROM consumes bank-select writes");
    expect(cartridge->cpu_read(0x8000, value) && value == 0x20,
           "UxROM selects a lower program bank");
    expect(cartridge->cpu_read(0xC000, value) && value == 0x30,
           "UxROM keeps its fixed bank after switching");
    cartridge->reset();
    expect(cartridge->cpu_read(0x8000, value) && value == 0x10,
           "UxROM reset returns to bank zero");
}

}  // namespace

int run_cartridge_tests() {
    nrom_mirrors_single_program_bank();
    nrom_maps_character_memory();
    cartridge_parses_ines_and_routes_accesses();
    cartridge_allocates_character_ram();
    cartridge_maps_program_ram();
    trainer_initializes_program_ram();
    cartridge_rejects_invalid_images();
    uxrom_switches_lower_program_bank();
    return failures;
}
