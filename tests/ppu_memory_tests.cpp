#include "nes/cartridge.hpp"
#include "nes/ppu.hpp"
#include "test_harness.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace {

using nes::test::expect;

std::shared_ptr<nes::Cartridge> cartridge_with_mirroring(std::uint8_t flags6,
                                                         bool character_ram = true) {
    const auto character_banks = static_cast<std::uint8_t>(character_ram ? 0U : 1U);
    std::vector<std::uint8_t> image(
        16U + 16U * 1024U + static_cast<std::size_t>(character_banks) * 8U * 1024U, 0);
    image[0] = 'N';
    image[1] = 'E';
    image[2] = 'S';
    image[3] = 0x1A;
    image[4] = 1;
    image[5] = character_banks;
    image[6] = flags6;
    return nes::Cartridge::from_ines(image);
}

void pattern_memory_routes_through_the_cartridge() {
    nes::Ppu ppu;
    ppu.connect_cartridge(cartridge_with_mirroring(0x00));
    ppu.ppu_write(0x0000, 0x12);
    ppu.ppu_write(0x1FFF, 0x34);
    expect(ppu.ppu_read(0x0000) == 0x12, "PPU maps the first CHR RAM byte");
    expect(ppu.ppu_read(0x1FFF) == 0x34, "PPU maps the last CHR RAM byte");
    expect(ppu.ppu_read(0x4000) == 0x12, "PPU addresses mirror every 16 KiB");

    nes::Ppu rom_ppu;
    rom_ppu.connect_cartridge(cartridge_with_mirroring(0x00, false));
    rom_ppu.ppu_write(0x0000, 0xFF);
    expect(rom_ppu.ppu_read(0x0000) == 0x00, "CHR ROM remains read only");
}

void vertical_nametable_mirroring_uses_alternating_tables() {
    nes::Ppu ppu;
    ppu.connect_cartridge(cartridge_with_mirroring(0x01));
    ppu.ppu_write(0x2000, 0x21);
    ppu.ppu_write(0x2400, 0x42);
    expect(ppu.ppu_read(0x2800) == 0x21, "vertical mirroring joins tables zero and two");
    expect(ppu.ppu_read(0x2C00) == 0x42, "vertical mirroring joins tables one and three");
    expect(ppu.ppu_read(0x3000) == 0x21, "nametable space mirrors through $3EFF");
}

void horizontal_nametable_mirroring_uses_table_pairs() {
    nes::Ppu ppu;
    ppu.connect_cartridge(cartridge_with_mirroring(0x00));
    ppu.ppu_write(0x2000, 0x13);
    ppu.ppu_write(0x2800, 0x31);
    expect(ppu.ppu_read(0x2400) == 0x13, "horizontal mirroring joins the upper pair");
    expect(ppu.ppu_read(0x2C00) == 0x31, "horizontal mirroring joins the lower pair");
}

void four_screen_mirroring_keeps_tables_independent() {
    nes::Ppu ppu;
    ppu.connect_cartridge(cartridge_with_mirroring(0x08));
    ppu.ppu_write(0x2000, 0x10);
    ppu.ppu_write(0x2400, 0x20);
    ppu.ppu_write(0x2800, 0x30);
    ppu.ppu_write(0x2C00, 0x40);
    expect(ppu.ppu_read(0x2000) == 0x10, "four-screen table zero is independent");
    expect(ppu.ppu_read(0x2400) == 0x20, "four-screen table one is independent");
    expect(ppu.ppu_read(0x2800) == 0x30, "four-screen table two is independent");
    expect(ppu.ppu_read(0x2C00) == 0x40, "four-screen table three is independent");
}

void palette_memory_applies_hardware_mirrors() {
    nes::Ppu ppu;
    ppu.ppu_write(0x3F00, 0x2A);
    expect(ppu.ppu_read(0x3F10) == 0x2A, "universal background colour mirrors at $3F10");
    ppu.ppu_write(0x3F14, 0x7F);
    expect(ppu.ppu_read(0x3F04) == 0x3F, "palette values are limited to six bits");
    expect(ppu.ppu_read(0x3F24) == 0x3F, "palette RAM repeats every 32 bytes");
}

void reset_preserves_video_memory() {
    nes::Ppu ppu;
    ppu.ppu_write(0x2000, 0x5A);
    ppu.ppu_write(0x3F00, 0x2A);
    ppu.reset();
    expect(ppu.ppu_read(0x2000) == 0x5A, "PPU reset preserves nametable RAM");
    expect(ppu.ppu_read(0x3F00) == 0x2A, "PPU reset preserves palette RAM");
}

}  // namespace

int run_ppu_memory_tests() {
    const auto before = nes::test::failures;
    pattern_memory_routes_through_the_cartridge();
    vertical_nametable_mirroring_uses_alternating_tables();
    horizontal_nametable_mirroring_uses_table_pairs();
    four_screen_mirroring_keeps_tables_independent();
    palette_memory_applies_hardware_mirrors();
    reset_preserves_video_memory();
    return nes::test::failures - before;
}
