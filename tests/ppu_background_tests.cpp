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

void clock_until(nes::Ppu& ppu, std::int16_t scanline, std::int16_t cycle) {
    constexpr std::uint32_t frame_clocks = 341U * 262U;
    for (std::uint32_t count = 0; count < frame_clocks; ++count) {
        const auto state = ppu.state();
        if (state.scanline == scanline && state.cycle == cycle) {
            return;
        }
        ppu.clock();
    }
    expect(false, "background test reached the requested PPU position");
}

void background_pipeline_fetches_tile_data() {
    nes::Ppu ppu;
    ppu.connect_cartridge(background_cartridge());
    ppu.ppu_write(0x2001, 3);
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
    ppu.ppu_write(0x2001, 1);
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
    ppu.ppu_write(0x2001, 2);
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

void horizontal_scroll_crosses_nametable_boundaries() {
    nes::Ppu ppu;
    ppu.cpu_write(0x2006, 0x00);
    ppu.cpu_write(0x2006, 0x1F);
    ppu.cpu_write(0x2001, 0x08);
    for (int count = 0; count < 9; ++count) {
        ppu.clock();
    }
    const auto address = ppu.state().vram_address;
    expect((address & 0x001FU) == 0, "coarse X wraps after tile 31");
    expect((address & 0x0400U) != 0U, "coarse X wrap switches horizontal nametables");
}

void pre_render_cycles_copy_the_scroll_address() {
    nes::Ppu ppu;
    ppu.cpu_write(0x2005, 0xAD);
    ppu.cpu_write(0x2005, 0x6B);
    ppu.cpu_write(0x2000, 0x03);
    ppu.cpu_write(0x2001, 0x08);
    clock_until(ppu, -1, 305);

    const auto state = ppu.state();
    expect((state.vram_address & 0x041FU) == (state.temporary_address & 0x041FU),
           "cycle 257 copies horizontal scroll bits");
    expect((state.vram_address & 0x7BE0U) == (state.temporary_address & 0x7BE0U),
           "pre-render cycles copy vertical scroll bits");
    expect(state.fine_x == 5, "fine X remains separate from the loopy address");
}

void vertical_scroll_wraps_the_visible_nametable() {
    nes::Ppu ppu;
    ppu.cpu_write(0x2005, 0x00);
    ppu.cpu_write(0x2005, 0xEF);
    ppu.cpu_write(0x2001, 0x08);
    clock_until(ppu, 0, 257);

    const auto address = ppu.state().vram_address;
    expect((address & 0x7000U) == 0, "fine Y wraps after scanline seven");
    expect((address & 0x03E0U) == 0, "coarse Y wraps after visible row 29");
    expect((address & 0x0800U) != 0U, "vertical wrap switches nametables");
}

void fill_background(nes::Ppu& ppu, std::uint8_t tile, std::uint8_t attribute) {
    for (std::uint16_t address = 0x2000; address < 0x23C0; ++address) {
        ppu.ppu_write(address, tile);
    }
    for (std::uint16_t address = 0x23C0; address < 0x2400; ++address) {
        ppu.ppu_write(address, attribute);
    }
}

void clock_to_first_visible_pixels(nes::Ppu& ppu, int pixels) {
    clock_until(ppu, 0, static_cast<std::int16_t>(pixels + 1));
}

void background_pixels_are_composed_into_the_framebuffer() {
    nes::Ppu ppu;
    ppu.connect_cartridge(background_cartridge());
    fill_background(ppu, 1, 0xAA);
    for (std::uint16_t row = 0; row < 8; ++row) {
        ppu.ppu_write(static_cast<std::uint16_t>(0x0010U + row), 0xFF);
        ppu.ppu_write(static_cast<std::uint16_t>(0x0018U + row), 0x00);
    }
    ppu.ppu_write(0x3F00, 0x0F);
    ppu.ppu_write(0x3F09, 0x21);
    ppu.cpu_write(0x2001, 0x0A);
    clock_to_first_visible_pixels(ppu, 8);

    expect(ppu.pixel(0, 0) == 0x21, "background shifters render the first visible pixel");
    expect(ppu.pixel(7, 0) == 0x21, "background shifters render a complete tile row");
    expect(ppu.framebuffer().size() == nes::Ppu::screen_width * nes::Ppu::screen_height,
           "PPU exposes a complete 256 by 240 framebuffer");
}

void background_left_edge_can_be_clipped() {
    nes::Ppu ppu;
    ppu.connect_cartridge(background_cartridge());
    fill_background(ppu, 1, 0x00);
    for (std::uint16_t row = 0; row < 8; ++row) {
        ppu.ppu_write(static_cast<std::uint16_t>(0x0010U + row), 0xFF);
    }
    ppu.ppu_write(0x3F00, 0x0F);
    ppu.ppu_write(0x3F01, 0x16);
    ppu.cpu_write(0x2001, 0x08);
    clock_to_first_visible_pixels(ppu, 9);

    expect(ppu.pixel(0, 0) == 0x0F, "disabled left-column rendering uses the backdrop colour");
    expect(ppu.pixel(7, 0) == 0x0F, "background clipping covers the first eight pixels");
    expect(ppu.pixel(8, 0) == 0x16, "background rendering resumes after the clipped column");
}

}  // namespace

int run_ppu_background_tests() {
    const auto before = nes::test::failures;
    background_pipeline_fetches_tile_data();
    background_pipeline_loads_pattern_and_attribute_shifters();
    control_selects_the_background_pattern_table();
    horizontal_scroll_crosses_nametable_boundaries();
    pre_render_cycles_copy_the_scroll_address();
    vertical_scroll_wraps_the_visible_nametable();
    background_pixels_are_composed_into_the_framebuffer();
    background_left_edge_can_be_clipped();
    return nes::test::failures - before;
}
