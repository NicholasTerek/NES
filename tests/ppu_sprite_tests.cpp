#include "nes/ppu.hpp"
#include "test_harness.hpp"

#include <cstdint>

namespace {

using nes::test::expect;

void hide_all_sprites(nes::Ppu& ppu) {
    for (std::uint16_t index = 0; index < 64; ++index) {
        ppu.oam_write(static_cast<std::uint8_t>(index * 4U), 0x80);
    }
}

void clock_until(nes::Ppu& ppu, std::int16_t scanline, std::int16_t cycle) {
    constexpr std::uint32_t frame_clocks = 341U * 262U;
    for (std::uint32_t count = 0; count < frame_clocks; ++count) {
        if (ppu.state().scanline == scanline && ppu.state().cycle == cycle) {
            return;
        }
        ppu.clock();
    }
    expect(false, "sprite test reached the requested PPU position");
}

void cpu_oam_registers_address_object_memory() {
    nes::Ppu ppu;
    ppu.cpu_write(0x2003, 0xFC);
    ppu.cpu_write(0x2004, 0x11);
    ppu.cpu_write(0x2004, 0x22);
    ppu.cpu_write(0x2004, 0x33);
    ppu.cpu_write(0x2004, 0x44);
    expect(ppu.oam_read(0xFC) == 0x11, "OAMDATA writes at the selected OAM address");
    expect(ppu.oam_read(0xFF) == 0x44, "OAMDATA writes reach the end of object memory");
    expect(ppu.state().oam_address == 0x00, "OAM address wraps after 256 bytes");
}

void oam_data_reads_do_not_increment_the_address() {
    nes::Ppu ppu;
    ppu.oam_write(0x20, 0xA5);
    ppu.cpu_write(0x2003, 0x20);
    expect(ppu.cpu_read(0x2004) == 0xA5, "OAMDATA reads the selected object byte");
    expect(ppu.cpu_read(0x2004) == 0xA5, "OAMDATA reads do not increment OAMADDR");
    expect(ppu.state().oam_address == 0x20, "OAM read preserves its address");
}

void direct_oam_access_supports_dma_transfers() {
    nes::Ppu ppu;
    ppu.oam_write(0x00, 0x12);
    ppu.oam_write(0xFF, 0x34);
    expect(ppu.oam().front() == 0x12, "direct OAM access exposes the first DMA byte");
    expect(ppu.oam().back() == 0x34, "direct OAM access exposes the final DMA byte");
}

void reset_preserves_object_memory_but_resets_its_address() {
    nes::Ppu ppu;
    ppu.oam_write(0x42, 0x7E);
    ppu.cpu_write(0x2003, 0x42);
    ppu.reset();
    expect(ppu.oam_read(0x42) == 0x7E, "PPU reset preserves object memory");
    expect(ppu.state().oam_address == 0, "PPU reset clears OAMADDR");
}

void visible_sprites_are_selected_in_oam_order() {
    nes::Ppu ppu;
    hide_all_sprites(ppu);
    ppu.oam_write(0, 0xFF);
    ppu.oam_write(1, 3);
    ppu.oam_write(2, 0x01);
    ppu.oam_write(3, 12);
    ppu.oam_write(4, 0xFF);
    ppu.oam_write(5, 7);
    ppu.oam_write(6, 0x02);
    ppu.oam_write(7, 24);
    ppu.cpu_write(0x2001, 0x10);
    clock_until(ppu, -1, 258);

    expect(ppu.state().sprite_count == 2, "PPU selects sprites visible on scanline zero");
    expect(ppu.state().sprite_zero_possible, "sprite zero remains identifiable after evaluation");
    expect(ppu.active_sprites()[0].tile == 3 && ppu.active_sprites()[0].x == 12,
           "earlier OAM entries keep higher sprite priority");
    expect(ppu.active_sprites()[1].tile == 7 && ppu.active_sprites()[1].x == 24,
           "PPU caches each visible sprite entry");
}

void ninth_visible_sprite_sets_overflow() {
    nes::Ppu ppu;
    hide_all_sprites(ppu);
    for (std::uint8_t index = 0; index < 9; ++index) {
        ppu.oam_write(static_cast<std::uint8_t>(index * 4U), 0xFF);
    }
    ppu.cpu_write(0x2001, 0x10);
    clock_until(ppu, -1, 258);
    expect(ppu.state().sprite_count == 8, "scanline cache is limited to eight sprites");
    expect((ppu.state().status & 0x20U) != 0U, "ninth visible sprite sets overflow");
}

void sprite_patterns_honor_table_and_flip_controls() {
    nes::Ppu ppu;
    hide_all_sprites(ppu);
    ppu.oam_write(0, 0xFF);
    ppu.oam_write(1, 2);
    ppu.oam_write(2, 0x40);
    ppu.ppu_write(0x1020, 0x80);
    ppu.ppu_write(0x1028, 0x40);
    ppu.cpu_write(0x2000, 0x08);
    ppu.cpu_write(0x2001, 0x10);
    clock_until(ppu, -1, 258);

    expect(ppu.sprite_pattern_low()[0] == 0x01,
           "horizontal flip reverses the sprite low pattern plane");
    expect(ppu.sprite_pattern_high()[0] == 0x02,
           "horizontal flip reverses the sprite high pattern plane");
}

void vertical_flip_selects_the_opposite_sprite_row() {
    nes::Ppu ppu;
    hide_all_sprites(ppu);
    ppu.oam_write(0, 0xFF);
    ppu.oam_write(1, 1);
    ppu.oam_write(2, 0x80);
    ppu.ppu_write(0x0010, 0x11);
    ppu.ppu_write(0x0017, 0x77);
    ppu.cpu_write(0x2001, 0x10);
    clock_until(ppu, -1, 258);
    expect(ppu.sprite_pattern_low()[0] == 0x77,
           "vertical flip fetches the opposite row of an 8 by 8 sprite");
}

void sixteen_pixel_sprites_select_the_table_from_the_tile_id() {
    nes::Ppu ppu;
    hide_all_sprites(ppu);
    ppu.oam_write(0, 0xFF);
    ppu.oam_write(1, 3);
    ppu.ppu_write(0x1020, 0x5A);
    ppu.cpu_write(0x2000, 0x20);
    ppu.cpu_write(0x2001, 0x10);
    clock_until(ppu, -1, 258);
    expect(ppu.sprite_pattern_low()[0] == 0x5A,
           "8 by 16 sprites select their pattern table from tile bit zero");
}

void sixteen_pixel_sprites_fetch_both_tiles_and_flip_vertically() {
    nes::Ppu normal;
    hide_all_sprites(normal);
    normal.oam_write(0, 0xFF);
    normal.oam_write(1, 2);
    normal.ppu_write(0x0030, 0x6C);
    normal.cpu_write(0x2000, 0x20);
    normal.cpu_write(0x2001, 0x10);
    clock_until(normal, 7, 258);
    expect(normal.sprite_pattern_low()[0] == 0x6C,
           "the lower half of an 8 by 16 sprite uses the following tile");

    nes::Ppu flipped;
    hide_all_sprites(flipped);
    flipped.oam_write(0, 0xFF);
    flipped.oam_write(1, 2);
    flipped.oam_write(2, 0x80);
    flipped.ppu_write(0x0027, 0xA7);
    flipped.cpu_write(0x2000, 0x20);
    flipped.cpu_write(0x2001, 0x10);
    clock_until(flipped, 7, 258);
    expect(flipped.sprite_pattern_low()[0] == 0xA7,
           "vertical flip crosses the two tiles of an 8 by 16 sprite");
}

void sprite_pixels_render_from_object_memory() {
    nes::Ppu ppu;
    hide_all_sprites(ppu);
    ppu.oam_write(0, 0xFF);
    ppu.oam_write(1, 1);
    ppu.oam_write(2, 0x00);
    ppu.oam_write(3, 0);
    ppu.ppu_write(0x0010, 0x80);
    ppu.ppu_write(0x3F11, 0x26);
    ppu.cpu_write(0x2001, 0x14);
    clock_until(ppu, 0, 2);
    expect(ppu.pixel(0, 0) == 0x26, "sprite pattern pixel reaches the framebuffer");
}

void sprite_x_counter_delays_pattern_shifting() {
    nes::Ppu ppu;
    hide_all_sprites(ppu);
    ppu.oam_write(0, 0xFF);
    ppu.oam_write(1, 1);
    ppu.oam_write(3, 3);
    ppu.ppu_write(0x0010, 0x80);
    ppu.ppu_write(0x3F00, 0x0F);
    ppu.ppu_write(0x3F11, 0x30);
    ppu.cpu_write(0x2001, 0x14);
    clock_until(ppu, 0, 5);

    expect(ppu.pixel(2, 0) == 0x0F, "sprite remains transparent before its X coordinate");
    expect(ppu.pixel(3, 0) == 0x30, "sprite begins rendering at its OAM X coordinate");
}

void fill_background(nes::Ppu& ppu, std::uint8_t tile) {
    for (std::uint16_t address = 0x2000; address < 0x23C0; ++address) {
        ppu.ppu_write(address, tile);
    }
}

nes::Ppu layered_pixel_ppu(std::uint8_t sprite_attributes,
                           std::uint8_t sprite_pattern = 0x80) {
    nes::Ppu ppu;
    hide_all_sprites(ppu);
    fill_background(ppu, 1);
    ppu.oam_write(0, 0xFF);
    ppu.oam_write(1, 2);
    ppu.oam_write(2, sprite_attributes);
    ppu.oam_write(3, 0);
    for (std::uint16_t row = 0; row < 8; ++row) {
        ppu.ppu_write(static_cast<std::uint16_t>(0x0010U + row), 0xFF);
    }
    ppu.ppu_write(0x0020, sprite_pattern);
    ppu.ppu_write(0x3F01, 0x16);
    ppu.ppu_write(0x3F11, 0x26);
    ppu.cpu_write(0x2001, 0x1E);
    return ppu;
}

void sprite_priority_selects_foreground_or_background() {
    auto front = layered_pixel_ppu(0x00);
    clock_until(front, 0, 2);
    expect(front.pixel(0, 0) == 0x26, "front-priority sprite wins an opaque collision");

    auto behind = layered_pixel_ppu(0x20);
    clock_until(behind, 0, 2);
    expect(behind.pixel(0, 0) == 0x16, "behind-priority sprite yields to opaque background");
}

void transparent_sprite_pixels_reveal_the_background() {
    auto ppu = layered_pixel_ppu(0x00, 0x00);
    clock_until(ppu, 0, 2);
    expect(ppu.pixel(0, 0) == 0x16, "transparent sprite pixel leaves background visible");
}

void sprite_zero_collision_sets_the_status_flag() {
    auto ppu = layered_pixel_ppu(0x20);
    clock_until(ppu, 0, 2);
    expect((ppu.state().status & 0x40U) != 0U,
           "opaque sprite zero collision sets the hit flag regardless of priority");
}

void other_sprites_do_not_trigger_sprite_zero_hit() {
    auto ppu = layered_pixel_ppu(0x00);
    ppu.oam_write(4, ppu.oam_read(0));
    ppu.oam_write(5, ppu.oam_read(1));
    ppu.oam_write(6, ppu.oam_read(2));
    ppu.oam_write(7, ppu.oam_read(3));
    ppu.oam_write(0, 0x80);
    clock_until(ppu, 0, 2);
    expect((ppu.state().status & 0x40U) == 0U,
           "opaque collision from a later sprite does not set sprite-zero hit");
}

void left_edge_clipping_suppresses_sprite_zero_hit() {
    auto ppu = layered_pixel_ppu(0x00);
    ppu.cpu_write(0x2001, 0x18);
    clock_until(ppu, 0, 2);
    expect((ppu.state().status & 0x40U) == 0U,
           "clipped left-edge pixels do not trigger sprite-zero hit");
}

}  // namespace

int run_ppu_sprite_tests() {
    const auto before = nes::test::failures;
    cpu_oam_registers_address_object_memory();
    oam_data_reads_do_not_increment_the_address();
    direct_oam_access_supports_dma_transfers();
    reset_preserves_object_memory_but_resets_its_address();
    visible_sprites_are_selected_in_oam_order();
    ninth_visible_sprite_sets_overflow();
    sprite_patterns_honor_table_and_flip_controls();
    vertical_flip_selects_the_opposite_sprite_row();
    sixteen_pixel_sprites_select_the_table_from_the_tile_id();
    sixteen_pixel_sprites_fetch_both_tiles_and_flip_vertically();
    sprite_pixels_render_from_object_memory();
    sprite_x_counter_delays_pattern_shifting();
    sprite_priority_selects_foreground_or_background();
    transparent_sprite_pixels_reveal_the_background();
    sprite_zero_collision_sets_the_status_flag();
    other_sprites_do_not_trigger_sprite_zero_hit();
    left_edge_clipping_suppresses_sprite_zero_hit();
    return nes::test::failures - before;
}
