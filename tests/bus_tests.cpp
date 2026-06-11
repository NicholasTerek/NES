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
    empty_cartridges_are_rejected();
    reset_vector_flows_from_cartridge_to_cpu();
    unmapped_cpu_addresses_have_open_bus_defaults();
    return nes::test::failures - before;
}
