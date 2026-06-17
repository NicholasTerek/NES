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

}  // namespace

int run_ppu_sprite_tests() {
    const auto before = nes::test::failures;
    cpu_oam_registers_address_object_memory();
    oam_data_reads_do_not_increment_the_address();
    direct_oam_access_supports_dma_transfers();
    reset_preserves_object_memory_but_resets_its_address();
    visible_sprites_are_selected_in_oam_order();
    ninth_visible_sprite_sets_overflow();
    return nes::test::failures - before;
}
