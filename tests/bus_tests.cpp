#include "nes/bus.hpp"
#include "nes/cartridge.hpp"
#include "test_harness.hpp"

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

using nes::test::expect;

std::shared_ptr<nes::Cartridge> nrom_cartridge() {
    std::vector<std::uint8_t> image(16U + 16U * 1024U + 8U * 1024U, 0);
    image[0] = 'N';
    image[1] = 'E';
    image[2] = 'S';
    image[3] = 0x1A;
    image[4] = 1;
    image[5] = 1;
    image[16] = 0xA9;
    image[16U + 0x3FFCU] = 0x00;
    image[16U + 0x3FFDU] = 0x80;
    return nes::Cartridge::from_ines(image);
}

std::shared_ptr<nes::Cartridge> uxrom_cartridge() {
    constexpr std::size_t bank_size = 16U * 1024U;
    std::vector<std::uint8_t> image(16U + 4U * bank_size, 0);
    image[0] = 'N';
    image[1] = 'E';
    image[2] = 'S';
    image[3] = 0x1A;
    image[4] = 4;
    image[6] = 0x20;
    image[16] = 0x10;
    image[16U + bank_size] = 0x20;
    image[16U + 2U * bank_size] = 0x30;
    image[16U + 3U * bank_size] = 0x40;
    return nes::Cartridge::from_ines(image);
}

std::shared_ptr<nes::Cartridge> chr_ram_cartridge() {
    std::vector<std::uint8_t> image(16U + 16U * 1024U, 0);
    image[0] = 'N';
    image[1] = 'E';
    image[2] = 'S';
    image[3] = 0x1A;
    image[4] = 1;
    return nes::Cartridge::from_ines(image);
}

void internal_ram_repeats_through_its_cpu_window() {
    nes::Bus bus;
    bus.cpu_write(0x0002, 0x5A);
    expect(bus.cpu_read(0x0002) == 0x5A, "bus reads internal CPU RAM");
    expect(bus.cpu_read(0x0802) == 0x5A, "first CPU RAM mirror repeats data");
    expect(bus.cpu_read(0x1002) == 0x5A, "second CPU RAM mirror repeats data");
    expect(bus.cpu_read(0x1802) == 0x5A, "third CPU RAM mirror repeats data");

    bus.cpu_write(0x17FF, 0xA5);
    expect(bus.cpu_read(0x07FF) == 0xA5, "writes use the mirrored RAM address");
}

void cartridge_owns_the_program_rom_window() {
    nes::Bus bus;
    bus.insert_cartridge(nrom_cartridge());
    expect(bus.cpu_read(0x8000) == 0xA9, "bus routes high CPU reads to cartridge");
    expect(bus.cpu_read(0xC000) == 0xA9, "bus preserves NROM program mirroring");
    bus.cpu_write(0x8000, 0x00);
    expect(bus.cpu_read(0x8000) == 0xA9, "program ROM ignores CPU writes");
}

void cartridge_ram_is_visible_on_the_cpu_bus() {
    nes::Bus bus;
    bus.insert_cartridge(nrom_cartridge());
    bus.cpu_write(0x6000, 0x57);
    bus.cpu_write(0x7FFF, 0x75);
    expect(bus.cpu_read(0x6000) == 0x57, "bus routes the first cartridge RAM byte");
    expect(bus.cpu_read(0x7FFF) == 0x75, "bus routes the last cartridge RAM byte");
}

void mapper_register_writes_flow_through_the_bus() {
    nes::Bus bus;
    bus.insert_cartridge(uxrom_cartridge());
    expect(bus.cpu_read(0x8000) == 0x10, "bus exposes the initial UxROM bank");
    expect(bus.cpu_read(0xC000) == 0x40, "bus exposes the fixed UxROM bank");
    bus.cpu_write(0x8000, 2);
    expect(bus.cpu_read(0x8000) == 0x30, "bus forwards UxROM bank selection");
    expect(bus.cpu_read(0xC000) == 0x40, "bank switching leaves the fixed bank intact");
    bus.reset();
    nes::test::drain(bus.cpu());
    expect(bus.cpu_read(0x8000) == 0x10, "bus reset resets the cartridge mapper");
}

void ppu_registers_repeat_through_the_cpu_window() {
    nes::Bus bus;
    bus.cpu_write(0x2008, 0x04);
    expect(bus.cpu_read(0x2000, true) == 0x04, "bus mirrors PPUCTRL every eight bytes");

    bus.cpu_write(0x3FFE, 0x3F);
    bus.cpu_write(0x2006, 0x00);
    bus.cpu_write(0x2007, 0x2A);
    expect(bus.ppu().ppu_read(0x3F00) == 0x2A,
           "bus routes mirrored PPUADDR and PPUDATA writes");
}

void cartridge_character_memory_is_connected_to_the_ppu() {
    nes::Bus bus;
    const auto cartridge = chr_ram_cartridge();
    bus.insert_cartridge(cartridge);
    bus.ppu().ppu_write(0x1234, 0xA5);
    expect(bus.ppu().ppu_read(0x1234) == 0xA5,
           "inserting a cartridge connects its character memory to the PPU");
    expect(bus.cartridge() == cartridge, "CPU and PPU share the inserted cartridge");
}

void reset_clears_ppu_register_state() {
    nes::Bus bus;
    bus.cpu_write(0x2000, 0x80);
    bus.cpu_write(0x2005, 0xFF);
    bus.reset();
    expect(bus.ppu().state().control == 0, "bus reset clears PPUCTRL");
    expect(!bus.ppu().state().write_latch, "bus reset clears the PPU write latch");
}

