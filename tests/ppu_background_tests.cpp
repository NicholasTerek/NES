#include "nes/cartridge.hpp"
#include "nes/ppu.hpp"
#include "test_harness.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace {

using nes::test::expect;

std::shared_ptr<nes::Cartridge> background_cartridge() {
    std::vector<std::uint8_t> image(16U + 16U * 1024U, 0);
    image[0] = 'N';
    image[1] = 'E';
    image[2] = 'S';
    image[3] = 0x1A;
    image[4] = 1;
    return nes::Cartridge::from_ines(image);
}

void background_pipeline_fetches_tile_data() {
    nes::Ppu ppu;
    ppu.connect_cartridge(background_cartridge());
    ppu.ppu_write(0x2000, 3);
    ppu.ppu_write(0x23C0, 0x03);
    ppu.ppu_write(0x0030, 0x80);
    ppu.ppu_write(0x0038, 0x40);
    ppu.cpu_write(0x2001, 0x08);

    for (int count = 0; count < 16; ++count) {
        ppu.clock();
    }
    const auto state = ppu.state();
    expect(state.next_tile_id == 3, "background pipeline fetches the nametable tile ID");
    expect(state.next_tile_attribute == 3,
           "background pipeline selects the tile attribute quadrant");
    expect(state.next_tile_low == 0x80, "background pipeline fetches the low CHR plane");
    expect(state.next_tile_high == 0x40, "background pipeline fetches the high CHR plane");
}

void background_pipeline_loads_pattern_and_attribute_shifters() {
    nes::Ppu ppu;
    ppu.connect_cartridge(background_cartridge());
    ppu.ppu_write(0x2000, 1);
    ppu.ppu_write(0x23C0, 0x03);
    ppu.ppu_write(0x0010, 0xAA);
    ppu.ppu_write(0x0018, 0x55);
    ppu.cpu_write(0x2001, 0x08);

    for (int count = 0; count < 18; ++count) {
        ppu.clock();
    }
    const auto state = ppu.state();
    expect((state.pattern_shift_low & 0x00FFU) == 0xAA,
           "tile low plane loads into the pattern shifter");
    expect((state.pattern_shift_high & 0x00FFU) == 0x55,
           "tile high plane loads into the pattern shifter");
    expect((state.attribute_shift_low & 0x00FFU) == 0xFF,
           "palette low bit expands across the attribute shifter");
    expect((state.attribute_shift_high & 0x00FFU) == 0xFF,
           "palette high bit expands across the attribute shifter");
}

void control_selects_the_background_pattern_table() {
    nes::Ppu ppu;
    ppu.connect_cartridge(background_cartridge());
    ppu.ppu_write(0x2000, 2);
    ppu.ppu_write(0x0020, 0x11);
    ppu.ppu_write(0x1020, 0x77);
    ppu.cpu_write(0x2000, 0x10);
    ppu.cpu_write(0x2001, 0x08);

    for (int count = 0; count < 16; ++count) {
        ppu.clock();
    }
    expect(ppu.state().next_tile_low == 0x77,
           "PPUCTRL selects the upper background pattern table");
}

}  // namespace

int run_ppu_background_tests() {
    const auto before = nes::test::failures;
    background_pipeline_fetches_tile_data();
    background_pipeline_loads_pattern_and_attribute_shifters();
    control_selects_the_background_pattern_table();
    return nes::test::failures - before;
}
