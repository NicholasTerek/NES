#include "nes/cartridge.hpp"
#include "nes/mapper.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
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

void nrom_maps_two_program_banks() {
    nes::Mapper0 mapper(2, 1);
    expect(mapper.cpu_read(0x8000) == 0x0000, "NROM-256 maps its first byte");
    expect(mapper.cpu_read(0xBFFF) == 0x3FFF, "NROM-256 maps its lower bank boundary");
    expect(mapper.cpu_read(0xC000) == 0x4000, "NROM-256 maps its upper bank");
    expect(mapper.cpu_read(0xFFFF) == 0x7FFF, "NROM-256 maps its final byte");

    expect(!mapper.cpu_write(0x7FFF, 0x12).handled,
           "NROM does not consume writes below program ROM");
    const auto rom_write = mapper.cpu_write(0x8000, 0x12);
    expect(rom_write.handled && !rom_write.address,
           "NROM consumes program ROM writes without modifying storage");
}

void nrom_maps_character_memory() {
    nes::Mapper0 rom_mapper(1, 1);
    expect(rom_mapper.ppu_read(0x0000) == 0x0000, "NROM maps the first pattern byte");
    expect(rom_mapper.ppu_read(0x1FFF) == 0x1FFF, "NROM exposes pattern memory");
    expect(!rom_mapper.ppu_read(0x2000), "NROM stops mapping at the pattern-table edge");
    const auto rom_write = rom_mapper.ppu_write(0x0100);
    expect(rom_write.handled && !rom_write.address, "CHR ROM consumes writes without changing");

    nes::Mapper0 ram_mapper(1, 0);
    expect(ram_mapper.ppu_write(0x0000).address == 0x0000,
           "CHR RAM accepts its first write");
    expect(ram_mapper.ppu_write(0x1FFF).address == 0x1FFF,
           "CHR RAM accepts its final write");
    expect(!ram_mapper.ppu_write(0x2000).handled,
           "CHR RAM rejects writes beyond pattern memory");
}

