#include "nes/ppu.hpp"
#include "test_harness.hpp"

#include <cstdint>

namespace {

using nes::test::expect;

void control_and_mask_registers_retain_configuration() {
    nes::Ppu ppu;
    ppu.cpu_write(0x2000, 0x9F);
    ppu.cpu_write(0x2001, 0x1B);
    const auto state = ppu.state();
    expect(state.control == 0x9F, "PPUCTRL retains its configuration");
    expect(state.mask == 0x1B, "PPUMASK retains its configuration");
    expect((state.temporary_address & 0x0C00U) == 0x0C00U,
           "PPUCTRL copies nametable selection into the temporary address");
    expect(ppu.cpu_read(0x2008, true) == 0x9F, "PPU register addresses mirror every eight bytes");
}

void scroll_writes_populate_the_loopy_address() {
    nes::Ppu ppu;
    ppu.cpu_write(0x2005, 0xAD);
    auto state = ppu.state();
    expect(state.fine_x == 5, "first PPUSCROLL write stores fine X");
    expect((state.temporary_address & 0x001FU) == 21U,
           "first PPUSCROLL write stores coarse X");
    expect(state.write_latch, "first PPUSCROLL write advances the shared latch");

    ppu.cpu_write(0x2005, 0x6B);
    state = ppu.state();
    expect(((state.temporary_address >> 5U) & 0x001FU) == 13U,
           "second PPUSCROLL write stores coarse Y");
    expect(((state.temporary_address >> 12U) & 0x0007U) == 3U,
           "second PPUSCROLL write stores fine Y");
    expect(!state.write_latch, "second PPUSCROLL write resets the shared latch");
}

void address_and_data_writes_follow_the_increment_mode() {
    nes::Ppu ppu;
    ppu.cpu_write(0x2006, 0x3F);
    ppu.cpu_write(0x2006, 0x00);
    ppu.cpu_write(0x2007, 0x2A);
    expect(ppu.ppu_read(0x3F00) == 0x2A, "PPUDATA writes through the PPU address bus");
    expect(ppu.state().vram_address == 0x3F01, "PPUDATA defaults to single-byte increments");

    ppu.cpu_write(0x2000, 0x04);
    ppu.cpu_write(0x2006, 0x20);
    ppu.cpu_write(0x2006, 0x00);
    ppu.cpu_write(0x2007, 0x55);
    expect(ppu.ppu_read(0x2000) == 0x55, "PPUDATA writes nametable memory");
    expect(ppu.state().vram_address == 0x2020, "PPUCTRL selects 32-byte increments");
}

void data_reads_use_the_internal_buffer() {
    nes::Ppu ppu;
    ppu.ppu_write(0x2000, 0x12);
    ppu.ppu_write(0x2001, 0x34);
    ppu.cpu_write(0x2006, 0x20);
    ppu.cpu_write(0x2006, 0x00);
    expect(ppu.cpu_read(0x2007) == 0x00, "first nametable read returns the old PPU buffer");
    expect(ppu.cpu_read(0x2007) == 0x12, "second nametable read returns buffered data");
    expect(ppu.state().data_buffer == 0x34, "PPUDATA refills its buffer after each read");

    ppu.ppu_write(0x3F00, 0x2C);
    ppu.cpu_write(0x2006, 0x3F);
    ppu.cpu_write(0x2006, 0x00);
    expect(ppu.cpu_read(0x2007) == 0x2C, "palette reads bypass the PPU data buffer");
}

void status_reads_reset_the_shared_write_latch() {
    nes::Ppu ppu;
    ppu.cpu_write(0x2005, 0xFF);
    expect(ppu.state().write_latch, "scroll write primes the shared latch");
    expect((ppu.cpu_read(0x2002) & 0x1FU) == 0x1FU,
           "PPUSTATUS exposes the low bits of the PPU open bus");
    expect(!ppu.state().write_latch, "PPUSTATUS read resets the shared latch");
}

void grayscale_mode_masks_palette_output() {
    nes::Ppu ppu;
    ppu.ppu_write(0x3F00, 0x2F);
    expect(ppu.ppu_read(0x3F00) == 0x2F, "normal palette reads retain six colour bits");
    ppu.cpu_write(0x2001, 0x01);
    expect(ppu.ppu_read(0x3F00) == 0x20, "grayscale mode retains only luminance bits");
}

}  // namespace

int run_ppu_register_tests() {
    const auto before = nes::test::failures;
    control_and_mask_registers_retain_configuration();
    scroll_writes_populate_the_loopy_address();
    address_and_data_writes_follow_the_increment_mode();
    data_reads_use_the_internal_buffer();
    status_reads_reset_the_shared_write_latch();
    grayscale_mode_masks_palette_output();
    return nes::test::failures - before;
}
