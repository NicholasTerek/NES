#include "nes/ppu.hpp"
#include "test_harness.hpp"

#include <cstdint>

namespace {

using nes::test::expect;

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

}  // namespace

int run_ppu_sprite_tests() {
    const auto before = nes::test::failures;
    cpu_oam_registers_address_object_memory();
    oam_data_reads_do_not_increment_the_address();
    direct_oam_access_supports_dma_transfers();
    reset_preserves_object_memory_but_resets_its_address();
    return nes::test::failures - before;
}