void uxrom_maps_program_boundaries() {
    nes::Mapper2 mapper(4, 0);
    expect(mapper.cpu_read(0x8000) == 0x0000, "UxROM starts with bank zero");
    expect(mapper.cpu_read(0xBFFF) == 0x3FFF, "UxROM maps the switchable bank end");
    expect(mapper.cpu_read(0xC000) == 0xC000, "UxROM fixes the last bank");
    expect(mapper.cpu_read(0xFFFF) == 0xFFFF, "UxROM maps the fixed bank end");
    expect(!mapper.cpu_read(0x7FFF), "UxROM ignores addresses below program ROM");

    expect(!mapper.cpu_write(0x7FFF, 2).handled,
           "UxROM ignores bank writes below program ROM");
    expect(mapper.cpu_write(0x8000, 2).handled, "UxROM consumes bank-select writes");
    expect(mapper.cpu_read(0x8000) == 0x8000, "UxROM selects the requested bank");
    expect(mapper.cpu_write(0xFFFF, 5).handled,
           "UxROM accepts bank writes across the program window");
    expect(mapper.cpu_read(0x8000) == 0x4000,
           "UxROM wraps bank values to available storage");
    mapper.reset();
    expect(mapper.cpu_read(0x8000) == 0x0000, "UxROM reset restores bank zero");
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

std::shared_ptr<nes::Cartridge> mmc1_cartridge() {
    constexpr std::size_t header = 16;
    constexpr std::size_t program_bank = 16U * 1024U;
    constexpr std::size_t character_bank = 4U * 1024U;
    auto image = ines_image(4, 2, 0x10);
    for (std::size_t bank = 0; bank < 4; ++bank) {
        std::fill_n(image.begin() + static_cast<std::ptrdiff_t>(header + bank * program_bank),
                    program_bank, static_cast<std::uint8_t>(0x10U * (bank + 1U)));
        std::fill_n(image.begin() + static_cast<std::ptrdiff_t>(
                        header + 4U * program_bank + bank * character_bank),
                    character_bank, static_cast<std::uint8_t>(bank + 1U));
    }
    return nes::Cartridge::from_ines(image);
}

void mmc1_write(nes::Cartridge& cartridge, std::uint16_t address, std::uint8_t value) {
    for (std::uint8_t bit = 0; bit < 5; ++bit) {
        expect(cartridge.cpu_write(address, static_cast<std::uint8_t>((value >> bit) & 1U)),
               "MMC1 serial register accepts each configuration bit");
    }
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

void cartridge_keeps_rom_read_only() {
    auto image = ines_image(2, 1);
    image[16] = 0x9A;
    image[16 + 32 * 1024] = 0xA9;
    const auto cartridge = nes::Cartridge::from_ines(image);

    std::uint8_t value = 0;
    expect(cartridge->cpu_write(0x8000, 0x00), "cartridge handles program ROM writes");
    expect(cartridge->cpu_read(0x8000, value) && value == 0x9A,
           "program ROM remains unchanged after a CPU write");
    expect(cartridge->ppu_write(0x0000, 0x00), "cartridge handles character ROM writes");
    expect(cartridge->ppu_read(0x0000, value) && value == 0xA9,
           "character ROM remains unchanged after a PPU write");
    expect(!cartridge->cpu_read(0x5FFF, value), "cartridge leaves lower CPU space unmapped");
    expect(!cartridge->ppu_read(0x2000, value), "cartridge leaves nametable space unmapped");
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

void cartridge_loads_ines_files() {
    auto image = ines_image(1, 1, 0x01);
    image[16] = 0x6A;
    const auto path = std::filesystem::temp_directory_path() / "nes-cartridge-load-test.nes";
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        expect(static_cast<bool>(stream), "cartridge test ROM can be created");
        stream.write(reinterpret_cast<const char*>(image.data()),
                     static_cast<std::streamsize>(image.size()));
        expect(static_cast<bool>(stream), "cartridge test ROM can be written");
    }

    const auto cartridge = nes::Cartridge::load(path);
    std::error_code error;
    std::filesystem::remove(path, error);
    expect(!error, "cartridge test ROM is removed after loading");
    expect(cartridge->mirror() == nes::Mirror::vertical,
           "file loader preserves cartridge header metadata");
    std::uint8_t value = 0;
    expect(cartridge->cpu_read(0x8000, value) && value == 0x6A,
           "file loader preserves cartridge program data");
}

void cartridge_reports_missing_files() {
    const auto path = std::filesystem::temp_directory_path() / "nes-missing-cartridge.nes";
    std::error_code error;
    std::filesystem::remove(path, error);

    bool rejected = false;
    try {
        static_cast<void>(nes::Cartridge::load(path));
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    expect(rejected, "missing cartridge files fail explicitly");
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

    auto unsupported = ines_image(1, 1, 0xF0);
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
    auto image = ines_image(4, 0, 0x20);
    image[16] = 0x10;
    image[16 + 16 * 1024] = 0x20;
    image[16 + 3 * 16 * 1024] = 0x40;
    const auto cartridge = nes::Cartridge::from_ines(image);
    std::uint8_t value = 0;

    expect(cartridge->cpu_read(0x8000, value) && value == 0x10,
           "UxROM starts with bank zero selected");
    expect(cartridge->cpu_read(0xC000, value) && value == 0x40,
           "UxROM fixes the last bank at the top of memory");
    expect(cartridge->cpu_write(0x8000, 1), "UxROM consumes bank-select writes");
    expect(cartridge->cpu_read(0x8000, value) && value == 0x20,
           "UxROM selects a lower program bank");
    expect(cartridge->cpu_read(0xC000, value) && value == 0x40,
           "UxROM keeps its fixed bank after switching");
    cartridge->reset();
    expect(cartridge->cpu_read(0x8000, value) && value == 0x10,
           "UxROM reset returns to bank zero");
}

void mmc1_switches_program_banks_in_each_mode() {
    auto cartridge = mmc1_cartridge();
    std::uint8_t value = 0;
    expect(cartridge->mapper_id() == 1, "iNES mapper one selects MMC1");
    expect(cartridge->cpu_read(0x8000, value) && value == 0x10,
           "MMC1 initially maps selected bank zero at $8000");
    expect(cartridge->cpu_read(0xC000, value) && value == 0x40,
           "MMC1 initially fixes the final program bank at $C000");

    mmc1_write(*cartridge, 0xE000, 0x02);
    expect(cartridge->cpu_read(0x8000, value) && value == 0x30,
           "MMC1 mode three switches the lower 16 KiB bank");
    mmc1_write(*cartridge, 0x8000, 0x08);
    expect(cartridge->cpu_read(0x8000, value) && value == 0x10,
           "MMC1 mode two fixes the first bank at $8000");
    expect(cartridge->cpu_read(0xC000, value) && value == 0x30,
           "MMC1 mode two switches the upper 16 KiB bank");
    mmc1_write(*cartridge, 0x8000, 0x00);
    expect(cartridge->cpu_read(0x8000, value) && value == 0x30,
           "MMC1 32 KiB mode selects an even bank pair");
    expect(cartridge->cpu_read(0xC000, value) && value == 0x40,
           "MMC1 32 KiB mode exposes the second bank in the selected pair");
}

void mmc1_switches_character_banks_and_mirroring() {
    auto cartridge = mmc1_cartridge();
    std::uint8_t value = 0;
    mmc1_write(*cartridge, 0x8000, 0x1C);
    mmc1_write(*cartridge, 0xA000, 0x01);
    mmc1_write(*cartridge, 0xC000, 0x02);
    expect(cartridge->ppu_read(0x0000, value) && value == 0x02,
           "MMC1 selects an independent lower 4 KiB CHR bank");
    expect(cartridge->ppu_read(0x1000, value) && value == 0x03,
           "MMC1 selects an independent upper 4 KiB CHR bank");
    expect(cartridge->mirror() == nes::Mirror::one_screen_low,
           "MMC1 control selects lower one-screen mirroring");
    mmc1_write(*cartridge, 0x8000, 0x1D);
    expect(cartridge->mirror() == nes::Mirror::one_screen_high,
           "MMC1 control selects upper one-screen mirroring");
    mmc1_write(*cartridge, 0x8000, 0x1E);
    expect(cartridge->mirror() == nes::Mirror::vertical,
           "MMC1 control selects vertical mirroring");
    mmc1_write(*cartridge, 0x8000, 0x1F);
    expect(cartridge->mirror() == nes::Mirror::horizontal,
           "MMC1 control selects horizontal mirroring");
}

void mmc1_controls_program_ram_access() {
    auto cartridge = mmc1_cartridge();
    std::uint8_t value = 0;
    expect(cartridge->cpu_write(0x6000, 0x5A), "MMC1 program RAM starts enabled");
    mmc1_write(*cartridge, 0xE000, 0x10);
    expect(!cartridge->cpu_read(0x6000, value), "MMC1 can disable program RAM reads");
    expect(!cartridge->cpu_write(0x6000, 0xA5), "MMC1 can disable program RAM writes");
    mmc1_write(*cartridge, 0xE000, 0x00);
    expect(cartridge->cpu_read(0x6000, value) && value == 0x5A,
           "re-enabled MMC1 program RAM preserves its contents");
}

}  // namespace

int run_cartridge_tests() {
    nrom_mirrors_single_program_bank();
    nrom_maps_two_program_banks();
    nrom_maps_character_memory();
    uxrom_maps_program_boundaries();
    cartridge_parses_ines_and_routes_accesses();
    cartridge_allocates_character_ram();
    cartridge_keeps_rom_read_only();
    cartridge_maps_program_ram();
    trainer_initializes_program_ram();
    cartridge_loads_ines_files();
    cartridge_reports_missing_files();
    cartridge_rejects_invalid_images();
    uxrom_switches_lower_program_bank();
    mmc1_switches_program_banks_in_each_mode();
    mmc1_switches_character_banks_and_mirroring();
    mmc1_controls_program_ram_access();
    return failures;
}