void oam_dma_copies_a_cpu_memory_page() {
    nes::Bus bus;
    bus.reset();
    for (std::uint16_t offset = 0; offset < 256; ++offset) {
        bus.cpu_write(static_cast<std::uint16_t>(0x0200U + offset),
                      static_cast<std::uint8_t>(offset ^ 0x5AU));
    }
    bus.cpu_write(0x2003, 0x10);
    const auto cpu_cycles = bus.cpu().state().cycles;
    const auto start_clock = bus.system_clock();
    bus.cpu_write(0x4014, 0x02);
    expect(bus.dma_active(), "$4014 write begins an OAM DMA transfer");

    for (int clocks = 0; bus.dma_active() && clocks < 2'000; ++clocks) {
        bus.clock();
    }
    expect(!bus.dma_active(), "OAM DMA completes after one memory page");
    expect(bus.ppu().oam_read(0x10) == 0x5A, "DMA begins at the current OAM address");
    expect(bus.ppu().oam_read(0x0F) == static_cast<std::uint8_t>(0xFFU ^ 0x5AU),
           "DMA wraps across the end of object memory");
    expect(bus.cpu().state().cycles == cpu_cycles, "OAM DMA stalls the CPU");
    const auto elapsed = bus.system_clock() - start_clock;
    expect(elapsed >= 513U * 3U && elapsed <= 514U * 3U,
           "OAM DMA consumes 513 or 514 CPU cycles");
}

void controllers_latch_and_shift_both_ports() {
    nes::Bus bus;
    bus.set_controller_state(0, 0b1010'0101);
    bus.set_controller_state(1, 0b0101'1010);
    expect(bus.controller_state(0) == 0b1010'0101,
           "the bus retains the first controller's buttons");
    expect(bus.controller_state(1) == 0b0101'1010,
           "the bus retains the second controller's buttons");

    bus.cpu_write(0x4016, 1);
    bus.cpu_write(0x4016, 0);
    for (int bit = 0; bit < 8; ++bit) {
        expect(bus.cpu_read(0x4016) == ((0b1010'0101U >> bit) & 1U),
               "port one shifts A through Right in button order");
        expect(bus.cpu_read(0x4017) == ((0b0101'1010U >> bit) & 1U),
               "port two has an independent controller shift register");
    }
    expect(bus.cpu_read(0x4016) == 1, "controller reads return one after eight buttons");
    expect(bus.cpu_read(0x4017) == 1, "both ports return one after their button data");
}

void controller_strobe_reports_live_a_button() {
    nes::Bus bus;
    bus.set_controller_state(0, 0x01);
    bus.cpu_write(0x4016, 1);
    expect(bus.cpu_read(0x4016) == 1, "a high strobe repeatedly reports the A button");
    expect(bus.cpu_read(0x4016) == 1, "a high strobe does not advance the controller");
    bus.set_controller_state(0, 0x00);
    expect(bus.cpu_read(0x4016) == 0, "a high strobe observes live controller input");

    bus.set_controller_state(0, 0x03);
    bus.cpu_write(0x4016, 0);
    expect(bus.cpu_read(0x4016, true) == 1, "read-only input inspection sees the next bit");
    expect(bus.cpu_read(0x4016, true) == 1, "read-only input inspection does not shift");
    expect(bus.cpu_read(0x4016) == 1, "a normal read consumes the inspected A button");
    expect(bus.cpu_read(0x4016) == 1, "the next read returns the B button");
}

void invalid_controller_ports_are_rejected() {
    nes::Bus bus;
    bool write_rejected = false;
    bool read_rejected = false;
    try {
        bus.set_controller_state(2, 0);
    } catch (const std::out_of_range&) {
        write_rejected = true;
    }
    try {
        static_cast<void>(bus.controller_state(2));
    } catch (const std::out_of_range&) {
        read_rejected = true;
    }
    expect(write_rejected, "the bus rejects invalid controller writes");
    expect(read_rejected, "the bus rejects invalid controller reads");
}

void empty_cartridges_are_rejected() {
    nes::Bus bus;
    bool rejected = false;
    try {
        bus.insert_cartridge(nullptr);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    expect(rejected, "bus rejects an empty cartridge slot");
}

void reset_vector_flows_from_cartridge_to_cpu() {
    nes::Bus bus;
    const auto cartridge = nrom_cartridge();
    bus.insert_cartridge(cartridge);
    expect(bus.cartridge() == cartridge, "bus retains the inserted cartridge");
    bus.reset();
    nes::test::drain(bus.cpu());
    expect(bus.cpu().state().program_counter == 0x8000,
           "CPU reset vector is read through the cartridge bus");
}

void unmapped_cpu_addresses_have_open_bus_defaults() {
    nes::Bus bus;
    expect(bus.cpu_read(0x4000) == 0, "unmapped CPU reads return zero");
    bus.cpu_write(0x4000, 0xFF);
    expect(bus.cpu_read(0x4000) == 0, "unmapped CPU writes have no effect");
}

}  // namespace

int run_bus_tests() {
    const auto before = nes::test::failures;
    internal_ram_repeats_through_its_cpu_window();
    cartridge_owns_the_program_rom_window();
    cartridge_ram_is_visible_on_the_cpu_bus();
    mapper_register_writes_flow_through_the_bus();
    ppu_registers_repeat_through_the_cpu_window();
    cartridge_character_memory_is_connected_to_the_ppu();
    reset_clears_ppu_register_state();
    oam_dma_copies_a_cpu_memory_page();
    controllers_latch_and_shift_both_ports();
    controller_strobe_reports_live_a_button();
    invalid_controller_ports_are_rejected();
    empty_cartridges_are_rejected();
    reset_vector_flows_from_cartridge_to_cpu();
    unmapped_cpu_addresses_have_open_bus_defaults();
    return nes::test::failures - before;
}
